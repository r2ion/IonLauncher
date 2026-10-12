#include "common/netmessages.h"
#include "tier0/callbacks.h"
#include "engine/client/clientstate.h"
#include "engine/net_chan.h"
#include "engine/server/server_limits.h"
#include "tier0/threadtools.h"
#include "tier0/vanilla.h"
#include "tier1/cvar.h"
#include "util/utils.h"

DECLARE_MODULE(NetMessagesHooks)

#include <cstdio>
#include <memory>

NET_SignonState::NET_SignonState()
	: m_nSignonState(eSignonState::NONE),
	  m_nSpawnCount(0),
	  m_szMapName{},
	  m_szGameMode{},
	  m_nPlaylistVersion(0),
	  m_bSendPlaylists(false),
	  m_szPlaylistName{}
{
	m_nGroup = 0;
	m_bReliable = true;
	m_NetChannel = nullptr;
	m_pMessageHandler = nullptr;
}

NET_SignonState::NET_SignonState(eSignonState state, std::int32_t spawnCount)
	: NET_SignonState()
{
	m_nSignonState = state;
	m_nSpawnCount = spawnCount;
}

bool NET_SignonState::Process()
{
	return m_pMessageHandler->ProcessSignonState(this);
}

bool NET_SignonState::ReadFromBuffer(bf_read* buffer)
{
	m_nSignonState = static_cast<eSignonState>(buffer->ReadByte());
	m_nSpawnCount = buffer->ReadLong();
	buffer->ReadString(m_szMapName, sizeof(m_szMapName));
	buffer->ReadString(m_szGameMode, sizeof(m_szGameMode));
	const unsigned int flags = buffer->ReadByte();
	m_bSendPlaylists = (flags & 1) != 0;
	m_nPlaylistVersion = buffer->ReadLong();
	buffer->ReadString(m_szPlaylistName, sizeof(m_szPlaylistName));
	if (buffer->IsOverflowed())
		return false;

	if (m_NetChannel && m_NetChannel->m_Socket == NS_CLIENT && GetBaseLocalClient)
	{
		if (CClientState* const client = GetBaseLocalClient())
			client->GetClientStateExtended()->SetIsExtendedServer((flags & 0x80) != 0);
	}
	return true;
}

bool NET_SignonState::WriteToBuffer(bf_write* buffer)
{
	buffer->WriteByte(static_cast<int>(m_nSignonState));
	buffer->WriteLong(m_nSpawnCount);
	buffer->WriteString(m_szMapName);
	buffer->WriteString(m_szGameMode);
	buffer->WriteByte(m_bSendPlaylists | (g_bSendingServerSignonCapability ? 0x80 : 0));
	buffer->WriteLong(m_nPlaylistVersion);
	buffer->WriteString(m_szPlaylistName);
	return !buffer->IsOverflowed();
}

const char* NET_SignonState::ToString() const
{
	static char text[1024];
	std::snprintf(text, sizeof(text), "%s: state %i, count %i", GetName(), static_cast<int>(m_nSignonState), m_nSpawnCount);
	return text;
}

using ClientTickProcess_t = bool (*)(CLC_ClientTick*);
using ClientTickRead_t = bool (*)(CLC_ClientTick*, bf_read*);
using ClientTickWrite_t = bool (*)(CLC_ClientTick*, bf_write*);
using ClientTickToString_t = const char* (*)(const CLC_ClientTick*);

ClientTickProcess_t CLC_ClientTick__Process;
ClientTickRead_t CLC_ClientTick__ReadFromBuffer;
ClientTickWrite_t CLC_ClientTick__WriteToBuffer;
ClientTickToString_t CLC_ClientTick__ToString;

CLC_ClientTick::CLC_ClientTick()
	: m_nDeltaTick(0), m_nStringTableTick(0), m_flFrameTime(0), m_flFrameTimeStdDeviation(0), m_nServerCPU(0)
{
	m_nGroup = 0;
	m_bReliable = false;
	m_NetChannel = nullptr;
	m_pMessageHandler = nullptr;
}

bool CLC_ClientTick::Process()
{
	return CLC_ClientTick__Process(this);
}

bool CLC_ClientTick::ReadFromBuffer(bf_read* buffer)
{
	return CLC_ClientTick__ReadFromBuffer(this, buffer);
}

bool CLC_ClientTick::WriteToBuffer(bf_write* buffer)
{
	return CLC_ClientTick__WriteToBuffer(this, buffer);
}

const char* CLC_ClientTick::ToString() const
{
	return CLC_ClientTick__ToString(this);
}

ON_DLL_LOAD_CLIENT("engine.dll", ClientTickMessageMethods, [](CModule module)
{
	CLC_ClientTick__Process = module.Offset(0x75CC0).RCast<ClientTickProcess_t>();
	CLC_ClientTick__ReadFromBuffer = module.Offset(0x220AF0).RCast<ClientTickRead_t>();
	CLC_ClientTick__WriteToBuffer = module.Offset(0x22AA80).RCast<ClientTickWrite_t>();
	CLC_ClientTick__ToString = module.Offset(0x229540).RCast<ClientTickToString_t>();
})
DECLARE_HOOK(NET_SignonState::ReadFromBuffer, engine.dll + 0x222630, [](auto& hook, NET_SignonState* message, bf_read* buffer) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(message, buffer);

	return message->NET_SignonState::ReadFromBuffer(buffer);
})

DECLARE_HOOK(NET_SignonState::WriteToBuffer, engine.dll + 0x22B0E0, [](auto& hook, NET_SignonState* message, bf_write* buffer) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(message, buffer);

	if (!g_bSendingClientSignonCapability)
		return message->NET_SignonState::WriteToBuffer(buffer);

	const bool previousFlag = message->m_bSendPlaylists;
	message->m_bIsExtendedClient = true;
	const ScopeGuard restoreFlag([message, previousFlag] { message->m_bSendPlaylists = previousFlag; });
	return message->NET_SignonState::WriteToBuffer(buffer);
})
// block bad netmessages
// Servers can literally request a screenshot from any client, yeah no
// clang-format off
DECLARE_HOOK(CLC_Screenshot::WriteToBuffer, engine.dll + 0x22AF20,
([](auto& hook, void* thisptr, void* buffer) -> bool // 48 89 5C 24 ? 57 48 83 EC 20 8B 42 10
// clang-format on
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(thisptr, buffer);
	return false;
}))

// clang-format off
DECLARE_HOOK(CLC_Screenshot::ReadFromBuffer, engine.dll + 0x221F00,
([](auto& hook, void* thisptr, void* buffer) -> bool // 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA 48 8B 52 38
// clang-format on
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(thisptr, buffer);
	return false;
}))

// This is unused ingame and a big client=>server=>client exploit vector
// clang-format off
DECLARE_HOOK(Base_CmdKeyValues::ReadFromBuffer, engine.dll + 0x220040,
([](auto& hook, void* thisptr, void* buffer) -> bool // 40 55 48 81 EC ? ? ? ? 48 8D 6C 24 ? 48 89 5D 70
// clang-format on
{
	NOTE_UNUSED(hook);
	NOTE_UNUSED(thisptr);
	NOTE_UNUSED(buffer);
	return false;
}))
// clang-format off
DECLARE_HOOK(NET_SetConVar::Process, engine.dll + 0x75CF0,
([](auto& hook, NET_SetConVar* msg) -> bool // 48 8B D1 48 8B 49 18 48 8B 01 48 FF 60 10
// clang-format on
{

	constexpr int ENTRY_STR_LEN = MAX_OSPATH;
	bool bIsServerFrame = ThreadInServerFrameThread();

	std::string BLOCK_PREFIX =
		std::string {"NET_SetConVar ("} + (bIsServerFrame ? "server" : "client") + "): Blocked dangerous/invalid msg: ";

	if (bIsServerFrame)
	{
		constexpr int SETCONVAR_SANITY_AMOUNT_LIMIT = 69;
		if (msg->m_ConVars.Count() < 1 || msg->m_ConVars.Count() > SETCONVAR_SANITY_AMOUNT_LIMIT)
		{
			return BLOCKED_INFO("Invalid m_ConVars_count (" << msg->m_ConVars.Count() << ")");
		}
	}

	for (int i = 0; i < msg->m_ConVars.Count(); i++)
	{
		auto entry = msg->m_ConVars.Base() + i;

		// Safety check for memory access
		if (CMemory(entry).IsMemoryReadable(sizeof(*entry)))
		{
			// Find null terminators
			bool nameValid = false, valValid = false;
			for (int i = 0; i < ENTRY_STR_LEN; i++)
			{
				if (!entry->name[i])
					nameValid = true;
				if (!entry->value[i])
					valValid = true;
			}

			if (!nameValid || !valValid)
				return BLOCKED_INFO("Missing null terminators");

			// we only need to check if these cvars are valid on client as it will set actual cvars there
			// on server this won't set any actual convars, only keyvalues in the player, which doesn't have really any potential for dumb
			// stuff
			if (!bIsServerFrame)
			{
				ConVar* pVar = g_pCVar->FindVar(entry->name);
				if (pVar)
				{
					const char* canonicalName = pVar->GetName();
					memcpy(entry->name, canonicalName, strlen(canonicalName) + 1); // Force name to match case

					if (!pVar->IsFlagSet(FCVAR_REPLICATED))
					{
						spdlog::warn(
							"Blocking replication of remote cvar {} from server (server's var has flag REPLICATED, while ours does not)",
							entry->name);

						// don't block, as non-malicious servers might send bad cvars, and we still want those clients to be able to
						// connect
						memset(entry->name, 0, ENTRY_STR_LEN);
						memset(entry->value, 0, ENTRY_STR_LEN);
					}
				}
			}
		}
		else
		{
			return BLOCKED_INFO("Unreadable memory at " << (void*)entry); // Not risking that one, they all gotta be readable
		}
	}

	return hook.Original(msg);
}))

using NET_BufferToBufferCompress_t = bool(__fastcall*)(void* destination, std::size_t* destinationSize, const void* source, std::size_t sourceSize);
static NET_BufferToBufferCompress_t NET_BufferToBufferCompress = nullptr;

DECLARE_HOOK(SVC_Playlists::WriteToBuffer, engine.dll + 0x22BBD0, [](auto& hook, SVC_Playlists* message, bf_write* buffer) -> bool
{
    static std::unique_ptr<unsigned char[]> compressedBuffer;
    static std::size_t compressedBufferCapacity = 0;
    static ConVar* const svCompressPlaylists = g_pCVar->FindVar("sv_compressPlaylists");

    if (message->m_compressed || !message->m_buffer || message->m_bufferSize <= 0 || !NET_BufferToBufferCompress || !svCompressPlaylists ||
        !svCompressPlaylists->GetBool())
    {
        return hook.Original(message, buffer);
    }

    const std::size_t sourceSize = static_cast<std::size_t>(message->m_bufferSize);
    if (compressedBufferCapacity < sourceSize)
    {
        compressedBuffer = std::make_unique_for_overwrite<unsigned char[]>(sourceSize);
        compressedBufferCapacity = sourceSize;
    }

    std::size_t compressedSize = sourceSize;
    if (!NET_BufferToBufferCompress(compressedBuffer.get(), &compressedSize, message->m_buffer, sourceSize) || compressedSize == 0 ||
        compressedSize >= sourceSize)
    {
        return hook.Original(message, buffer);
    }

    const int compressedBufferSize = static_cast<int>(compressedSize);
    buffer->WriteOneBit(true);
    buffer->WriteLong(compressedBufferSize);
    buffer->WriteLong(message->m_bufferSize);
    buffer->WriteBytes(compressedBuffer.get(), compressedBufferSize);
    if (buffer->IsOverflowed())
        return false;

    spdlog::info("  SVC_Playlists compressed for send: {} -> {} bytes", message->m_bufferSize, compressedBufferSize);
    return true;
})

ON_DLL_LOAD("engine.dll", NetMessages, [](CModule)
{
    DISPATCH_HOOK(NetMessagesHooks, NET_SignonState::ReadFromBuffer)
    DISPATCH_HOOK(NetMessagesHooks, NET_SignonState::WriteToBuffer)
})

ON_DLL_LOAD_RELIESON("engine.dll", NetworkMessageSecurity, ServerLimits, [](CModule)
{
    DISPATCH_HOOK(NetMessagesHooks, CLC_Screenshot::WriteToBuffer)
    DISPATCH_HOOK(NetMessagesHooks, CLC_Screenshot::ReadFromBuffer)
    DISPATCH_HOOK(NetMessagesHooks, Base_CmdKeyValues::ReadFromBuffer)
    DISPATCH_HOOK(NetMessagesHooks, NET_SetConVar::Process)
})

ON_DLL_LOAD_RELIESON("engine.dll", PlaylistMessageCompression, ConCommand, [](CModule module)
{
    NET_BufferToBufferCompress = module.Offset(0x218F50).RCast<decltype(NET_BufferToBufferCompress)>();
    DISPATCH_HOOK(NetMessagesHooks, SVC_Playlists::WriteToBuffer)
})
