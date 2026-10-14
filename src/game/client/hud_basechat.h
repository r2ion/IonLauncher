#pragma once

#include "mathlib/color.h"
#include "vgui_controls/RichText.h"

#include <cstddef>

class CBaseHudChat
{
public:
	static CBaseHudChat** allHuds;
	bool ProcessMessage(const char* message, int inboxId, bool isTeam, bool isDead);

	char unknown1[720];

	Color m_sameTeamColor;
	Color m_enemyTeamColor;
	Color m_mainTextColor;
	Color m_networkNameColor;

	char unknown2[12];

	int m_unknownContext;

	char unknown3[8];

	vgui::RichText* m_richText;

	CBaseHudChat* next;
	CBaseHudChat* previous;
};

static_assert(offsetof(CBaseHudChat, m_sameTeamColor) == 0x2D0);
static_assert(offsetof(CBaseHudChat, m_unknownContext) == 0x2EC);
static_assert(offsetof(CBaseHudChat, m_richText) == 0x2F8);
static_assert(offsetof(CBaseHudChat, next) == 0x300);
static_assert(sizeof(CBaseHudChat) == 0x310);

