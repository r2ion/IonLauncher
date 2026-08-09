#pragma once

#include <cstdint>

inline constexpr char VENGINE_GAME_UI_FUNCS_INTERFACE_VERSION[] = "VENGINE_GAMEUIFUNCS_VERSION005";

class IGameUIFuncs
{
public:
	virtual bool IsKeyDown(const char* pKeyName, bool& isDown) = 0;
	virtual const char* GetBindingForButtonCode(std::uint32_t buttonCode) = 0;
	virtual int GetButtonCodeForBind(const char* pBinding, std::uint32_t userId) = 0;
	virtual void GetDesktopResolution(int& width, int& height) = 0;
};

static_assert(sizeof(IGameUIFuncs) == sizeof(void*));
