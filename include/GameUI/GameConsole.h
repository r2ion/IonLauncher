#pragma once

#include "GameUI/IGameConsole.h"
#include "vgui_controls/consoledialog.h"

#include <cstddef>
#include <type_traits>

class CGameConsoleDialog : public vgui::CConsoleDialog
{
public:
	void OnCommandSubmitted(const char* command) override = 0;
	virtual void GameConsoleDialogSlot299() = 0;

	std::byte m_GameConsoleDialogData3A8[5];
	bool m_bUnknown3AD;
	std::byte m_GameConsoleDialogData3AE[2];
};

static_assert(std::is_base_of_v<vgui::CConsoleDialog, CGameConsoleDialog>);
static_assert(offsetof(CGameConsoleDialog, m_bUnknown3AD) == 0x3AD);
static_assert(sizeof(CGameConsoleDialog) == 0x3B0);

class CGameConsole : public IGameConsole
{
public:
	bool m_bInitialized;
	CGameConsoleDialog* m_pConsole;
};

static_assert(std::is_base_of_v<IGameConsole, CGameConsole>);
static_assert(offsetof(CGameConsole, m_bInitialized) == 0x8);
static_assert(offsetof(CGameConsole, m_pConsole) == 0x10);
static_assert(sizeof(CGameConsole) == 0x18);
