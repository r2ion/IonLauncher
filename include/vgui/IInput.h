#pragma once

#include "interface.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui.h"

namespace vgui
{
class Cursor;
using HCursor = unsigned long;

class IInput : public IBaseInterface
{
public:
	virtual ~IInput() = default;
	virtual void SetMouseFocus(VPANEL newMouseFocus) = 0;
	virtual void SetMouseCapture(VPANEL panel) = 0;
	virtual void GetKeyCodeText(KeyCode code, char* buffer, int bufferLength) = 0;
	virtual VPANEL GetFocus() = 0;
	virtual VPANEL GetCalculatedFocus() = 0;
	virtual VPANEL GetMouseOver() = 0;
	virtual void SetCursorPos(int x, int y) = 0;
	virtual void GetCursorPos(int& x, int& y) = 0;
	virtual bool WasMousePressed(MouseCode code) = 0;
	virtual bool WasMouseDoublePressed(MouseCode code) = 0;
	virtual bool IsMouseDown(MouseCode code) = 0;
	virtual void SetCursorOveride(HCursor cursor) = 0;
	virtual HCursor GetCursorOveride() = 0;
	virtual bool WasMouseReleased(MouseCode code) = 0;
	virtual bool WasKeyPressed(KeyCode code) = 0;
	virtual bool IsKeyDown(KeyCode code) = 0;
	virtual bool WasKeyTyped(KeyCode code) = 0;
	virtual bool WasKeyReleased(KeyCode code) = 0;
	virtual VPANEL GetAppModalSurface() = 0;
	virtual void SetAppModalSurface(VPANEL panel) = 0;
	virtual void ReleaseAppModalSurface() = 0;
	virtual void GetCursorPosition(int& x, int& y) = 0;
	virtual void SetIMEWindow(void* window) = 0;
	virtual void* GetIMEWindow() = 0;
	virtual void OnChangeIME(bool forward) = 0;
	virtual int GetCurrentIMEHandle() = 0;
	virtual int GetEnglishIMEHandle() = 0;
	virtual void GetIMELanguageName(wchar_t* buffer, int bufferSizeInBytes) = 0;
	virtual void GetIMELanguageShortCode(wchar_t* buffer, int bufferSizeInBytes) = 0;

	struct LanguageItem
	{
		wchar_t m_ShortName[4];
		wchar_t m_MenuName[128];
		int m_HandleValue;
		bool m_Active;
	};

	struct ConversionModeItem
	{
		wchar_t m_MenuName[128];
		int m_HandleValue;
		bool m_Active;
	};

	struct SentenceModeItem
	{
		wchar_t m_MenuName[128];
		int m_HandleValue;
		bool m_Active;
	};

	virtual int GetIMELanguageList(LanguageItem* destination, int destinationCount) = 0;
	virtual int GetIMEConversionModes(ConversionModeItem* destination, int destinationCount) = 0;
	virtual int GetIMESentenceModes(SentenceModeItem* destination, int destinationCount) = 0;
	virtual void OnChangeIMEByHandle(int handleValue) = 0;
	virtual void OnChangeIMEConversionModeByHandle(int handleValue) = 0;
	virtual void OnChangeIMESentenceModeByHandle(int handleValue) = 0;
	virtual void OnInputLanguageChanged() = 0;
	virtual void OnIMEStartComposition() = 0;
	virtual void OnIMEComposition(int flags) = 0;
	virtual void OnIMEEndComposition() = 0;
	virtual void OnIMEShowCandidates() = 0;
	virtual void OnIMEChangeCandidates() = 0;
	virtual void OnIMECloseCandidates() = 0;
	virtual void OnIMERecomputeModes() = 0;
	virtual int GetCandidateListCount() = 0;
	virtual void GetCandidate(int index, wchar_t* destination, int destinationSizeInBytes) = 0;
	virtual int GetCandidateListSelectedItem() = 0;
	virtual int GetCandidateListPageSize() = 0;
	virtual int GetCandidateListPageStart() = 0;
	virtual void SetCandidateWindowPos(int x, int y) = 0;
	virtual bool GetShouldInvertCompositionString() = 0;
	virtual bool CandidateListStartsAtOne() = 0;
	virtual void SetCandidateListPageStart(int start) = 0;
	virtual void SetMouseCaptureEx(VPANEL panel, MouseCode captureStartMouseCode) = 0;
	virtual void RegisterKeyCodeUnhandledListener(VPANEL panel) = 0;
	virtual void UnregisterKeyCodeUnhandledListener(VPANEL panel) = 0;
	virtual void OnKeyCodeUnhandled(int keyCode) = 0;
	virtual void SetModalSubTree(VPANEL subTree, VPANEL unhandledMouseClickListener,
		bool restrictMessagesToSubTree = true) = 0;
	virtual void ReleaseModalSubTree() = 0;
	virtual VPANEL GetModalSubTree() = 0;
	virtual void SetModalSubTreeReceiveMessages(bool state) = 0;
	virtual bool ShouldModalSubTreeReceiveMessages() const = 0;
	virtual VPANEL GetMouseCapture() = 0;
	virtual VPANEL GetMouseFocus() = 0;
	virtual void SetModalSubTreeShowMouse(bool state) = 0;
	virtual bool ShouldModalSubTreeShowMouse() const = 0;
};
}
