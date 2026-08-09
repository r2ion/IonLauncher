#pragma once

#include <cstddef>
#include <cstdint>

struct player_info_t
{
	char m_Name[256];                  // 0x000
	char m_ClanTag[16];                // 0x100
	char m_CommunityName[64];          // 0x110
	std::uint32_t m_CommunityUserId;   // 0x150
	bool m_FakePlayer;                 // 0x154
	std::byte m_Reserved0155[3];       // 0x155
};

static_assert(sizeof(player_info_t) == 0x158);
static_assert(alignof(player_info_t) == alignof(std::uint32_t));
static_assert(offsetof(player_info_t, m_Name) == 0x000);
static_assert(offsetof(player_info_t, m_ClanTag) == 0x100);
static_assert(offsetof(player_info_t, m_CommunityName) == 0x110);
static_assert(offsetof(player_info_t, m_CommunityUserId) == 0x150);
static_assert(offsetof(player_info_t, m_FakePlayer) == 0x154);
static_assert(offsetof(player_info_t, m_Reserved0155) == 0x155);
