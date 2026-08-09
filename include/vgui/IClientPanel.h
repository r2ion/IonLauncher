#pragma once

#include <cstdint>

namespace vgui
{
class Panel;

class IClientPanel
{
public:
	virtual std::uintptr_t GetVPanel() = 0;
	virtual std::uintptr_t Think() = 0;
	virtual std::uintptr_t PerformApplySchemeSettings() = 0;
	virtual std::uintptr_t PaintTraverse() = 0;
	virtual std::uintptr_t Repaint() = 0;
	virtual std::uintptr_t IsWithinTraverse() = 0;
	virtual std::uintptr_t GetInset() = 0;
	virtual std::uintptr_t GetClipRect() = 0;
	virtual std::uintptr_t OnChildAdded() = 0;
	virtual std::uintptr_t OnSizeChanged() = 0;
	virtual std::uintptr_t InternalFocusChanged() = 0;
	virtual std::uintptr_t RequestInfo() = 0;
	virtual std::uintptr_t RequestFocus() = 0;
	virtual std::uintptr_t RequestFocusPrev() = 0;
	virtual std::uintptr_t RequestFocusNext() = 0;
	virtual std::uintptr_t Unknown15() = 0;
	virtual std::uintptr_t Unknown16() = 0;
	virtual std::uintptr_t OnMessage() = 0;
	virtual std::uintptr_t GetCurrentKeyFocus() = 0;
	virtual std::uintptr_t GetTabPosition() = 0;
	virtual std::uintptr_t Unknown20() = 0;
	virtual const char* GetName() = 0;
	virtual const char* GetClassName() = 0;
	virtual std::uintptr_t GetScheme() = 0;
	virtual std::uintptr_t IsProportional() = 0;
	virtual bool IsAutoDeleteSet() = 0;
	virtual void DeletePanel() = 0;
	virtual void* QueryInterface(std::uintptr_t interfaceId) = 0;
	virtual Panel* GetPanel() = 0;
	virtual const char* GetModuleName() = 0;
	virtual void OnTick() = 0;
};

static_assert(sizeof(IClientPanel) == sizeof(void*));
} // namespace vgui
