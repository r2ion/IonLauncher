#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct ClientPartyStatePlayer
{
	std::uint64_t platformUserId; // 0x000; read from the response as an unsigned 64-bit value
	char name[256]; // 0x008
	char version4Text[64]; // 0x108; present in response protocol version 4 and later
	std::uint32_t version4Value0; // 0x148
	std::uint32_t version4Value1; // 0x14C
	std::uint32_t version4Value2; // 0x150
	std::uint32_t teamNumber; // 0x154; encoded as 8 bits
	std::uint32_t score; // 0x158
	std::uint32_t kills; // 0x15C; encoded as 16 bits
	std::uint32_t deaths; // 0x160; encoded as 16 bits
	std::uint8_t reserved0164[4]; // 0x164
};

static_assert(std::is_standard_layout_v<ClientPartyStatePlayer>);
static_assert(sizeof(ClientPartyStatePlayer) == 0x168);
static_assert(offsetof(ClientPartyStatePlayer, platformUserId) == 0x000);
static_assert(offsetof(ClientPartyStatePlayer, name) == 0x008);
static_assert(offsetof(ClientPartyStatePlayer, version4Text) == 0x108);
static_assert(offsetof(ClientPartyStatePlayer, version4Value0) == 0x148);
static_assert(offsetof(ClientPartyStatePlayer, version4Value1) == 0x14C);
static_assert(offsetof(ClientPartyStatePlayer, version4Value2) == 0x150);
static_assert(offsetof(ClientPartyStatePlayer, teamNumber) == 0x154);
static_assert(offsetof(ClientPartyStatePlayer, score) == 0x158);
static_assert(offsetof(ClientPartyStatePlayer, kills) == 0x15C);
static_assert(offsetof(ClientPartyStatePlayer, deaths) == 0x160);
static_assert(offsetof(ClientPartyStatePlayer, reserved0164) == 0x164);

struct ClientPartyState
{
	static constexpr std::size_t PLAYER_CAPACITY = 32;
	static constexpr std::size_t TEAM_CAPACITY = 32;

	char sourceAddress[128]; // 0x000
	std::uint32_t protocolVersion; // 0x080; encoded as 8 bits
	std::uint8_t version2Flag; // 0x084
	std::uint8_t reserved0085[3]; // 0x085
	std::uint32_t version2Value32; // 0x088
	std::uint32_t version2Value16; // 0x08C
	std::uint64_t version2Id; // 0x090
	char version2Text[128]; // 0x098
	char datacenter[64]; // 0x118
	char gamemode[32]; // 0x158
	char matchMetadata[32]; // 0x178
	char playlist[32]; // 0x198
	char map[32]; // 0x1B8
	std::uint16_t responseValue16; // 0x1D8
	std::uint8_t reserved01DA[2]; // 0x1DA
	std::uint32_t responseValue32; // 0x1DC
	std::uint32_t maxClients; // 0x1E0
	std::uint32_t numClients; // 0x1E4
	std::uint32_t version3Value8; // 0x1E8
	std::uint32_t maxRounds; // 0x1EC
	std::uint32_t roundsWonIMC; // 0x1F0
	std::uint32_t roundsWonMilitia; // 0x1F4
	std::uint32_t timeLimitSecs; // 0x1F8
	std::uint32_t elapsedTimeSecs; // 0x1FC
	std::uint32_t maxScore; // 0x200
	std::uint8_t reserved0204[4]; // 0x204
	ClientPartyStatePlayer clients[PLAYER_CAPACITY]; // 0x208
	std::uint32_t teamScores[TEAM_CAPACITY]; // 0x2F08
};

static_assert(std::is_standard_layout_v<ClientPartyState>);
static_assert(sizeof(ClientPartyState) == 0x2F88);
static_assert(offsetof(ClientPartyState, sourceAddress) == 0x000);
static_assert(offsetof(ClientPartyState, protocolVersion) == 0x080);
static_assert(offsetof(ClientPartyState, version2Flag) == 0x084);
static_assert(offsetof(ClientPartyState, reserved0085) == 0x085);
static_assert(offsetof(ClientPartyState, version2Value32) == 0x088);
static_assert(offsetof(ClientPartyState, version2Value16) == 0x08C);
static_assert(offsetof(ClientPartyState, version2Id) == 0x090);
static_assert(offsetof(ClientPartyState, version2Text) == 0x098);
static_assert(offsetof(ClientPartyState, datacenter) == 0x118);
static_assert(offsetof(ClientPartyState, gamemode) == 0x158);
static_assert(offsetof(ClientPartyState, matchMetadata) == 0x178);
static_assert(offsetof(ClientPartyState, playlist) == 0x198);
static_assert(offsetof(ClientPartyState, map) == 0x1B8);
static_assert(offsetof(ClientPartyState, responseValue16) == 0x1D8);
static_assert(offsetof(ClientPartyState, reserved01DA) == 0x1DA);
static_assert(offsetof(ClientPartyState, responseValue32) == 0x1DC);
static_assert(offsetof(ClientPartyState, maxClients) == 0x1E0);
static_assert(offsetof(ClientPartyState, numClients) == 0x1E4);
static_assert(offsetof(ClientPartyState, version3Value8) == 0x1E8);
static_assert(offsetof(ClientPartyState, maxRounds) == 0x1EC);
static_assert(offsetof(ClientPartyState, roundsWonIMC) == 0x1F0);
static_assert(offsetof(ClientPartyState, roundsWonMilitia) == 0x1F4);
static_assert(offsetof(ClientPartyState, timeLimitSecs) == 0x1F8);
static_assert(offsetof(ClientPartyState, elapsedTimeSecs) == 0x1FC);
static_assert(offsetof(ClientPartyState, maxScore) == 0x200);
static_assert(offsetof(ClientPartyState, reserved0204) == 0x204);
static_assert(offsetof(ClientPartyState, clients) == 0x208);
static_assert(offsetof(ClientPartyState, teamScores) == 0x2F08);
