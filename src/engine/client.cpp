#include "client.h"
#include "server/r2server.h"

void (*CClient__Disconnect)(void* self, uint32_t unknownButAlways1, const char* reason, ...);
void (*CClient__SendDataBlock)(void* self, bf_write* msg);
CClient* g_pClientArray;

void CClient::Disconnect(const Reputation_t nRepLevel, const char* reason, ...)
{
	if (m_nSignonState != eSignonState::NONE)
	{
		char szBuf[1024];
		{
			va_list vArgs;
			va_start(vArgs, reason);

			const int ret = snprintf(szBuf, sizeof(szBuf), reason, vArgs);

			if (ret < 0)
				szBuf[0] = '\0';

			va_end(vArgs);
		}
		CClient__Disconnect(this, nRepLevel, szBuf);
	}
}


CClientExtended* CClient::GetClientExtended(void) const
{
	return g_pServer->GetClientExtended(m_nUserID);
}

AUTOHOOK_INIT()

AUTOHOOK(CClient__Clear, engine.dll + 0x101480, void, __fastcall, (CClient* thisptr))
{
	/*g_pServer->GetClientExtended(thisptr->m_nUserID)->Reset();*/
	thisptr->GetClientExtended()->Reset();
	CClient__Clear(thisptr);
}

ON_DLL_LOAD("engine.dll", CClient, (CModule module))
{
	AUTOHOOK_DISPATCH()

	CClient__Disconnect = module.Offset(0x1012C0).RCast<void (*)(void*, uint32_t, const char*, ...)>();
	CClient__SendDataBlock = module.Offset(0x104870).RCast<void (*)(void*, bf_write*)>();
	g_pClientArray = module.Offset(0x12A53F90).RCast<CClient*>();
}
