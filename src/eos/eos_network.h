#pragma once

#include <cstdint>

#include "eos_sdk.h"

#include "fake_ip_layer.h"

extern std::mutex g_socketRouteMutex;
extern std::unordered_map<SOCKET, eos::PacketRoute> g_socketRoutes;
extern std::unordered_map<SOCKET, bool> g_fakeIpRecvSockets;

namespace eos
{

bool Initialize();
bool InitializeNetworking();
void ShutdownNetworking();
bool IsReady();

bool RegisterPeerByString(const char* remoteProductUserId,
                          const char* socketName,
                          uint8_t channel,
                          FakeEndpoint* outEndpoint);
} // namespace eos
