#pragma once

#include <cstddef>
#include <cstdint>

class CNetAdr;
struct netpacket_t;

bool EOS_Init();
void EOS_ShutdownNetworking();
bool EOS_IsReady();

bool EOS_IsFakeAddress(const CNetAdr& address);
bool EOS_SendPacket(int sourceSocket, const CNetAdr& destination, const std::uint8_t* data, std::size_t size);
bool EOS_ReceivePacket(int destinationSocket, netpacket_t* packet);

bool EOS_GetLocalEndpoint(CNetAdr& endpoint);
void EOS_ResetPacketQueue();
