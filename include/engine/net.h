#pragma once

#include <cstddef>
#include <cstdint>

#include "tier1/bitbuf.h"

#define CONNECTIONLESS_HEADER 0xFFFFFFFF

typedef const char*(__fastcall* netadr_s__GetEncryptionKey_t)(void* thisptr);
extern netadr_s__GetEncryptionKey_t netadr_s__GetEncryptionKey;

class CNetChan;

enum netadrtype_t : int
{
	NA_NULL = 0,
	NA_LOOPBACK,
	NA_IP,
};

enum netsocket_e : int
{
	NS_CLIENT = 0,	// client socket
	NS_SERVER,		// server socket

	// unknown as this seems unused in R5, but if the socket equals to this in
	// CServer::ConnectionlessPacketHandler() in case C2S_Challenge, the packet
	// sent back won't be encrypted
	NS_UNK0,

	// used for chat room, communities, discord presence, EA/Origin, etc
	NS_PRESENCE,

	MAX_SOCKETS // 4 in R5
};

class CNetAdr
{
public:
	CNetAdr(void)                  { Clear(); }
	CNetAdr(const char* const pch) { SetFromString(pch); }
	void	Clear(void);

	inline void	SetIP(const in6_addr* const inAdr)  { ip = *inAdr; }
	inline void	SetPort(const uint16_t newport)     { port = newport; }
	inline void	SetType(const netadrtype_t newtype) { type = newtype; }

	bool	SetFromSockadr(struct sockaddr_storage* s);
	bool	SetFromString(const char* const pch, const bool bUseDNS = false);

	inline netadrtype_t	GetType(void) const { return type; }
	inline uint16_t		GetPort(void) const { return port; }
	inline const in6_addr* GetIP(void) const { return &ip; }
	// not 100% if this is the encryption key but it's some sort of key definitely, and used in connectWithKey
	inline const char* GetEncryptionKey(void) const { return netadr_s__GetEncryptionKey((void*)this); }

	bool		CompareAdr(const CNetAdr& other) const;
	inline bool	ComparePort(const CNetAdr& other) const { return port == other.port; }
	inline bool	IsLoopback(void) const { return type == netadrtype_t::NA_LOOPBACK; } // true if engine loopback buffers are used.

	const char*	ToString(const bool onlyBase = false) const;
	size_t		ToString(char* const pchBuffer, const size_t unBufferSize, const bool onlyBase = false) const;
	void		ToAdrinfo(addrinfo* pHint) const;
	void		ToSockadr(struct sockaddr_storage* const s) const;
private:
	netadrtype_t type;
	in6_addr ip;
	uint16_t port;
};

static_assert(sizeof(CNetAdr) == 0x18);
static_assert(sizeof(netadrtype_t) == 0x04);
static_assert(sizeof(in6_addr) == 0x10);

typedef class CNetAdr netadr_t;

struct netpacket_t
{
	netadr_t from;            // 0x00: sender address
	int source;               // 0x18: receiving netsocket_e
	double received;          // 0x20: receive timestamp
	std::uint8_t* data;       // 0x28: raw packet bytes
	bf_read message;          // 0x30: bit-buffer view over data
	int size;                 // 0x70: decompressed byte count
	int wireSize;             // 0x74: byte count before decompression
	bool stream;              // 0x78: packet arrived through stream transport
	std::byte m_Padding0079[7];
	netpacket_t* next;        // 0x80: internal receive-queue link
};

static_assert(sizeof(netpacket_t) == 0x88);
static_assert(alignof(netpacket_t) == 0x8);
static_assert(offsetof(netpacket_t, from) == 0x00);
static_assert(offsetof(netpacket_t, source) == 0x18);
static_assert(offsetof(netpacket_t, received) == 0x20);
static_assert(offsetof(netpacket_t, data) == 0x28);
static_assert(offsetof(netpacket_t, message) == 0x30);
static_assert(offsetof(netpacket_t, size) == 0x70);
static_assert(offsetof(netpacket_t, wireSize) == 0x74);
static_assert(offsetof(netpacket_t, stream) == 0x78);
static_assert(offsetof(netpacket_t, next) == 0x80);

extern int (*NET_SendPacket)(CNetChan* pChan, int iSocket, const netadr_t* toAdr, const uint8_t* pData, unsigned int nLen, void* pVoicePayload, bool bCompress, int unMillisecondsDelay, bool bEncrypt);
