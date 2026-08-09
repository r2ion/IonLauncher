#include "engine/client/client.h"
#include "server/r2server.h"

CClientDisconnectFn CClient__Disconnect;
CClientSendDataBlockFn CClient__SendDataBlock;
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

DECLARE_MODULE(EngineClientHooks)

DECLARE_HOOK(CClient__Clear, engine.dll + 0x101480, [](auto& hook, CClient* thisptr)
{
	/*g_pServer->GetClientExtended(thisptr->m_nUserID)->Reset();*/
	thisptr->GetClientExtended()->Reset();
	hook.Original(thisptr);
})

ON_DLL_LOAD("engine.dll", CClient, [](CModule module)
{
	DISPATCH_MODULE(EngineClientHooks)

	CClient__Disconnect = module.Offset(0x1012C0).RCast<CClientDisconnectFn>();
	CClient__SendDataBlock = module.Offset(0x104870).RCast<CClientSendDataBlockFn>();
	g_pClientArray = module.Offset(0x12A53F90).RCast<CClient*>();
})
