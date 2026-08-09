#pragma once

#include <cstddef>
#include <cstdint>

struct client_textmessage_t
{
	std::int32_t m_Effect;
	std::uint8_t m_PrimaryColor[4];
	std::uint8_t m_SecondaryColor[4];
	float m_X;
	float m_Y;
	float m_FadeInTime;
	float m_FadeOutTime;
	float m_HoldTime;
	float m_EffectTime;
	std::byte m_Reserved24[0xC];
	const char* m_pMessage;
	const char* m_pName;
	bool m_bRoundedRectBackdrop;
	std::uint8_t m_Padding41[3];
	float m_BoxSize;
	std::uint8_t m_BoxColor[4];
	std::uint32_t m_Padding4C;
	const char* m_pClearMessage;
};

static_assert(sizeof(client_textmessage_t) == 0x58);
static_assert(offsetof(client_textmessage_t, m_X) == 0xC);
static_assert(offsetof(client_textmessage_t, m_FadeInTime) == 0x14);
static_assert(offsetof(client_textmessage_t, m_pMessage) == 0x30);
static_assert(offsetof(client_textmessage_t, m_pName) == 0x38);
static_assert(offsetof(client_textmessage_t, m_bRoundedRectBackdrop) == 0x40);
static_assert(offsetof(client_textmessage_t, m_BoxSize) == 0x44);
static_assert(offsetof(client_textmessage_t, m_BoxColor) == 0x48);
static_assert(offsetof(client_textmessage_t, m_pClearMessage) == 0x50);
