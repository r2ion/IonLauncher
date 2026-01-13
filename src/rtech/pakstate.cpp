#include "pakstate.h"
#include "core/tier0.h"
#include "util/utils.h"

PakGlobalState_s* g_pakGlobalState;

void ConCommand_dump_pak_info(const CCommand& args)
{
	PakGlobalState_s* pakGlobals = Pak_GetGlobals();
	if (!pakGlobals)
	{
		spdlog::info("Pak global state not initialized!");
		return;
	}

	for (int i = 0; i < pakGlobals->loadedPakCount; i++)
	{
		PakLoadedInfo_s& pakInfo = pakGlobals->loadedPaks[i];
        spdlog::info("Pak {}: filename='{}', handle={}, status={}, assetCount={}, hModule={:p}",
            i,
            pakInfo.filename ? pakInfo.filename : "<null>",
            static_cast<int>(pakInfo.handle),
            static_cast<int>(pakInfo.status),
            pakInfo.assetCount,
            reinterpret_cast<void*>(pakInfo.hModule));
	}
}

AUTOHOOK_INIT()


PakGlobalState_s* Pak_GetGlobals()
{
	if(g_pakGlobalState)
		return g_pakGlobalState;

	HMODULE hRpakGame = GetModuleHandleA("rtech_game.dll");

	if (!hRpakGame)
		return nullptr;

	CModule rtechGameModule(hRpakGame);

	g_pakGlobalState = rtechGameModule.Offset(0x43270).RCast<PakGlobalState_s*>();
	return g_pakGlobalState;
}

ON_DLL_LOAD_RELIESON("engine.dll", PakFileEngine, ConCommand, (CModule module))
{
	RegisterConCommand("ns_dump_pak_info", ConCommand_dump_pak_info, "Dumps information about loaded PAK files.", FCVAR_DONTRECORD);

}
