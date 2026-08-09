#pragma once

#include "interface.h"

#include <cstdint>

inline constexpr char GAMECLIENTEXPORTS_INTERFACE_VERSION[] = "GameClientExports001";

class IGameClientExports : public IBaseInterface
{
public:
	virtual bool IsPlayerGameVoiceMuted(int playerIndex) = 0;
	virtual void MutePlayerGameVoice(int playerIndex) = 0;
	virtual void UnmutePlayerGameVoice(int playerIndex) = 0;
	virtual void OnGameUIActivated() = 0;
	virtual void OnGameUIHidden() = 0;
	virtual bool ClientWantsBlurEffect() = 0;
	virtual void NullSub07() = 0;
	virtual void NullSub08() = 0;
	virtual std::intptr_t ReturnZero09() = 0;
	virtual bool IsActiveViewportPanelVisible() = 0;
};

static_assert(sizeof(IGameClientExports) == sizeof(void*));
