#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "tier0/module.h"

#ifndef __CONCAT2
#define __CONCAT2(x, y) x##y
#endif
#ifndef CONCAT2
#define CONCAT2(x, y) __CONCAT2(x, y)
#endif
#ifndef __STR
#define __STR(s) #s
#endif

typedef void (*DllLoadCallbackFuncType)(CModule moduleAddress);

void AddDllLoadCallback(std::string dll, DllLoadCallbackFuncType callback, std::string tag = "", std::vector<std::string> reliesOn = {});
void AddDllLoadCallbackForDedicatedServer(
	std::string dll,
	DllLoadCallbackFuncType callback,
	std::string tag = "",
	std::vector<std::string> reliesOn = {});
void AddDllLoadCallbackForClient(
	std::string dll,
	DllLoadCallbackFuncType callback,
	std::string tag = "",
	std::vector<std::string> reliesOn = {});

enum class eDllLoadCallbackSide
{
	UNSIDED,
	CLIENT,
	DEDICATED_SERVER
};

class __dllLoadCallback
{
  public:
	__dllLoadCallback() = delete;
	__dllLoadCallback(
		eDllLoadCallbackSide side,
		const std::string dllName,
		DllLoadCallbackFuncType callback,
		std::string uniqueStr,
		std::string reliesOn);
};

#define __ON_DLL_LOAD(dllName, side, uniquestr, reliesOn, lambdaExpr)                                                                                \
	namespace                                                                                                                                         \
	{                                                                                                                                                 \
	inline auto CONCAT2(__dllLoadCallbackLambda_, uniquestr) = lambdaExpr;                                                                            \
	void CONCAT2(__dllLoadCallback, uniquestr)(CModule module)                                                                                        \
	{                                                                                                                                                 \
		CONCAT2(__dllLoadCallbackLambda_, uniquestr)(module);                                                                                           \
	}                                                                                                                                                 \
	__dllLoadCallback CONCAT2(__dllLoadCallbackInstance, __LINE__)(side, dllName, CONCAT2(__dllLoadCallback, uniquestr), __STR(uniquestr),            \
																																						 reliesOn);                       \
	}

#define ON_DLL_LOAD(dllName, uniquestr, lambdaExpr) __ON_DLL_LOAD(dllName, eDllLoadCallbackSide::UNSIDED, uniquestr, "", lambdaExpr)
#define ON_DLL_LOAD_RELIESON(dllName, uniquestr, reliesOn, lambdaExpr)                                                                               \
	__ON_DLL_LOAD(dllName, eDllLoadCallbackSide::UNSIDED, uniquestr, __STR(reliesOn), lambdaExpr)
#define ON_DLL_LOAD_CLIENT(dllName, uniquestr, lambdaExpr) __ON_DLL_LOAD(dllName, eDllLoadCallbackSide::CLIENT, uniquestr, "", lambdaExpr)
#define ON_DLL_LOAD_CLIENT_RELIESON(dllName, uniquestr, reliesOn, lambdaExpr)                                                                        \
	__ON_DLL_LOAD(dllName, eDllLoadCallbackSide::CLIENT, uniquestr, __STR(reliesOn), lambdaExpr)
#define ON_DLL_LOAD_DEDI(dllName, uniquestr, lambdaExpr) __ON_DLL_LOAD(dllName, eDllLoadCallbackSide::DEDICATED_SERVER, uniquestr, "", lambdaExpr)
#define ON_DLL_LOAD_DEDI_RELIESON(dllName, uniquestr, reliesOn, lambdaExpr)                                                                          \
	__ON_DLL_LOAD(dllName, eDllLoadCallbackSide::DEDICATED_SERVER, uniquestr, __STR(reliesOn), lambdaExpr)

class CallbackManager
{
  public:
	CallbackManager() = default;

	void Initialize();
	void Shutdown();
	void ProcessLoadedModules();
	void QueueModuleForCallbacks(HMODULE hModule);
	void DrainPendingModuleCallbacks();

	void AddDllCallback(std::string dll, DllLoadCallbackFuncType callback, std::string tag, std::vector<std::string> reliesOn);

  private:
	void RunModuleCallbacks(HMODULE hModule);
	void CallDllLoadCallbacks(const char* moduleName, HMODULE moduleHandle);
};

extern CallbackManager* g_pCallbackManager;
