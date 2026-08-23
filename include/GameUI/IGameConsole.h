#pragma once

#include "interface.h"

#include <cstdint>

namespace vgui
{
using VPANEL = std::uintptr_t;
}

inline constexpr char GAMECONSOLE_INTERFACE_VERSION[] = "GameConsole004";

class IGameConsole : public IBaseInterface
{
public:
	virtual ~IGameConsole() = default;
	virtual void Activate() = 0;
	virtual void Initialize() = 0;
	virtual void Hide() = 0;
	virtual void Clear() = 0;
	virtual bool IsConsoleVisible() = 0;
	virtual void SetParent(vgui::VPANEL parent) = 0;
};

static_assert(sizeof(IGameConsole) == sizeof(void*));
