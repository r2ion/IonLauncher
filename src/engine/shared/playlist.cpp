#include "playlist.h"
#include "common/netmessages.h"
#include "core/convar/concommand.h"
#include "dedicated/dedicated.h"
#include "engine/client/clientstate.h"
#include "engine/r2engine.h"
#include "server/serverpresence.h"
#include "tier0/vanilla.h"
#include "tier1/strtools.h"
#include "vscript/languages/squirrel_re/squirrel.h"

PlaylistVarOverrides g_ServerOverrides;
bool* b_OverridesChanged;
bool* b_PendingOverrides;
bool* b_UseSingleplayerAimAssist;

PlaylistVarOverride* PlaylistVarOverrides::Find(const char* name)
{
    for (int i = 0; i < count; ++i)
    {
        if (!V_stricmp(entries[i].name, name))
            return &entries[i];
    }
    return nullptr;
}

bool PlaylistVarOverrides::Set(const char* name, const char* value)
{
    const size_t nameLength = strnlen(name, sizeof(PlaylistVarOverride::name));
    const size_t valueLength = strnlen(value, sizeof(PlaylistVarOverride::value));
    if (nameLength == sizeof(PlaylistVarOverride::name) || valueLength == sizeof(PlaylistVarOverride::value))
        return false;

    PlaylistVarOverride* entry = Find(name);
    if (!entry)
    {
        if (count == MAX_PLAYLIST_VAR_OVERRIDES)
            return false;
        entry = &entries[count++];
        memcpy(entry->name, name, nameLength + 1);
    }
    memcpy(entry->value, value, valueLength + 1);
    return true;
}

bool Playlist_ReadOverrideString(bf_read& buffer, long long endBit, char* destination, size_t capacity)
{
    if (endBit - buffer.GetNumBitsRead() < 8)
        return false;
    const unsigned int length = buffer.ReadUBitLong(8);
    if (length >= capacity || endBit - buffer.GetNumBitsRead() < length * 8)
        return false;
    if (!buffer.ReadBytes(destination, length))
        return false;
    destination[length] = '\0';
    return true;
}

bool PlaylistVarOverrides::Read(bf_read& buffer, int lengthBits)
{
    if (buffer.IsOverflowed() || lengthBits < 8 || lengthBits > buffer.GetNumBitsLeft())
        return false;

    const auto endBit = buffer.GetNumBitsRead() + lengthBits;
    PlaylistVarOverrides received;
    received.count = buffer.ReadUBitLong(8);
    for (int i = 0; i < received.count; ++i)
    {
        auto& entry = received.entries[i];
        if (!Playlist_ReadOverrideString(buffer, endBit, entry.name, sizeof(entry.name)) ||
            !Playlist_ReadOverrideString(buffer, endBit, entry.value, sizeof(entry.value)))
            return false;
    }
    if (buffer.IsOverflowed() || buffer.GetNumBitsRead() != endBit)
        return false;

    memcpy(entries, received.entries, received.count * sizeof(PlaylistVarOverride));
    count = received.count;
    return true;
}

void PlaylistVarOverrides::Write(bf_write& buffer) const
{
    buffer.WriteByte(count);
    for (int i = 0; i < count; ++i)
    {
        const int nameLength = static_cast<int>(strlen(entries[i].name));
        const int valueLength = static_cast<int>(strlen(entries[i].value));
        buffer.WriteByte(nameLength);
        buffer.WriteBytes(entries[i].name, nameLength);
        buffer.WriteByte(valueLength);
        buffer.WriteBytes(entries[i].value, valueLength);
    }
}

PlaylistVarOverrides& Playlist_GetOverrides()
{
    auto& client = CClientState::sm_ClientStateExtended;
    return client.m_bHasPlaylistVarOverrides ? client.m_PlaylistVarOverrides : g_ServerOverrides;
}

void Playlist_UpdateAimAssistRules()
{
    const char* value = R2::GetCurrentPlaylistVar("aimassist_sp_rules", true);
    *b_UseSingleplayerAimAssist = value && value[0] == '1';
}

// idk maybe not the best
CClientStateExtended* Playlist_GetRemoteClientState()
{
    if (IsDedicatedServer())
        return nullptr;

    auto* client = GetBaseLocalClient();
    if (client->m_nSignonState < eSignonState::CONNECTED || !client->m_NetChannel || client->m_NetChannel->m_ChannelInfo.remoteAddress.IsLoopback())
        return nullptr;

    return client->GetClientStateExtended();
}

DECLARE_MODULE(PlaylistHooks)

ConVar* Cvar_ns_use_clc_SetPlaylistVarOverride;

// clang-format off
DECLARE_HOOK(clc_SetPlaylistVarOverride::Process, engine.dll + 0x222180,
[](auto& hook, void* a1, void* a2) -> char
             // clang-format on
{
    // the private_match playlist on mp_lobby is the only situation where there should be any legitimate sending of this netmessage
    if (!Cvar_ns_use_clc_SetPlaylistVarOverride->GetBool() || strcmp(R2::GetCurrentPlaylistName(), "private_match") ||
        strcmp(g_pGlobals->m_pMapName, "mp_lobby"))
        return 1;

    return hook.Original(a1, a2);
})

// clang-format off
DECLARE_HOOK(SetCurrentPlaylist, engine.dll + 0x18EB20,
[](auto& hook, const char* pPlaylistName) -> bool
             // clang-format on
{
    bool bSuccess = hook.Original(pPlaylistName);

    if (bSuccess)
    {
        spdlog::info("Set playlist to {}", R2::GetCurrentPlaylistName());
        g_pServerPresence->SetPlaylist(R2::GetCurrentPlaylistName());
    }

    return bSuccess;
})

// clang-format off
DECLARE_HOOK(SetPlaylistVarOverride, engine.dll + 0x18ED00,
[](auto& hook, const char* pVarName, const char* pValue) -> bool
             // clang-format on
{
    if (strnlen(pVarName, sizeof(PlaylistVarOverride::name)) == sizeof(PlaylistVarOverride::name) ||
        strnlen(pValue, sizeof(PlaylistVarOverride::value)) == sizeof(PlaylistVarOverride::value))
        return false;

    if (Playlist_GetRemoteClientState())
        return hook.Original(pVarName, pValue);

    if (!g_ServerOverrides.Set(pVarName, pValue))
    {
        spdlog::warn("Cannot add playlist override '{}': maximum of {} entries reached", pVarName, MAX_PLAYLIST_VAR_OVERRIDES);
        return false;
    }
    *b_OverridesChanged = true;
    return true;
})

DECLARE_HOOK(Playlist_FindOverride, engine.dll + 0x18BD40,
             [](auto&, const char* name) -> PlaylistVarOverride* { return Playlist_GetOverrides().Find(name); })

DECLARE_HOOK(Playlist_GetOverride, engine.dll + 0x18D3A0, [](auto&, int index) -> PlaylistVarOverride*
{
    auto& overrides = Playlist_GetOverrides();
    return index >= 0 && index < overrides.count ? &overrides.entries[index] : nullptr;
})

DECLARE_HOOK(Playlist_GetOverrideCount, engine.dll + 0x18D3C0, [](auto&) -> int { return Playlist_GetOverrides().count; })

DECLARE_HOOK(Playlist_ClearOverrides, engine.dll + 0x18C0E0, [](auto&)
{
    if (g_ServerOverrides.count)
    {
        g_ServerOverrides.count = 0;
        *b_OverridesChanged = true;
    }
})

DECLARE_HOOK(Playlist_UpdateAimAssistRules, engine.dll + 0x18BBC0, [](auto&) { Playlist_UpdateAimAssistRules(); })

DECLARE_HOOK(Playlist_WriteOverridesToBuffer, engine.dll + 0x18EF50, [](auto&, SVC_PlaylistOverrides* message)
{
    constexpr size_t bufferBytes = (1 + MAX_PLAYLIST_VAR_OVERRIDES * sizeof(PlaylistVarOverride) + 3) & ~size_t(3);
    alignas(4) static thread_local unsigned char buffer[bufferBytes];
    message->m_DataOut.StartWriting(buffer, sizeof(buffer));
    g_ServerOverrides.Write(message->m_DataOut);
})

DECLARE_HOOK(CClientState::ProcessPlaylistOverrides, engine.dll + 0x1A1490,
             [](auto&, IServerMessageHandler* handler, SVC_PlaylistOverrides* message) -> bool
{
    auto* client = static_cast<CClientState*>(handler);
    if (client->m_NetChannel && client->m_NetChannel->m_ChannelInfo.remoteAddress.IsLoopback())
        return true;
    auto* extended = client->GetClientStateExtended();
    if (!extended->m_PlaylistVarOverrides.Read(message->m_DataIn, message->m_nLength))
        return false;
    extended->m_bHasPlaylistVarOverrides = true;
    *b_PendingOverrides = false;
    return true;
})

DECLARE_HOOK(CClientState::Disconnect_PlaylistOverrides, engine.dll + 0x8DC50, [](auto& hook, CClientState* client, bool sendTrackingContext)
{
    hook.Original(client, sendTrackingContext);
    client->GetClientStateExtended()->Reset();
    *b_PendingOverrides = false;
    Playlist_UpdateAimAssistRules();
})

// clang-format off
DECLARE_HOOK(GetCurrentPlaylistVar, engine.dll + 0x18C680,
[](auto& hook, const char* pVarName, bool bUseOverrides) -> const char*
             // clang-format on
{
    if (!bUseOverrides && !strcmp(pVarName, "max_players"))
        bUseOverrides = true;

    return hook.Original(pVarName, bUseOverrides);
})

// clang-format off
DECLARE_HOOK(GetCurrentGamemodeMaxPlayers, engine.dll + 0x18C430,
[](auto& hook) -> int
             // clang-format on
{
    const char* pMaxPlayers = R2::GetCurrentPlaylistVar("max_players", 0);
    if (!pMaxPlayers)
        return hook.Original();

    int iMaxPlayers = atoi(pMaxPlayers);
    return iMaxPlayers;
})

void ConCommand_playlist(const CCommand& args)
{
    if (args.ArgC() < 2)
        return;

    R2::SetCurrentPlaylist(args.Arg(1));
}

void ConCommand_setplaylistvaroverride(const CCommand& args)
{
    if (args.ArgC() < 3)
        return;

    auto* client = Playlist_GetRemoteClientState();
    for (int i = 1; i + 1 < args.ArgC(); i += 2)
    {
        if (client)
        {
            if (client->m_PlaylistVarOverrides.Set(args.Arg(i), args.Arg(i + 1)))
                client->m_bHasPlaylistVarOverrides = true;
        }
        else
        {
            R2::SetPlaylistVarOverride(args.Arg(i), args.Arg(i + 1));
        }
    }
}

ON_DLL_LOAD_RELIESON("engine.dll", PlaylistHooks, (ConCommand, ConVar, R2Engine), [](CModule module)
{
    R2::GetCurrentPlaylistName = module.Offset(0x18C640).RCast<decltype(R2::GetCurrentPlaylistName)>();
    R2::SetCurrentPlaylist = module.Offset(0x18EB20).RCast<decltype(R2::SetCurrentPlaylist)>();
    R2::SetPlaylistVarOverride = module.Offset(0x18ED00).RCast<decltype(R2::SetPlaylistVarOverride)>();
    R2::GetCurrentPlaylistVar = module.Offset(0x18C680).RCast<decltype(R2::GetCurrentPlaylistVar)>();
    b_OverridesChanged = module.Offset(0x1397FB51).RCast<bool*>();
    b_PendingOverrides = module.Offset(0x1397FB52).RCast<bool*>();
    b_UseSingleplayerAimAssist = module.Offset(0x139875AE).RCast<bool*>();

    DISPATCH_MODULE(PlaylistHooks)

    // playlist is the name of the command on respawn servers, but we already use setplaylist so can't get rid of it
    RegisterConCommand("playlist", ConCommand_playlist, "Sets the current playlist", FCVAR_NONE);
    RegisterConCommand("setplaylist", ConCommand_playlist, "Sets the current playlist", FCVAR_NONE);
    RegisterConCommand("setplaylistvaroverrides", ConCommand_setplaylistvaroverride, "sets a playlist var override", FCVAR_NONE);

    // note: clc_SetPlaylistVarOverride is pretty insecure, since it allows for entirely arbitrary playlist var overrides to be sent to the
    // server, this is somewhat restricted on custom servers to prevent it being done outside of private matches, but ideally it should be
    // disabled altogether, since the custom menus won't use it anyway this should only really be accepted if you want vanilla client
    // compatibility
    // sonny: This has been patched in vanilla for yonks and on Northstar I can't see what the issue is since Process is hooked to validate some stuff
    // anyway private matches are basically a no-mans-land for playlist var validation anyway.
    Cvar_ns_use_clc_SetPlaylistVarOverride =
        new ConVar("ns_use_clc_SetPlaylistVarOverride", "1", FCVAR_GAMEDLL, "Whether the server should accept clc_SetPlaylistVarOverride messages");
})
