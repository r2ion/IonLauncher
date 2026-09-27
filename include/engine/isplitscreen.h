#pragma once

class CNetChan;

class ISplitScreen
{
public:
	virtual bool Init() = 0;
	virtual void Shutdown() = 0;
	virtual bool AddSplitScreenUser(int slot, int player) = 0;
	virtual bool AddBaseUser(int slot, int player) = 0;
	virtual bool RemoveSplitScreenUser(int slot, int player) = 0;
	virtual int GetActiveSplitScreenPlayerSlot() = 0;
	virtual int SetActiveSplitScreenPlayerSlot(int slot) = 0;
	virtual bool IsValidSplitScreenSlot(int slot) = 0;
	virtual int FirstValidSplitScreenSlot() = 0;
	virtual int NextValidSplitScreenSlot(int slot) = 0;
	virtual int GetNumSplitScreenPlayers() = 0;
	virtual int GetSplitScreenPlayerEntity(int slot) = 0;
	virtual CNetChan* GetSplitScreenPlayerNetChan(int slot) = 0;
	virtual bool IsDisconnecting(int slot) = 0;
	virtual void SetDisconnecting(int slot, bool disconnecting) = 0;
	virtual bool SetLocalPlayerIsResolvable(const char* context, int line, bool resolvable) = 0;
	virtual bool IsLocalPlayerResolvable() = 0;
};

extern ISplitScreen* g_pSplitScreenMgr;
