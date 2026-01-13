#include "eos_layer.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

#include <Lmcons.h>

#include "eos_logging_manager.h"
#include "eos_threading.h"
#include "ns_version.h"

namespace
{

constexpr int kAsyncPollIntervalMs = 10;
constexpr char kDefaultSocketName[] = "ion";
std::recursive_mutex g_sdkMutex;

std::string Base64Decode(const std::string& encoded)
{
    static constexpr char kBase64Chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string decoded;
    decoded.reserve((encoded.size() * 3) / 4);

    std::array<int, 256> decodeTable{};
    decodeTable.fill(-1);
    for (int i = 0; i < 64; ++i)
        decodeTable[static_cast<unsigned char>(kBase64Chars[i])] = i;

    int val = 0;
    int bits = -8;
    for (unsigned char c : encoded)
    {
        if (decodeTable[c] == -1)
            break;
        val = (val << 6) + decodeTable[c];
        bits += 6;
        if (bits >= 0)
        {
            decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return decoded;
}

struct PromiseCleanup
{
    template <typename T>
    static void SetValue(std::promise<T>* promise, const T& value)
    {
        if (!promise)
            return;
        try
        {
            promise->set_value(value);
        }
        catch (...)
        {
            // Ignore multiple set attempts.
        }
    }
};

std::string BuildDisplayName()
{
	const char* name = g_pCVar->FindVar("name")->GetString();
	if (name && name[0] != '\0')
		return std::string(name);

    return "ion_user";
}

void EOS_CALL OnCreateDeviceIdCallback(const EOS_Connect_CreateDeviceIdCallbackInfo* data)
{
    using PromisePtr = std::shared_ptr<std::promise<EOS_EResult>>;
    auto* holder = static_cast<PromisePtr*>(data->ClientData);
    if (!holder)
        return;

    PromiseCleanup::SetValue(holder->get(), data->ResultCode);
    delete holder; // drop our ref; promise is destroyed when all shared_ptrs go away
}

void EOS_CALL OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* data)
{
    using PromisePtr = std::shared_ptr<std::promise<eos::LoginCallbackPayload>>;
    auto* holder = static_cast<PromisePtr*>(data->ClientData);
    if (!holder)
        return;

    eos::LoginCallbackPayload payload{};
    payload.result = data->ResultCode;
    payload.user = data->LocalUserId;
    payload.continuanceToken = data->ContinuanceToken;

    PromiseCleanup::SetValue(holder->get(), payload);
    delete holder; // same as above
}

void EOS_CALL OnIncomingConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* data)
{
    auto* layer = data ? static_cast<eos::EosLayer*>(data->ClientData) : nullptr;
    if (layer)
    {
        layer->HandleConnectionRequest(data);
    }
}

void EOS_CALL OnPeerConnectionEstablished(const EOS_P2P_OnPeerConnectionEstablishedInfo* data)
{
    auto* layer = data ? static_cast<eos::EosLayer*>(data->ClientData) : nullptr;
    if (layer)
    {
        layer->HandleConnectionEstablished(data);
    }
}

} // namespace

namespace eos
{

std::recursive_mutex& GetSdkMutex()
{
    return g_sdkMutex;
}

EosLayer& EosLayer::Instance()
{
    static EosLayer instance;
    return instance;
}

EosLayer::~EosLayer()
{
    Shutdown();
}

bool EosLayer::Initialize(const char* productId,
                          const char* sandboxId,
                          const char* deploymentId,
                          const char* productName,
                          const char* productVersion)
{
    if (IsInitialized())
        return true;

    if (!productId || !sandboxId || !deploymentId)
        return false;

    m_productName = productName ? productName : "ion";
    m_productVersion = productVersion ? productVersion : "1.0.0";

    EOS_InitializeOptions initOptions{};
    initOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
    initOptions.ProductName = m_productName.c_str();
    initOptions.ProductVersion = m_productVersion.c_str();

    EOS_EResult initResult = EOS_EResult::EOS_UnexpectedError;
    {
        SdkLock lock(GetSdkMutex());
        initResult = EOS_Initialize(&initOptions);
    }
    if (initResult != EOS_EResult::EOS_Success && initResult != EOS_EResult::EOS_AlreadyConfigured)
    {
        NS::log::EOS->error("Initialize failed ({})", static_cast<int>(initResult));
        return false;
    }

    m_sdkInitialized.store(true, std::memory_order_release);
    Logging::Initialize();

    static const std::string clientId = EOS_CLIENT_ID;
    static const std::string clientSecret = Base64Decode(EOS_CLIENT_SECRET_B64);

    EOS_Platform_Options platformOptions{};
    platformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
    platformOptions.ProductId = productId;
    platformOptions.SandboxId = sandboxId;
    platformOptions.DeploymentId = deploymentId;
    platformOptions.ClientCredentials.ClientId = clientId.c_str();
    platformOptions.ClientCredentials.ClientSecret = clientSecret.c_str();
    platformOptions.Flags = EOS_PF_DISABLE_OVERLAY;
    platformOptions.TickBudgetInMilliseconds = 1;

    {
        SdkLock lock(GetSdkMutex());
        m_platformHandle = EOS_Platform_Create(&platformOptions);
    }
    if (!m_platformHandle)
    {
        NS::log::EOS->error("Failed to create platform handle");
        Shutdown();
        return false;
    }

    {
        SdkLock lock(GetSdkMutex());
        m_connectHandle = EOS_Platform_GetConnectInterface(m_platformHandle);
        m_p2pHandle = EOS_Platform_GetP2PInterface(m_platformHandle);
    }
    if (!m_connectHandle || !m_p2pHandle)
    {
        NS::log::EOS->error("Missing required interfaces");
        Shutdown();
        return false;
    }

    m_defaultSocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
    std::snprintf(m_defaultSocketId.SocketName,
                  EOS_P2P_SOCKETID_SOCKETNAME_SIZE,
                  "%s",
                  kDefaultSocketName);

    EOS_P2P_SetRelayControlOptions relayOptions{};
    relayOptions.ApiVersion = EOS_P2P_SETRELAYCONTROL_API_LATEST;
    relayOptions.RelayControl = EOS_ERelayControl::EOS_RC_AllowRelays;
    {
        SdkLock lock(GetSdkMutex());
        EOS_P2P_SetRelayControl(m_p2pHandle, &relayOptions);
    }

    m_fakeIpLayer = std::make_unique<FakeIpLayer>(m_p2pHandle, nullptr);

    if (!CreateDeviceId())
    {
		NS::log::EOS->error("Failed to create device ID");
        Shutdown();
        return false;
    }

    if (!LoginWithDeviceId())
    {
		NS::log::EOS->error("Failed to login with device ID");
        Shutdown();
        return false;
    }

    StartPumpThread();
    m_initialized.store(true, std::memory_order_release);
    return true;
}

void EosLayer::Shutdown()
{
    StopPumpThread();
    UnregisterSocketNotifications();

    if (m_fakeIpLayer)
    {
        m_fakeIpLayer->Clear();
        m_fakeIpLayer.reset();
    }

    m_localUser.store(nullptr, std::memory_order_release);

    if (m_platformHandle)
    {
        {
            SdkLock lock(GetSdkMutex());
            EOS_Platform_Release(m_platformHandle);
        }
        m_platformHandle = nullptr;
    }

    m_connectHandle = nullptr;
    m_p2pHandle = nullptr;

    if (m_sdkInitialized.exchange(false))
    {
        Logging::Shutdown();
        {
            SdkLock lock(GetSdkMutex());
            EOS_Shutdown();
        }
    }

    m_initialized.store(false, std::memory_order_release);
}

bool EosLayer::CreateDeviceId()
{
    if (!m_connectHandle)
        return false;

    auto promise = std::make_shared<std::promise<EOS_EResult>>();
    auto future = promise->get_future();

    EOS_Connect_CreateDeviceIdOptions options{};
    options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    options.DeviceModel = "PC Windows";

    // heap‑allocate a copy of the shared_ptr so the callback always has a live promise
    auto* holder = new std::shared_ptr<std::promise<EOS_EResult>>(promise);

    {
        SdkLock lock(GetSdkMutex());
        EOS_Connect_CreateDeviceId(m_connectHandle, &options, holder, &OnCreateDeviceIdCallback);
    }

    EOS_EResult result = EOS_EResult::EOS_UnexpectedError;
    if (!WaitForResult(future, &result, 6000))
    {
        NS::log::EOS->error("Timed out waiting for CreateDeviceId");
        return false;
    }

    if (result == EOS_EResult::EOS_Success || result == EOS_EResult::EOS_DuplicateNotAllowed)
        return true;

    NS::log::EOS->error("CreateDeviceId failed ({})", static_cast<int>(result));
    return false;
}

bool EosLayer::LoginWithDeviceId()
{
    if (!m_connectHandle)
        return false;

    auto promise = std::make_shared<std::promise<LoginCallbackPayload>>();
    auto future = promise->get_future();

    auto displayName = std::make_shared<std::string>(BuildDisplayName());

    EOS_Connect_UserLoginInfo userInfo{};
    userInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
    userInfo.DisplayName = displayName->c_str();

    EOS_Connect_Credentials credentials{};
    credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
    credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
    credentials.Token = nullptr;

    EOS_Connect_LoginOptions loginOptions{};
    loginOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
    loginOptions.Credentials = &credentials;
    loginOptions.UserLoginInfo = &userInfo;

    auto* holder = new std::shared_ptr<std::promise<LoginCallbackPayload>>(promise);

    {
        SdkLock lock(GetSdkMutex());
        EOS_Connect_Login(m_connectHandle, &loginOptions, holder, &OnConnectLoginCallback);
    }

    LoginCallbackPayload payload{};
    if (!WaitForLogin(future, &payload, 6000))
    {
        NS::log::EOS->error("Timed out waiting for login");
        return false;
    }

    if (payload.result == EOS_EResult::EOS_Success && payload.user)
    {
        m_localUser.store(payload.user, std::memory_order_release);

        char puidBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
        int32_t puidBufferLen = static_cast<int32_t>(sizeof(puidBuffer));
        bool printedUser = false;
        {
            SdkLock lock(GetSdkMutex());
            printedUser = EOS_ProductUserId_ToString(payload.user, puidBuffer, &puidBufferLen) == EOS_EResult::EOS_Success;
        }
        if (printedUser)
        {
            NS::log::EOS->info("Logged in as {}", puidBuffer);
        }
        else
        {
            NS::log::EOS->info("Logged in with ProductUserId (string conversion failed)");
        }

        if (m_fakeIpLayer)
        {
            m_fakeIpLayer->SetLocalUser(payload.user);
        }
        if (!RegisterSocketNotifications())
        {
            NS::log::EOS->error("Failed to register socket notifications");
            return false;
        }
        return true;
    }

    if (payload.result == EOS_EResult::EOS_InvalidUser && payload.continuanceToken)
    {
        NS::log::EOS->info("DeviceID login requires account linking (continuance token)");
    }
    else
    {
        NS::log::EOS->error("Login failed ({})", static_cast<int>(payload.result));
    }

    return false;
}

bool EosLayer::RequestReloginAsync()
{
    // If EOS isn't initialized, don't try to re-login.
    if (!IsInitialized())
        return false;

    bool expected = false;
    if (!m_reloginInProgress.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        // Already trying to re-login; don't spam.
        return false;
    }

    std::thread([this]()
    {
        NS::log::EOS->warn("LocalUserId invalid, attempting EOS re-login");

        if (!LoginWithDeviceId())
        {
            NS::log::EOS->error("EOS re-login failed, stopping P2P receive");
        }

        m_reloginInProgress.store(false, std::memory_order_release);
    }).detach();

    return true;
}

bool EosLayer::WaitForResult(std::future<EOS_EResult>& future,
                             EOS_EResult* outResult,
                             int timeoutMs)
{
    const auto start = std::chrono::steady_clock::now();
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        PumpOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(kAsyncPollIntervalMs));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > timeoutMs)
            return false;
    }

    if (outResult)
    {
        *outResult = future.get();
    }
    else
    {
        future.get();
    }
    return true;
}

bool EosLayer::WaitForLogin(std::future<LoginCallbackPayload>& future,
                            LoginCallbackPayload* outPayload,
                            int timeoutMs)
{
    const auto start = std::chrono::steady_clock::now();
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        PumpOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(kAsyncPollIntervalMs));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > timeoutMs)
            return false;
    }

    if (outPayload)
    {
        *outPayload = future.get();
    }
    else
    {
        future.get();
    }
    return true;
}

void EosLayer::PumpOnce()
{
    std::lock_guard guard(m_tickMutex);
    try
    {
        if (m_platformHandle)
        {
            SdkLock lock(GetSdkMutex());
            EOS_Platform_Tick(m_platformHandle);
        }
        if (m_fakeIpLayer)
            m_fakeIpLayer->PumpIncoming();
    }
    catch (const std::exception& e)
    {
        NS::log::EOS->error("Exception in PumpOnce: {}", e.what());
    }
}

void EosLayer::StartPumpThread()
{
    if (m_shouldRun.exchange(true))
        return;

    m_pumpThread = std::thread([this]()
    {
        while (m_shouldRun.load(std::memory_order_acquire))
        {
            PumpOnce();
            std::this_thread::sleep_for(std::chrono::milliseconds(kAsyncPollIntervalMs));
        }
    });
}

void EosLayer::StopPumpThread()
{
    if (!m_shouldRun.exchange(false))
        return;

    if (m_pumpThread.joinable())
    {
        m_pumpThread.join();
    }
}

bool EosLayer::RegisterSocketNotifications()
{
    if (!m_p2pHandle)
        return false;

    auto* localUser = m_localUser.load(std::memory_order_acquire);
    if (!localUser)
        return false;

    UnregisterSocketNotifications();

    if (m_defaultSocketId.ApiVersion == 0)
    {
        m_defaultSocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        std::snprintf(m_defaultSocketId.SocketName,
                      EOS_P2P_SOCKETID_SOCKETNAME_SIZE,
                      "%s",
                      kDefaultSocketName);
    }

    EOS_P2P_AddNotifyPeerConnectionRequestOptions requestOptions{};
    requestOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
    requestOptions.LocalUserId = localUser;
    requestOptions.SocketId = &m_defaultSocketId;
    {
        SdkLock lock(GetSdkMutex());
        m_connectionRequestNotificationId = EOS_P2P_AddNotifyPeerConnectionRequest(
            m_p2pHandle, &requestOptions, this, &OnIncomingConnectionRequest);
    }
    if (m_connectionRequestNotificationId == EOS_INVALID_NOTIFICATIONID)
        return false;

    EOS_P2P_AddNotifyPeerConnectionEstablishedOptions establishedOptions{};
    establishedOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONESTABLISHED_API_LATEST;
    establishedOptions.LocalUserId = localUser;
    establishedOptions.SocketId = &m_defaultSocketId;
    {
        SdkLock lock(GetSdkMutex());
        m_connectionEstablishedNotificationId = EOS_P2P_AddNotifyPeerConnectionEstablished(
            m_p2pHandle, &establishedOptions, this, &OnPeerConnectionEstablished);
    }
    if (m_connectionEstablishedNotificationId == EOS_INVALID_NOTIFICATIONID)
    {
        {
            SdkLock lock(GetSdkMutex());
            EOS_P2P_RemoveNotifyPeerConnectionRequest(m_p2pHandle, m_connectionRequestNotificationId);
        }
        m_connectionRequestNotificationId = EOS_INVALID_NOTIFICATIONID;
        return false;
    }

    NS::log::EOS->info("Registered socket notifications for \"{}\"", m_defaultSocketId.SocketName);
    return true;
}

void EosLayer::UnregisterSocketNotifications()
{
    if (m_p2pHandle && m_connectionRequestNotificationId != EOS_INVALID_NOTIFICATIONID)
    {
        SdkLock lock(GetSdkMutex());
        EOS_P2P_RemoveNotifyPeerConnectionRequest(m_p2pHandle, m_connectionRequestNotificationId);
        m_connectionRequestNotificationId = EOS_INVALID_NOTIFICATIONID;
    }
    if (m_p2pHandle && m_connectionEstablishedNotificationId != EOS_INVALID_NOTIFICATIONID)
    {
        SdkLock lock(GetSdkMutex());
        EOS_P2P_RemoveNotifyPeerConnectionEstablished(m_p2pHandle, m_connectionEstablishedNotificationId);
        m_connectionEstablishedNotificationId = EOS_INVALID_NOTIFICATIONID;
    }
}

void EosLayer::HandleConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* info)
{
    if (!info || !m_p2pHandle)
        return;

    const EOS_P2P_SocketId* socketId = info->SocketId ? info->SocketId : &m_defaultSocketId;

    EOS_P2P_AcceptConnectionOptions acceptOptions{};
    acceptOptions.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
    acceptOptions.LocalUserId = info->LocalUserId;
    acceptOptions.RemoteUserId = info->RemoteUserId;
    acceptOptions.SocketId = socketId;

    EOS_EResult result = EOS_EResult::EOS_UnexpectedError;
    {
        SdkLock lock(GetSdkMutex());
        result = EOS_P2P_AcceptConnection(m_p2pHandle, &acceptOptions);
    }
    if (result != EOS_EResult::EOS_Success)
    {
        NS::log::EOS->error("AcceptConnection failed ({})", static_cast<int>(result));
        return;
    }

    char remoteId[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    int32_t remoteIdLen = static_cast<int32_t>(sizeof(remoteId));
    bool haveRemoteId = false;
    if (info->RemoteUserId)
    {
        SdkLock lock(GetSdkMutex());
        haveRemoteId = EOS_ProductUserId_ToString(info->RemoteUserId, remoteId, &remoteIdLen) ==
                       EOS_EResult::EOS_Success;
    }
    if (haveRemoteId)
    {
        NS::log::EOS->info("Accepted connection request from {} on socket {}", remoteId, socketId->SocketName);
    }
    else
    {
        NS::log::EOS->info("Accepted connection request on socket {}", socketId->SocketName);
    }

    if (m_fakeIpLayer && info->RemoteUserId)
    {
        m_fakeIpLayer->RegisterPeer(info->RemoteUserId, socketId->SocketName, 0);
    }
}

void EosLayer::HandleConnectionEstablished(const EOS_P2P_OnPeerConnectionEstablishedInfo* info)
{
    if (!info)
        return;

    char remoteId[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    int32_t remoteIdLen = static_cast<int32_t>(sizeof(remoteId));
    bool haveRemoteId = false;
    if (info->RemoteUserId)
    {
        SdkLock lock(GetSdkMutex());
        haveRemoteId = EOS_ProductUserId_ToString(info->RemoteUserId, remoteId, &remoteIdLen) ==
                       EOS_EResult::EOS_Success;
    }
    if (haveRemoteId)
    {
        NS::log::EOS->info("Connection established with {} (type={})",
            remoteId,
            static_cast<int>(info->ConnectionType));
    }
}

} // namespace eos
