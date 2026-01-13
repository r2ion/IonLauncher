#include "eos_network.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "eos_layer.h"
#include "eos_threading.h"
#include "core/convar/cvar.h"
#include "util/version.h"

#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

ConVar* ns_allow_eos = nullptr;

std::mutex g_socketRouteMutex;
std::unordered_map<SOCKET, eos::PacketRoute> g_socketRoutes;
std::unordered_map<SOCKET, bool> g_fakeIpRecvSockets;

namespace
{

constexpr char kProductId[] = "38a41893d2fa4e73801e0daed2060cb6";
constexpr char kSandboxId[] = "d15ffd5f9b5b479aa7c382a8f6896cee";
constexpr char kDeploymentId[] = "aa85e873e78244909bef018a8532741c";
constexpr char kProductName[] = "ion";

struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;
};

using SendToFn = int (WSAAPI*)(SOCKET, const char*, int, int, const sockaddr*, int);
using RecvFromFn = int (WSAAPI*)(SOCKET, char*, int, int, sockaddr*, int*);
using CloseSocketFn = int (WSAAPI*)(SOCKET);

SendToFn g_realSendTo = nullptr;
RecvFromFn g_realRecvFrom = nullptr;
CloseSocketFn g_realCloseSocket = nullptr;
LPVOID g_sendToTarget = nullptr;
LPVOID g_recvFromTarget = nullptr;
LPVOID g_closeSocketTarget = nullptr;
bool g_hooksInstalled = false;

bool ExtractFakeEndpoint(const sockaddr* address, eos::FakeEndpoint& outEndpoint)
{
    if (!address || address->sa_family != AF_INET6)
        return false;

    const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
    if (ntohs(ipv6->sin6_addr.u.Word[0]) != 0x3FFE)
        return false;

    outEndpoint.address = ipv6->sin6_addr;
    outEndpoint.port = ntohs(ipv6->sin6_port);
    return true;
}

void WriteSockaddrForFakeEndpoint(const eos::FakeEndpoint& endpoint,
                                  sockaddr* outAddress,
                                  int* outLength)
{
    if (!outAddress || !outLength)
        return;

    const uint16_t prefix    = ntohs(endpoint.address.u.Word[0]);
    const uint16_t realPort  = endpoint.port;
    uint16_t pretendPort     = eos::GetPretendRemotePort();
    const int    inLen       = *outLength;

    if (!pretendPort)
        pretendPort = endpoint.port;

    if (*outLength >= static_cast<int>(sizeof(sockaddr_in6)))
    {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(outAddress);
        std::memset(ipv6, 0, sizeof(sockaddr_in6));
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port   = htons(pretendPort);
        ipv6->sin6_addr   = endpoint.address;
        *outLength        = sizeof(sockaddr_in6);

        char addrStr[INET6_ADDRSTRLEN] = {};
        InetNtopA(AF_INET6, &ipv6->sin6_addr, addrStr, sizeof(addrStr));
        return;
    }

    if (*outLength >= static_cast<int>(sizeof(sockaddr_in)))
    {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(outAddress);
        std::memset(ipv4, 0, sizeof(sockaddr_in));
        ipv4->sin_family           = AF_INET;
        ipv4->sin_port             = htons(pretendPort);
        ipv4->sin_addr.S_un.S_addr = 0;
        *outLength                 = sizeof(sockaddr_in);
    }
}

uint16_t GetLocalSocketPort(SOCKET socketHandle)
{
    sockaddr_storage addr{};
    int addrLen = sizeof(addr);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        return 0;

    if (addr.ss_family == AF_INET)
        return ntohs(reinterpret_cast<sockaddr_in*>(&addr)->sin_port);

    if (addr.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);

    return 0;
}

eos::PacketRoute ClassifySocketRoute(uint16_t port)
{
	if (!port)
		return eos::PacketRoute::None;

	const uint16_t clientPort = static_cast<uint16_t>(g_pCVar->FindVar("clientport")->GetInt());
	const uint16_t hostPort = static_cast<uint16_t>(g_pCVar->FindVar("hostport")->GetInt());

	if (port == clientPort)
		return eos::PacketRoute::Client;

	if (port == hostPort)
		return eos::PacketRoute::Server;

	return eos::PacketRoute::None;
}

eos::PacketRoute DetermineSocketRoute(SOCKET socketHandle)
{
    {
        std::lock_guard lock(g_socketRouteMutex);
        auto it = g_socketRoutes.find(socketHandle);
        if (it != g_socketRoutes.end())
            return it->second;
    }

    const uint16_t port = GetLocalSocketPort(socketHandle);
    const eos::PacketRoute route = ClassifySocketRoute(port);

    {
        std::lock_guard lock(g_socketRouteMutex);
        g_socketRoutes[socketHandle] = route;
    }

    return route;
}

int WSAAPI HookedSendTo(SOCKET socketHandle,
                        const char* buffer,
                        int length,
                        int flags,
                        const sockaddr* destAddr,
                        int destLen)
{
    eos::FakeEndpoint endpoint{};
    if (!buffer || length <= 0 || !ExtractFakeEndpoint(destAddr, endpoint))
    {
        return g_realSendTo
            ? g_realSendTo(socketHandle, buffer, length, flags, destAddr, destLen)
            : SOCKET_ERROR;
    }

    auto* layer = eos::EosLayer::Instance().GetFakeIpLayer();
    if (!layer)
    {
        WSASetLastError(WSAENOTCONN);
        return SOCKET_ERROR;
    }

    const eos::PacketRoute route = DetermineSocketRoute(socketHandle);
    if (!layer->SendToPeer(endpoint,
                           reinterpret_cast<const uint8_t*>(buffer),
                           static_cast<size_t>(length),
                           route))
    {
        WSASetLastError(WSAECONNABORTED);
        return SOCKET_ERROR;
    }

    return length;
}

int WSAAPI HookedRecvFrom(SOCKET socketHandle,
                          char* buffer,
                          int length,
                          int flags,
                          sockaddr* from,
                          int* fromLen)
{
	uintptr_t evilahnetreceivedatagram = (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandleA("engine.dll");
    if (evilahnetreceivedatagram != 0x21B5DB)
    {
        return g_realRecvFrom
            ? g_realRecvFrom(socketHandle, buffer, length, flags, from, fromLen)
            : SOCKET_ERROR;
    }

    auto* layer = eos::EosLayer::Instance().GetFakeIpLayer();
    if (layer)
    {
        eos::PendingPacket packet;
        const eos::PacketRoute desiredRoute = DetermineSocketRoute(socketHandle);
        if (layer->PopPacket(desiredRoute, packet))
        {
            const int copyLength =
                static_cast<int>(std::min<size_t>(static_cast<size_t>(length),
                                                  packet.payload.size()));
            if (buffer && copyLength > 0)
                std::memcpy(buffer, packet.payload.data(), copyLength);

            if (from && fromLen)
            {
                WriteSockaddrForFakeEndpoint(packet.sender, from, fromLen);
            }

            return copyLength;
        }
    }

    // Fallback: normal network traffic (or no EOS packet available)
    return g_realRecvFrom
        ? g_realRecvFrom(socketHandle, buffer, length, flags, from, fromLen)
        : SOCKET_ERROR;
}

int WSAAPI HookedCloseSocket(SOCKET s)
{
    {
        std::lock_guard lock(g_socketRouteMutex);
        g_socketRoutes.erase(s);
        g_fakeIpRecvSockets.erase(s);
    }
    return g_realCloseSocket ? g_realCloseSocket(s) : 0;
}

bool InstallSocketHooks()
{
    if (g_hooksInstalled)
        return true;

    void* origSend = HookImportByOrdinal("engine.dll", "WSOCK32.dll", 20, reinterpret_cast<void*>(&HookedSendTo));
    void* origRecv = HookImportByOrdinal("engine.dll", "WSOCK32.dll", 17, reinterpret_cast<void*>(&HookedRecvFrom));
	void* origClose = HookImportByOrdinal("engine.dll", "WSOCK32.dll", 3, reinterpret_cast<void*>(&HookedCloseSocket));

    if (!origSend || !origRecv || !origClose)
        return false;

    g_realSendTo   = reinterpret_cast<SendToFn>(origSend);
    g_realRecvFrom = reinterpret_cast<RecvFromFn>(origRecv);
	g_realCloseSocket = reinterpret_cast<CloseSocketFn>(origClose);

    g_hooksInstalled = true;
    return true;
}

void RemoveSocketHooks()
{
    if (!g_hooksInstalled)
        return;

	HookImportByOrdinal("engine.dll", "WSOCK32.dll", 3, reinterpret_cast<void*>(g_realCloseSocket));
	HookImportByOrdinal("engine.dll", "WSOCK32.dll", 17, reinterpret_cast<void*>(g_realRecvFrom));
	HookImportByOrdinal("engine.dll", "WSOCK32.dll", 20, reinterpret_cast<void*>(g_realSendTo));

	g_realSendTo = nullptr;
	g_realRecvFrom = nullptr;
	g_realCloseSocket = nullptr;
	g_hooksInstalled = false;
}

} // namespace

namespace eos
{

bool Initialize()
{
	if(ns_allow_eos->GetInt() != 1)
        return false;

    auto& layer = EosLayer::Instance();
    if (layer.IsInitialized())
        return true;

    if (!layer.Initialize(kProductId, kSandboxId, kDeploymentId, kProductName, version))
    {
        NS::log::EOS->error("Failed to initialize networking layer");
        return false;
    }

	InitializeNetworking();

    return true;
}

bool InitializeNetworking()
{
    // Only install hooks - EOS initialization will happen lazily when needed
    if (!InstallSocketHooks())
    {
        NS::log::EOS->error("Failed to install socket hooks");
        return false;
    }

    NS::log::EOS->info("Hooks installed for version {}", version);
    return true;
}

void ShutdownNetworking()
{
    RemoveSocketHooks();
    EosLayer::Instance().Shutdown();
}

bool RegisterPeerByString(const char* remoteProductUserId,
                          const char* socketName,
                          uint8_t channel,
                          FakeEndpoint* outEndpoint)
{
    auto& layer = EosLayer::Instance();
    if (!layer.IsReady() || !remoteProductUserId || !socketName)
        return false;

    auto* fakeLayer = layer.GetFakeIpLayer();
    if (!fakeLayer)
        return false;

    EOS_ProductUserId remoteUser = nullptr;
    {
        SdkLock lock(GetSdkMutex());
        remoteUser = EOS_ProductUserId_FromString(remoteProductUserId);
    }

    if (!remoteUser)
        return false;

    const FakeEndpoint endpoint = fakeLayer->RegisterPeer(remoteUser, socketName, channel);
    if (!endpoint.IsValid())
        return false;

    if (outEndpoint)
        *outEndpoint = endpoint;

    return true;
}

} // namespace eos

ON_DLL_LOAD_RELIESON("engine.dll", EOSNetwork, ConVar, (CModule module))
{
	ns_allow_eos = new ConVar("ns_has_agreed_allow_eos", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Allow using EOS P2P networking.");
}
