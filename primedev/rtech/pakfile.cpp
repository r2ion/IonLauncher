#include "pakfile.h"
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
            pakInfo.handle,
            static_cast<int>(pakInfo.status),
            pakInfo.assetCount,
            reinterpret_cast<void*>(pakInfo.hModule));
	}
}

AUTOHOOK_INIT()

using Pak_Free_t = PakHandle_t(__fastcall*)(PakHandle_t handle);
Pak_Free_t Pak_Free = nullptr;

HOOK(v_Pak_Free, o_Pak_Free, PakHandle_t*, __fastcall, (PakHandle_t handle))
{
    PakGlobalState_s* pakGlobals = Pak_GetGlobals();
    if (!pakGlobals)
        return o_Pak_Free(handle);

    PakLoadedInfo_s* pak = &pakGlobals->loadedPaks[handle & PAK_MAX_LOADED_PAKS_MASK];

    // The engine frees four QWORDs starting at &slabBuffers; sanitize those inline slots.
    void** slabSlots = reinterpret_cast<void**>(&pak->slabBuffers);
    for (int i = 0; i < PAK_SLAB_BUFFER_TYPES; ++i)
    {
        if (!slabSlots[i] || IsBadReadPtr2(slabSlots[i]))
            slabSlots[i] = nullptr;
    }

    return o_Pak_Free(handle);
}

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

	CModule rtechGameModule("rtech_game.dll");
    Pak_Free = rtechGameModule.Offset(0x8900).RCast<Pak_Free_t>();
    v_Pak_Free.Dispatch(reinterpret_cast<LPVOID*>(&Pak_Free));
}
