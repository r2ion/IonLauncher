#pragma once
#include "interface.h"

#include <cstddef>

struct AppSystemInfo_t
{
	const char* m_pModuleName;
	const char* m_pInterfaceName;
};

enum InitReturnVal_t
{
	INIT_FAILED = 0,
	INIT_OK,

	INIT_LAST_VAL,
};
enum AppSystemTier_t
{
	APP_SYSTEM_TIER0 = 0,
	APP_SYSTEM_TIER1,
	APP_SYSTEM_TIER2,
	APP_SYSTEM_TIER3,

	APP_SYSTEM_TIER_OTHER,
};

class IAppSystem
{
public:
	// Here's where the app systems get to learn about each other
	virtual bool Connect(const CreateInterfaceFn factory) = 0;
	virtual void Disconnect() = 0;

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void* QueryInterface(const char* const pInterfaceName) = 0;

	// Init, shutdown
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;

	// Returns all dependent libraries
	virtual const AppSystemInfo_t* GetDependencies() { return NULL; }
	virtual AppSystemTier_t GetTier() = 0;

	// Reconnect to a particular interface
	virtual void Reconnect(const CreateInterfaceFn factory, const char* const pInterfaceName) = 0;
};

template<class IInterface>
class CBaseAppSystem : public IInterface
{
public:
	virtual bool Connect(const CreateInterfaceFn factory) { return true; }
	virtual void Disconnect() {}
	virtual void* QueryInterface(const char* const pInterfaceName) { return NULL; }
	virtual InitReturnVal_t Init() { return INIT_OK; }
	virtual void Shutdown() {}
	virtual const AppSystemInfo_t* GetDependencies() { return NULL; }
	virtual AppSystemTier_t GetTier() { return APP_SYSTEM_TIER_OTHER; }
	virtual void Reconnect(const CreateInterfaceFn factory, const char* const pInterfaceName) {}
};

template<class IInterface>
class CTier0AppSystem : public CBaseAppSystem<IInterface>
{
public:
	AppSystemTier_t GetTier() override { return APP_SYSTEM_TIER0; }
};

static_assert(sizeof(AppSystemInfo_t) == 0x10);
static_assert(offsetof(AppSystemInfo_t, m_pModuleName) == 0x0);
static_assert(offsetof(AppSystemInfo_t, m_pInterfaceName) == 0x8);
static_assert(sizeof(IAppSystem) == sizeof(void*));
