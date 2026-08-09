#pragma once

#include "interface.h"

#include <cstdint>

inline constexpr char RUNGAMEENGINE_INTERFACE_VERSION[] = "RunGameEngine005";

class IRunGameEngine : public IBaseInterface
{
public:
	virtual bool IsRunning() = 0;
	virtual bool AddTextCommand(const char* pText) = 0;
	virtual bool RunEngine(const char* pGameDirectory, const char* pCommandLineParameters) = 0;
	virtual bool IsInGame() = 0;
	virtual bool GetGameInfo(char* pInfoBuffer, int bufferSize) = 0;
	virtual void SetTrackerUserID(int trackerId, const char* pTrackerName) = 0;
	virtual int GetPlayerCount() = 0;
	virtual std::uint32_t GetPlayerFriendsID(int playerIndex) = 0;
	virtual const char* GetPlayerName(int friendsId, char* pName, int nameLength) = 0;
	virtual const char* GetPlayerFriendsName(int friendsId, char* pName, int nameLength) = 0;
};

static_assert(sizeof(IRunGameEngine) == sizeof(void*));
