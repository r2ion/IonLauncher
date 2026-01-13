#include "room.h"
#include "core/vanilla.h"

typedef void* (*JoinPlayerGameRoom_t)(const char* roomId);
JoinPlayerGameRoom_t JoinPlayerGameRoom;

char* room1;
char* room2;
char* room3;

AUTOHOOK_INIT()

// clang-format off
AUTOHOOK(JoinPlayerRoomHook, engine.dll + 0x187C70, __int64, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);

	return JoinPlayerRoomHook(a1);
}

void ConCommand_ns_join_room(const CCommand& args)
{
	// 187C70
	auto roomId = args.Arg(1);
	JoinPlayerGameRoom(roomId);
}

void ConCommand_ns_dump_room(const CCommand& args)
{
	NOTE_UNUSED(args);

	if (room1)
		spdlog::info("room related offset: {}", room1);

	if (room2)
		spdlog::info("community room: {}", room2);

	if (room3)
		spdlog::info("inviteRoom: {}", room3);

	const char* match_partySub = g_pCVar->FindVar("match_partySub")->GetString();

	if (match_partySub)
		spdlog::info("match_partySub: {}", match_partySub);
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", PartyRoom, ConCommand, (CModule module))
{
	JoinPlayerGameRoom = module.Offset(0x187C70).RCast<JoinPlayerGameRoom_t>();
	RegisterConCommand("ns_join_room", ConCommand_ns_join_room, "Join a server by its room ID (value of match_partySub)", FCVAR_CLIENTDLL);

	room1 = module.Offset(0x1314E538).RCast<char*>();
	room2 = module.Offset(0x13F8E2D0).RCast<char*>();
	room3 = module.Offset(0x1314B1CC).RCast<char*>();
	RegisterConCommand("ns_dump_room", ConCommand_ns_dump_room, "Dump room info", FCVAR_CLIENTDLL);
}
