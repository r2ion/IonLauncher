#pragma once

#include "engine/bitbuf.h"

#define CONNECTIONLESS_HEADER 0xFFFFFFFF

typedef const char*(__fastcall* netadr_s__GetEncryptionKey_t)(void* thisptr);
extern netadr_s__GetEncryptionKey_t netadr_s__GetEncryptionKey;

class CNetChan;

typedef enum
{
	NA_NULL = 0,
	NA_LOOPBACK,
	NA_IP,
} netadrtype_t;

enum netsocket_e
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

#pragma pack(push, 1)
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
#pragma pack(pop)

typedef class CNetAdr netadr_t;

#pragma pack(push, 1)
typedef struct netpacket_s
{
	netadr_t adr; // sender address
	// int				source;		// received source
	char unk[10];
	double received_time;
	unsigned char* data; // pointer to raw packet data
	bf_read* message; // easy bitbuf data access // 'inpacket.message' etc etc (pointer)
	char unk2[16];
	int size;

	// bf_read			message;	// easy bitbuf data access // 'inpacket.message' etc etc (pointer)
	// int				size;		// size in bytes
	// int				wiresize;   // size in bytes before decompression
	// bool			stream;		// was send as stream
	// struct netpacket_s* pNext;	// for internal use, should be NULL in public
} netpacket_t;
#pragma pack(pop)

extern int (*NET_SendPacket)(CNetChan* pChan, int iSocket, const netadr_t* toAdr, const uint8_t* pData, unsigned int nLen, void* pVoicePayload, bool bCompress, int unMillisecondsDelay, bool bEncrypt);
