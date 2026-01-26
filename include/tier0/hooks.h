#pragma once

#include <windows.h>

#include <cctype>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <MinHook.h>
#include <intrin.h>

#include "logging/logging.h"
#include "tier0/module.h"

#ifndef __CONCAT2
#define __CONCAT2(x, y) x##y
#endif
#ifndef CONCAT2
#define CONCAT2(x, y) __CONCAT2(x, y)
#endif
#ifndef __CONCAT3
#define __CONCAT3(x, y, z) x##y##z
#endif
#ifndef CONCAT3
#define CONCAT3(x, y, z) __CONCAT3(x, y, z)
#endif
#ifndef __STR
#define __STR(s) #s
#endif

// Forward declarations from core hooks system
uintptr_t ParseDLLOffsetString(const char* pAddrString);

#ifndef NS_DLL_LOAD_CALLBACK_TYPE_DEFINED
#define NS_DLL_LOAD_CALLBACK_TYPE_DEFINED
typedef void (*DllLoadCallbackFuncType)(CModule moduleAddress);
void AddDllLoadCallback(std::string dll, DllLoadCallbackFuncType callback, std::string tag = "", std::vector<std::string> reliesOn = {});
void AddDllLoadCallbackForDedicatedServer(
	std::string dll, DllLoadCallbackFuncType callback, std::string tag = "", std::vector<std::string> reliesOn = {});
void AddDllLoadCallbackForClient(
	std::string dll, DllLoadCallbackFuncType callback, std::string tag = "", std::vector<std::string> reliesOn = {});
#endif

//-----------------------------------------------------------------------------
// Purpose: Init minhook
//-----------------------------------------------------------------------------
void HookSys_Init();

//-----------------------------------------------------------------------------
// Purpose: MH_MakeHook wrapper
// Input  : *ppOriginal - Original function being detoured
//          pDetour - Detour function
//-----------------------------------------------------------------------------
inline void HookAttach(PVOID* ppOriginal, PVOID pDetour)
{
	PVOID pAddr = *ppOriginal;
	if (MH_CreateHook(pAddr, pDetour, ppOriginal) == MH_OK)
	{
		if (MH_EnableHook(pAddr) != MH_OK)
		{
			spdlog::error("Failed enabling a function hook!");
		}
	}
	else
	{
		spdlog::error("Failed creating a function hook!");
	}
}

void* HookImportByOrdinal(const char* module, const char* targetDll, WORD targetOrdinal, void* replacement);
void* HookImportByName(const char* module, const char* targetDll, const char* funcName, void* replacement);

void CallLoadLibraryACallbacks(LPCSTR lpLibFileName, HMODULE moduleAddress);

void CallAllPendingDLLLoadCallbacks();

// new dll load callback stuff
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

// adds a callback to be called when a given dll is loaded, for creating hooks and such
#define __ON_DLL_LOAD(dllName, side, uniquestr, reliesOn, lambdaExpr)                                                                       \
	namespace                                                                                                                               \
	{                                                                                                                                       \
		inline auto CONCAT2(__dllLoadCallbackLambda_, uniquestr) = lambdaExpr;                                                              \
		void CONCAT2(__dllLoadCallback, uniquestr)(CModule module)                                                                           \
		{                                                                                                                                   \
			CONCAT2(__dllLoadCallbackLambda_, uniquestr)(module);                                                                             \
		}                                                                                                                                   \
		__dllLoadCallback CONCAT2(__dllLoadCallbackInstance, __LINE__)(                                                                     \
			side, dllName, CONCAT2(__dllLoadCallback, uniquestr), __STR(uniquestr), reliesOn);                                              \
	}                                                                                                                                       \

#define ON_DLL_LOAD(dllName, uniquestr, lambdaExpr) __ON_DLL_LOAD(dllName, eDllLoadCallbackSide::UNSIDED, uniquestr, "", lambdaExpr)
#define ON_DLL_LOAD_RELIESON(dllName, uniquestr, reliesOn, lambdaExpr)                                                                    \
	__ON_DLL_LOAD(dllName, eDllLoadCallbackSide::UNSIDED, uniquestr, __STR(reliesOn), lambdaExpr)
#define ON_DLL_LOAD_CLIENT(dllName, uniquestr, lambdaExpr) __ON_DLL_LOAD(dllName, eDllLoadCallbackSide::CLIENT, uniquestr, "", lambdaExpr)
#define ON_DLL_LOAD_CLIENT_RELIESON(dllName, uniquestr, reliesOn, lambdaExpr)                                                             \
	__ON_DLL_LOAD(dllName, eDllLoadCallbackSide::CLIENT, uniquestr, __STR(reliesOn), lambdaExpr)
#define ON_DLL_LOAD_DEDI(dllName, uniquestr, lambdaExpr) __ON_DLL_LOAD(dllName, eDllLoadCallbackSide::DEDICATED_SERVER, uniquestr, "", lambdaExpr)
#define ON_DLL_LOAD_DEDI_RELIESON(dllName, uniquestr, reliesOn, lambdaExpr)                                                               \
	__ON_DLL_LOAD(dllName, eDllLoadCallbackSide::DEDICATED_SERVER, uniquestr, __STR(reliesOn), lambdaExpr)

// new macro hook stuff
class __autohook;
class __autovar;

class __fileAutohook
{
public:
	std::vector<__autohook*> hooks;
	std::vector<__autovar*> vars;

	void Dispatch();
	void DispatchForModule(const char* pModuleName);
};

// initialise autohooks for this file
#define AUTOHOOK_INIT()                                                                                                                    \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		__fileAutohook __FILEAUTOHOOK;                                                                                                     \
	}

// dispatch all autohooks in this file
#define AUTOHOOK_DISPATCH() __FILEAUTOHOOK.Dispatch();

#define AUTOHOOK_DISPATCH_MODULE(moduleName) __FILEAUTOHOOK.DispatchForModule(__STR(moduleName));

class __autohook
{
public:
	enum AddressResolutionMode
	{
		OFFSET_STRING, // we're using a string that of the format dllname.dll + offset
		ABSOLUTE_ADDR, // we're using an absolute address, we don't need to process it at all
		PROCADDRESS // resolve using GetModuleHandle and GetProcAddress
	};

	char* pFuncName;

	LPVOID pHookFunc;
	LPVOID* ppOrigFunc;

	// address resolution props
	AddressResolutionMode iAddressResolutionMode;
	char* pAddrString = nullptr; // for OFFSET_STRING
	LPVOID iAbsoluteAddress = nullptr; // for ABSOLUTE_ADDR
	char* pModuleName; // for PROCADDRESS
	char* pProcName; // for PROCADDRESS

public:
	__autohook() = delete;

	__autohook(__fileAutohook* autohook, const char* funcName, LPVOID absoluteAddress, LPVOID* orig, LPVOID func)
		: pHookFunc(func)
		, ppOrigFunc(orig)
		, iAbsoluteAddress(absoluteAddress)
	{
		iAddressResolutionMode = ABSOLUTE_ADDR;

		const size_t iFuncNameStrlen = strlen(funcName) + 1;
		pFuncName = new char[iFuncNameStrlen];
		memcpy(pFuncName, funcName, iFuncNameStrlen);

		autohook->hooks.push_back(this);
	}

	__autohook(__fileAutohook* autohook, const char* funcName, const char* addrString, LPVOID* orig, LPVOID func)
		: pHookFunc(func)
		, ppOrigFunc(orig)
	{
		iAddressResolutionMode = OFFSET_STRING;

		const size_t iFuncNameStrlen = strlen(funcName) + 1;
		pFuncName = new char[iFuncNameStrlen];
		memcpy(pFuncName, funcName, iFuncNameStrlen);

		const size_t iAddrStrlen = strlen(addrString) + 1;
		pAddrString = new char[iAddrStrlen];
		memcpy(pAddrString, addrString, iAddrStrlen);

		autohook->hooks.push_back(this);
	}

	__autohook(__fileAutohook* autohook, const char* funcName, const char* moduleName, const char* procName, LPVOID* orig, LPVOID func)
		: pHookFunc(func)
		, ppOrigFunc(orig)
	{
		iAddressResolutionMode = PROCADDRESS;

		const size_t iFuncNameStrlen = strlen(funcName) + 1;
		pFuncName = new char[iFuncNameStrlen];
		memcpy(pFuncName, funcName, iFuncNameStrlen);

		const size_t iModuleNameStrlen = strlen(moduleName) + 1;
		pModuleName = new char[iModuleNameStrlen];
		memcpy(pModuleName, moduleName, iModuleNameStrlen);

		const size_t iProcNameStrlen = strlen(procName) + 1;
		pProcName = new char[iProcNameStrlen];
		memcpy(pProcName, procName, iProcNameStrlen);

		autohook->hooks.push_back(this);
	}

	~__autohook()
	{
		delete[] pFuncName;

		if (pAddrString)
			delete[] pAddrString;

		if (pModuleName)
			delete[] pModuleName;

		if (pProcName)
			delete[] pProcName;
	}

	void Dispatch()
	{
		LPVOID targetAddr = nullptr;

		// determine the address of the function we're hooking
		switch (iAddressResolutionMode)
		{
		case ABSOLUTE_ADDR:
		{
			targetAddr = iAbsoluteAddress;
			break;
		}

		case OFFSET_STRING:
		{
			targetAddr = (LPVOID)ParseDLLOffsetString(pAddrString);
			break;
		}

		case PROCADDRESS:
		{
			targetAddr = (LPVOID)GetProcAddress(GetModuleHandleA(pModuleName), pProcName);
			break;
		}
		}

		if (!targetAddr)
			spdlog::error("Address for hook {} is invalid", pFuncName);
		else if (MH_CreateHook(targetAddr, pHookFunc, ppOrigFunc) == MH_OK)
		{
			if (MH_EnableHook(targetAddr) == MH_OK)
				spdlog::info("Enabling hook {}", pFuncName);
			else
				spdlog::error("MH_EnableHook failed for function {}", pFuncName);
		}
		else
			spdlog::error("MH_CreateHook failed for function {}", pFuncName);
	}
};

// hook a function at a given offset from a dll to be dispatched with AUTOHOOK_DISPATCH()
#define AUTOHOOK(name, addrString, type, callingConvention, args)                                                                          \
	type callingConvention CONCAT2(__autohookfunc, name) args;                                                                             \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		type(*name) args;                                                                                                                  \
		__autohook CONCAT2(__autohook, __LINE__)(                                                                                          \
			&__FILEAUTOHOOK, __STR(name), __STR(addrString), (LPVOID*)&name, (LPVOID)CONCAT2(__autohookfunc, name));                       \
	}                                                                                                                                      \
	type callingConvention CONCAT2(__autohookfunc, name) args

// hook a function at a given absolute constant address to be dispatched with AUTOHOOK_DISPATCH()
#define AUTOHOOK_ABSOLUTEADDR(name, addr, type, callingConvention, args)                                                                   \
	type callingConvention CONCAT2(__autohookfunc, name) args;                                                                             \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		type(*name) args;                                                                                                                  \
		__autohook                                                                                                                         \
			CONCAT2(__autohook, __LINE__)(&__FILEAUTOHOOK, __STR(name), addr, (LPVOID*)&name, (LPVOID)CONCAT2(__autohookfunc, name));      \
	}                                                                                                                                      \
	type callingConvention CONCAT2(__autohookfunc, name) args

// hook a function at a given module and exported function to be dispatched with AUTOHOOK_DISPATCH()
#define AUTOHOOK_PROCADDRESS(name, moduleName, procName, type, callingConvention, args)                                                    \
	type callingConvention CONCAT2(__autohookfunc, name) args;                                                                             \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		type(*name) args;                                                                                                                  \
		__autohook CONCAT2(__autohook, __LINE__)(                                                                                          \
			&__FILEAUTOHOOK, __STR(name), __STR(moduleName), __STR(procName), (LPVOID*)&name, (LPVOID)CONCAT2(__autohookfunc, name));      \
	}                                                                                                                                      \
	type callingConvention CONCAT2(__autohookfunc, name)                                                                                   \
	args

class ManualHook
{
public:
	std::string svFuncName;

	LPVOID pHookFunc;
	LPVOID* ppOrigFunc;

public:
	ManualHook() = delete;
	ManualHook(const char* funcName, LPVOID func);
	ManualHook(const char* funcName, LPVOID* orig, LPVOID func);
	bool Dispatch(LPVOID addr, LPVOID* orig = nullptr);
};

// hook a function to be dispatched manually later
#define HOOK(varName, originalFunc, type, callingConvention, args)                                                                         \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		type(*originalFunc) args;                                                                                                          \
	}                                                                                                                                      \
	type callingConvention CONCAT2(__manualhookfunc, varName) args;                                                                        \
	ManualHook varName = ManualHook(__STR(varName), (LPVOID*)&originalFunc, (LPVOID)CONCAT2(__manualhookfunc, varName));                   \
	type callingConvention CONCAT2(__manualhookfunc, varName) args

#define HOOK_NOORIG(varName, type, callingConvention, args)                                                                                \
	type callingConvention CONCAT2(__manualhookfunc, varName) args;                                                                        \
	ManualHook varName = ManualHook(__STR(varName), (LPVOID)CONCAT2(__manualhookfunc, varName));                                           \
	type callingConvention CONCAT2(__manualhookfunc, varName)                                                                              \
	args

void MakeHook(LPVOID pTarget, LPVOID pDetour, void* ppOriginal, const char* pFuncName = "");
#define MAKEHOOK(pTarget, pDetour, ppOriginal) MakeHook((LPVOID)pTarget, (LPVOID)pDetour, (void*)ppOriginal, __STR(pDetour))

class __autovar
{
public:
	char* m_pAddrString;
	void** m_pTarget;

public:
	__autovar(__fileAutohook* pAutohook, const char* pAddrString, void** pTarget)
	{
		m_pTarget = pTarget;

		const size_t iAddrStrlen = strlen(pAddrString) + 1;
		m_pAddrString = new char[iAddrStrlen];
		memcpy(m_pAddrString, pAddrString, iAddrStrlen);

		pAutohook->vars.push_back(this);
	}

	void Dispatch() { *m_pTarget = (void*)ParseDLLOffsetString(m_pAddrString); }
};

// VAR_AT(engine.dll+0x404, ConVar*, Cvar_host_timescale)
#define VAR_AT(addrString, type, name)                                                                                                     \
	type name;                                                                                                                             \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		__autovar CONCAT2(__autovar, __LINE__)(&__FILEAUTOHOOK, __STR(addrString), (void**)&name);                                         \
	}

// FUNCTION_AT(engine.dll + 0xDEADBEEF, void, __fastcall, SomeFunc, (void* a1))
#define FUNCTION_AT(addrString, type, callingConvention, name, args)                                                                       \
	type(*name) args;                                                                                                                      \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		__autovar CONCAT2(__autovar, __LINE__)(&__FILEAUTOHOOK, __STR(addrString), (void**)&name);                                         \
	}

// int* g_pSomeInt;
// DEFINED_VAR_AT(engine.dll + 0x5005, g_pSomeInt)
#define DEFINED_VAR_AT(addrString, name)                                                                                                   \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		__autovar CONCAT2(__autovar, __LINE__)(&__FILEAUTOHOOK, __STR(addrString), (void**)&name);                                         \
	}

namespace HookSys
{
	template <typename T> struct lambda_traits : lambda_traits<decltype(&T::operator())>
	{
	};

	template <typename C, typename R, typename... Args> struct lambda_traits<R (C::*)(Args...)>
	{
		using return_type = R;
		using args_tuple = std::tuple<Args...>;
	};

	template <typename C, typename R, typename... Args> struct lambda_traits<R (C::*)(Args...) const>
	{
		using return_type = R;
		using args_tuple = std::tuple<Args...>;
	};

	struct any_return
	{
		template <typename T> operator T() const;
	};

	struct hook_placeholder
	{
		template <typename... Args> any_return Original(Args&&...) const { return {}; }
		void* ReturnAddress() const { return nullptr; }
	};

	template <typename LambdaT, typename = void> struct lambda_traits_for_hook : lambda_traits<decltype(&LambdaT::operator())>
	{
	};

	template <typename LambdaT>
	struct lambda_traits_for_hook<LambdaT, std::void_t<decltype(&LambdaT::template operator()<hook_placeholder>)>>
		: lambda_traits<decltype(&LambdaT::template operator()<hook_placeholder>)>
	{
	};

	template <typename Tuple> struct tuple_tail;

	template <typename Head, typename... Tail> struct tuple_tail<std::tuple<Head, Tail...>>
	{
		using type = std::tuple<Tail...>;
	};

	template <template <typename...> class Template, typename Tuple> struct apply_tuple;

	template <template <typename...> class Template, typename... Args> struct apply_tuple<Template, std::tuple<Args...>>
	{
		using type = Template<Args...>;
	};

	template <typename Fn, typename HookT, typename... Args> decltype(auto) InvokeHookTarget(Fn&& fn, HookT& hook, Args&&... args)
	{
		if constexpr (std::is_invocable_v<Fn, HookT&, Args...>)
		{
			if constexpr (std::is_void_v<std::invoke_result_t<Fn, HookT&, Args...>>)
			{
				std::invoke(std::forward<Fn>(fn), hook, std::forward<Args>(args)...);
				return;
			}
			else
			{
				return std::invoke(std::forward<Fn>(fn), hook, std::forward<Args>(args)...);
			}
		}

		if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>)
		{
			std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
			return;
		}
		else
		{
			return std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
		}
	}

	template <typename Method, typename InstanceT, typename HookT, typename... Args>
	decltype(auto) InvokeMethodHook(Method method, InstanceT&& instance, HookT& hook, Args&&... args)
	{
		if constexpr (std::is_invocable_v<Method, InstanceT, HookT&, Args...>)
		{
			if constexpr (std::is_void_v<std::invoke_result_t<Method, InstanceT, HookT&, Args...>>)
			{
				std::invoke(method, std::forward<InstanceT>(instance), hook, std::forward<Args>(args)...);
				return;
			}
			else
			{
				return std::invoke(method, std::forward<InstanceT>(instance), hook, std::forward<Args>(args)...);
			}
		}

		if constexpr (std::is_void_v<std::invoke_result_t<Method, InstanceT, Args...>>)
		{
			std::invoke(method, std::forward<InstanceT>(instance), std::forward<Args>(args)...);
			return;
		}
		else
		{
			return std::invoke(method, std::forward<InstanceT>(instance), std::forward<Args>(args)...);
		}
	}

	enum class AddressMode
	{
		OffsetString,
		AbsoluteAddress,
		ProcAddress
	};

	class LambdaHookBase
	{
	public:
		virtual ~LambdaHookBase() = default;
		virtual bool Dispatch() = 0;
		virtual void* GetOriginalRaw() const { return nullptr; }

		const std::string& DebugName() const { return m_debugName; }
		const std::string& AddressString() const { return m_addrString; }
		const std::string& ModuleName() const { return m_moduleName; }
		const std::string& ProcName() const { return m_procName; }
		AddressMode Mode() const { return m_addrMode; }
		uintptr_t AbsoluteAddress() const { return m_absoluteAddress; }

		void ConfigureDebugName(const char* debugName) { SetDebugName(debugName); }
		void ConfigureOffsetAddress(const char* addrString) { SetOffsetAddress(addrString); }
		void ConfigureAbsoluteAddress(uintptr_t addr) { SetAbsoluteAddress(addr); }
		void ConfigureProcAddress(const char* moduleName, const char* procName) { SetProcAddress(moduleName, procName); }

	protected:
		void SetDebugName(const char* debugName) { m_debugName = debugName ? debugName : ""; }

		void SetOffsetAddress(const char* addrString)
		{
			m_addrMode = AddressMode::OffsetString;
			m_addrString = addrString ? addrString : "";
			m_moduleName = ExtractModuleName(m_addrString.c_str());
		}

		void SetAbsoluteAddress(uintptr_t addr)
		{
			m_addrMode = AddressMode::AbsoluteAddress;
			m_absoluteAddress = addr;
		}

		void SetProcAddress(const char* moduleName, const char* procName)
		{
			m_addrMode = AddressMode::ProcAddress;
			m_moduleName = moduleName ? moduleName : "";
			m_procName = procName ? procName : "";
		}

		uintptr_t ResolveAddress() const
		{
			switch (m_addrMode)
			{
			case AddressMode::OffsetString:
				return ParseDLLOffsetString(m_addrString.c_str());
			case AddressMode::AbsoluteAddress:
				return m_absoluteAddress;
			case AddressMode::ProcAddress:
				return reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA(m_moduleName.c_str()), m_procName.c_str()));
			}

			return 0;
		}

		static std::string ExtractModuleName(const char* addrString)
		{
			if (!addrString || !*addrString)
				return {};

			size_t i = 0;
			while (addrString[i] && !std::isspace(static_cast<unsigned char>(addrString[i])) && addrString[i] != '+')
				++i;

			if (i == 0)
				return {};

			return std::string(addrString, i);
		}

	private:
		std::string m_debugName;
		std::string m_addrString;
		std::string m_moduleName;
		std::string m_procName;
		AddressMode m_addrMode = AddressMode::OffsetString;
		uintptr_t m_absoluteAddress = 0;
	};

	inline std::unordered_map<std::string, std::shared_ptr<LambdaHookBase>> g_hookRegistry;

	inline std::unordered_map<std::string, std::shared_ptr<LambdaHookBase>>& GetHookRegistry()
	{
		return g_hookRegistry;
	}

	inline void RegisterHook(LambdaHookBase* hook)
	{
		if (!hook)
			return;

		GetHookRegistry()[hook->DebugName()] = std::shared_ptr<LambdaHookBase>(hook, [](LambdaHookBase*) {});
	}

	inline std::shared_ptr<LambdaHookBase> FindHook(const std::string& debugName)
	{
		auto& registry = GetHookRegistry();
		auto it = registry.find(debugName);
		return it != registry.end() ? it->second : nullptr;
	}

	template <typename Fn> inline Fn GetOriginalFunction(const std::shared_ptr<LambdaHookBase>& hook)
	{
		return hook ? reinterpret_cast<Fn>(hook->GetOriginalRaw()) : nullptr;
	}

	template <typename HookT> struct LambdaHookRegistrationOffset
	{
		LambdaHookRegistrationOffset(const char* debugName, const char* addrString)
		{
			HookT& hook = HookT::Instance();
			hook.ConfigureDebugName(debugName);
			hook.ConfigureOffsetAddress(addrString);
			RegisterHook(&hook);

			if (!hook.ModuleName().empty())
				AddDllLoadCallback(hook.ModuleName(), &HookT::OnModuleLoaded, debugName ? debugName : "");
			else
				hook.Dispatch();
		}
	};

	template <typename HookT> struct LambdaHookRegistrationAbsolute
	{
		LambdaHookRegistrationAbsolute(const char* debugName, uintptr_t addr)
		{
			HookT& hook = HookT::Instance();
			hook.ConfigureDebugName(debugName);
			hook.ConfigureAbsoluteAddress(addr);
			RegisterHook(&hook);
			hook.Dispatch();
		}
	};

	template <typename HookT> struct LambdaHookRegistrationProc
	{
		LambdaHookRegistrationProc(const char* debugName, const char* moduleName, const char* procName)
		{
			HookT& hook = HookT::Instance();
			hook.ConfigureDebugName(debugName);
			hook.ConfigureProcAddress(moduleName, procName);
			RegisterHook(&hook);
			if (moduleName && *moduleName)
				AddDllLoadCallback(moduleName, &HookT::OnModuleLoaded, debugName ? debugName : "");
			else
				hook.Dispatch();
		}
	};
} // namespace HookSys

// default calling convention for DECLARE_HOOK
#ifndef HOOKSYS_CALLCONV
#define HOOKSYS_CALLCONV __fastcall
#endif

// Lambda hook using an offset string (e.g. engine.dll + 0x123456)
#define DECLARE_HOOK_CC(debugName, addrString, callingConvention, lambda)                                                                  \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		inline auto CONCAT2(__lambdaHookLambda_, __LINE__) = lambda;                                                                       \
		struct CONCAT2(__lambdaHook_, __LINE__)                                                                                            \
			: public HookSys::LambdaHookBase                                                                                               \
		{                                                                                                                                  \
			using Self = CONCAT2(__lambdaHook_, __LINE__);                                                                                 \
			using LambdaT = std::decay_t<decltype(CONCAT2(__lambdaHookLambda_, __LINE__))>;                                                \
			using Traits = HookSys::lambda_traits_for_hook<LambdaT>;                                                                       \
			using ReturnT = typename Traits::return_type;                                                                                  \
			using FullArgs = typename Traits::args_tuple;                                                                                  \
			static_assert(std::tuple_size_v<FullArgs> >= 1, "Hook lambda must take hook ref as first arg");                                \
			using ArgsTuple = typename HookSys::tuple_tail<FullArgs>::type;                                                                \
			template <typename... Args> struct Helper                                                                                      \
			{                                                                                                                              \
				using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                   \
				static ReturnT callingConvention Detour(Args... args)                                                                      \
				{                                                                                                                          \
					if constexpr (std::is_void_v<ReturnT>)                                                                                 \
						Self::Instance().Invoke(args...);                                                                                  \
					else                                                                                                                   \
						return Self::Instance().Invoke(args...);                                                                           \
				}                                                                                                                          \
			};                                                                                                                             \
			using HelperT = typename HookSys::apply_tuple<Helper, ArgsTuple>::type;                                                        \
			using OriginalFn = typename HelperT::OriginalFn;                                                                               \
			inline static OriginalFn s_original = nullptr;                                                                                 \
			inline static thread_local void* s_returnAddress = nullptr;                                                                    \
			inline static LambdaT s_lambda = CONCAT2(__lambdaHookLambda_, __LINE__);                                                       \
			static Self& Instance()                                                                                                        \
			{                                                                                                                              \
				static Self inst;                                                                                                          \
				return inst;                                                                                                               \
			}                                                                                                                              \
			void* ReturnAddress() const { return s_returnAddress; }                                                                        \
			template <typename... Args> ReturnT Invoke(Args... args)                                                                       \
			{                                                                                                                              \
				if constexpr (std::is_void_v<ReturnT>)                                                                                     \
					s_lambda(Instance(), args...);                                                                                         \
				else                                                                                                                       \
					return s_lambda(Instance(), args...);                                                                                  \
			}                                                                                                                              \
			template <typename... Args> ReturnT Original(Args... args)                                                                     \
			{                                                                                                                              \
				if (!s_original)                                                                                                           \
				{                                                                                                                          \
					spdlog::error("Original function for hook {} is null", DebugName());                                                   \
					if constexpr (!std::is_void_v<ReturnT>)                                                                                \
						return ReturnT {};                                                                                                 \
					else                                                                                                                   \
						return;                                                                                                            \
				}                                                                                                                          \
				if constexpr (std::is_void_v<ReturnT>)                                                                                     \
					s_original(args...);                                                                                                   \
				else                                                                                                                       \
					return s_original(args...);                                                                                            \
			}                                                                                                                              \
			bool Dispatch() override                                                                                                       \
			{                                                                                                                              \
				const uintptr_t addr = ResolveAddress();                                                                                   \
				if (!addr)                                                                                                                 \
				{                                                                                                                          \
					spdlog::error("Address for hook {} is invalid", DebugName());                                                          \
					return false;                                                                                                          \
				}                                                                                                                          \
				if (MH_CreateHook(                                                                                                         \
						reinterpret_cast<LPVOID>(addr),                                                                                    \
						reinterpret_cast<LPVOID>(&HelperT::Detour),                                                                        \
						reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                  \
				{                                                                                                                          \
					if (MH_EnableHook(reinterpret_cast<LPVOID>(addr)) == MH_OK)                                                            \
					{                                                                                                                      \
						spdlog::info("Enabling hook {}", DebugName());                                                                     \
						return true;                                                                                                       \
					}                                                                                                                      \
					spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                    \
				}                                                                                                                          \
				else                                                                                                                       \
					spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                    \
				return false;                                                                                                              \
			}                                                                                                                              \
			void* GetOriginalRaw() const override { return reinterpret_cast<void*>(s_original); }                                          \
			static void OnModuleLoaded(CModule) { Instance().Dispatch(); }                                                                 \
		};                                                                                                                                 \
		HookSys::LambdaHookRegistrationOffset<CONCAT2(__lambdaHook_, __LINE__)>                                                            \
			CONCAT2(__lambdaHookReg_, __LINE__)(__STR(debugName), __STR(addrString));                                                      \
	}

#define DECLARE_HOOK(debugName, addrString, lambda) DECLARE_HOOK_CC(debugName, addrString, HOOKSYS_CALLCONV, lambda)

// Hook using a regular free function (with or without hook ref as first arg)
#define DECLARE_HOOK_FN_CC(debugName, addrString, callingConvention, func)                                                                 \
	DECLARE_HOOK_CC(                                                                                                                       \
		debugName,                                                                                                                         \
		addrString,                                                                                                                        \
		callingConvention,                                                                                                                 \
		[](auto& hook, auto... args) -> decltype(auto)                                                                                     \
		{                                                                                                                                  \
			if constexpr (std::is_void_v<decltype(HookSys::InvokeHookTarget(func, hook, args...))>)                                        \
			{                                                                                                                              \
				HookSys::InvokeHookTarget(func, hook, args...);                                                                            \
				return;                                                                                                                    \
			}                                                                                                                              \
			else                                                                                                                           \
			{                                                                                                                              \
				return HookSys::InvokeHookTarget(func, hook, args...);                                                                     \
			}                                                                                                                              \
		})

#define DECLARE_HOOK_FN(debugName, addrString, func) DECLARE_HOOK_FN_CC(debugName, addrString, HOOKSYS_CALLCONV, func)

// Lambda hook using an absolute address
#define DECLARE_HOOK_ABSOLUTE_CC(debugName, addr, callingConvention, lambda)                                                               \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		inline auto CONCAT2(__lambdaHookAbsLambda_, __LINE__) = lambda;                                                                    \
		struct CONCAT2(__lambdaHookAbs_, __LINE__)                                                                                         \
			: public HookSys::LambdaHookBase                                                                                               \
		{                                                                                                                                  \
			using Self = CONCAT2(__lambdaHookAbs_, __LINE__);                                                                              \
			using LambdaT = std::decay_t<decltype(CONCAT2(__lambdaHookAbsLambda_, __LINE__))>;                                             \
			using Traits = HookSys::lambda_traits_for_hook<LambdaT>;                                                                       \
			using ReturnT = typename Traits::return_type;                                                                                  \
			using FullArgs = typename Traits::args_tuple;                                                                                  \
			static_assert(std::tuple_size_v<FullArgs> >= 1, "Hook lambda must take hook ref as first arg");                                \
			using ArgsTuple = typename HookSys::tuple_tail<FullArgs>::type;                                                                \
			template <typename... Args> struct Helper                                                                                      \
			{                                                                                                                              \
				using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                   \
				static ReturnT callingConvention Detour(Args... args)                                                                      \
				{                                                                                                                          \
					Self::s_returnAddress = _ReturnAddress();                                                                              \
					if constexpr (std::is_void_v<ReturnT>)                                                                                 \
						Self::Instance().Invoke(args...);                                                                                  \
					else                                                                                                                   \
						return Self::Instance().Invoke(args...);                                                                           \
				}                                                                                                                          \
			};                                                                                                                             \
			using HelperT = typename HookSys::apply_tuple<Helper, ArgsTuple>::type;                                                        \
			using OriginalFn = typename HelperT::OriginalFn;                                                                               \
			inline static OriginalFn s_original = nullptr;                                                                                 \
			inline static thread_local void* s_returnAddress = nullptr;                                                                    \
			inline static LambdaT s_lambda = CONCAT2(__lambdaHookAbsLambda_, __LINE__);                                                    \
			static Self& Instance()                                                                                                        \
			{                                                                                                                              \
				static Self inst;                                                                                                          \
				return inst;                                                                                                               \
			}                                                                                                                              \
			void* ReturnAddress() const { return s_returnAddress; }                                                                        \
			template <typename... Args> ReturnT Invoke(Args... args)                                                                       \
			{                                                                                                                              \
				if constexpr (std::is_void_v<ReturnT>)                                                                                     \
					s_lambda(Instance(), args...);                                                                                         \
				else                                                                                                                       \
					return s_lambda(Instance(), args...);                                                                                  \
			}                                                                                                                              \
			template <typename... Args> ReturnT Original(Args... args)                                                                     \
			{                                                                                                                              \
				if (!s_original)                                                                                                           \
				{                                                                                                                          \
					spdlog::error("Original function for hook {} is null", DebugName());                                                   \
					if constexpr (!std::is_void_v<ReturnT>)                                                                                \
						return ReturnT {};                                                                                                 \
					else                                                                                                                   \
						return;                                                                                                            \
				}                                                                                                                          \
				if constexpr (std::is_void_v<ReturnT>)                                                                                     \
					s_original(args...);                                                                                                   \
				else                                                                                                                       \
					return s_original(args...);                                                                                            \
			}                                                                                                                              \
			bool Dispatch() override                                                                                                       \
			{                                                                                                                              \
				const uintptr_t addrResolved = ResolveAddress();                                                                           \
				if (!addrResolved)                                                                                                         \
				{                                                                                                                          \
					spdlog::error("Address for hook {} is invalid", DebugName());                                                          \
					return false;                                                                                                          \
				}                                                                                                                          \
				if (MH_CreateHook(                                                                                                         \
						reinterpret_cast<LPVOID>(addrResolved),                                                                            \
						reinterpret_cast<LPVOID>(&HelperT::Detour),                                                                        \
						reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                  \
				{                                                                                                                          \
					if (MH_EnableHook(reinterpret_cast<LPVOID>(addrResolved)) == MH_OK)                                                    \
					{                                                                                                                      \
						spdlog::info("Enabling hook {}", DebugName());                                                                     \
						return true;                                                                                                       \
					}                                                                                                                      \
					spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                    \
				}                                                                                                                          \
				else                                                                                                                       \
					spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                    \
				return false;                                                                                                              \
			}                                                                                                                              \
			void* GetOriginalRaw() const override { return reinterpret_cast<void*>(s_original); }                                          \
			static void OnModuleLoaded(CModule) { Instance().Dispatch(); }                                                                 \
		};                                                                                                                                 \
		HookSys::LambdaHookRegistrationAbsolute<CONCAT2(__lambdaHookAbs_, __LINE__)>                                                       \
			CONCAT2(__lambdaHookAbsReg_, __LINE__)(__STR(debugName), static_cast<uintptr_t>(addr));                                        \
	}

#define DECLARE_HOOK_ABSOLUTE(debugName, addr, lambda) DECLARE_HOOK_ABSOLUTE_CC(debugName, addr, HOOKSYS_CALLCONV, lambda)

// Hook using a regular free function at an absolute address
#define DECLARE_HOOK_ABSOLUTE_FN_CC(debugName, addr, callingConvention, func)                                                              \
	DECLARE_HOOK_ABSOLUTE_CC(                                                                                                              \
		debugName,                                                                                                                         \
		addr,                                                                                                                              \
		callingConvention,                                                                                                                 \
		[](auto& hook, auto... args) -> decltype(auto)                                                                                     \
		{                                                                                                                                  \
			if constexpr (std::is_void_v<decltype(HookSys::InvokeHookTarget(func, hook, args...))>)                                        \
			{                                                                                                                              \
				HookSys::InvokeHookTarget(func, hook, args...);                                                                            \
				return;                                                                                                                    \
			}                                                                                                                              \
			else                                                                                                                           \
			{                                                                                                                              \
				return HookSys::InvokeHookTarget(func, hook, args...);                                                                     \
			}                                                                                                                              \
		})

#define DECLARE_HOOK_ABSOLUTE_FN(debugName, addr, func) DECLARE_HOOK_ABSOLUTE_FN_CC(debugName, addr, HOOKSYS_CALLCONV, func)

// Lambda hook using GetProcAddress
#define DECLARE_HOOK_PROC_CC(debugName, moduleName, procName, callingConvention, lambda)                                                   \
	namespace                                                                                                                              \
	{                                                                                                                                      \
		inline auto CONCAT2(__lambdaHookProcLambda_, __LINE__) = lambda;                                                                   \
		struct CONCAT2(__lambdaHookProc_, __LINE__)                                                                                        \
			: public HookSys::LambdaHookBase                                                                                               \
		{                                                                                                                                  \
			using Self = CONCAT2(__lambdaHookProc_, __LINE__);                                                                             \
			using LambdaT = std::decay_t<decltype(CONCAT2(__lambdaHookProcLambda_, __LINE__))>;                                            \
			using Traits = HookSys::lambda_traits_for_hook<LambdaT>;                                                                       \
			using ReturnT = typename Traits::return_type;                                                                                  \
			using FullArgs = typename Traits::args_tuple;                                                                                  \
			static_assert(std::tuple_size_v<FullArgs> >= 1, "Hook lambda must take hook ref as first arg");                                \
			using ArgsTuple = typename HookSys::tuple_tail<FullArgs>::type;                                                                \
			template <typename... Args> struct Helper                                                                                      \
			{                                                                                                                              \
				using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                   \
				static ReturnT callingConvention Detour(Args... args)                                                                      \
				{                                                                                                                          \
					Self::s_returnAddress = _ReturnAddress();                                                                              \
					if constexpr (std::is_void_v<ReturnT>)                                                                                 \
						Self::Instance().Invoke(args...);                                                                                  \
					else                                                                                                                   \
						return Self::Instance().Invoke(args...);                                                                           \
				}                                                                                                                          \
			};                                                                                                                             \
			using HelperT = typename HookSys::apply_tuple<Helper, ArgsTuple>::type;                                                        \
			using OriginalFn = typename HelperT::OriginalFn;                                                                               \
			inline static OriginalFn s_original = nullptr;                                                                                 \
			inline static thread_local void* s_returnAddress = nullptr;                                                                    \
			inline static LambdaT s_lambda = CONCAT2(__lambdaHookProcLambda_, __LINE__);                                                   \
			static Self& Instance()                                                                                                        \
			{                                                                                                                              \
				static Self inst;                                                                                                          \
				return inst;                                                                                                               \
			}                                                                                                                              \
			void* ReturnAddress() const { return s_returnAddress; }                                                                        \
			template <typename... Args> ReturnT Invoke(Args... args)                                                                       \
			{                                                                                                                              \
				if constexpr (std::is_void_v<ReturnT>)                                                                                     \
					s_lambda(Instance(), args...);                                                                                         \
				else                                                                                                                       \
					return s_lambda(Instance(), args...);                                                                                  \
			}                                                                                                                              \
			template <typename... Args> ReturnT Original(Args... args)                                                                     \
			{                                                                                                                              \
				if (!s_original)                                                                                                           \
				{                                                                                                                          \
					spdlog::error("Original function for hook {} is null", DebugName());                                                   \
					if constexpr (!std::is_void_v<ReturnT>)                                                                                \
						return ReturnT {};                                                                                                 \
					else                                                                                                                   \
						return;                                                                                                            \
				}                                                                                                                          \
				if constexpr (std::is_void_v<ReturnT>)                                                                                     \
					s_original(args...);                                                                                                   \
				else                                                                                                                       \
					return s_original(args...);                                                                                            \
			}                                                                                                                              \
			bool Dispatch() override                                                                                                       \
			{                                                                                                                              \
				const uintptr_t addrResolved = ResolveAddress();                                                                           \
				if (!addrResolved)                                                                                                         \
				{                                                                                                                          \
					spdlog::error("Address for hook {} is invalid", DebugName());                                                          \
					return false;                                                                                                          \
				}                                                                                                                          \
				if (MH_CreateHook(                                                                                                         \
						reinterpret_cast<LPVOID>(addrResolved),                                                                            \
						reinterpret_cast<LPVOID>(&HelperT::Detour),                                                                        \
						reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                  \
				{                                                                                                                          \
					if (MH_EnableHook(reinterpret_cast<LPVOID>(addrResolved)) == MH_OK)                                                    \
					{                                                                                                                      \
						spdlog::info("Enabling hook {}", DebugName());                                                                     \
						return true;                                                                                                       \
					}                                                                                                                      \
					spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                    \
				}                                                                                                                          \
				else                                                                                                                       \
					spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                    \
				return false;                                                                                                              \
			}                                                                                                                              \
			void* GetOriginalRaw() const override { return reinterpret_cast<void*>(s_original); }                                          \
			static void OnModuleLoaded(CModule) { Instance().Dispatch(); }                                                                 \
		};                                                                                                                                 \
		HookSys::LambdaHookRegistrationProc<CONCAT2(__lambdaHookProc_, __LINE__)>                                                          \
			CONCAT2(__lambdaHookProcReg_, __LINE__)(__STR(debugName), __STR(moduleName), __STR(procName));                                 \
	}

#define DECLARE_HOOK_PROC(debugName, moduleName, procName, lambda)                                                                         \
	DECLARE_HOOK_PROC_CC(debugName, moduleName, procName, HookSys_CALLCONV, lambda)

// Hook using a regular free function resolved via GetProcAddress
#define DECLARE_HOOK_PROC_FN_CC(debugName, moduleName, procName, callingConvention, func)                                                  \
	DECLARE_HOOK_PROC_CC(                                                                                                                  \
		debugName,                                                                                                                         \
		moduleName,                                                                                                                        \
		procName,                                                                                                                          \
		callingConvention,                                                                                                                 \
		[](auto& hook, auto... args) -> decltype(auto)                                                                                     \
		{                                                                                                                                  \
			if constexpr (std::is_void_v<decltype(HookSys::InvokeHookTarget(func, hook, args...))>)                                        \
			{                                                                                                                              \
				HookSys::InvokeHookTarget(func, hook, args...);                                                                            \
				return;                                                                                                                    \
			}                                                                                                                              \
			else                                                                                                                           \
			{                                                                                                                              \
				return HookSys::InvokeHookTarget(func, hook, args...);                                                                     \
			}                                                                                                                              \
		})

#define DECLARE_HOOK_PROC_FN(debugName, moduleName, procName, func)                                                                        \
	DECLARE_HOOK_PROC_FN_CC(debugName, moduleName, procName, HOOKSYS_CALLCONV, func)

// Hook using a non-static member function. instanceExpr should be a pointer or reference to the instance.
#define DECLARE_HOOK_METHOD_CC(debugName, addrString, callingConvention, methodPtr, instanceExpr)                                          \
	DECLARE_HOOK_CC(                                                                                                                       \
		debugName,                                                                                                                         \
		addrString,                                                                                                                        \
		callingConvention,                                                                                                                 \
		[](auto& hook, auto... args) -> decltype(auto)                                                                                     \
		{                                                                                                                                  \
			if constexpr (std::is_void_v<decltype(HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...))>)                     \
			{                                                                                                                              \
				HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                         \
				return;                                                                                                                    \
			}                                                                                                                              \
			else                                                                                                                           \
			{                                                                                                                              \
				return HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                  \
			}                                                                                                                              \
		})

#define DECLARE_HOOK_METHOD(debugName, addrString, methodPtr, instanceExpr)                                                                \
	DECLARE_HOOK_METHOD_CC(debugName, addrString, HOOKSYS_CALLCONV, methodPtr, instanceExpr)

#define DECLARE_HOOK_ABSOLUTE_METHOD_CC(debugName, addr, callingConvention, methodPtr, instanceExpr)                                       \
	DECLARE_HOOK_ABSOLUTE_CC(                                                                                                              \
		debugName,                                                                                                                         \
		addr,                                                                                                                              \
		callingConvention,                                                                                                                 \
		[](auto& hook, auto... args) -> decltype(auto)                                                                                     \
		{                                                                                                                                  \
			if constexpr (std::is_void_v<decltype(HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...))>)                     \
			{                                                                                                                              \
				HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                         \
				return;                                                                                                                    \
			}                                                                                                                              \
			else                                                                                                                           \
			{                                                                                                                              \
				return HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                  \
			}                                                                                                                              \
		})

#define DECLARE_HOOK_ABSOLUTE_METHOD(debugName, addr, methodPtr, instanceExpr)                                                             \
	DECLARE_HOOK_ABSOLUTE_METHOD_CC(debugName, addr, HOOKSYS_CALLCONV, methodPtr, instanceExpr)

#define DECLARE_HOOK_PROC_METHOD_CC(debugName, moduleName, procName, callingConvention, methodPtr, instanceExpr)                           \
	DECLARE_HOOK_PROC_CC(                                                                                                                  \
		debugName,                                                                                                                         \
		moduleName,                                                                                                                        \
		procName,                                                                                                                          \
		callingConvention,                                                                                                                 \
		[](auto& hook, auto... args) -> decltype(auto)                                                                                     \
		{                                                                                                                                  \
			if constexpr (std::is_void_v<decltype(HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...))>)                     \
			{                                                                                                                              \
				HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                         \
				return;                                                                                                                    \
			}                                                                                                                              \
			else                                                                                                                           \
			{                                                                                                                              \
				return HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                  \
			}                                                                                                                              \
		})

#define DECLARE_HOOK_PROC_METHOD(debugName, moduleName, procName, methodPtr, instanceExpr)                                                 \
	DECLARE_HOOK_PROC_METHOD_CC(debugName, moduleName, procName, HOOKSYS_CALLCONV, methodPtr, instanceExpr)
