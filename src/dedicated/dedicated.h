#pragma once
#include "appframework/IAppSystem.h"

class CDedicatedExports : public IAppSystem
{
public:
	bool Connect(CreateInterfaceFn factory) override; // 0
	void Disconnect() override; // 1
	void* QueryInterface(const char* interfaceName) override; // 2
	InitReturnVal_t Init() override; // 3
	void Shutdown() override; // 4
	const AppSystemInfo_t* GetDependencies() override; // 5
	AppSystemTier_t GetTier() override; // 6
	void Reconnect(CreateInterfaceFn factory, const char* interfaceName) override; // 7
	virtual void Sys_Printf(const char* msg); // 8
	virtual void RunServer(); // 9
};

static_assert(sizeof(CDedicatedExports) == 8);

bool IsDedicatedServer();
