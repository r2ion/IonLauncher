//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "interface.h"
#include "tier1/utlvector.h"
#include "vgui.h"

#include <cstdint>

#ifdef SendMessage
#undef SendMessage
#endif

class KeyValues;
struct DmxElementUnpackStructure_t;
class CDmxElement;

namespace vgui
{
class IClientPanel;
class Panel;
class SurfacePlat;

class IPanel : public IBaseInterface
{
public:
	virtual ~IPanel() = default;
	virtual void Init(VPANEL panel, IClientPanel* clientPanel) = 0;
	virtual void SetPos(VPANEL panel, int x, int y) = 0;
	virtual void GetPos(VPANEL panel, int& x, int& y) = 0;
	virtual void SetSize(VPANEL panel, int wide, int tall) = 0;
	virtual void GetSize(VPANEL panel, int& wide, int& tall) = 0;
	virtual int GetWide(VPANEL panel) = 0;
	virtual int GetTall(VPANEL panel) = 0;
	virtual void SetMinimumSize(VPANEL panel, int wide, int tall) = 0;
	virtual void GetMinimumSize(VPANEL panel, int& wide, int& tall) = 0;
	virtual void SetZPos(VPANEL panel, int z) = 0;
	virtual int GetZPos(VPANEL panel) = 0;
	virtual void GetAbsPos(VPANEL panel, int& x, int& y) = 0;
	virtual void GetClipRect(VPANEL panel, int& x0, int& y0, int& x1, int& y1) = 0;
	virtual void Unknown14(VPANEL panel, int& x0, int& y0, int& x1, int& y1) = 0;
	virtual void SetInset(VPANEL panel, int left, int top, int right, int bottom) = 0;
	virtual void GetInset(VPANEL panel, int& left, int& top, int& right, int& bottom) = 0;
	virtual void SetVisible(VPANEL panel, bool state) = 0;
	virtual bool IsVisible(VPANEL panel) = 0;
	virtual void SetParent(VPANEL panel, VPANEL newParent) = 0;
	virtual int GetChildCount(VPANEL panel) = 0;
	virtual VPANEL GetChild(VPANEL panel, int index) = 0;
	virtual CUtlVector<VPANEL>& GetChildren(VPANEL panel) = 0;
	virtual std::uintptr_t Unknown23(VPANEL panel) = 0;
	virtual std::uintptr_t Unknown24(VPANEL panel) = 0;
	virtual std::uintptr_t Unknown25(VPANEL panel) = 0;
	virtual VPANEL GetParent(VPANEL panel) = 0;
	virtual void MoveToFront(VPANEL panel) = 0;
	virtual void MoveToBack(VPANEL panel) = 0;
	virtual bool HasParent(VPANEL panel, VPANEL potentialParent) = 0;
	virtual bool IsPopup(VPANEL panel) = 0;
	virtual void SetPopup(VPANEL panel, bool state) = 0;
	virtual bool IsFullyVisible(VPANEL panel) = 0;
	virtual HScheme GetScheme(VPANEL panel) = 0;
	virtual bool IsProportional(VPANEL panel) = 0;
	virtual bool IsAutoDeleteSet(VPANEL panel) = 0;
	virtual void DeletePanel(VPANEL panel) = 0;
	virtual void SetKeyBoardInputEnabled(VPANEL panel, bool state) = 0;
	virtual void SetMouseInputEnabled(VPANEL panel, bool state) = 0;
	virtual bool IsKeyBoardInputEnabled(VPANEL panel) = 0;
	virtual bool IsMouseInputEnabled(VPANEL panel) = 0;
	virtual void Solve(VPANEL panel) = 0;
	virtual const char* GetName(VPANEL panel) = 0;
	virtual const char* GetClassName(VPANEL panel) = 0;
	virtual void SendMessage(VPANEL panel, KeyValues* params, VPANEL fromPanel) = 0;
	virtual void Think(VPANEL panel) = 0;
	virtual void PaintTraverse(VPANEL panel, bool forceRepaint, bool allowForce = true) = 0;
	virtual void Repaint(VPANEL panel) = 0;
	virtual VPANEL IsWithinTraverse(VPANEL panel, int x, int y, bool traversePopups) = 0;
	virtual void OnChildAdded(VPANEL panel, VPANEL child) = 0;
	virtual void OnSizeChanged(VPANEL panel, int newWide, int newTall) = 0;
	virtual void InternalFocusChanged(VPANEL panel, bool lost) = 0;
	virtual bool RequestInfo(VPANEL panel, KeyValues* outputData) = 0;
	virtual void RequestFocus(VPANEL panel, int direction = 0) = 0;
	virtual bool RequestFocusPrev(VPANEL panel, VPANEL existingPanel) = 0;
	virtual bool RequestFocusNext(VPANEL panel, VPANEL existingPanel) = 0;
	virtual VPANEL GetCurrentKeyFocus(VPANEL panel) = 0;
	virtual int GetTabPosition(VPANEL panel) = 0;
	virtual SurfacePlat* Plat(VPANEL panel) = 0;
	virtual void SetPlat(VPANEL panel, SurfacePlat* platform) = 0;
	virtual Panel* GetPanel(VPANEL panel, const char* destinationModule) = 0;
	virtual bool IsEnabled(VPANEL panel) = 0;
	virtual void SetEnabled(VPANEL panel, bool state) = 0;
	virtual bool IsTopmostPopup(VPANEL panel) = 0;
	virtual void SetTopmostPopup(VPANEL panel, bool state) = 0;
	virtual void SetMessageContextId(VPANEL panel, int contextId) = 0;
	virtual int GetMessageContextId(VPANEL panel) = 0;
	virtual const DmxElementUnpackStructure_t* GetUnpackStructure(VPANEL panel) const = 0;
	virtual void OnUnserialized(VPANEL panel, CDmxElement* element) = 0;
	virtual void SetSiblingPin(VPANEL panel, VPANEL sibling, std::uint8_t panelCorner = 0,
		std::uint8_t siblingCorner = 0) = 0;
};

inline constexpr char VGUI_PANEL_INTERFACE_VERSION[] = "VGUI_Panel009";
} // namespace vgui
