#include "vscript/languages/squirrel_re/squirrel.h"
#include "game/client/r1/r1_hud_scriptelements.h"
#include "game/client/portal2/gameui/gameui_util.h"
#include "common/netmessages.h"
#include "engine/client/cdll_engine_int.h"
#include "engine/client/clientstate.h"
#include "engine/client/connection_manager.h"
#include "engine/client/origin_sdk.h"
#include "engine/cmd.h"
#include "engine/demo.h"
#include "engine/net.h"
#include "engine/server/serverauthentication.h"
#include "masterserver/masterserver.h"
#include "tier1/bitbuf.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"
#include "tier1/utlstring.h"
#include "util/wininfo.h"
#include "vgui_controls/Panel.h"

#include <cstddef>
#include <cstdlib>

// functions for viewing server browser

ADD_SQFUNC("void", NSRequestServerList, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	g_pMasterServerManager->RequestServerList();
	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsRequestingServerList, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pMasterServerManager->m_bScriptRequestingServerList);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSMasterServerConnectionSuccessful, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pMasterServerManager->m_bSuccessfullyConnected);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSGetServerCount, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, (SQInteger)g_pMasterServerManager->m_vRemoteServers.size());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSClearRecievedServerList, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	g_pMasterServerManager->ClearServerList();
	return SQRESULT_NULL;
}

// functions for authenticating with servers

ADD_SQFUNC("string", NSGetServerIDFromIndex, "int serverIndex", "", ScriptContext::UI)
{
	SQInteger serverIndex = g_pSquirrel[context]->getinteger(sqvm, 1);

	if (serverIndex >= g_pMasterServerManager->m_vRemoteServers.size())
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Tried to get server ID from index {} when only {} servers are available",
				serverIndex,
				g_pMasterServerManager->m_vRemoteServers.size())
				.c_str());
		return SQRESULT_ERROR;
	}

	const RemoteServerInfo& remoteServer = g_pMasterServerManager->m_vRemoteServers[serverIndex];

	g_pSquirrel[context]->pushstring(sqvm, remoteServer.id, -1);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSTryAuthWithServer, "int serverIndex, string password = ''", "", ScriptContext::UI)
{
	SQInteger serverIndex = g_pSquirrel[context]->getinteger(sqvm, 1);
	const SQChar* password = g_pSquirrel[context]->getstring(sqvm, 2);

	if (serverIndex >= g_pMasterServerManager->m_vRemoteServers.size())
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Tried to auth with server index {} when only {} servers are available",
				serverIndex,
				g_pMasterServerManager->m_vRemoteServers.size())
				.c_str());
		return SQRESULT_ERROR;
	}

	// send off persistent data first, don't worry about server/client stuff, since m_additionalPlayerData should only have entries when
	// we're a local server note: this seems like it could create a race condition, test later
	for (auto& pair : g_pServerAuthentication->m_PlayerAuthenticationData)
		g_pServerAuthentication->WritePersistentData(pair.first);

	// do auth
	g_pMasterServerManager->AuthenticateWithServer(
		g_pLocalPlayerUserID,
		g_pMasterServerManager->m_sOwnClientAuthToken,
		g_pMasterServerManager->m_vRemoteServers[serverIndex],
		(char*)password);

	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsAuthenticatingWithServer, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pMasterServerManager->m_bScriptAuthenticatingWithGameServer);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSWasAuthSuccessful, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("string", NSGetAuthedServerIPv4, "", "", ScriptContext::UI)
{
	if (!g_pMasterServerManager->m_bHasPendingConnectionInfo)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm, fmt::format("Tried to get authed server info before any pending connection info was available").c_str());
		return SQRESULT_ERROR;
	}

	RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;

	std::string ipString = fmt::format(
		"{}.{}.{}.{}",
		info.ip.S_un.S_un_b.s_b1,
		info.ip.S_un.S_un_b.s_b2,
		info.ip.S_un.S_un_b.s_b3,
		info.ip.S_un.S_un_b.s_b4);

	g_pSquirrel[context]->pushstring(sqvm, ipString.c_str(), -1);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSGetAuthedServerPort, "", "", ScriptContext::UI)
{
	if (!g_pMasterServerManager->m_bHasPendingConnectionInfo)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm, fmt::format("Tried to get authed server info before any pending connection info was available").c_str());
		return SQRESULT_ERROR;
	}

	RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;

	g_pSquirrel[context]->pushinteger(sqvm, (SQInteger)info.port);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSConnectToAuthedServer, "", "", ScriptContext::UI)
{
	if (!g_pMasterServerManager->m_bHasPendingConnectionInfo)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm, fmt::format("Tried to connect to authed server before any pending connection info was available").c_str());
		return SQRESULT_ERROR;
	}

	RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;

	// set auth token, then try to connect
	// i'm honestly not entirely sure how silentconnect works regarding ports and encryption so using connect for now
	g_pCVar->FindVar("serverfilter")->SetValue(info.authToken);
	Cbuf_AddText(
		Cbuf_GetCurrentPlayer(),
		fmt::format(
			"connect {}.{}.{}.{}:{}",
			info.ip.S_un.S_un_b.s_b1,
			info.ip.S_un.S_un_b.s_b2,
			info.ip.S_un.S_un_b.s_b3,
			info.ip.S_un.S_un_b.s_b4,
			info.port)
			.c_str(),
		cmd_source_t::kCommandSrcCode);

	g_pMasterServerManager->m_bHasPendingConnectionInfo = false;
	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSTryAuthWithLocalServer, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	// do auth request
	g_pMasterServerManager->AuthenticateWithOwnServer(g_pLocalPlayerUserID, g_pMasterServerManager->m_sOwnClientAuthToken, {});

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSCompleteAuthWithLocalServer, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	// literally just set serverfilter
	// note: this assumes we have no authdata other than our own
	if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
		g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

	return SQRESULT_NULL;
}

ADD_SQFUNC("string", NSGetAuthFailReason, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushstring(sqvm, g_pMasterServerManager->m_sAuthFailureReason.c_str(), -1);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("array<ServerInfo>", NSGetGameServers, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);
	for (size_t i = 0; i < g_pMasterServerManager->m_vRemoteServers.size(); i++)
	{
		const RemoteServerInfo& remoteServer = g_pMasterServerManager->m_vRemoteServers[i];

		g_pSquirrel[context]->pushnewstructinstance(sqvm, 11);

		// index
		g_pSquirrel[context]->pushinteger(sqvm, (SQInteger)i);
		g_pSquirrel[context]->sealstructslot(sqvm, 0);

		// id
		g_pSquirrel[context]->pushstring(sqvm, remoteServer.id, -1);
		g_pSquirrel[context]->sealstructslot(sqvm, 1);

		// name
		g_pSquirrel[context]->pushstring(sqvm, remoteServer.name, -1);
		g_pSquirrel[context]->sealstructslot(sqvm, 2);

		// description
		g_pSquirrel[context]->pushstring(sqvm, remoteServer.description.c_str(), -1);
		g_pSquirrel[context]->sealstructslot(sqvm, 3);

		// map
		g_pSquirrel[context]->pushstring(sqvm, remoteServer.map, -1);
		g_pSquirrel[context]->sealstructslot(sqvm, 4);

		// playlist
		g_pSquirrel[context]->pushstring(sqvm, remoteServer.playlist, -1);
		g_pSquirrel[context]->sealstructslot(sqvm, 5);

		// playerCount
		g_pSquirrel[context]->pushinteger(sqvm, remoteServer.playerCount);
		g_pSquirrel[context]->sealstructslot(sqvm, 6);

		// maxPlayerCount
		g_pSquirrel[context]->pushinteger(sqvm, remoteServer.maxPlayers);
		g_pSquirrel[context]->sealstructslot(sqvm, 7);

		// requiresPassword
		g_pSquirrel[context]->pushbool(sqvm, remoteServer.requiresPassword);
		g_pSquirrel[context]->sealstructslot(sqvm, 8);

		// region
		g_pSquirrel[context]->pushstring(sqvm, remoteServer.region, -1);
		g_pSquirrel[context]->sealstructslot(sqvm, 9);

		// requiredMods
		g_pSquirrel[context]->newarray(sqvm);
		for (const RemoteModInfo& mod : remoteServer.requiredMods)
		{
			g_pSquirrel[context]->pushnewstructinstance(sqvm, 2);

			// name
			g_pSquirrel[context]->pushstring(sqvm, mod.Name.c_str(), -1);
			g_pSquirrel[context]->sealstructslot(sqvm, 0);

			// version
			g_pSquirrel[context]->pushstring(sqvm, mod.Version.c_str(), -1);
			g_pSquirrel[context]->sealstructslot(sqvm, 1);

			g_pSquirrel[context]->arrayappend(sqvm, -2);
		}
		g_pSquirrel[context]->sealstructslot(sqvm, 10);

		g_pSquirrel[context]->arrayappend(sqvm, -2);
	}
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSCancelConnection, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	g_pConnectionManager->Interrupt();
	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSRequestServerInfo, "string ip, int port, bool requestMods, bool serverAuthUs", "", ScriptContext::UI)
{
	const SQChar* ip = g_pSquirrel[context]->getstring(sqvm, 1);
	SQInteger port = g_pSquirrel[context]->getinteger(sqvm, 2);
	bool requestMods = g_pSquirrel[context]->getbool(sqvm, 3);
	bool serverAuthUs = g_pSquirrel[context]->getbool(sqvm, 4);

	g_bReceivedServerInfo.store(false, std::memory_order_release);
	g_bReceivedAuthNotify.store(false, std::memory_order_release);

	netadr_t addr;

	if (std::strchr(ip, ':') != nullptr)
	{
		// IPv6
		std::string formatted = fmt::format("[{}]:{}", ip, port);
		addr = CNetAdr(formatted.c_str());

		char dummyPackBuf[32];
		bf_write dummyPack(dummyPackBuf, sizeof(dummyPackBuf));
		dummyPack.WriteLong(CONNECTIONLESS_HEADER);
		NET_SendPacket(nullptr, NS_CLIENT, &addr, dummyPack.GetData(), dummyPack.GetNumBytesWritten(), nullptr, false, 0, true);
	}
	else
	{
		std::string formatted = fmt::format("[::ffff:{}]:{}", ip, port);
		addr = CNetAdr(formatted.c_str());
	}

	g_bNextServerAllowingAuthUs = false;
	g_bNextServerAuthUs = false;

	char buffer[256];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);

	if(serverAuthUs)
	{
		g_bNextServerAuthUs = true;
		msg.WriteByte(1);
		msg.WriteString(g_pLocalPlayerUserID);
		msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);
	}
	else
	{
		msg.WriteByte(0);
	}

	g_bListeningforCustomServerInfoPacket = true;

	NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsServerAuthingUs, "", "Returns true if the last requested server is authenticating us.", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_bNextServerAllowingAuthUs);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("string", NSGetNameFromServerInfo, "", "Returns the name from the last received server info packet.", ScriptContext::UI)
{
	g_pSquirrel[context]->pushstring(sqvm, g_szLastServerInfoName);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSMarkConnectingToServer, "bool state", "Skips the retry logic and marks that we're connecting to the server.", ScriptContext::UI)
{
	g_bConnectingToServer = g_pSquirrel[context]->getbool(sqvm, 1);
	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsMasterServerAuthenticated, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pMasterServerManager->m_bOriginAuthWithMasterServerDone.load(std::memory_order_acquire));
	return SQRESULT_NOTNULL;
}

/*
global struct MasterServerAuthResult
{
	bool success
	string errorCode
	string errorMessage
}
*/



ADD_SQFUNC("void", NSRequestMasterServerAuth, "", "", ScriptContext::UI)
{
	if (g_pMasterServerManager->m_bOriginAuthWithMasterServerDone.load(std::memory_order_acquire) ||
		g_pMasterServerManager->m_bOriginAuthWithMasterServerInProgress.load(std::memory_order_acquire))
		return SQRESULT_NULL;

	if (g_pCVar->FindVar("ns_has_agreed_to_send_token")->GetInt() == AGREED_TO_SEND_TOKEN)
		g_pMasterServerManager->AuthenticateOriginWithMasterServer();

	return SQRESULT_NULL;
}

ADD_SQFUNC("MasterServerAuthResult", NSGetMasterServerAuthResult, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 3);

	g_pSquirrel[context]->pushbool(sqvm, g_pMasterServerManager->m_bOriginAuthWithMasterServerSuccessful.load(std::memory_order_acquire));
	g_pSquirrel[context]->sealstructslot(sqvm, 0);

	g_pSquirrel[context]->pushstring(sqvm, g_pMasterServerManager->m_sOriginAuthWithMasterServerErrorCode.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);

	g_pSquirrel[context]->pushstring(sqvm, g_pMasterServerManager->m_sOriginAuthWithMasterServerErrorMessage.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("table< string, string >", NSGetFriendSubscriptionMap, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->newtable(sqvm);

	if (g_IDPartySubMap.empty())
		return SQRESULT_NOTNULL;

	for (std::pair<__int64, std::string> item : g_IDPartySubMap)
	{
		g_pSquirrel[context]->pushstring(sqvm, std::to_string(item.first).c_str(), -1);
		g_pSquirrel[context]->pushstring(sqvm, item.second.c_str(), -1);

		g_pSquirrel[context]->newslot(sqvm, -3, false);
	}

	return SQRESULT_NOTNULL;
}

// mirror this in script
enum eMainMenuPromoDataProperty
{
	newInfoTitle1,
	newInfoTitle2,
	newInfoTitle3,

	largeButtonTitle,
	largeButtonText,
	largeButtonUrl,
	largeButtonImageIndex,

	smallButton1Title,
	smallButton1Url,
	smallButton1ImageIndex,

	smallButton2Title,
	smallButton2Url,
	smallButton2ImageIndex
};

ADD_SQFUNC("void", NSRequestCustomMainMenuPromos, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	g_pMasterServerManager->RequestMainMenuPromos();
	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSHasCustomMainMenuPromoData, "", "", ScriptContext::UI)
{
	g_pSquirrel[ScriptContext::UI]->pushbool(sqvm, g_pMasterServerManager->m_bHasMainMenuPromoData);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("var", NSGetCustomMainMenuPromoData, "int promoDataKey", "", ScriptContext::UI)
{
	if (!g_pMasterServerManager->m_bHasMainMenuPromoData)
		return SQRESULT_NULL;

	switch (g_pSquirrel[ScriptContext::UI]->getinteger(sqvm, 1))
	{
	case eMainMenuPromoDataProperty::newInfoTitle1:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.newInfoTitle1.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::newInfoTitle2:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.newInfoTitle2.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::newInfoTitle3:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.newInfoTitle3.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::largeButtonTitle:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.largeButtonTitle.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::largeButtonText:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.largeButtonText.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::largeButtonUrl:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.largeButtonUrl.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::largeButtonImageIndex:
	{
		g_pSquirrel[ScriptContext::UI]->pushinteger(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.largeButtonImageIndex);
		break;
	}

	case eMainMenuPromoDataProperty::smallButton1Title:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.smallButton1Title.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::smallButton1Url:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.smallButton1Url.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::smallButton1ImageIndex:
	{
		g_pSquirrel[ScriptContext::UI]->pushinteger(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.smallButton1ImageIndex);
		break;
	}

	case eMainMenuPromoDataProperty::smallButton2Title:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.smallButton2Title.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::smallButton2Url:
	{
		g_pSquirrel[ScriptContext::UI]->pushstring(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.smallButton2Url.c_str());
		break;
	}

	case eMainMenuPromoDataProperty::smallButton2ImageIndex:
	{
		g_pSquirrel[ScriptContext::UI]->pushinteger(sqvm, g_pMasterServerManager->m_sMainMenuPromoData.smallButton2ImageIndex);
		break;
	}
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", Demo_IsPlayingBack, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pDemoPlayer->IsPlayingBack());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", Demo_TogglePause, "", "", ScriptContext::UI)
{
	if (g_pDemoPlayer->IsPlaybackPaused())
	{
		g_pDemoPlayer->ResumePlayback();
	}
	else
	{
		g_pDemoPlayer->PausePlayback(0);
	}

	return SQRESULT_NULL;
}

ADD_SQFUNC("int", Demo_GetPlaybackTick, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, g_pDemoPlayer->GetPlaybackTick());

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", Demo_GetTotalTicks, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, g_pDemoPlayer->GetTotalTicks());

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("array<string>", Demo_GetDemoFiles, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);


	for (const auto& entry : fs::directory_iterator("./r2"))
	{
		if (entry.path().extension() == ".dem")
		{
			const std::string filename = entry.path().filename().string();
			g_pSquirrel[context]->pushstring(sqvm, filename.c_str());
			g_pSquirrel[context]->arrayappend(sqvm, -2);
		}
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", Demo_StopPlayback, "", "", ScriptContext::UI)
{
	Cbuf_AddText(Cbuf_GetCurrentPlayer(), "stopdemo", cmd_source_t::kCommandSrcCode);
	Cbuf_Execute();
	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSCopyToClipboard, "string text", "", ScriptContext::UI)
{
	const SQChar* text = g_pSquirrel[context]->getstring(sqvm, 1);

	if (!OpenClipboard(nullptr))
		return SQRESULT_NULL;

	EmptyClipboard();

	size_t len = strlen(text) + 1;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	if (hMem)
	{
		void* pMem = GlobalLock(hMem);
		if (pMem)
		{
			memcpy(pMem, text, len);
			GlobalUnlock(hMem);
			SetClipboardData(CF_TEXT, hMem);
		}
		else
		{
			GlobalFree(hMem);
		}
	}

	CloseClipboard();

	return SQRESULT_NULL;
}

ADD_SQFUNC("string", NSPasteFromClipboard, "", "", ScriptContext::UI)
{
	if (!OpenClipboard(nullptr))
	{
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr)
	{
		CloseClipboard();
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	char* pText = static_cast<char*>(GlobalLock(hData));
	if (pText == nullptr)
	{
		CloseClipboard();
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	std::string result(pText);

	GlobalUnlock(hData);
	CloseClipboard();

	g_pSquirrel[context]->pushstring(sqvm, result.c_str());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("vector ornull", NSGetCursorPosition, "", "", ScriptContext::UI)
{
	RECT rcClient;
	POINT p;
	if (GetCursorPos(&p) && ScreenToClient(*g_gameHWND, &p) && GetClientRect(*g_gameHWND, &rcClient))
	{
		if (GetAncestor(GetForegroundWindow(), GA_ROOTOWNER) != *g_gameHWND)
			return SQRESULT_NULL;

		g_pSquirrel[context]->pushvector(
			sqvm,
			{p.x > 0 ? p.x > rcClient.right ? rcClient.right : (float)p.x : 0,
			 p.y > 0 ? p.y > rcClient.bottom ? rcClient.bottom : (float)p.y : 0,
			 0});
		return SQRESULT_NOTNULL;
	}
	g_pSquirrel[context]->raiseerror(sqvm, "Failed retrieving cursor position of game window");
	return SQRESULT_ERROR;
}

static_assert(sizeof(CUtlString) == 0x20);

class CDialogListButton
{
public:
	void RemoveListItems();
	void ChangeConVar(const char* convarName);
	bool IsDefaultValue();
};


void CDialogListButton::RemoveListItems()
{
	*reinterpret_cast<int*>(reinterpret_cast<std::byte*>(this) + 0x530) = 0;
}

void CDialogListButton::ChangeConVar(const char* convarName)
{
	auto* convarRef = static_cast<CGameUIConVarRef*>(std::malloc(sizeof(CGameUIConVarRef)));
	convarRef->Init(convarName);
	*reinterpret_cast<CGameUIConVarRef**>(reinterpret_cast<std::byte*>(this) + 0x4E8) = convarRef;
}

ADD_SQFUNC("void", Hud_DialogList_RemoveListItems, "var elem", "", ScriptContext::UI)
{
	CClientScriptHudElement* hudElement = g_pSquirrel[context]->gethudelement<CClientScriptHudElement>(sqvm, 1);

	if (!hudElement)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

	vgui::Panel* dialogListButton = hudElement->GetPanel();
	const char* name = dialogListButton->GetName();

	if (!dialogListButton->IsDialogListButton())
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
		return SQRESULT_ERROR;
	}

	reinterpret_cast<CDialogListButton*>(dialogListButton)->RemoveListItems();

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", Hud_ChangeDialogListConVar, "var elem, string convarName", "", ScriptContext::UI)
{
	CClientScriptHudElement* hudElement = g_pSquirrel[context]->gethudelement<CClientScriptHudElement>(sqvm, 1);
	const char* convarName = g_pSquirrel[context]->getstring(sqvm, 2);

	if (!hudElement)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

	vgui::Panel* dialogListButton = hudElement->GetPanel();
	const char* name = dialogListButton->GetName();

	if (!dialogListButton->IsDialogListButton())
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
		return SQRESULT_ERROR;
	}

	reinterpret_cast<CDialogListButton*>(dialogListButton)->ChangeConVar(convarName);

	return SQRESULT_NULL;
}

using sub_738940_t = __int64 (__fastcall *)(uint64_t);
sub_738940_t sub_738940 = nullptr;
using sub_4A3620_t = int (__fastcall *)(void);
sub_4A3620_t sub_4A3620 = nullptr;
using sub_733BB0_t = unsigned int (__fastcall *)(const unsigned __int8 *, const unsigned __int8 *);
sub_733BB0_t sub_733BB0 = nullptr;

static IVEngineClient** g_ppScriptMenuEngineClient = nullptr;

bool CDialogListButton::IsDefaultValue()
{
	uintptr_t a1 = reinterpret_cast<uintptr_t>(this);
	const auto* playlistVariable = reinterpret_cast<const CUtlString*>(reinterpret_cast<const std::byte*>(this) + 0x4F0);
  __int64 v3; // rcx
  __int64 v4; // rax
  __int64 v5; // rbx
  unsigned __int8 *v6; // rdi
  int v7; // eax
  const char* v9; // rax
  const char* v12; // rdi
  const char* v13; // rax
  const char* v15; // rax

  IVEngineClient* engineClient = *g_ppScriptMenuEngineClient;

  if ( *(DWORD *)(a1 + 1132) != 3 )
    return 0;
  v3 = *(uint64_t *)(a1 + 1256);
  if ( v3 )
  {
    v4 = sub_738940(*(uint64_t *)(v3 + 8));
    v5 = *(uint64_t *)(a1 + 1256);
    v6 = (unsigned __int8 *)v4;
    v7 = sub_4A3620();
    if ( (unsigned int)sub_733BB0(v6, *reinterpret_cast<const unsigned __int8 **>(*reinterpret_cast<uint64_t *>(v5 + 16LL * v7 + 8) + 72LL)) )
      return 1;
  }
  if ( playlistVariable->Length() )
  {
    v9 = playlistVariable->Get();
    v12 = engineClient->GetCurrentPlaylistGameModeVar(
            *reinterpret_cast<unsigned __int8*>(a1 + 1296),
            v9,
            false);
    v13 = playlistVariable->Get();
    v15 = engineClient->GetCurrentPlaylistGameModeVar(
            *reinterpret_cast<unsigned __int8*>(a1 + 1296),
            v13,
            true);

	// this can be null when we use Hud_DialogList_RemoveListItems
	if( !v12 )
		return 0;
    if ( (unsigned int)sub_733BB0(reinterpret_cast<const unsigned __int8*>(v12), reinterpret_cast<const unsigned __int8*>(v15)) )
      return 1;
  }
  return 0;
}

DECLARE_MODULE(ScriptHudElemHooks)

DECLARE_HOOK(DialogListButton__IsDefaultValue, client.dll + 0x4C96A0, ([](auto& hook, CDialogListButton* dialogListButton) -> bool
{
	NOTE_UNUSED(hook);
	return dialogListButton->IsDefaultValue();
}))


ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ScriptMenuBindings, ConVar, [](CModule module)
{
	sub_738940 = module.Offset(0x738940).RCast<sub_738940_t>();
	sub_4A3620 = module.Offset(0x4A3620).RCast<sub_4A3620_t>();
	sub_733BB0 = module.Offset(0x733BB0).RCast<sub_733BB0_t>();
	g_ppScriptMenuEngineClient = module.Offset(0xC3D940).RCast<IVEngineClient**>();

	DISPATCH_MODULE(ScriptHudElemHooks);
})
