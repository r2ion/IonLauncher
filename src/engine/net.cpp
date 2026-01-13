#include "net.h"

//===========================================================================//
//
// Purpose: implementation of the CNetAdr class.
// --------------------------------------------------------------------------
//===========================================================================//

char* (*GetIpStringFromClient)(int64_t a1);
int (*NET_SendPacket)(CNetChan* pChan, int iSocket, const netadr_t* toAdr, const uint8_t* pData, unsigned int nLen, void* pVoicePayload, bool bCompress, int unMillisecondsDelay, bool bEncrypt);
netadr_s__GetEncryptionKey_t netadr_s__GetEncryptionKey = nullptr;

//////////////////////////////////////////////////////////////////////
// Clears IP.
//////////////////////////////////////////////////////////////////////
void CNetAdr::Clear(void)
{
	ip = { };
	port = 0;
	type = netadrtype_t::NA_NULL;
}

//////////////////////////////////////////////////////////////////////
// Compares two addresses.
//////////////////////////////////////////////////////////////////////
bool CNetAdr::CompareAdr(const CNetAdr& other) const
{
	if (other.type == netadrtype_t::NA_LOOPBACK)
	{
		return true;
	}

	return IN6_ADDR_EQUAL(&ip, &other.ip);
}

//////////////////////////////////////////////////////////////////////
// Convert address to string.
//////////////////////////////////////////////////////////////////////
const char* CNetAdr::ToString(const bool bOnlyBase) const
{
	// Select a static buffer.
	static char s[4][128];
	static int slot = 0;
	const int useSlot = (slot++) % 4;

	// Render into it.
	ToString(s[useSlot], sizeof(s[0]), bOnlyBase);

	// Pray the caller uses it before it gets clobbered.
	return s[useSlot];
}

//////////////////////////////////////////////////////////////////////
// Convert address to string.
//////////////////////////////////////////////////////////////////////
size_t CNetAdr::ToString(char* const pchBuffer, const size_t unBufferSize, const bool bOnlyBase) const
{
	if (type == netadrtype_t::NA_NULL)
	{
		strncpy(pchBuffer, "null", unBufferSize); pchBuffer[unBufferSize - 1] = '\0';
		return std::min(sizeof("null")-1, unBufferSize);
	}
	else if (type == netadrtype_t::NA_LOOPBACK)
	{
		strncpy(pchBuffer, "loopback", unBufferSize); pchBuffer[unBufferSize - 1] = '\0';
		return std::min(sizeof("loopback")-1, unBufferSize);
	}
	else if (type == netadrtype_t::NA_IP)
	{
		char stringBuf[128];
		inet_ntop(AF_INET6, &ip, stringBuf, INET6_ADDRSTRLEN);

		const int ret = bOnlyBase
			? snprintf(pchBuffer, unBufferSize, "%s", stringBuf)
			: snprintf(pchBuffer, unBufferSize, "[%s]:%i", stringBuf, (int)ntohs(port));

		return ret < 0 ? 0 : std::min(static_cast<size_t>(ret), unBufferSize);
	}
	else
	{
		strncpy(pchBuffer, "unknown", unBufferSize); pchBuffer[unBufferSize - 1] = '\0';
		return std::min(sizeof("unknown")-1, unBufferSize);
	}
}

//////////////////////////////////////////////////////////////////////
// Converts address to address info.
//////////////////////////////////////////////////////////////////////
void CNetAdr::ToAdrinfo(addrinfo* pHint) const
{
	addrinfo hint{};
	hint.ai_flags = AI_PASSIVE;
	hint.ai_family = AF_INET6;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;

	char szBuffer[33];
	const int results = getaddrinfo(ToString(true), _itoa(GetPort(), szBuffer, 10), &hint, &pHint);

	if (results != 0)
	{
		wchar_t	* wszError = gai_strerror(results);
		spdlog::warn(L"Address info translation failed: ({})", wszError);
	}
}

//////////////////////////////////////////////////////////////////////
// Converts address to socket address.
//////////////////////////////////////////////////////////////////////
void CNetAdr::ToSockadr(struct sockaddr_storage* const pSadr) const
{
	reinterpret_cast<sockaddr_in6*>(pSadr)->sin6_family = AF_INET6;
	reinterpret_cast<sockaddr_in6*>(pSadr)->sin6_port = port;

	if (GetType() == netadrtype_t::NA_IP)
	{
		reinterpret_cast<sockaddr_in6*>(pSadr)->sin6_addr = ip;
	}
	else if (GetType() == netadrtype_t::NA_LOOPBACK)
	{
		reinterpret_cast<sockaddr_in6*>(pSadr)->sin6_addr = in6addr_loopback;
	}
}

//////////////////////////////////////////////////////////////////////
// Sets address from socket address.
//////////////////////////////////////////////////////////////////////
bool CNetAdr::SetFromSockadr(struct sockaddr_storage* const s)
{
	char szAdrv6[INET6_ADDRSTRLEN];
	sockaddr_in6* pAdrv6 = reinterpret_cast<sockaddr_in6*>(s);

	if (inet_ntop(pAdrv6->sin6_family, &pAdrv6->sin6_addr, szAdrv6, sizeof(szAdrv6)) &&
		SetFromString(szAdrv6))
	{
		SetPort(pAdrv6->sin6_port);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
// Checks whether the provided string contains IPv4 characters.
//////////////////////////////////////////////////////////////////////
static bool ContainsIPv4Chars(const char* const pszAddress)
{
	const char* it = pszAddress;
	char curr = *pszAddress;
	char last;

	const int64_t lut = 0x7E0000007E07FFll;
	do
	{
		if (((curr - '0') > '6') || !_bittest64(&lut, (curr - '0')))
		{
			last = curr;

			if (curr != '.')
				break;
		}

		last = *++it;
		curr = last;
	} while (last);

	return last != 0;
}

//////////////////////////////////////////////////////////////////////
// Checks whether the provided string contains IPv6 characters.
//////////////////////////////////////////////////////////////////////
static bool ContainsIPv6Chars(const char* const pszAddress)
{
	const char* it = pszAddress;
	char curr = *pszAddress;
	char last;

	do
	{
		if ((curr - '0') > '\t')
		{
			last = curr;

			if (curr != '.')
				break;
		}

		last = *++it;
		curr = last;
	} while (last);

	return last != 0;
}

//////////////////////////////////////////////////////////////////////
// Returns whether the provided string should be formatted as IPv4.
//////////////////////////////////////////////////////////////////////
static bool ShouldFormatAsIPv4(const char* const pszAddress)
{
	if (!strchr(pszAddress, ':'))
	{
		if (!*pszAddress)
			return true;

		return ContainsIPv6Chars(pszAddress);
	}

	return ContainsIPv4Chars(pszAddress) && !ContainsIPv6Chars(pszAddress);
}

//////////////////////////////////////////////////////////////////////
// Sets address from string.
//////////////////////////////////////////////////////////////////////
bool CNetAdr::SetFromString(const char* const pch, const bool bUseDNS)
{
	Clear();
	if (!pch)
		return false;

	SetType(netadrtype_t::NA_IP);

	char szAddress[128];
	strncpy(szAddress, pch, sizeof(szAddress));

	char* pszAddress = szAddress;
	szAddress[sizeof(szAddress) - 1] = '\0';

	if (szAddress[0] == '[') // Skip bracket.
	{
		pszAddress = &szAddress[1];
	}

	char* const bracketEnd = strchr(szAddress, ']');

	if (bracketEnd) // Get and remove the last bracket.
	{
		*bracketEnd = '\0';

		char* const portStart = &bracketEnd[1];
		char* const pchColon = strrchr(portStart, ':');

		if (pchColon && strchr(portStart, ':') == pchColon)
		{
			pchColon[0] = '\0'; // Set the port.
			SetPort(uint16_t(htons(uint16_t(atoi(&pchColon[1])))));
		}
	}

	if (ShouldFormatAsIPv4(pszAddress))
	{
		char szNewAddressV4[128];
		snprintf(szNewAddressV4, sizeof(szNewAddressV4), "::FFFF:%s", pszAddress);

		if (inet_pton(AF_INET6, szNewAddressV4, &this->ip) > 0)
		{
			return true;
		}
	}
	else // Address is formatted as IPv6.
	{
		if (inet_pton(AF_INET6, pszAddress, &this->ip) > 0)
		{
			return true;
		}
	}

	if (bUseDNS) // Perform DNS lookup instead.
	{
		ADDRINFOA hints;

		hints.ai_family = AF_INET6;
		hints.ai_flags = AI_ALL | AI_V4MAPPED;
		hints.ai_socktype = NULL;
		hints.ai_addrlen = NULL;
		hints.ai_canonname = nullptr;
		hints.ai_addr = nullptr;
		hints.ai_next = nullptr;

		PADDRINFOA ppResult;

		if (getaddrinfo(pszAddress, nullptr, &hints, &ppResult))
		{
			freeaddrinfo(ppResult);
			return false;
		}

		SetIP(reinterpret_cast<in6_addr*>(&ppResult->ai_addr->sa_data[6]));
		freeaddrinfo(ppResult);

		return true;
	}

	return false;
}

const char* __fastcall GetIpStringFromClientHook(int64_t a1) {
	if (a1 == 0)
	{
		return "null";
	}
	void* net_chan = reinterpret_cast<void*>(a1 + 0x368C);
	if (net_chan == nullptr)
	{
		return "null";
	}
	return GetIpStringFromClient(a1);
}

ON_DLL_LOAD("engine.dll", Net, (CModule module))
{
	GetIpStringFromClient = module.Offset(0x2101A0).RCast<decltype(GetIpStringFromClient)>();
	HookAttach(&(PVOID&)GetIpStringFromClient, (PVOID)GetIpStringFromClientHook);
	NET_SendPacket = module.Offset(0x21C240).RCast<decltype(NET_SendPacket)>();
	netadr_s__GetEncryptionKey = module.Offset(0x2154C0).RCast<netadr_s__GetEncryptionKey_t>();
}
