#include "fake_ip_layer.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "core/convar/cvar.h"
#include "dedicated/dedicated.h"
#include "engine/r2engine.h"
#include "eos_threading.h"
#include "eos_layer.h"

ConVar* Cvar_eos_current_endpoint = nullptr;

namespace
{

constexpr uint16_t kFakeIPv6Prefix = 0x3FFE; // Deprecated 6bone space
constexpr size_t kProductBytes = 16;

using eos::PacketRoute;

PacketRoute NormalizeRoute(PacketRoute route)
{
    return route == PacketRoute::None ? PacketRoute::All : route;
}

PacketRoute CombineRoutes(PacketRoute lhs, PacketRoute rhs)
{
    return NormalizeRoute(static_cast<PacketRoute>(
        static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)));
}

bool HexCharToValue(char c, uint8_t& out)
{
    if (c >= '0' && c <= '9')
    {
        out = static_cast<uint8_t>(c - '0');
        return true;
    }
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (c >= 'a' && c <= 'f')
    {
        out = static_cast<uint8_t>(10 + c - 'a');
        return true;
    }
    return false;
}

bool ProductIdToBytes(const std::string& productId, std::array<uint8_t, kProductBytes>& outBytes)
{
    std::string normalized;
    normalized.reserve(productId.size());
    for (char c : productId)
    {
        if (c == '-' || c == '{' || c == '}')
            continue;
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (normalized.size() < kProductBytes * 2)
        return false;

    for (size_t i = 0; i < kProductBytes; ++i)
    {
        uint8_t hi = 0;
        uint8_t lo = 0;
        if (!HexCharToValue(normalized[i * 2], hi) ||
            !HexCharToValue(normalized[i * 2 + 1], lo))
        {
            return false;
        }

        outBytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return true;
}

std::string BytesToProductIdString(const std::array<uint8_t, kProductBytes>& bytes)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(kProductBytes * 2, '0');
    for (size_t i = 0; i < kProductBytes; ++i)
    {
        result[i * 2] = kHex[(bytes[i] >> 4) & 0xF];
        result[i * 2 + 1] = kHex[bytes[i] & 0xF];
    }
    return result;
}

} // namespace

namespace eos
{

FakeIpLayer::FakeIpLayer(EOS_HP2P p2pHandle, EOS_ProductUserId localUser)
    : m_p2pHandle(p2pHandle)
{
    m_localUser.store(localUser, std::memory_order_relaxed);
    UpdateLocalEndpoint();
}

bool FakeIpLayer::NormalizeProductId(const std::string& productId, std::string& normalized) const
{
    std::array<uint8_t, kProductBytes> bytes{};
    if (!ProductIdToBytes(productId, bytes))
        return false;

    normalized = BytesToProductIdString(bytes);
    return true;
}

bool FakeIpLayer::EncodeProductId(const std::string& productId, FakeEndpoint& endpoint) const
{
    std::array<uint8_t, kProductBytes> bytes{};
    if (!ProductIdToBytes(productId, bytes))
        return false;

    // ProductUserIds always begin with 0x00, 0x02 so we only need to transmit the last 14 bytes.
    endpoint.address = in6_addr{};
    endpoint.address.u.Word[0] = htons(kFakeIPv6Prefix);
    std::memcpy(endpoint.address.u.Byte + 2, bytes.data() + 2, 14);

    uint16_t hostPort = static_cast<uint16_t>(g_pCVar->FindVar("clientport")->GetInt());

	int ss_state = IsDedicatedServer() ? ss_active : (g_pServerState ? static_cast<int>(*g_pServerState) : ss_dead);

    if (!(ss_state > ss_dead))
        hostPort = static_cast<uint16_t>(g_pCVar->FindVar("hostport")->GetInt());

    if (hostPort == 0)
        hostPort = static_cast<uint16_t>((static_cast<uint16_t>(bytes[14]) << 8) | bytes[15]);

    endpoint.port = hostPort;
    return true;
}

bool FakeIpLayer::DecodeProductId(const FakeEndpoint& endpoint, std::string& productId) const
{
    if (ntohs(endpoint.address.u.Word[0]) != kFakeIPv6Prefix)
        return false;

    std::array<uint8_t, kProductBytes> bytes{};
    bytes[0] = 0x00;
    bytes[1] = 0x02;
    std::memcpy(bytes.data() + 2, endpoint.address.u.Byte + 2, 14);

    productId = BytesToProductIdString(bytes);
    return true;
}

bool FakeIpLayer::GetProductIdString(EOS_ProductUserId user, std::string& outString) const
{
    if (!user)
        return false;

    char buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    int32_t length = static_cast<int32_t>(sizeof(buffer));
    EOS_EResult toStringResult = EOS_EResult::EOS_UnexpectedError;
    {
        SdkLock lock(GetSdkMutex());
        toStringResult = EOS_ProductUserId_ToString(user, buffer, &length);
    }
    if (toStringResult != EOS_EResult::EOS_Success)
        return false;

    outString.assign(buffer, length);
    return true;
}

FakeEndpoint FakeIpLayer::RegisterPeer(EOS_ProductUserId remoteUser,
                                       const char* socketName,
                                       uint8_t channel)
{
    FakeEndpoint endpoint{};
    if (!m_p2pHandle || !remoteUser || !socketName)
        return endpoint;

    std::string productIdRaw;
    if (!GetProductIdString(remoteUser, productIdRaw))
        return endpoint;

    std::string normalizedId;
    if (!NormalizeProductId(productIdRaw, normalizedId))
        return endpoint;

    if (!EncodeProductId(normalizedId, endpoint))
    {
        NS::log::EOS->error("Failed to encode product user id for peer registration: {}", normalizedId);
        return FakeEndpoint{};
    }

    PeerBinding binding{};
	binding.route = PacketRoute::Server;
    binding.remoteUser = remoteUser;
    binding.channel = channel;
    binding.endpoint = endpoint;
    binding.productUserId = normalizedId;
    binding.socketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
    std::snprintf(binding.socketId.SocketName,
                  EOS_P2P_SOCKETID_SOCKETNAME_SIZE,
                  "%s",
                  socketName);

    {
        std::lock_guard guard(m_bindingsMutex);
        m_peerBindings[normalizedId] = binding;
    }

    return endpoint;
}

bool FakeIpLayer::SendToPeer(const FakeEndpoint& endpoint,
                             const uint8_t* data,
                             size_t length,
                             PacketRoute route)
{
	EOS_ProductUserId localUser = m_localUser.load(std::memory_order_acquire);
    if (!m_p2pHandle || !localUser || !data || length == 0)
        return false;

    const PacketRoute normalizedRoute = NormalizeRoute(route);

    std::string productId;
    if (!DecodeProductId(endpoint, productId))
        return false;

    PeerBinding binding{};
    {
        std::lock_guard guard(m_bindingsMutex);
        auto it = m_peerBindings.find(productId);

        if (it == m_peerBindings.end())
        {
            // The peer is not registered yet, but we have their ID from the IPv6 address.
            // We must reconstruct the EOS Handle and register them implicitly.

            EOS_ProductUserId remoteUserHandle = nullptr;
            {
                SdkLock lock(GetSdkMutex());
                remoteUserHandle = EOS_ProductUserId_FromString(productId.c_str());
            }
            if (!remoteUserHandle)
            {
				NS::log::EOS->error("Failed to convert ProductID string to Handle for {}", productId.c_str());
                return false;
            }

            binding.remoteUser = remoteUserHandle;
            binding.channel = 0; // Default channel for implicit connections
            binding.endpoint = endpoint;
            binding.productUserId = productId;
            binding.socketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
            binding.route = normalizedRoute;

            std::snprintf(binding.socketId.SocketName, EOS_P2P_SOCKETID_SOCKETNAME_SIZE, "ion");

            // Insert into map so future lookups succeed
            m_peerBindings[productId] = binding;

			NS::log::EOS->info("Implicitly registered peer {} from sendto address", productId);
        }
        else
        {
            it->second.route = CombineRoutes(it->second.route, normalizedRoute);
            binding = it->second;
        }
    }
    EOS_P2P_SendPacketOptions sendOpts{};
    sendOpts.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
    sendOpts.LocalUserId = localUser;
    sendOpts.RemoteUserId = binding.remoteUser;
    sendOpts.SocketId = &binding.socketId;
    sendOpts.Channel = binding.channel;
    sendOpts.Reliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
    sendOpts.bAllowDelayedDelivery = EOS_TRUE;
    sendOpts.Data = data;
    sendOpts.DataLengthBytes = static_cast<uint32_t>(length);

    EOS_EResult result = EOS_EResult::EOS_UnexpectedError;
    {
        SdkLock lock(GetSdkMutex());
        result = EOS_P2P_SendPacket(m_p2pHandle, &sendOpts);
    }
    if (result != EOS_EResult::EOS_Success)
    {
		NS::log::EOS->error("SendPacket failed ({})", static_cast<int>(result));
        return false;
    }

    return true;
}

void FakeIpLayer::PumpIncoming()
{
    EOS_ProductUserId localUser = m_localUser.load(std::memory_order_acquire);
    if (!m_p2pHandle || !localUser)
        return;

    while (true)
    {
        EOS_P2P_GetNextReceivedPacketSizeOptions sizeOpts{};
        sizeOpts.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
        sizeOpts.LocalUserId = localUser;

        uint32_t packetSize = 0;
        EOS_EResult sizeResult = EOS_EResult::EOS_UnexpectedError;
        {
            SdkLock lock(GetSdkMutex());
            sizeResult = EOS_P2P_GetNextReceivedPacketSize(m_p2pHandle, &sizeOpts, &packetSize);
        }

        if (sizeResult == EOS_EResult::EOS_InvalidAuth)
        {
            auto& layer = EosLayer::Instance();
            // Only log when we actually start a new re-login attempt.
            if (layer.RequestReloginAsync())
            {
                NS::log::EOS->warn("LocalUserId invalid, re-login scheduled");
            }

            // Stop processing incoming packets until auth is fixed.
            return;
        }

        if (sizeResult != EOS_EResult::EOS_Success)
            break;

        PendingPacket packet;
        packet.payload.resize(packetSize);

        EOS_P2P_ReceivePacketOptions recvOpts{};
        recvOpts.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
        recvOpts.LocalUserId = localUser;
        recvOpts.MaxDataSizeBytes = packetSize;

        EOS_ProductUserId remoteUser = nullptr;
        EOS_P2P_SocketId socketId{};
        socketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strcpy_s(socketId.SocketName, "ion");
        uint8_t channel = 0;
        uint32_t outSize = 0;

        EOS_EResult recvResult = EOS_EResult::EOS_UnexpectedError;
        {
            SdkLock lock(GetSdkMutex());
            recvResult = EOS_P2P_ReceivePacket(
                m_p2pHandle,
                &recvOpts,
                &remoteUser,
                &socketId,
                &channel,
                packet.payload.data(),
                &outSize);
        }

        if (recvResult != EOS_EResult::EOS_Success)
            break;

        packet.payload.resize(outSize);

        FakeEndpoint endpoint{};
        PacketRoute packetRoute = PacketRoute::All;
        if (!ResolveEndpoint(remoteUser, socketId.SocketName, channel, endpoint, &packetRoute))
            continue;

        packet.sender = endpoint;
        packet.route = NormalizeRoute(packetRoute);

        {
            std::lock_guard guard(m_queueMutex);
            m_packets.emplace_back(std::move(packet));
        }
    }
}

bool FakeIpLayer::ResolveEndpoint(EOS_ProductUserId remoteUser,
                                  const char* /*socketName*/,
                                  uint8_t /*channel*/,
                                  FakeEndpoint& outEndpoint,
                                  PacketRoute* outRoute) const
{
    std::string productIdRaw;
    if (!GetProductIdString(remoteUser, productIdRaw))
        return false;

    std::string normalizedId;
    if (!NormalizeProductId(productIdRaw, normalizedId))
        return false;

    {
        std::lock_guard guard(m_bindingsMutex);
        auto it = m_peerBindings.find(normalizedId);
        if (it != m_peerBindings.end())
        {
            outEndpoint = it->second.endpoint;
            if (outRoute)
                *outRoute = NormalizeRoute(it->second.route);
            return true;
        }
    }

    if (!EncodeProductId(normalizedId, outEndpoint))
        return false;

    if (outRoute)
        *outRoute = PacketRoute::All;

    return true;
}

bool FakeIpLayer::PopPacket(PacketRoute desiredRoute, PendingPacket& outPacket)
{
    std::lock_guard guard(m_queueMutex);
    const PacketRoute normalizedDesired = NormalizeRoute(desiredRoute);
    if (m_packets.empty())
        return false;

    for (auto it = m_packets.begin(); it != m_packets.end(); ++it)
    {
        const PacketRoute packetRoute = NormalizeRoute(it->route);
        if (!AnyRoute(packetRoute & normalizedDesired))
            continue;

        outPacket = std::move(*it);
        m_packets.erase(it);
        return true;
    }
    return false;
}

void FakeIpLayer::SetLocalUser(EOS_ProductUserId localUser)
{
    m_localUser.store(localUser, std::memory_order_release);
    UpdateLocalEndpoint();
}

void FakeIpLayer::UpdateLocalEndpoint()
{
    auto* localUser = m_localUser.load(std::memory_order_acquire);
    if (!localUser)
        return;

    std::string productIdRaw;
    if (!GetProductIdString(localUser, productIdRaw))
        return;

    std::string normalizedId;
    if (!NormalizeProductId(productIdRaw, normalizedId))
        return;

    FakeEndpoint endpoint{};
    if (!EncodeProductId(normalizedId, endpoint))
        return;

    m_localEndpoint = endpoint;

    char buffer[INET6_ADDRSTRLEN]{};
    if (InetNtopA(AF_INET6, const_cast<in6_addr*>(&m_localEndpoint.address), buffer, sizeof(buffer)))
    {
        NS::log::EOS->info("Local FakeIPv6 [{}]:{}", buffer, m_localEndpoint.port);
		Cvar_eos_current_endpoint->SetValue(fmt::format("[{}]:{}", buffer, m_localEndpoint.port).c_str());
    }
}

void FakeIpLayer::Clear()
{
    {
        std::lock_guard guard(m_bindingsMutex);
        m_peerBindings.clear();
    }
    {
        std::lock_guard guard(m_queueMutex);
        m_packets.clear();
    }
}

bool IsServerNetContext()
{
    if (IsDedicatedServer())
        return true;

    return g_pServerState && *g_pServerState > ss_dead;
}

uint16_t GetPretendRemotePort()
{
    const uint16_t clientPort = g_pCVar->FindVar("clientport")->GetInt();
    const uint16_t hostPort = g_pCVar->FindVar("hostport")->GetInt();

    if (IsServerNetContext())
        return clientPort ? clientPort : hostPort;

    return hostPort ? hostPort : clientPort;
}

} // namespace eos

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", EOSFakeIPLayer, ConVar, (CModule module))
{
	Cvar_eos_current_endpoint = new ConVar(
		"eos_current_endpoint",
		"",
		FCVAR_REPLICATED,
		"The host's EOS Fake IP Layer endpoint. (readonly)");
}
