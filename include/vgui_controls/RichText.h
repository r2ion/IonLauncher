#pragma once

#include "vgui_controls/Panel.h"

#include <cstddef>
#include <type_traits>

namespace vgui
{
class RichTextCommon : public Panel
{
public:
	virtual void SetText(const wchar_t* text) = 0;
	virtual void SetText(const char* text) = 0;
	virtual int GetText(int offset, char* text, int bufferSize) = 0;
	virtual int GetText(int offset, wchar_t* text, int bufferSize) = 0;
	virtual void SetFont(unsigned long font) = 0;
	virtual void InsertChar(wchar_t character) = 0;
	virtual void InsertString(const wchar_t* text) = 0;
	virtual void InsertString(const char* text) = 0;
	virtual void SelectNone() = 0;
	virtual void SelectAllText() = 0;
	virtual void SelectNoText() = 0;
	virtual void CutSelected() = 0;
	virtual void CopySelected() = 0;
	virtual void SetPanelInteractive(bool interactive) = 0;
	virtual void SetUnusedScrollbarInvisible(bool invisible) = 0;
	virtual void SetDrawTextOnly(bool drawTextOnly) = 0;
	virtual void GotoTextStart() = 0;
	virtual void GotoTextEnd() = 0;
	virtual void ScrollUp() = 0;
	virtual void ScrollDown() = 0;
	virtual void MoveScrollBar(int delta) = 0;
	virtual void SetVerticalScrollbar(bool state) = 0;
	virtual void SetMaximumCharCount(int maximumCharacters) = 0;
	virtual void InsertColorChange(Color color) = 0;
	virtual void InsertAlphaChange(int alpha) = 0;
	virtual void InsertClickableTextStart(const char* clickAction) = 0;
	virtual void InsertClickableTextEnd() = 0;
	virtual void InsertPossibleURLString(const char* text, Color urlTextColor, Color normalTextColor) = 0;
	virtual void InsertFade(float sustain, float length) = 0;
	virtual void ResetAllFades(bool hold, bool onlyExpired, float newSustain) = 0;
	virtual void SetToFullHeight() = 0;
	virtual int GetNumLines() = 0;

private:
	std::byte m_RichTextCommonData[0x118];
};

static_assert(std::is_base_of_v<Panel, RichTextCommon>);
static_assert(sizeof(RichTextCommon) == 0x380);

class RichText : public RichTextCommon
{
};

static_assert(std::is_base_of_v<RichTextCommon, RichText>);
static_assert(sizeof(RichText) == 0x380);
} // namespace vgui
