#pragma once

#include "vgui_controls/Frame.h"
#include "mathlib/color.h"

#include <cstddef>
#include <type_traits>

class IConsoleDisplayFunc
{
public:
	virtual void ColorPrint(const Color& color, const char* message) = 0;
	virtual void Print(const char* message) = 0;
	virtual void DPrint(const char* message) = 0;
	virtual void GetConsoleText(char* text, int bufferSize) const = 0;
};

static_assert(sizeof(IConsoleDisplayFunc) == sizeof(void*));

namespace vgui
{
class CConsolePanel : public EditablePanel, public ::IConsoleDisplayFunc
{
public:
	virtual void ConsolePanelSlot255() = 0;
	virtual void ConsolePanelSlot256() = 0;
	virtual void ConsolePanelSlot257() = 0;

private:
	std::byte m_ConsolePanelData[0x280];
};

static_assert(std::is_base_of_v<EditablePanel, CConsolePanel>);
static_assert(std::is_base_of_v<IConsoleDisplayFunc, CConsolePanel>);
static_assert(sizeof(CConsolePanel) == 0x540);

class CConsoleDialog : public Frame
{
public:
	virtual void OnCommandSubmitted(const char* command) = 0;

protected:
	CConsolePanel* m_pConsolePanel;
};

static_assert(std::is_base_of_v<Frame, CConsoleDialog>);
static_assert(sizeof(CConsoleDialog) == 0x3A8);
} // namespace vgui
