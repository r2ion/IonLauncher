#include "eos.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <wincrypt.h>

#include <eos_connect.h>
#include <eos_logging.h>
#include <eos_p2p.h>
#include <eos_sdk.h>

#include "core/tier0.h"
#include "dedicated/dedicated.h"
#include "engine/client/clientstate.h"
#include "engine/net.h"
#include "engine/r2engine.h"
#include "engine/server/server.h"
#include "tier0/frametask.h"
#include "tier1/cvar.h"
#include "tier1/strtools.h"
#include "util/version.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include <ns_version.h>

#define EOS_PRODUCT_ID "38a41893d2fa4e73801e0daed2060cb6"
#define EOS_SANDBOX_ID "d15ffd5f9b5b479aa7c382a8f6896cee"
#define EOS_DEPLOYMENT_ID "aa85e873e78244909bef018a8532741c"
#define EOS_PRODUCT_NAME "ion"
#define EOS_SOCKET_NAME "ion"
#define EOS_FAKE_ADDRESS_PREFIX 0x3FFE
#define EOS_PRODUCT_ID_BYTES 16
#define EOS_SOCKET_COUNT 2
#define EOS_MAX_ENGINE_PACKET_SIZE (256u * 1024u)
#define EOS_MAX_QUEUED_BYTES (8u * 1024u * 1024u)
#define EOS_MAX_QUEUED_PACKETS 512u
#define EOS_MAX_REASSEMBLY_BYTES (8u * 1024u * 1024u)
#define EOS_MAX_REASSEMBLIES 64u
#define EOS_REASSEMBLY_LIFETIME 5.0
#define EOS_INITIAL_LOGIN_WAIT_SECONDS 6
#define EOS_MAX_PACKETS_PER_TICK 256u
#define EOS_FRAME_MAGIC 0x31534E49u
#define EOS_FRAME_VERSION 1u
#define EOS_FRAGMENT_PAYLOAD_SIZE (EOS_P2P_MAX_PACKET_SIZE - sizeof(EosPacketHeader))
#define EOS_MAX_FRAGMENT_COUNT ((EOS_MAX_ENGINE_PACKET_SIZE + EOS_FRAGMENT_PAYLOAD_SIZE - 1) / EOS_FRAGMENT_PAYLOAD_SIZE)

struct EosPacketHeader
{
    std::uint32_t magic;
    std::uint8_t version;
    std::uint8_t destinationSocket;
    std::uint16_t fragmentIndex;
    std::uint16_t fragmentCount;
    std::uint16_t reserved;
    std::uint32_t packetId;
    std::uint32_t totalSize;
};

static_assert(sizeof(EosPacketHeader) == 20);
static_assert(EOS_P2P_MAX_PACKET_SIZE > sizeof(EosPacketHeader));

struct EosPendingPacket
{
    in6_addr sender{};
    std::vector<std::uint8_t> payload;
};

struct EosReassembly
{
    std::array<std::uint8_t, 16> sender{};
    std::uint32_t packetId = 0;
    std::uint8_t destinationSocket = 0;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> receivedFragments;
    std::size_t receivedCount = 0;
    double lastUpdate = 0.0;
};

enum EosAuthState
{
    EOS_AUTH_IDLE,
    EOS_AUTH_NEED_DEVICE_ID,
    EOS_AUTH_CREATING_DEVICE_ID,
    EOS_AUTH_NEED_LOGIN,
    EOS_AUTH_LOGGING_IN,
    EOS_AUTH_NEED_NOTIFICATION,
    EOS_AUTH_READY,
};

struct EosState
{
    std::recursive_mutex sdkMutex;
    std::mutex controlMutex;
    std::condition_variable controlCondition;
    std::mutex packetMutex;
    std::thread processThread;

    std::atomic<bool> initialized{false};
    std::atomic<bool> running{false};
    std::atomic<bool> ready{false};
    bool setupComplete = false;
    bool setupSucceeded = false;
    bool sdkInitialized = false;

    EOS_HPlatform platform = nullptr;
    EOS_HConnect connect = nullptr;
    EOS_HP2P p2p = nullptr;
    EOS_P2P_SocketId socketId{};
    EOS_ProductUserId localUser = nullptr;

    EosAuthState authState = EOS_AUTH_IDLE;
    double nextAuthAttempt = 0.0;
    unsigned int retryCount = 0;
    std::string displayName;

    EOS_NotificationId authExpirationNotification = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId loginStatusNotification = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId connectionRequestNotification = EOS_INVALID_NOTIFICATIONID;

    std::uint32_t nextPacketId = 1;
    EOS_EResult lastSendError = EOS_EResult::EOS_Success;

    in6_addr localAddress{};
    std::uint16_t localEndpointPort = 0;
    std::uint16_t clientPort = 0;
    std::uint16_t hostPort = 0;

    std::array<std::deque<EosPendingPacket>, EOS_SOCKET_COUNT> packetQueues;
    std::array<std::size_t, EOS_SOCKET_COUNT> queuedBytes{};
    std::vector<EosReassembly> reassemblies;
    std::size_t reassemblyBytes = 0;
    double nextAssemblyCleanup = 0.0;

    ConVar* allowEos = nullptr;
    ConVar* currentEndpoint = nullptr;
    ConVar* processIntervalMs = nullptr;
};

static EosState eos;

static bool EOS_ProductUserToString(EOS_ProductUserId user, char (&buffer)[EOS_PRODUCTUSERID_MAX_LENGTH + 1])
{
    if (!user)
        return false;
    int32_t length = static_cast<int32_t>(sizeof(buffer));
    return EOS_ProductUserId_ToString(user, buffer, &length) == EOS_EResult::EOS_Success;
}

static bool EOS_ProductUserToAddress(EOS_ProductUserId user, in6_addr& address)
{
    char productId[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    if (!EOS_ProductUserToString(user, productId) || std::strlen(productId) != EOS_PRODUCT_ID_BYTES * 2)
        return false;

    std::array<std::uint8_t, EOS_PRODUCT_ID_BYTES> bytes{};
    V_hextobinary(productId, EOS_PRODUCT_ID_BYTES * 2, bytes.data(), bytes.size());

    address = {};
    address.u.Word[0] = htons(EOS_FAKE_ADDRESS_PREFIX);
    std::memcpy(address.u.Byte + 2, bytes.data() + 2, 14);
    return true;
}

static EOS_ProductUserId EOS_ProductUserFromAddress(const in6_addr& address)
{
    if (ntohs(address.u.Word[0]) != EOS_FAKE_ADDRESS_PREFIX)
        return nullptr;

    std::array<std::uint8_t, EOS_PRODUCT_ID_BYTES> bytes{};
    bytes[0] = 0x00;
    bytes[1] = 0x02;
    std::memcpy(bytes.data() + 2, address.u.Byte + 2, 14);
    char productId[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    V_binarytohex(bytes.data(), bytes.size(), productId, sizeof(productId));
    return EOS_ProductUserId_FromString(productId);
}

bool EOS_IsFakeAddress(const CNetAdr& address)
{
    return address.GetType() == netadrtype_t::NA_IP && ntohs(address.GetIP()->u.Word[0]) == EOS_FAKE_ADDRESS_PREFIX;
}

static int EOS_GetDestinationSocket(const int sourceSocket)
{
    if (sourceSocket == NS_CLIENT)
        return NS_SERVER;
    if (sourceSocket == NS_SERVER)
        return NS_CLIENT;
    return -1;
}

static EOS_ELogLevel EOS_GetLogLevel()
{
#if BUILD_DEBUG
    return EOS_ELogLevel::EOS_LOG_VeryVerbose;
#else
    if (CommandLine() && CommandLine()->FindParm("-eoslogverbose") != 0)
        return EOS_ELogLevel::EOS_LOG_VeryVerbose;
    return EOS_ELogLevel::EOS_LOG_Info;
#endif
}

static void EOS_CALL EOS_LogCallback(const EOS_LogMessage* message)
{
    if (!message || !message->Message)
        return;

    const char* category = message->Category ? message->Category : "Unknown";
    switch (message->Level)
    {
    case EOS_ELogLevel::EOS_LOG_Fatal:
    case EOS_ELogLevel::EOS_LOG_Error:
        NS::log::EOS->error("[{}] {}", category, message->Message);
        break;
    case EOS_ELogLevel::EOS_LOG_Warning:
        NS::log::EOS->warn("[{}] {}", category, message->Message);
        break;
    case EOS_ELogLevel::EOS_LOG_Info:
        NS::log::EOS->info("[{}] {}", category, message->Message);
        break;
    case EOS_ELogLevel::EOS_LOG_Verbose:
        NS::log::EOS->debug("[{}] {}", category, message->Message);
        break;
    case EOS_ELogLevel::EOS_LOG_VeryVerbose:
        NS::log::EOS->trace("[{}] {}", category, message->Message);
        break;
    default:
        NS::log::EOS->info("[{}] {}", category, message->Message);
        break;
    }
}

void EOS_ResetPacketQueue()
{
    std::lock_guard lock(eos.packetMutex);
    for (auto& queue : eos.packetQueues)
        queue.clear();
    eos.queuedBytes.fill(0);
    eos.reassemblies.clear();
    eos.reassemblyBytes = 0;
}

static void EOS_CleanupReassembliesLocked(const double now)
{
    for (auto reassembly = eos.reassemblies.begin(); reassembly != eos.reassemblies.end();)
    {
        if (now - reassembly->lastUpdate <= EOS_REASSEMBLY_LIFETIME)
        {
            ++reassembly;
            continue;
        }
        eos.reassemblyBytes -= reassembly->data.size();
        reassembly = eos.reassemblies.erase(reassembly);
    }
}

static void EOS_MakeReassemblyRoomLocked(const std::size_t bytes)
{
    while (!eos.reassemblies.empty() && (eos.reassemblies.size() >= EOS_MAX_REASSEMBLIES || eos.reassemblyBytes + bytes > EOS_MAX_REASSEMBLY_BYTES))
    {
        auto oldest = std::min_element(eos.reassemblies.begin(), eos.reassemblies.end(),
                                       [](const EosReassembly& left, const EosReassembly& right) { return left.lastUpdate < right.lastUpdate; });
        eos.reassemblyBytes -= oldest->data.size();
        eos.reassemblies.erase(oldest);
    }
}

static void EOS_EnqueuePacketLocked(const int destinationSocket, EosPendingPacket&& packet)
{
    auto& queue = eos.packetQueues[destinationSocket];
    auto& queuedBytes = eos.queuedBytes[destinationSocket];
    while (!queue.empty() && (queue.size() >= EOS_MAX_QUEUED_PACKETS || queuedBytes + packet.payload.size() > EOS_MAX_QUEUED_BYTES))
    {
        queuedBytes -= queue.front().payload.size();
        queue.pop_front();
    }
    if (packet.payload.size() <= EOS_MAX_QUEUED_BYTES)
    {
        queuedBytes += packet.payload.size();
        queue.emplace_back(std::move(packet));
    }
}

static void EOS_ProcessFrame(const in6_addr& sender, const std::uint8_t* data, const std::size_t size)
{
    if (size < sizeof(EosPacketHeader))
        return;

    EosPacketHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if (header.magic != EOS_FRAME_MAGIC || header.version != EOS_FRAME_VERSION || header.reserved != 0 ||
        (header.destinationSocket != NS_CLIENT && header.destinationSocket != NS_SERVER) || header.totalSize > EOS_MAX_ENGINE_PACKET_SIZE ||
        header.fragmentCount == 0 || header.fragmentCount > EOS_MAX_FRAGMENT_COUNT || header.fragmentIndex >= header.fragmentCount)
    {
        return;
    }

    const std::size_t expectedFragmentCount =
        std::max<std::size_t>(1, (header.totalSize + EOS_FRAGMENT_PAYLOAD_SIZE - 1) / EOS_FRAGMENT_PAYLOAD_SIZE);
    if (header.fragmentCount != expectedFragmentCount)
        return;

    const std::size_t offset = static_cast<std::size_t>(header.fragmentIndex) * EOS_FRAGMENT_PAYLOAD_SIZE;
    const std::size_t expectedPayloadSize = std::min<std::size_t>(EOS_FRAGMENT_PAYLOAD_SIZE, static_cast<std::size_t>(header.totalSize) - offset);
    if (size - sizeof(header) != expectedPayloadSize)
        return;

    const auto* payload = data + sizeof(header);
    std::lock_guard lock(eos.packetMutex);
    if (header.fragmentCount == 1)
    {
        EosPendingPacket packet;
        packet.sender = sender;
        packet.payload.assign(payload, payload + expectedPayloadSize);
        EOS_EnqueuePacketLocked(header.destinationSocket, std::move(packet));
        return;
    }

    std::array<std::uint8_t, 16> senderBytes{};
    std::memcpy(senderBytes.data(), &sender, senderBytes.size());
    const double now = g_PlatFloatTime();
    EOS_CleanupReassembliesLocked(now);
    auto found = std::find_if(eos.reassemblies.begin(), eos.reassemblies.end(), [&](const EosReassembly& candidate)
    { return candidate.sender == senderBytes && candidate.packetId == header.packetId && candidate.destinationSocket == header.destinationSocket; });
    if (found == eos.reassemblies.end())
    {
        EOS_MakeReassemblyRoomLocked(header.totalSize);
        EosReassembly assembly;
        assembly.sender = senderBytes;
        assembly.packetId = header.packetId;
        assembly.destinationSocket = header.destinationSocket;
        assembly.data.resize(header.totalSize);
        assembly.receivedFragments.resize(header.fragmentCount);
        assembly.lastUpdate = now;
        eos.reassemblyBytes += assembly.data.size();
        eos.reassemblies.emplace_back(std::move(assembly));
        found = std::prev(eos.reassemblies.end());
    }
    else if (found->data.size() != header.totalSize || found->receivedFragments.size() != header.fragmentCount)
    {
        eos.reassemblyBytes -= found->data.size();
        eos.reassemblies.erase(found);
        return;
    }

    found->lastUpdate = now;
    if (!found->receivedFragments[header.fragmentIndex])
    {
        std::memcpy(found->data.data() + offset, payload, expectedPayloadSize);
        found->receivedFragments[header.fragmentIndex] = 1;
        ++found->receivedCount;
    }
    if (found->receivedCount != found->receivedFragments.size())
        return;

    EosPendingPacket packet;
    packet.sender = sender;
    packet.payload = std::move(found->data);
    eos.reassemblyBytes -= packet.payload.size();
    eos.reassemblies.erase(found);
    EOS_EnqueuePacketLocked(header.destinationSocket, std::move(packet));
}

static void EOS_RequestLoginLocked(const char* reason, const bool authenticationInvalid)
{
    if (authenticationInvalid)
        eos.ready.store(false, std::memory_order_release);
    if (eos.authState == EOS_AUTH_LOGGING_IN)
        return;

    NS::log::EOS->warn("Scheduling EOS login refresh after {}", reason);
    eos.authState = EOS_AUTH_NEED_LOGIN;
    eos.nextAuthAttempt = g_PlatFloatTime();
    eos.controlCondition.notify_all();
}

static void EOS_ScheduleRetryLocked(const EosAuthState state, const char* operation, const EOS_EResult result)
{
    const unsigned int exponent = std::min(eos.retryCount, 4u);
    const unsigned int delaySeconds = std::min(30u, 1u << exponent);
    ++eos.retryCount;
    eos.authState = state;
    eos.nextAuthAttempt = g_PlatFloatTime() + delaySeconds;
    NS::log::EOS->warn("EOS {} failed ({}); retrying in {} seconds", operation, EOS_EResult_ToString(result), delaySeconds);
}

static void EOS_FinishAuthenticationLocked()
{
    eos.authState = EOS_AUTH_READY;
    eos.ready.store(true, std::memory_order_release);
    eos.controlCondition.notify_all();
}

static void EOS_UpdateLocalEndpointLocked(EOS_ProductUserId user)
{
    in6_addr address{};
    if (!EOS_ProductUserToAddress(user, address))
        return;

    if (eos.localEndpointPort == 0)
    {
        eos.localEndpointPort = static_cast<std::uint16_t>((static_cast<std::uint16_t>(address.u.Byte[14]) << 8) | address.u.Byte[15]);
    }
    eos.localAddress = address;

    char addressString[INET6_ADDRSTRLEN]{};
    if (!InetNtopA(AF_INET6, &address, addressString, sizeof(addressString)))
        return;

    const std::string endpoint = fmt::format("[{}]:{}", addressString, eos.localEndpointPort);
    NS::log::EOS->info("Local FakeIPv6 {}", endpoint);
    RunInMainThread([endpoint]()
    {
        if (eos.currentEndpoint)
            eos.currentEndpoint->SetValue(endpoint.c_str());
    });
}

static void EOS_RemoveConnectionNotificationLocked()
{
    if (eos.p2p && eos.connectionRequestNotification != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_P2P_RemoveNotifyPeerConnectionRequest(eos.p2p, eos.connectionRequestNotification);
    }
    eos.connectionRequestNotification = EOS_INVALID_NOTIFICATIONID;
}

static bool EOS_AddConnectionNotificationLocked()
{
    if (!eos.p2p || !eos.localUser)
        return false;

    EOS_RemoveConnectionNotificationLocked();
    EOS_P2P_AddNotifyPeerConnectionRequestOptions options{};
    options.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
    options.LocalUserId = eos.localUser;
    options.SocketId = &eos.socketId;
    eos.connectionRequestNotification =
        EOS_P2P_AddNotifyPeerConnectionRequest(eos.p2p, &options, nullptr, [](const EOS_P2P_OnIncomingConnectionRequestInfo* data)
    {
        if (!data)
            return;
        std::lock_guard lock(eos.sdkMutex);
        if (!eos.running.load(std::memory_order_acquire) || !eos.p2p || !data->RemoteUserId || !data->SocketId ||
            std::strcmp(data->SocketId->SocketName, EOS_SOCKET_NAME) != 0)
        {
            return;
        }

        EOS_P2P_AcceptConnectionOptions accept{};
        accept.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
        accept.LocalUserId = data->LocalUserId;
        accept.RemoteUserId = data->RemoteUserId;
        accept.SocketId = &eos.socketId;
        const EOS_EResult result = EOS_P2P_AcceptConnection(eos.p2p, &accept);
        if (result != EOS_EResult::EOS_Success)
        {
            NS::log::EOS->error("EOS_P2P_AcceptConnection failed ({})", EOS_EResult_ToString(result));
            return;
        }

        char remoteId[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
        if (EOS_ProductUserToString(data->RemoteUserId, remoteId))
        {
            NS::log::EOS->info("Accepted connection request from {} on socket {}", remoteId, EOS_SOCKET_NAME);
        }
    });
    if (eos.connectionRequestNotification == EOS_INVALID_NOTIFICATIONID)
        return false;

    NS::log::EOS->info("Registered process-lifetime EOS P2P socket notification for \"{}\"", EOS_SOCKET_NAME);
    return true;
}

static void EOS_CALL EOS_CreateDeviceIdComplete(const EOS_Connect_CreateDeviceIdCallbackInfo* data)
{
    if (!data)
        return;
    std::lock_guard lock(eos.sdkMutex);
    if (!eos.running.load(std::memory_order_acquire))
        return;
    if (!EOS_EResult_IsOperationComplete(data->ResultCode))
    {
        NS::log::EOS->warn("EOS device ID creation is pending SDK retry ({})", EOS_EResult_ToString(data->ResultCode));
        return;
    }

    if (data->ResultCode == EOS_EResult::EOS_Success || data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
    {
        eos.retryCount = 0;
        eos.authState = EOS_AUTH_NEED_LOGIN;
        eos.nextAuthAttempt = g_PlatFloatTime();
        eos.controlCondition.notify_all();
        return;
    }
    EOS_ScheduleRetryLocked(EOS_AUTH_NEED_DEVICE_ID, "device ID creation", data->ResultCode);
}

static void EOS_CALL EOS_LoginComplete(const EOS_Connect_LoginCallbackInfo* data)
{
    if (!data)
        return;
    std::lock_guard lock(eos.sdkMutex);
    if (!eos.running.load(std::memory_order_acquire))
        return;
    if (!EOS_EResult_IsOperationComplete(data->ResultCode))
    {
        NS::log::EOS->warn("EOS login is pending SDK retry ({})", EOS_EResult_ToString(data->ResultCode));
        return;
    }
    if (data->ResultCode != EOS_EResult::EOS_Success || !data->LocalUserId)
    {
        EOS_ScheduleRetryLocked(EOS_AUTH_NEED_LOGIN, "login", data->ResultCode);
        return;
    }

    in6_addr previousAddress{};
    in6_addr refreshedAddress{};
    const bool sameUser = eos.localUser && EOS_ProductUserToAddress(eos.localUser, previousAddress) &&
                          EOS_ProductUserToAddress(data->LocalUserId, refreshedAddress) &&
                          std::memcmp(&previousAddress, &refreshedAddress, sizeof(previousAddress)) == 0;

    eos.localUser = data->LocalUserId;
    eos.retryCount = 0;
    EOS_UpdateLocalEndpointLocked(data->LocalUserId);

    char productUserId[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    if (EOS_ProductUserToString(data->LocalUserId, productUserId))
        NS::log::EOS->info("Logged in as {}", productUserId);

    if (sameUser && eos.connectionRequestNotification != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_FinishAuthenticationLocked();
        return;
    }

    EOS_RemoveConnectionNotificationLocked();
    EOS_ResetPacketQueue();
    eos.authState = EOS_AUTH_NEED_NOTIFICATION;
    eos.nextAuthAttempt = g_PlatFloatTime();
}

static void EOS_CALL EOS_AuthExpiration(const EOS_Connect_AuthExpirationCallbackInfo* data)
{
    if (!data)
        return;
    std::lock_guard lock(eos.sdkMutex);
    if (!eos.running.load(std::memory_order_acquire) || data->LocalUserId != eos.localUser)
        return;

    NS::log::EOS->warn("EOS authentication is approaching expiration; refreshing it without closing P2P");
    EOS_RequestLoginLocked("authentication expiration notification", false);
}

static void EOS_CALL EOS_LoginStatusChanged(const EOS_Connect_LoginStatusChangedCallbackInfo* data)
{
    if (!data)
        return;
    std::lock_guard lock(eos.sdkMutex);
    NS::log::EOS->info("EOS login status changed from {} to {}", static_cast<int>(data->PreviousStatus), static_cast<int>(data->CurrentStatus));
    if (!eos.running.load(std::memory_order_acquire) || data->CurrentStatus == EOS_ELoginStatus::EOS_LS_LoggedIn ||
        data->LocalUserId != eos.localUser)
    {
        return;
    }
    EOS_RequestLoginLocked("login status notification", true);
}

static bool EOS_AddAuthNotificationsLocked()
{
    EOS_Connect_AddNotifyAuthExpirationOptions expiration{};
    expiration.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
    eos.authExpirationNotification = EOS_Connect_AddNotifyAuthExpiration(eos.connect, &expiration, nullptr, EOS_AuthExpiration);
    if (eos.authExpirationNotification == EOS_INVALID_NOTIFICATIONID)
        return false;

    EOS_Connect_AddNotifyLoginStatusChangedOptions status{};
    status.ApiVersion = EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
    eos.loginStatusNotification = EOS_Connect_AddNotifyLoginStatusChanged(eos.connect, &status, nullptr, EOS_LoginStatusChanged);
    if (eos.loginStatusNotification == EOS_INVALID_NOTIFICATIONID)
    {
        EOS_Connect_RemoveNotifyAuthExpiration(eos.connect, eos.authExpirationNotification);
        eos.authExpirationNotification = EOS_INVALID_NOTIFICATIONID;
        return false;
    }
    return true;
}

static void EOS_RemoveAuthNotificationsLocked()
{
    if (eos.connect && eos.authExpirationNotification != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_Connect_RemoveNotifyAuthExpiration(eos.connect, eos.authExpirationNotification);
    }
    if (eos.connect && eos.loginStatusNotification != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_Connect_RemoveNotifyLoginStatusChanged(eos.connect, eos.loginStatusNotification);
    }
    eos.authExpirationNotification = EOS_INVALID_NOTIFICATIONID;
    eos.loginStatusNotification = EOS_INVALID_NOTIFICATIONID;
}

static void EOS_BeginCreateDeviceIdLocked()
{
    EOS_Connect_CreateDeviceIdOptions options{};
    options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    options.DeviceModel = "PC Windows";
    eos.authState = EOS_AUTH_CREATING_DEVICE_ID;
    EOS_Connect_CreateDeviceId(eos.connect, &options, nullptr, EOS_CreateDeviceIdComplete);
}

static void EOS_BeginLoginLocked()
{
    EOS_Connect_UserLoginInfo userInfo{};
    userInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
    userInfo.DisplayName = eos.displayName.c_str();

    EOS_Connect_Credentials credentials{};
    credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
    credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;

    EOS_Connect_LoginOptions options{};
    options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
    options.Credentials = &credentials;
    options.UserLoginInfo = &userInfo;

    eos.authState = EOS_AUTH_LOGGING_IN;
    EOS_Connect_Login(eos.connect, &options, nullptr, EOS_LoginComplete);
}

static void EOS_ProcessAuthenticationLocked()
{
    if (g_PlatFloatTime() < eos.nextAuthAttempt)
        return;

    switch (eos.authState)
    {
    case EOS_AUTH_NEED_DEVICE_ID:
        EOS_BeginCreateDeviceIdLocked();
        break;
    case EOS_AUTH_NEED_LOGIN:
        EOS_BeginLoginLocked();
        break;
    case EOS_AUTH_NEED_NOTIFICATION:
        if (EOS_AddConnectionNotificationLocked())
            EOS_FinishAuthenticationLocked();
        else
            EOS_ScheduleRetryLocked(EOS_AUTH_NEED_NOTIFICATION, "P2P notification registration", EOS_EResult::EOS_UnexpectedError);
        break;
    default:
        break;
    }
}

static void EOS_ProcessIncomingLocked()
{
    if (!eos.p2p || !eos.localUser)
        return;

    for (std::size_t receivedPackets = 0; receivedPackets < EOS_MAX_PACKETS_PER_TICK; ++receivedPackets)
    {
        EOS_P2P_GetNextReceivedPacketSizeOptions sizeOptions{};
        sizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
        sizeOptions.LocalUserId = eos.localUser;

        std::uint32_t packetSize = 0;
        const EOS_EResult sizeResult = EOS_P2P_GetNextReceivedPacketSize(eos.p2p, &sizeOptions, &packetSize);
        if (sizeResult == EOS_EResult::EOS_NotFound)
            return;
        if (sizeResult == EOS_EResult::EOS_InvalidAuth)
        {
            EOS_RequestLoginLocked("P2P receive reported invalid authentication", true);
            return;
        }
        if (sizeResult != EOS_EResult::EOS_Success)
        {
            NS::log::EOS->warn("EOS_P2P_GetNextReceivedPacketSize failed ({})", EOS_EResult_ToString(sizeResult));
            return;
        }
        if (packetSize > EOS_P2P_MAX_PACKET_SIZE)
        {
            NS::log::EOS->error("EOS reported an oversized P2P packet ({} bytes)", packetSize);
            return;
        }

        std::array<std::uint8_t, EOS_P2P_MAX_PACKET_SIZE> data{};
        EOS_P2P_ReceivePacketOptions receive{};
        receive.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
        receive.LocalUserId = eos.localUser;
        receive.MaxDataSizeBytes = static_cast<std::uint32_t>(data.size());

        EOS_ProductUserId remoteUser = nullptr;
        EOS_P2P_SocketId socketId{};
        socketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        std::uint8_t channel = 0;
        std::uint32_t bytesWritten = 0;
        const EOS_EResult receiveResult = EOS_P2P_ReceivePacket(eos.p2p, &receive, &remoteUser, &socketId, &channel, data.data(), &bytesWritten);
        if (receiveResult == EOS_EResult::EOS_InvalidAuth)
        {
            EOS_RequestLoginLocked("P2P receive reported invalid authentication", true);
            return;
        }
        if (receiveResult != EOS_EResult::EOS_Success)
        {
            NS::log::EOS->warn("EOS_P2P_ReceivePacket failed ({})", EOS_EResult_ToString(receiveResult));
            return;
        }
        if (channel != 0 || std::strcmp(socketId.SocketName, EOS_SOCKET_NAME) != 0)
            continue;

        in6_addr sender{};
        if (EOS_ProductUserToAddress(remoteUser, sender))
            EOS_ProcessFrame(sender, data.data(), bytesWritten);
    }
}

static void EOS_ShutdownLocked()
{
    EOS_RemoveConnectionNotificationLocked();
    EOS_RemoveAuthNotificationsLocked();
    eos.localUser = nullptr;
    eos.authState = EOS_AUTH_IDLE;

    if (eos.platform)
        EOS_Platform_Release(eos.platform);
    eos.platform = nullptr;
    eos.connect = nullptr;
    eos.p2p = nullptr;

    if (eos.sdkInitialized)
    {
        EOS_Logging_SetCallback(nullptr);
        EOS_Shutdown();
        eos.sdkInitialized = false;
    }

    eos.ready.store(false, std::memory_order_release);
    eos.initialized.store(false, std::memory_order_release);
    eos.localAddress = {};
    EOS_ResetPacketQueue();
}

static bool EOS_SetupLocked()
{
    EOS_InitializeOptions initialize{};
    initialize.ApiVersion = EOS_INITIALIZE_API_LATEST;
    initialize.ProductName = EOS_PRODUCT_NAME;
    initialize.ProductVersion = version;

    const EOS_EResult initializeResult = EOS_Initialize(&initialize);
    if (initializeResult != EOS_EResult::EOS_Success && initializeResult != EOS_EResult::EOS_AlreadyConfigured)
    {
        NS::log::EOS->error("EOS_Initialize failed ({})", EOS_EResult_ToString(initializeResult));
        return false;
    }
    eos.sdkInitialized = true;

    if (EOS_Logging_SetCallback(EOS_LogCallback) != EOS_EResult::EOS_Success)
        NS::log::EOS->error("Failed to register the EOS SDK logging callback");
    const EOS_EResult logResult = EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_GetLogLevel());
    if (logResult != EOS_EResult::EOS_Success)
    {
        NS::log::EOS->error("Failed to set the EOS SDK log level ({})", EOS_EResult_ToString(logResult));
    }

    std::array<unsigned char, sizeof(EOS_CLIENT_SECRET_B64)> clientSecret{};
    DWORD clientSecretSize = static_cast<DWORD>(clientSecret.size() - 1);
    if (!CryptStringToBinaryA(EOS_CLIENT_SECRET_B64, 0, CRYPT_STRING_BASE64, clientSecret.data(), &clientSecretSize, nullptr, nullptr))
    {
        NS::log::EOS->error("Failed to decode EOS client secret ({})", GetLastError());
        EOS_ShutdownLocked();
        return false;
    }
    EOS_Platform_Options platform{};
    platform.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
    platform.ProductId = EOS_PRODUCT_ID;
    platform.SandboxId = EOS_SANDBOX_ID;
    platform.DeploymentId = EOS_DEPLOYMENT_ID;
    platform.ClientCredentials.ClientId = EOS_CLIENT_ID;
    platform.ClientCredentials.ClientSecret = reinterpret_cast<const char*>(clientSecret.data());
    platform.Flags = EOS_PF_DISABLE_OVERLAY;
    platform.TickBudgetInMilliseconds = 1;

    eos.platform = EOS_Platform_Create(&platform);
    if (!eos.platform)
    {
        NS::log::EOS->error("EOS_Platform_Create failed");
        EOS_ShutdownLocked();
        return false;
    }

    eos.connect = EOS_Platform_GetConnectInterface(eos.platform);
    eos.p2p = EOS_Platform_GetP2PInterface(eos.platform);
    if (!eos.connect || !eos.p2p)
    {
        NS::log::EOS->error("EOS platform is missing Connect or P2P interfaces");
        EOS_ShutdownLocked();
        return false;
    }

    eos.socketId = {};
    eos.socketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
    V_strncpy(eos.socketId.SocketName, EOS_SOCKET_NAME, sizeof(eos.socketId.SocketName));

    EOS_P2P_SetRelayControlOptions relay{};
    relay.ApiVersion = EOS_P2P_SETRELAYCONTROL_API_LATEST;
    relay.RelayControl = EOS_ERelayControl::EOS_RC_AllowRelays;
    const EOS_EResult relayResult = EOS_P2P_SetRelayControl(eos.p2p, &relay);
    if (relayResult != EOS_EResult::EOS_Success)
    {
        NS::log::EOS->error("EOS_P2P_SetRelayControl failed ({})", EOS_EResult_ToString(relayResult));
        EOS_ShutdownLocked();
        return false;
    }

    if (!EOS_AddAuthNotificationsLocked())
    {
        NS::log::EOS->error("Failed to register EOS authentication lifetime notifications");
        EOS_ShutdownLocked();
        return false;
    }

    eos.nextPacketId = static_cast<std::uint32_t>(static_cast<std::uint64_t>(g_PlatFloatTime() * 1000000.0) ^ GetCurrentProcessId());
    eos.retryCount = 0;
    eos.authState = EOS_AUTH_NEED_DEVICE_ID;
    eos.nextAuthAttempt = g_PlatFloatTime();
    eos.initialized.store(true, std::memory_order_release);
    return true;
}

static void EOS_Process()
{
    bool setupSucceeded = false;
    {
        std::lock_guard lock(eos.sdkMutex);
        setupSucceeded = EOS_SetupLocked();
    }
    if (!setupSucceeded)
        eos.running.store(false, std::memory_order_release);
    {
        std::lock_guard lock(eos.controlMutex);
        eos.setupSucceeded = setupSucceeded;
        eos.setupComplete = true;
    }
    eos.controlCondition.notify_all();
    if (!setupSucceeded)
        return;

    while (eos.running.load(std::memory_order_acquire))
    {
        {
            std::lock_guard lock(eos.sdkMutex);
            EOS_ProcessAuthenticationLocked();
            EOS_Platform_Tick(eos.platform);
            EOS_ProcessIncomingLocked();
        }

        const double now = g_PlatFloatTime();
        if (now >= eos.nextAssemblyCleanup)
        {
            std::lock_guard lock(eos.packetMutex);
            EOS_CleanupReassembliesLocked(now);
            eos.nextAssemblyCleanup = now + 1.0;
        }

        std::unique_lock lock(eos.controlMutex);
        eos.controlCondition.wait_for(lock, std::chrono::milliseconds(eos.processIntervalMs->GetInt()),
                                      []() { return !eos.running.load(std::memory_order_acquire); });
    }

    std::lock_guard lock(eos.sdkMutex);
    EOS_ShutdownLocked();
    eos.controlCondition.notify_all();
}

bool EOS_Init()
{
    if (eos.initialized.load(std::memory_order_acquire))
        return true;
    if (!eos.allowEos || eos.allowEos->GetInt() != 1)
        return false;

    eos.displayName = "ion_user";
    if (g_pCVar)
    {
        if (ConVar* name = g_pCVar->FindVar("name"); name && name->GetString()[0] != '\0')
            eos.displayName = name->GetString();
        if (ConVar* clientPort = g_pCVar->FindVar("clientport"))
            eos.clientPort = static_cast<std::uint16_t>(clientPort->GetInt());
        if (ConVar* hostPort = g_pCVar->FindVar("hostport"))
            eos.hostPort = static_cast<std::uint16_t>(hostPort->GetInt());
    }

    const bool serverContext = IsDedicatedServer() || (g_pServer && g_pServer->GetState() > ss_dead);
    eos.localEndpointPort = serverContext ? eos.clientPort : eos.hostPort;
    {
        std::lock_guard lock(eos.controlMutex);
        eos.setupComplete = false;
        eos.setupSucceeded = false;
    }
    eos.running.store(true, std::memory_order_release);
    eos.processThread = std::thread(EOS_Process);

    std::unique_lock lock(eos.controlMutex);
    const bool setupCompleted =
        eos.controlCondition.wait_for(lock, std::chrono::seconds(EOS_INITIAL_LOGIN_WAIT_SECONDS), []() { return eos.setupComplete; });
    const bool setupSucceeded = setupCompleted && eos.setupSucceeded;
    if (!setupSucceeded)
    {
        lock.unlock();
        NS::log::EOS->error(setupCompleted ? "EOS SDK setup failed on the process thread" : "EOS SDK setup timed out on the process thread");
        EOS_ShutdownNetworking();
        return false;
    }

    eos.controlCondition.wait_for(lock, std::chrono::seconds(EOS_INITIAL_LOGIN_WAIT_SECONDS),
                                  []() { return eos.ready.load(std::memory_order_acquire) || !eos.running.load(std::memory_order_acquire); });
    if (!eos.ready.load(std::memory_order_acquire))
        NS::log::EOS->warn("EOS authentication is not ready; the process thread will continue retrying");
    return true;
}

void EOS_ShutdownNetworking()
{
    eos.initialized.store(false, std::memory_order_release);
    eos.ready.store(false, std::memory_order_release);
    eos.running.store(false, std::memory_order_release);
    eos.controlCondition.notify_all();
    if (eos.processThread.joinable() && eos.processThread.get_id() != std::this_thread::get_id())
        eos.processThread.join();
}

bool EOS_IsReady()
{
    return eos.initialized.load(std::memory_order_acquire) && eos.ready.load(std::memory_order_acquire);
}

bool EOS_SendPacket(const int sourceSocket, const CNetAdr& destination, const std::uint8_t* data, const std::size_t size)
{
    if (!EOS_IsFakeAddress(destination) || (!data && size != 0) || size > EOS_MAX_ENGINE_PACKET_SIZE)
    {
        return false;
    }

    const int destinationSocket = EOS_GetDestinationSocket(sourceSocket);
    if (destinationSocket < 0)
        return false;

    std::lock_guard lock(eos.sdkMutex);
    if (!eos.initialized.load(std::memory_order_acquire) || !eos.ready.load(std::memory_order_acquire) || !eos.p2p || !eos.localUser)
    {
        return false;
    }

    EOS_ProductUserId remoteUser = EOS_ProductUserFromAddress(*destination.GetIP());
    if (!remoteUser)
        return false;

    const std::uint32_t packetId = eos.nextPacketId++;
    const std::size_t fragmentCount = std::max<std::size_t>(1, (size + EOS_FRAGMENT_PAYLOAD_SIZE - 1) / EOS_FRAGMENT_PAYLOAD_SIZE);
    std::array<std::uint8_t, EOS_P2P_MAX_PACKET_SIZE> frame{};

    for (std::size_t fragmentIndex = 0; fragmentIndex < fragmentCount; ++fragmentIndex)
    {
        const std::size_t offset = fragmentIndex * EOS_FRAGMENT_PAYLOAD_SIZE;
        const std::size_t payloadSize = std::min<std::size_t>(EOS_FRAGMENT_PAYLOAD_SIZE, size - offset);
        const EosPacketHeader header{
            .magic = EOS_FRAME_MAGIC,
            .version = EOS_FRAME_VERSION,
            .destinationSocket = static_cast<std::uint8_t>(destinationSocket),
            .fragmentIndex = static_cast<std::uint16_t>(fragmentIndex),
            .fragmentCount = static_cast<std::uint16_t>(fragmentCount),
            .reserved = 0,
            .packetId = packetId,
            .totalSize = static_cast<std::uint32_t>(size),
        };
        std::memcpy(frame.data(), &header, sizeof(header));
        if (payloadSize != 0)
            std::memcpy(frame.data() + sizeof(header), data + offset, payloadSize);

        EOS_P2P_SendPacketOptions options{};
        options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
        options.LocalUserId = eos.localUser;
        options.RemoteUserId = remoteUser;
        options.SocketId = &eos.socketId;
        options.Channel = 0;
        options.DataLengthBytes = static_cast<std::uint32_t>(sizeof(header) + payloadSize);
        options.Data = frame.data();
        options.bAllowDelayedDelivery = EOS_TRUE;
        options.Reliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
        options.bDisableAutoAcceptConnection = EOS_FALSE;

        const EOS_EResult result = EOS_P2P_SendPacket(eos.p2p, &options);
        if (result != EOS_EResult::EOS_Success)
        {
            if (result == EOS_EResult::EOS_InvalidAuth)
                EOS_RequestLoginLocked("P2P send reported invalid authentication", true);
            if (eos.lastSendError != result)
            {
                NS::log::EOS->warn("EOS_P2P_SendPacket failed ({})", EOS_EResult_ToString(result));
            }
            eos.lastSendError = result;
            return false;
        }
    }

    eos.lastSendError = EOS_EResult::EOS_Success;
    return true;
}

bool EOS_ReceivePacket(const int destinationSocket, netpacket_t* packet)
{
    if (!packet || !packet->data || (destinationSocket != NS_CLIENT && destinationSocket != NS_SERVER))
    {
        return false;
    }

    EosPendingPacket pending;
    {
        std::lock_guard lock(eos.packetMutex);
        auto& queue = eos.packetQueues[destinationSocket];
        if (queue.empty())
            return false;
        pending = std::move(queue.front());
        eos.queuedBytes[destinationSocket] -= pending.payload.size();
        queue.pop_front();
    }

    if (!pending.payload.empty())
        std::memcpy(packet->data, pending.payload.data(), pending.payload.size());
    packet->from.Clear();
    packet->from.SetType(netadrtype_t::NA_IP);
    packet->from.SetIP(&pending.sender);
    const std::uint16_t remotePort =
        destinationSocket == NS_SERVER ? (eos.clientPort ? eos.clientPort : eos.hostPort) : (eos.hostPort ? eos.hostPort : eos.clientPort);
    packet->from.SetPort(htons(remotePort));
    packet->source = destinationSocket;
    packet->size = static_cast<int>(pending.payload.size());
    packet->wireSize = packet->size;
    packet->stream = false;
    return true;
}

bool EOS_GetLocalEndpoint(CNetAdr& endpoint)
{
    std::lock_guard lock(eos.sdkMutex);
    if (!eos.initialized.load(std::memory_order_acquire) || !eos.ready.load(std::memory_order_acquire) ||
        ntohs(eos.localAddress.u.Word[0]) != EOS_FAKE_ADDRESS_PREFIX)
    {
        return false;
    }

    endpoint.Clear();
    endpoint.SetType(netadrtype_t::NA_IP);
    endpoint.SetIP(&eos.localAddress);
    endpoint.SetPort(htons(eos.localEndpointPort));
    return true;
}

ADD_SQFUNC("string", NSGetLocalP2PEndpointAddress, "", "", ScriptContext::UI)
{
    CNetAdr endpoint;
    if (!EOS_GetLocalEndpoint(endpoint))
    {
        g_pSquirrel[context]->pushstring(sqvm, "");
        return SQRESULT_NOTNULL;
    }

    char address[INET6_ADDRSTRLEN]{};
    InetNtopA(AF_INET6, endpoint.GetIP(), address, sizeof(address));
    g_pSquirrel[context]->pushstring(sqvm, address);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSGetLocalP2PEndpointPort, "", "", ScriptContext::UI)
{
    CNetAdr endpoint;
    g_pSquirrel[context]->pushinteger(sqvm, EOS_GetLocalEndpoint(endpoint) ? ntohs(endpoint.GetPort()) : 0);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSIsP2PConnection, "", "", ScriptContext::UI)
{
    ConVar* allowEos = g_pCVar ? g_pCVar->FindVar("ns_has_agreed_allow_eos") : nullptr;
    if (!allowEos || allowEos->GetInt() != 1)
    {
        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }

    CClientState* client = GetBaseLocalClient();
    if (client && std::string(client->m_szServerAddress).find("[3ffe:") != std::string::npos)
    {
        g_pSquirrel[context]->pushbool(sqvm, true);
        return SQRESULT_NOTNULL;
    }

    g_pSquirrel[context]->pushbool(sqvm, g_pServer && g_pServer->IsActive());
    return SQRESULT_NOTNULL;
}

ON_DLL_LOAD_RELIESON("engine.dll", EOSNetworking, ConVar, [](CModule module)
{
    eos.allowEos = new ConVar("ns_has_agreed_allow_eos", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Allow using EOS P2P networking.");
    eos.currentEndpoint = new ConVar("eos_current_endpoint", "", FCVAR_REPLICATED, "The host's EOS P2P endpoint. (readonly)");
    eos.processIntervalMs = new ConVar("eos_process_interval_ms", "10", FCVAR_RELEASE, "Milliseconds between EOS SDK processing iterations.", true,
                                       1.0f, false, 0.0f, nullptr);
    std::atexit(EOS_ShutdownNetworking);
})
