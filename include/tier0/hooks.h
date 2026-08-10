#pragma once

#include <windows.h>

#include <cassert>
#include <cctype>
#include <cstdarg>
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
#include "tier0/callbacks.h"
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


//-----------------------------------------------------------------------------
// Purpose: Init minhook
//-----------------------------------------------------------------------------
void HookSys_Init();

void* HookImportByOrdinal(const char* module, const char* targetDll, WORD targetOrdinal, void* replacement);
void* HookImportByName(const char* module, const char* targetDll, const char* funcName, void* replacement);

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
#define HOOK(varName, originalFunc, type, callingConvention, args)                                                                                   \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    type(*originalFunc) args;                                                                                                                        \
    }                                                                                                                                                \
    type callingConvention CONCAT2(__manualhookfunc, varName) args;                                                                                  \
    ManualHook varName = ManualHook(__STR(varName), (LPVOID*)&originalFunc, (LPVOID)CONCAT2(__manualhookfunc, varName));                             \
    type callingConvention CONCAT2(__manualhookfunc, varName) args

#define HOOK_NOORIG(varName, type, callingConvention, args)                                                                                          \
    type callingConvention CONCAT2(__manualhookfunc, varName) args;                                                                                  \
    ManualHook varName = ManualHook(__STR(varName), (LPVOID)CONCAT2(__manualhookfunc, varName));                                                     \
    type callingConvention CONCAT2(__manualhookfunc, varName)                                                                                        \
    args

void MakeHook(LPVOID pTarget, LPVOID pDetour, void* ppOriginal, const char* pFuncName = "");
#define MAKEHOOK(pTarget, pDetour, ppOriginal) MakeHook((LPVOID)pTarget, (LPVOID)pDetour, (void*)ppOriginal, __STR(pDetour))

namespace HookSys
{
template <typename T> struct lambda_traits : lambda_traits<decltype(&T::operator())>
{
    static constexpr bool is_variadic = false;
};

template <typename C, typename R, typename... Args> struct lambda_traits<R (C::*)(Args...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr bool is_variadic = false;
};

template <typename C, typename R, typename... Args> struct lambda_traits<R (C::*)(Args...) const>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr bool is_variadic = false;
};

template <typename C, typename R, typename... Args> struct lambda_traits<R (C::*)(Args..., ...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr bool is_variadic = true;
};

template <typename C, typename R, typename... Args> struct lambda_traits<R (C::*)(Args..., ...) const>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr bool is_variadic = true;
};

struct any_return
{
    template <typename T> operator T() const;
};

struct hook_placeholder
{
    template <typename... Args> any_return Original(Args&&...) const
    {
        return {};
    }
    void* ReturnAddress() const
    {
        return nullptr;
    }
    bool HasVarArgs() const
    {
        return false;
    }
    va_list* VarArgs()
    {
        return nullptr;
    }
};

inline thread_local const char* g_activeHookName = nullptr;

inline const char* GetActiveHookName()
{
    return g_activeHookName;
}

template <const char* HookDebugName> class HookInvocationScope
{
  public:
    HookInvocationScope() : m_previousHookName(g_activeHookName)
    {
        g_activeHookName = HookDebugName;
    }

    ~HookInvocationScope()
    {
        g_activeHookName = m_previousHookName;
    }

    HookInvocationScope(const HookInvocationScope&) = delete;
    HookInvocationScope& operator=(const HookInvocationScope&) = delete;

  private:
    const char* m_previousHookName;
};

template <typename LambdaT, typename = void> struct lambda_traits_for_hook : lambda_traits<decltype(&LambdaT::operator())>
{
};

template <typename LambdaT>
struct lambda_traits_for_hook<LambdaT, std::void_t<decltype(&LambdaT::template operator()<hook_placeholder>)>>
    : lambda_traits<decltype(&LambdaT::template operator()<hook_placeholder>)>
{
};

template <typename T> struct function_traits;

template <typename R, typename... Args> struct function_traits<R (*)(Args...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr bool is_variadic = false;
};

template <typename R, typename... Args> struct function_traits<R (*)(Args..., ...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr bool is_variadic = true;
};

template <typename R, typename... Args> struct function_traits<R (&)(Args...)> : function_traits<R (*)(Args...)>
{
};

template <typename R, typename... Args> struct function_traits<R (&)(Args..., ...)> : function_traits<R (*)(Args..., ...)>
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

template <typename Self, typename ReturnT, typename... Args> struct VarargHelper;

template <typename Self, typename ReturnT, typename... Args> ReturnT InvokeVarargsDetour(va_list& __va, Args... args)
{
    va_copy(Self::s_vaList, __va);
    Self::s_hasVaList = true;
    if constexpr (std::is_void_v<ReturnT>)
    {
        Self::Instance().InvokeVarargs(args...);
        Self::s_hasVaList = false;
        va_end(Self::s_vaList);
        va_end(__va);
        return;
    }
    else
    {
        auto __ret = Self::Instance().InvokeVarargs(args...);
        Self::s_hasVaList = false;
        va_end(Self::s_vaList);
        va_end(__va);
        return __ret;
    }
}

#define HOOKSYS_VA_TYPES_1 typename A0
#define HOOKSYS_VA_TARGS_1 A0
#define HOOKSYS_VA_ARGS_1 A0 a0
#define HOOKSYS_VA_NAMES_1 a0
#define HOOKSYS_VA_LAST_1 a0

#define HOOKSYS_VA_TYPES_2 typename A0, typename A1
#define HOOKSYS_VA_TARGS_2 A0, A1
#define HOOKSYS_VA_ARGS_2 A0 a0, A1 a1
#define HOOKSYS_VA_NAMES_2 a0, a1
#define HOOKSYS_VA_LAST_2 a1

#define HOOKSYS_VA_TYPES_3 typename A0, typename A1, typename A2
#define HOOKSYS_VA_TARGS_3 A0, A1, A2
#define HOOKSYS_VA_ARGS_3 A0 a0, A1 a1, A2 a2
#define HOOKSYS_VA_NAMES_3 a0, a1, a2
#define HOOKSYS_VA_LAST_3 a2

#define HOOKSYS_VA_TYPES_4 typename A0, typename A1, typename A2, typename A3
#define HOOKSYS_VA_TARGS_4 A0, A1, A2, A3
#define HOOKSYS_VA_ARGS_4 A0 a0, A1 a1, A2 a2, A3 a3
#define HOOKSYS_VA_NAMES_4 a0, a1, a2, a3
#define HOOKSYS_VA_LAST_4 a3

#define HOOKSYS_VA_TYPES_5 typename A0, typename A1, typename A2, typename A3, typename A4
#define HOOKSYS_VA_TARGS_5 A0, A1, A2, A3, A4
#define HOOKSYS_VA_ARGS_5 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4
#define HOOKSYS_VA_NAMES_5 a0, a1, a2, a3, a4
#define HOOKSYS_VA_LAST_5 a4

#define HOOKSYS_VA_TYPES_6 typename A0, typename A1, typename A2, typename A3, typename A4, typename A5
#define HOOKSYS_VA_TARGS_6 A0, A1, A2, A3, A4, A5
#define HOOKSYS_VA_ARGS_6 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5
#define HOOKSYS_VA_NAMES_6 a0, a1, a2, a3, a4, a5
#define HOOKSYS_VA_LAST_6 a5

#define HOOKSYS_VA_TYPES_7 typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6
#define HOOKSYS_VA_TARGS_7 A0, A1, A2, A3, A4, A5, A6
#define HOOKSYS_VA_ARGS_7 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6
#define HOOKSYS_VA_NAMES_7 a0, a1, a2, a3, a4, a5, a6
#define HOOKSYS_VA_LAST_7 a6

#define HOOKSYS_VA_TYPES_8 typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7
#define HOOKSYS_VA_TARGS_8 A0, A1, A2, A3, A4, A5, A6, A7
#define HOOKSYS_VA_ARGS_8 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7
#define HOOKSYS_VA_NAMES_8 a0, a1, a2, a3, a4, a5, a6, a7
#define HOOKSYS_VA_LAST_8 a7

#define HOOKSYS_VA_TYPES_9 typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8
#define HOOKSYS_VA_TARGS_9 A0, A1, A2, A3, A4, A5, A6, A7, A8
#define HOOKSYS_VA_ARGS_9 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8
#define HOOKSYS_VA_NAMES_9 a0, a1, a2, a3, a4, a5, a6, a7, a8
#define HOOKSYS_VA_LAST_9 a8

#define HOOKSYS_VA_TYPES_10                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9
#define HOOKSYS_VA_TARGS_10 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9
#define HOOKSYS_VA_ARGS_10 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9
#define HOOKSYS_VA_NAMES_10 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9
#define HOOKSYS_VA_LAST_10 a9

#define HOOKSYS_VA_TYPES_11                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10
#define HOOKSYS_VA_TARGS_11 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10
#define HOOKSYS_VA_ARGS_11 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10
#define HOOKSYS_VA_NAMES_11 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10
#define HOOKSYS_VA_LAST_11 a10

#define HOOKSYS_VA_TYPES_12                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10,  \
        typename A11
#define HOOKSYS_VA_TARGS_12 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11
#define HOOKSYS_VA_ARGS_12 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11
#define HOOKSYS_VA_NAMES_12 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11
#define HOOKSYS_VA_LAST_12 a11

#define HOOKSYS_VA_TYPES_13                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10,  \
        typename A11, typename A12
#define HOOKSYS_VA_TARGS_13 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12
#define HOOKSYS_VA_ARGS_13 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12
#define HOOKSYS_VA_NAMES_13 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12
#define HOOKSYS_VA_LAST_13 a12

#define HOOKSYS_VA_TYPES_14                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10,  \
        typename A11, typename A12, typename A13
#define HOOKSYS_VA_TARGS_14 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13
#define HOOKSYS_VA_ARGS_14 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13
#define HOOKSYS_VA_NAMES_14 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13
#define HOOKSYS_VA_LAST_14 a13

#define HOOKSYS_VA_TYPES_15                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10,  \
        typename A11, typename A12, typename A13, typename A14
#define HOOKSYS_VA_TARGS_15 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14
#define HOOKSYS_VA_ARGS_15 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14
#define HOOKSYS_VA_NAMES_15 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14
#define HOOKSYS_VA_LAST_15 a14

#define HOOKSYS_VA_TYPES_16                                                                                                                          \
    typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10,  \
        typename A11, typename A12, typename A13, typename A14, typename A15
#define HOOKSYS_VA_TARGS_16 A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15
#define HOOKSYS_VA_ARGS_16 A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15
#define HOOKSYS_VA_NAMES_16 a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15
#define HOOKSYS_VA_LAST_16 a15

#define HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(N)                                                                                                       \
    template <typename Self, typename ReturnT, HOOKSYS_VA_TYPES_##N> struct VarargHelper<Self, ReturnT, HOOKSYS_VA_TARGS_##N>                        \
    {                                                                                                                                                \
        using OriginalFn = ReturnT(__cdecl*)(HOOKSYS_VA_TARGS_##N, ...);                                                                             \
        static ReturnT __cdecl Detour(HOOKSYS_VA_ARGS_##N, ...)                                                                                      \
        {                                                                                                                                            \
            va_list __va;                                                                                                                            \
            va_start(__va, HOOKSYS_VA_LAST_##N);                                                                                                     \
            return HookSys::InvokeVarargsDetour<Self, ReturnT>(__va, HOOKSYS_VA_NAMES_##N);                                                          \
        }                                                                                                                                            \
    };

#define HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(N, CC)                                                                                                \
    template <HOOKSYS_VA_TYPES_##N> struct VarargHelper<HOOKSYS_VA_TARGS_##N>                                                                        \
    {                                                                                                                                                \
        using OriginalFn = ReturnT(CC*)(HOOKSYS_VA_TARGS_##N, ...);                                                                                  \
        static ReturnT CC Detour(HOOKSYS_VA_ARGS_##N, ...)                                                                                           \
        {                                                                                                                                            \
            va_list __va;                                                                                                                            \
            va_start(__va, HOOKSYS_VA_LAST_##N);                                                                                                     \
            return HookSys::InvokeVarargsDetour<Self, ReturnT>(__va, HOOKSYS_VA_NAMES_##N);                                                          \
        }                                                                                                                                            \
    };

#define HOOKSYS_DEFINE_VARARG_HELPERS_GLOBAL()                                                                                                       \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(1)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(2)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(3)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(4)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(5)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(6)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(7)                                                                                                           \
    HOOKSYS_DEFINE_VARARG_HELPER_GLOBAL(8)

#define HOOKSYS_DEFINE_VARARG_HELPERS_NESTED_CC(CC)                                                                                                  \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(1, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(2, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(3, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(4, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(5, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(6, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(7, CC)                                                                                                    \
    HOOKSYS_DEFINE_VARARG_HELPER_NESTED_CC(8, CC)

HOOKSYS_DEFINE_VARARG_HELPERS_GLOBAL()

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

    else if constexpr (std::is_invocable_v<Fn, Args...>)
    {
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

    else
    {
        static_assert(std::is_invocable_v<Fn, HookT&, Args...> || std::is_invocable_v<Fn, Args...>,
                      "Hook target is not invocable with either (hook, args...) or (args...) signature");
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

    else if constexpr (std::is_invocable_v<Method, InstanceT, Args...>)
    {
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

    else
    {
        static_assert(std::is_invocable_v<Method, InstanceT, HookT&, Args...> || std::is_invocable_v<Method, InstanceT, Args...>,
                      "Method hook target is not invocable with either (instance, hook, args...) or (instance, args...) signature");
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
    virtual void* GetOriginalRaw() const
    {
        return nullptr;
    }

    const std::string& DebugName() const
    {
        return m_debugName;
    }
    const std::string& AddressString() const
    {
        return m_addrString;
    }
    const std::string& ModuleName() const
    {
        return m_moduleName;
    }
    const std::string& ProcName() const
    {
        return m_procName;
    }
    AddressMode Mode() const
    {
        return m_addrMode;
    }
    uintptr_t AbsoluteAddress() const
    {
        return m_absoluteAddress;
    }

    void ConfigureDebugName(const char* debugName)
    {
        SetDebugName(debugName);
    }
    void ConfigureOffsetAddress(const char* addrString)
    {
        SetOffsetAddress(addrString);
    }
    void ConfigureAbsoluteAddress(uintptr_t addr)
    {
        SetAbsoluteAddress(addr);
    }
    void ConfigureProcAddress(const char* moduleName, const char* procName)
    {
        SetProcAddress(moduleName, procName);
    }

  protected:
    void SetDebugName(const char* debugName)
    {
        m_debugName = debugName ? debugName : "";
    }

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

class HookModule
{
  public:
    HookModule(const std::string& name, const std::string& filePath = {}) : m_name(name), m_filePath(filePath)
    {
    }

    const std::string& Name() const
    {
        return m_name;
    }

    const std::string& FilePath() const
    {
        return m_filePath;
    }

    void RegisterHook(LambdaHookBase* hook)
    {
        if (!hook)
            return;

        m_hooks[hook->DebugName()] = hook;
    }

    std::shared_ptr<LambdaHookBase> FindHook(const std::string& debugName) const
    {
        auto it = m_hooks.find(debugName);
        if (it == m_hooks.end())
            return nullptr;

        return std::shared_ptr<LambdaHookBase>(it->second, [](LambdaHookBase*) {});
    }

    bool DispatchHook(const std::string& debugName)
    {
        auto it = m_hooks.find(debugName);
        if (it == m_hooks.end())
            return false;

        return it->second->Dispatch();
    }

    void Dispatch()
    {
        for (auto& [name, hook] : m_hooks)
            hook->Dispatch();
    }

    void DispatchForModule(const char* moduleName)
    {
        if (!moduleName || !*moduleName)
            return;

        for (auto& [name, hook] : m_hooks)
        {
            switch (hook->Mode())
            {
            case AddressMode::OffsetString:
                if (!_stricmp(hook->ModuleName().c_str(), moduleName))
                    hook->Dispatch();
                break;

            case AddressMode::ProcAddress:
                if (!_stricmp(hook->ModuleName().c_str(), moduleName))
                    hook->Dispatch();
                break;

            default:
                break;
            }
        }
    }

  private:
    std::string m_name;
    std::string m_filePath;
    std::unordered_map<std::string, LambdaHookBase*> m_hooks;
};

inline std::vector<HookModule*>& GetHookModules()
{
    static std::vector<HookModule*> modules;
    return modules;
}

inline void RegisterHookModule(HookModule* module)
{
    if (!module)
        return;

    auto& modules = GetHookModules();
    if (std::find(modules.begin(), modules.end(), module) == modules.end())
        modules.push_back(module);
}

inline HookModule& GetOrCreateFileHookModule(const char* filePath, const char* moduleName = nullptr)
{
    static std::unordered_map<std::string, std::unique_ptr<HookModule>> modulesByFile;

    const std::string key = filePath ? filePath : "";
    const std::string effectiveName = moduleName && *moduleName ? moduleName : key;

    auto it = modulesByFile.find(key);
    if (it == modulesByFile.end())
    {
        auto module = std::make_unique<HookModule>(effectiveName, key);
        HookModule* modulePtr = module.get();
        modulesByFile.emplace(key, std::move(module));
        RegisterHookModule(modulePtr);
        return *modulePtr;
    }

    RegisterHookModule(it->second.get());
    return *it->second;
}

inline void RegisterHook(HookModule& module, LambdaHookBase* hook)
{
    if (!hook)
        return;

    module.RegisterHook(hook);
}

inline std::shared_ptr<LambdaHookBase> FindHook(const std::string& debugName)
{
    for (auto* module : GetHookModules())
    {
        auto hook = module->FindHook(debugName);
        if (hook)
            return hook;
    }

    return nullptr;
}

template <typename Fn> inline Fn GetOriginalFunction(const std::shared_ptr<LambdaHookBase>& hook)
{
    return hook ? reinterpret_cast<Fn>(hook->GetOriginalRaw()) : nullptr;
}

template <typename HookT> struct LambdaHookRegistrationOffset
{
    LambdaHookRegistrationOffset(HookModule& module, const char* debugName, const char* addrString)
    {
        HookT& hook = HookT::Instance();
        hook.ConfigureDebugName(debugName);
        hook.ConfigureOffsetAddress(addrString);
        RegisterHook(module, &hook);
    }
};

template <typename HookT> struct LambdaHookRegistrationAbsolute
{
    LambdaHookRegistrationAbsolute(HookModule& module, const char* debugName, uintptr_t addr)
    {
        HookT& hook = HookT::Instance();
        hook.ConfigureDebugName(debugName);
        hook.ConfigureAbsoluteAddress(addr);
        RegisterHook(module, &hook);
    }
};

template <typename HookT> struct LambdaHookRegistrationProc
{
    LambdaHookRegistrationProc(HookModule& module, const char* debugName, const char* moduleName, const char* procName)
    {
        HookT& hook = HookT::Instance();
        hook.ConfigureDebugName(debugName);
        hook.ConfigureProcAddress(moduleName, procName);
        RegisterHook(module, &hook);
    }
};
} // namespace HookSys

#define DECLARE_MODULE(name)                                                                                                                        \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline HookSys::HookModule& name = HookSys::GetOrCreateFileHookModule(__FILE__, __STR(name));                                                  \
    }

#define DISPATCH_MODULE(module) (module).Dispatch();

#define DISPATCH_HOOK(module, hookName) (module).DispatchHook(__STR(hookName));

// default calling convention for DECLARE_HOOK
#ifndef HOOKSYS_CALLCONV
#define HOOKSYS_CALLCONV __fastcall
#endif

// Lambda hook using an offset string (e.g. engine.dll + 0x123456)
#define DECLARE_HOOK_CC(debugName, addrString, callingConvention, lambda)                                                                            \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline constexpr char CONCAT2(__lambdaHookDebugName_, __LINE__)[] = __STR(debugName);                                                           \
    inline auto CONCAT2(__lambdaHookLambda_, __LINE__) = lambda;                                                                                     \
    template <const char* HookDebugName> struct CONCAT2(__lambdaHook_, __LINE__) : public HookSys::LambdaHookBase                                    \
    {                                                                                                                                                \
        using Self = CONCAT2(__lambdaHook_, __LINE__)<HookDebugName>;                                                                                \
        using LambdaT = std::decay_t<decltype(CONCAT2(__lambdaHookLambda_, __LINE__))>;                                                              \
        using Traits = HookSys::lambda_traits_for_hook<LambdaT>;                                                                                     \
        using ReturnT = typename Traits::return_type;                                                                                                \
        using FullArgs = typename Traits::args_tuple;                                                                                                \
        static_assert(std::tuple_size_v<FullArgs> >= 1, "Hook lambda must take hook ref as first arg");                                              \
        using ArgsTuple = typename HookSys::tuple_tail<FullArgs>::type;                                                                              \
        static constexpr bool kVarargs = Traits::is_variadic;                                                                                        \
        using FixedArgsTuple = ArgsTuple;                                                                                                            \
        template <typename... Args> struct Helper                                                                                                    \
        {                                                                                                                                            \
            using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                                 \
            static ReturnT callingConvention Detour(Args... args)                                                                                    \
            {                                                                                                                                        \
                if constexpr (std::is_void_v<ReturnT>)                                                                                               \
                    Self::Instance().Invoke(args...);                                                                                                \
                else                                                                                                                                 \
                    return Self::Instance().Invoke(args...);                                                                                         \
            }                                                                                                                                        \
        };                                                                                                                                           \
        template <typename... Args> struct VarargHelper                                                                                              \
        {                                                                                                                                            \
            static_assert(sizeof...(Args) > 0, "Varargs hooks require at least one fixed argument");                                                 \
            static_assert(sizeof...(Args) <= 8, "Varargs hooks support up to 8 fixed args");                                                         \
        };                                                                                                                                           \
        HOOKSYS_DEFINE_VARARG_HELPERS_NESTED_CC(callingConvention)                                                                                   \
        using HelperT = std::conditional_t<kVarargs, typename HookSys::apply_tuple<VarargHelper, FixedArgsTuple>::type,                              \
                                           typename HookSys::apply_tuple<Helper, ArgsTuple>::type>;                                                  \
        using OriginalFn = typename HelperT::OriginalFn;                                                                                             \
        inline static OriginalFn s_original = nullptr;                                                                                               \
        inline static thread_local void* s_returnAddress = nullptr;                                                                                  \
        inline static thread_local bool s_hasVaList = false;                                                                                         \
        inline static thread_local va_list s_vaList;                                                                                                 \
        inline static LambdaT s_lambda = CONCAT2(__lambdaHookLambda_, __LINE__);                                                                     \
        static Self& Instance()                                                                                                                      \
        {                                                                                                                                            \
            static Self inst;                                                                                                                        \
            return inst;                                                                                                                             \
        }                                                                                                                                            \
        void* ReturnAddress() const                                                                                                                  \
        {                                                                                                                                            \
            return s_returnAddress;                                                                                                                  \
        }                                                                                                                                            \
        bool HasVarArgs() const                                                                                                                      \
        {                                                                                                                                            \
            return s_hasVaList;                                                                                                                      \
        }                                                                                                                                            \
        va_list* VarArgs()                                                                                                                           \
        {                                                                                                                                            \
            assert(s_hasVaList && "VarArgs() called without active varargs");                                                                        \
            return &s_vaList;                                                                                                                        \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Invoke(Args... args)                                                                                     \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_lambda(Instance(), args...);                                                                                                       \
            else                                                                                                                                     \
                return s_lambda(Instance(), args...);                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT InvokeVarargs(Args... args)                                                                              \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_lambda(Instance(), args...);                                                                                                       \
            else                                                                                                                                     \
                return s_lambda(Instance(), args...);                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Original(Args... args)                                                                                   \
        {                                                                                                                                            \
            if (!s_original)                                                                                                                         \
            {                                                                                                                                        \
                spdlog::error("Original function for hook {} is null", DebugName());                                                                 \
                if constexpr (!std::is_void_v<ReturnT>)                                                                                              \
                    return ReturnT{};                                                                                                                \
                else                                                                                                                                 \
                    return;                                                                                                                          \
            }                                                                                                                                        \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_original(args...);                                                                                                                 \
            else                                                                                                                                     \
                return s_original(args...);                                                                                                          \
        }                                                                                                                                            \
        bool Dispatch() override                                                                                                                     \
        {                                                                                                                                            \
            const uintptr_t addr = ResolveAddress();                                                                                                 \
            if (!addr)                                                                                                                               \
            {                                                                                                                                        \
                spdlog::error("Address for hook {} is invalid", DebugName());                                                                        \
                return false;                                                                                                                        \
            }                                                                                                                                        \
            if (MH_CreateHook(reinterpret_cast<LPVOID>(addr), reinterpret_cast<LPVOID>(&HelperT::Detour), reinterpret_cast<LPVOID*>(&s_original)) == \
                MH_OK)                                                                                                                               \
            {                                                                                                                                        \
                if (MH_EnableHook(reinterpret_cast<LPVOID>(addr)) == MH_OK)                                                                          \
                {                                                                                                                                    \
                    spdlog::info("Enabling hook {}", DebugName());                                                                                   \
                    return true;                                                                                                                     \
                }                                                                                                                                    \
                spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                                  \
            }                                                                                                                                        \
            else                                                                                                                                     \
                spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                                  \
            return false;                                                                                                                            \
        }                                                                                                                                            \
        void* GetOriginalRaw() const override                                                                                                        \
        {                                                                                                                                            \
            return reinterpret_cast<void*>(s_original);                                                                                              \
        }                                                                                                                                            \
        static void OnModuleLoaded(CModule)                                                                                                          \
        {                                                                                                                                            \
            Instance().Dispatch();                                                                                                                   \
        }                                                                                                                                            \
    };                                                                                                                                               \
    HookSys::LambdaHookRegistrationOffset<                                                                                                           \
        CONCAT2(__lambdaHook_, __LINE__)<CONCAT2(__lambdaHookDebugName_, __LINE__)>> CONCAT2(__lambdaHookReg_, __LINE__)(                           \
        HookSys::GetOrCreateFileHookModule(__FILE__), CONCAT2(__lambdaHookDebugName_, __LINE__), __STR(addrString));                                \
    }

#define DECLARE_HOOK(debugName, addrString, lambda) DECLARE_HOOK_CC(debugName, addrString, HOOKSYS_CALLCONV, lambda)

// Hook using a regular free function (with or without hook ref as first arg)
#define DECLARE_HOOK_FN_CC(debugName, addrString, callingConvention, func)                                                                           \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline constexpr char CONCAT2(__funcHookDebugName_, __LINE__)[] = __STR(debugName);                                                             \
    template <const char* HookDebugName> struct CONCAT2(__funcHook_, __LINE__) : public HookSys::LambdaHookBase                                      \
    {                                                                                                                                                \
        using Self = CONCAT2(__funcHook_, __LINE__)<HookDebugName>;                                                                                  \
        using FnPtr = std::remove_reference_t<decltype(&func)>;                                                                                      \
        using Traits = HookSys::function_traits<FnPtr>;                                                                                              \
        using ReturnT = typename Traits::return_type;                                                                                                \
        using ArgsTuple = typename Traits::args_tuple;                                                                                                \
        static constexpr bool kVarargs = Traits::is_variadic;                                                                                        \
        using FixedArgsTuple = ArgsTuple;                                                                                                            \
        template <typename... Args> struct Helper                                                                                                    \
        {                                                                                                                                            \
            using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                                 \
            static ReturnT callingConvention Detour(Args... args)                                                                                    \
            {                                                                                                                                        \
                if constexpr (std::is_void_v<ReturnT>)                                                                                               \
                    Self::Instance().Invoke(args...);                                                                                                \
                else                                                                                                                                 \
                    return Self::Instance().Invoke(args...);                                                                                         \
            }                                                                                                                                        \
        };                                                                                                                                           \
        template <typename... Args> struct VarargHelper                                                                                              \
        {                                                                                                                                            \
            static_assert(sizeof...(Args) > 0, "Varargs hooks require at least one fixed argument");                                                 \
            static_assert(sizeof...(Args) <= 8, "Varargs hooks support up to 8 fixed args");                                                         \
        };                                                                                                                                           \
        HOOKSYS_DEFINE_VARARG_HELPERS_NESTED_CC(callingConvention)                                                                                   \
        using HelperT = std::conditional_t<kVarargs, typename HookSys::apply_tuple<VarargHelper, FixedArgsTuple>::type,                              \
                                           typename HookSys::apply_tuple<Helper, ArgsTuple>::type>;                                                  \
        using OriginalFn = typename HelperT::OriginalFn;                                                                                             \
        inline static OriginalFn s_original = nullptr;                                                                                               \
        inline static thread_local void* s_returnAddress = nullptr;                                                                                  \
        inline static thread_local bool s_hasVaList = false;                                                                                         \
        inline static thread_local va_list s_vaList;                                                                                                 \
        static Self& Instance()                                                                                                                      \
        {                                                                                                                                            \
            static Self inst;                                                                                                                        \
            return inst;                                                                                                                             \
        }                                                                                                                                            \
        void* ReturnAddress() const                                                                                                                  \
        {                                                                                                                                            \
            return s_returnAddress;                                                                                                                  \
        }                                                                                                                                            \
        bool HasVarArgs() const                                                                                                                      \
        {                                                                                                                                            \
            return s_hasVaList;                                                                                                                      \
        }                                                                                                                                            \
        va_list* VarArgs()                                                                                                                           \
        {                                                                                                                                            \
            assert(s_hasVaList && "VarArgs() called without active varargs");                                                                        \
            return &s_vaList;                                                                                                                        \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Invoke(Args... args)                                                                                     \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                func(args...);                                                                                                                       \
            else                                                                                                                                     \
                return func(args...);                                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT InvokeVarargs(Args... args)                                                                              \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                func(args...);                                                                                                                       \
            else                                                                                                                                     \
                return func(args...);                                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Original(Args... args)                                                                                   \
        {                                                                                                                                            \
            if (!s_original)                                                                                                                         \
            {                                                                                                                                        \
                spdlog::error("Original function for hook {} is null", DebugName());                                                                 \
                if constexpr (!std::is_void_v<ReturnT>)                                                                                              \
                    return ReturnT{};                                                                                                                \
                else                                                                                                                                 \
                    return;                                                                                                                          \
            }                                                                                                                                        \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_original(args...);                                                                                                                 \
            else                                                                                                                                     \
                return s_original(args...);                                                                                                          \
        }                                                                                                                                            \
        bool Dispatch() override                                                                                                                     \
        {                                                                                                                                            \
            const uintptr_t addr = ResolveAddress();                                                                                                 \
            if (!addr)                                                                                                                               \
            {                                                                                                                                        \
                spdlog::error("Address for hook {} is invalid", DebugName());                                                                        \
                return false;                                                                                                                        \
            }                                                                                                                                        \
            if (MH_CreateHook(reinterpret_cast<LPVOID>(addr), reinterpret_cast<LPVOID>(&HelperT::Detour), reinterpret_cast<LPVOID*>(&s_original)) == \
                MH_OK)                                                                                                                               \
            {                                                                                                                                        \
                if (MH_EnableHook(reinterpret_cast<LPVOID>(addr)) == MH_OK)                                                                          \
                {                                                                                                                                    \
                    spdlog::info("Enabling hook {}", DebugName());                                                                                   \
                    return true;                                                                                                                     \
                }                                                                                                                                    \
                spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                                  \
            }                                                                                                                                        \
            else                                                                                                                                     \
                spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                                  \
            return false;                                                                                                                            \
        }                                                                                                                                            \
        void* GetOriginalRaw() const override                                                                                                        \
        {                                                                                                                                            \
            return reinterpret_cast<void*>(s_original);                                                                                              \
        }                                                                                                                                            \
        static void OnModuleLoaded(CModule)                                                                                                          \
        {                                                                                                                                            \
            Instance().Dispatch();                                                                                                                   \
        }                                                                                                                                            \
    };                                                                                                                                               \
    HookSys::LambdaHookRegistrationOffset<                                                                                                           \
        CONCAT2(__funcHook_, __LINE__)<CONCAT2(__funcHookDebugName_, __LINE__)>> CONCAT2(__funcHookReg_, __LINE__)(                                 \
        HookSys::GetOrCreateFileHookModule(__FILE__), CONCAT2(__funcHookDebugName_, __LINE__), __STR(addrString));                                  \
    }

#define DECLARE_HOOK_FN(debugName, addrString, func) DECLARE_HOOK_FN_CC(debugName, addrString, HOOKSYS_CALLCONV, func)

// Lambda hook using an absolute address
#define DECLARE_HOOK_ABSOLUTE_CC(debugName, addr, callingConvention, lambda)                                                                         \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline constexpr char CONCAT2(__lambdaHookAbsDebugName_, __LINE__)[] = __STR(debugName);                                                        \
    inline auto CONCAT2(__lambdaHookAbsLambda_, __LINE__) = lambda;                                                                                  \
    template <const char* HookDebugName> struct CONCAT2(__lambdaHookAbs_, __LINE__) : public HookSys::LambdaHookBase                                 \
    {                                                                                                                                                \
        using Self = CONCAT2(__lambdaHookAbs_, __LINE__)<HookDebugName>;                                                                             \
        using LambdaT = std::decay_t<decltype(CONCAT2(__lambdaHookAbsLambda_, __LINE__))>;                                                           \
        using Traits = HookSys::lambda_traits_for_hook<LambdaT>;                                                                                     \
        using ReturnT = typename Traits::return_type;                                                                                                \
        using FullArgs = typename Traits::args_tuple;                                                                                                \
        static_assert(std::tuple_size_v<FullArgs> >= 1, "Hook lambda must take hook ref as first arg");                                              \
        using ArgsTuple = typename HookSys::tuple_tail<FullArgs>::type;                                                                              \
        template <typename... Args> struct Helper                                                                                                    \
        {                                                                                                                                            \
            using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                                 \
            static ReturnT callingConvention Detour(Args... args)                                                                                    \
            {                                                                                                                                        \
                Self::s_returnAddress = _ReturnAddress();                                                                                            \
                if constexpr (std::is_void_v<ReturnT>)                                                                                               \
                    Self::Instance().Invoke(args...);                                                                                                \
                else                                                                                                                                 \
                    return Self::Instance().Invoke(args...);                                                                                         \
            }                                                                                                                                        \
        };                                                                                                                                           \
        using HelperT = typename HookSys::apply_tuple<Helper, ArgsTuple>::type;                                                                      \
        using OriginalFn = typename HelperT::OriginalFn;                                                                                             \
        inline static OriginalFn s_original = nullptr;                                                                                               \
        inline static thread_local void* s_returnAddress = nullptr;                                                                                  \
        inline static LambdaT s_lambda = CONCAT2(__lambdaHookAbsLambda_, __LINE__);                                                                  \
        static Self& Instance()                                                                                                                      \
        {                                                                                                                                            \
            static Self inst;                                                                                                                        \
            return inst;                                                                                                                             \
        }                                                                                                                                            \
        void* ReturnAddress() const                                                                                                                  \
        {                                                                                                                                            \
            return s_returnAddress;                                                                                                                  \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Invoke(Args... args)                                                                                     \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_lambda(Instance(), args...);                                                                                                       \
            else                                                                                                                                     \
                return s_lambda(Instance(), args...);                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Original(Args... args)                                                                                   \
        {                                                                                                                                            \
            if (!s_original)                                                                                                                         \
            {                                                                                                                                        \
                spdlog::error("Original function for hook {} is null", DebugName());                                                                 \
                if constexpr (!std::is_void_v<ReturnT>)                                                                                              \
                    return ReturnT{};                                                                                                                \
                else                                                                                                                                 \
                    return;                                                                                                                          \
            }                                                                                                                                        \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_original(args...);                                                                                                                 \
            else                                                                                                                                     \
                return s_original(args...);                                                                                                          \
        }                                                                                                                                            \
        bool Dispatch() override                                                                                                                     \
        {                                                                                                                                            \
            const uintptr_t addrResolved = ResolveAddress();                                                                                         \
            if (!addrResolved)                                                                                                                       \
            {                                                                                                                                        \
                spdlog::error("Address for hook {} is invalid", DebugName());                                                                        \
                return false;                                                                                                                        \
            }                                                                                                                                        \
            if (MH_CreateHook(reinterpret_cast<LPVOID>(addrResolved), reinterpret_cast<LPVOID>(&HelperT::Detour),                                    \
                              reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                      \
            {                                                                                                                                        \
                if (MH_EnableHook(reinterpret_cast<LPVOID>(addrResolved)) == MH_OK)                                                                  \
                {                                                                                                                                    \
                    spdlog::info("Enabling hook {}", DebugName());                                                                                   \
                    return true;                                                                                                                     \
                }                                                                                                                                    \
                spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                                  \
            }                                                                                                                                        \
            else                                                                                                                                     \
                spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                                  \
            return false;                                                                                                                            \
        }                                                                                                                                            \
        void* GetOriginalRaw() const override                                                                                                        \
        {                                                                                                                                            \
            return reinterpret_cast<void*>(s_original);                                                                                              \
        }                                                                                                                                            \
        static void OnModuleLoaded(CModule)                                                                                                          \
        {                                                                                                                                            \
            Instance().Dispatch();                                                                                                                   \
        }                                                                                                                                            \
    };                                                                                                                                               \
    HookSys::LambdaHookRegistrationAbsolute<                                                                                                         \
        CONCAT2(__lambdaHookAbs_, __LINE__)<CONCAT2(__lambdaHookAbsDebugName_, __LINE__)>> CONCAT2(__lambdaHookAbsReg_, __LINE__)(                  \
        HookSys::GetOrCreateFileHookModule(__FILE__), CONCAT2(__lambdaHookAbsDebugName_, __LINE__), static_cast<uintptr_t>(addr));                   \
    }

#define DECLARE_HOOK_ABSOLUTE(debugName, addr, lambda) DECLARE_HOOK_ABSOLUTE_CC(debugName, addr, HOOKSYS_CALLCONV, lambda)

// Hook using a regular free function at an absolute address
#define DECLARE_HOOK_ABSOLUTE_FN_CC(debugName, addr, callingConvention, func)                                                                        \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline constexpr char CONCAT2(__funcAbsHookDebugName_, __LINE__)[] = __STR(debugName);                                                          \
    template <const char* HookDebugName> struct CONCAT2(__funcAbsHook_, __LINE__) : public HookSys::LambdaHookBase                                   \
    {                                                                                                                                                \
        using Self = CONCAT2(__funcAbsHook_, __LINE__)<HookDebugName>;                                                                               \
        using FnPtr = std::remove_reference_t<decltype(&func)>;                                                                                      \
        using Traits = HookSys::function_traits<FnPtr>;                                                                                              \
        using ReturnT = typename Traits::return_type;                                                                                                \
        using ArgsTuple = typename Traits::args_tuple;                                                                                                \
        static constexpr bool kVarargs = Traits::is_variadic;                                                                                        \
        using FixedArgsTuple = ArgsTuple;                                                                                                            \
        template <typename... Args> struct Helper                                                                                                    \
        {                                                                                                                                            \
            using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                                 \
            static ReturnT callingConvention Detour(Args... args)                                                                                    \
            {                                                                                                                                        \
                if constexpr (std::is_void_v<ReturnT>)                                                                                               \
                    Self::Instance().Invoke(args...);                                                                                                \
                else                                                                                                                                 \
                    return Self::Instance().Invoke(args...);                                                                                         \
            }                                                                                                                                        \
        };                                                                                                                                           \
        template <typename... Args> struct VarargHelper                                                                                              \
        {                                                                                                                                            \
            static_assert(sizeof...(Args) > 0, "Varargs hooks require at least one fixed argument");                                                 \
            static_assert(sizeof...(Args) <= 8, "Varargs hooks support up to 8 fixed args");                                                         \
        };                                                                                                                                           \
        HOOKSYS_DEFINE_VARARG_HELPERS_NESTED_CC(callingConvention)                                                                                   \
        using HelperT = std::conditional_t<kVarargs, typename HookSys::apply_tuple<VarargHelper, FixedArgsTuple>::type,                              \
                                           typename HookSys::apply_tuple<Helper, ArgsTuple>::type>;                                                  \
        using OriginalFn = typename HelperT::OriginalFn;                                                                                             \
        inline static OriginalFn s_original = nullptr;                                                                                               \
        inline static thread_local void* s_returnAddress = nullptr;                                                                                  \
        inline static thread_local bool s_hasVaList = false;                                                                                         \
        inline static thread_local va_list s_vaList;                                                                                                 \
        static Self& Instance()                                                                                                                      \
        {                                                                                                                                            \
            static Self inst;                                                                                                                        \
            return inst;                                                                                                                             \
        }                                                                                                                                            \
        void* ReturnAddress() const                                                                                                                  \
        {                                                                                                                                            \
            return s_returnAddress;                                                                                                                  \
        }                                                                                                                                            \
        bool HasVarArgs() const                                                                                                                      \
        {                                                                                                                                            \
            return s_hasVaList;                                                                                                                      \
        }                                                                                                                                            \
        va_list* VarArgs()                                                                                                                           \
        {                                                                                                                                            \
            assert(s_hasVaList && "VarArgs() called without active varargs");                                                                        \
            return &s_vaList;                                                                                                                        \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Invoke(Args... args)                                                                                     \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                func(args...);                                                                                                                       \
            else                                                                                                                                     \
                return func(args...);                                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT InvokeVarargs(Args... args)                                                                              \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                func(args...);                                                                                                                       \
            else                                                                                                                                     \
                return func(args...);                                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Original(Args... args)                                                                                   \
        {                                                                                                                                            \
            if (!s_original)                                                                                                                         \
            {                                                                                                                                        \
                spdlog::error("Original function for hook {} is null", DebugName());                                                                 \
                if constexpr (!std::is_void_v<ReturnT>)                                                                                              \
                    return ReturnT{};                                                                                                                \
                else                                                                                                                                 \
                    return;                                                                                                                          \
            }                                                                                                                                        \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_original(args...);                                                                                                                 \
            else                                                                                                                                     \
                return s_original(args...);                                                                                                          \
        }                                                                                                                                            \
        bool Dispatch() override                                                                                                                     \
        {                                                                                                                                            \
            const uintptr_t addrResolved = ResolveAddress();                                                                                         \
            if (!addrResolved)                                                                                                                       \
            {                                                                                                                                        \
                spdlog::error("Address for hook {} is invalid", DebugName());                                                                        \
                return false;                                                                                                                        \
            }                                                                                                                                        \
            if (MH_CreateHook(reinterpret_cast<LPVOID>(addrResolved), reinterpret_cast<LPVOID>(&HelperT::Detour),                                    \
                              reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                      \
            {                                                                                                                                        \
                if (MH_EnableHook(reinterpret_cast<LPVOID>(addrResolved)) == MH_OK)                                                                  \
                {                                                                                                                                    \
                    spdlog::info("Enabling hook {}", DebugName());                                                                                   \
                    return true;                                                                                                                     \
                }                                                                                                                                    \
                spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                                  \
            }                                                                                                                                        \
            else                                                                                                                                     \
                spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                                  \
            return false;                                                                                                                            \
        }                                                                                                                                            \
        void* GetOriginalRaw() const override                                                                                                        \
        {                                                                                                                                            \
            return reinterpret_cast<void*>(s_original);                                                                                              \
        }                                                                                                                                            \
        static void OnModuleLoaded(CModule)                                                                                                          \
        {                                                                                                                                            \
            Instance().Dispatch();                                                                                                                   \
        }                                                                                                                                            \
    };                                                                                                                                               \
    HookSys::LambdaHookRegistrationAbsolute<                                                                                                         \
        CONCAT2(__funcAbsHook_, __LINE__)<CONCAT2(__funcAbsHookDebugName_, __LINE__)>> CONCAT2(__funcAbsHookReg_, __LINE__)(                        \
        HookSys::GetOrCreateFileHookModule(__FILE__), CONCAT2(__funcAbsHookDebugName_, __LINE__), static_cast<uintptr_t>(addr));                     \
    }

#define DECLARE_HOOK_ABSOLUTE_FN(debugName, addr, func) DECLARE_HOOK_ABSOLUTE_FN_CC(debugName, addr, HOOKSYS_CALLCONV, func)

// Lambda hook using GetProcAddress
#define DECLARE_HOOK_PROC_CC(debugName, moduleName, procName, callingConvention, lambda)                                                             \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline constexpr char CONCAT2(__lambdaHookProcDebugName_, __LINE__)[] = __STR(debugName);                                                       \
    inline auto CONCAT2(__lambdaHookProcLambda_, __LINE__) = lambda;                                                                                 \
    template <const char* HookDebugName> struct CONCAT2(__lambdaHookProc_, __LINE__) : public HookSys::LambdaHookBase                                \
    {                                                                                                                                                \
        using Self = CONCAT2(__lambdaHookProc_, __LINE__)<HookDebugName>;                                                                            \
        using LambdaT = std::decay_t<decltype(CONCAT2(__lambdaHookProcLambda_, __LINE__))>;                                                          \
        using Traits = HookSys::lambda_traits_for_hook<LambdaT>;                                                                                     \
        using ReturnT = typename Traits::return_type;                                                                                                \
        using FullArgs = typename Traits::args_tuple;                                                                                                \
        static_assert(std::tuple_size_v<FullArgs> >= 1, "Hook lambda must take hook ref as first arg");                                              \
        using ArgsTuple = typename HookSys::tuple_tail<FullArgs>::type;                                                                              \
        static constexpr bool kVarargs = Traits::is_variadic;                                                                                        \
        using FixedArgsTuple = ArgsTuple;                                                                                                            \
        template <typename... Args> struct Helper                                                                                                    \
        {                                                                                                                                            \
            using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                                 \
            static ReturnT callingConvention Detour(Args... args)                                                                                    \
            {                                                                                                                                        \
                Self::s_returnAddress = _ReturnAddress();                                                                                            \
                if constexpr (std::is_void_v<ReturnT>)                                                                                               \
                    Self::Instance().Invoke(args...);                                                                                                \
                else                                                                                                                                 \
                    return Self::Instance().Invoke(args...);                                                                                         \
            }                                                                                                                                        \
        };                                                                                                                                           \
        template <typename... Args> struct VarargHelper                                                                                              \
        {                                                                                                                                            \
            static_assert(sizeof...(Args) > 0, "Varargs hooks require at least one fixed argument");                                                 \
            static_assert(sizeof...(Args) <= 8, "Varargs hooks support up to 8 fixed args");                                                         \
        };                                                                                                                                           \
        HOOKSYS_DEFINE_VARARG_HELPERS_NESTED_CC(callingConvention)                                                                                   \
        using HelperT = std::conditional_t<kVarargs, typename HookSys::apply_tuple<VarargHelper, FixedArgsTuple>::type,                              \
                                           typename HookSys::apply_tuple<Helper, ArgsTuple>::type>;                                                  \
        using OriginalFn = typename HelperT::OriginalFn;                                                                                             \
        inline static OriginalFn s_original = nullptr;                                                                                               \
        inline static thread_local void* s_returnAddress = nullptr;                                                                                  \
        inline static thread_local bool s_hasVaList = false;                                                                                         \
        inline static thread_local va_list s_vaList;                                                                                                 \
        inline static LambdaT s_lambda = CONCAT2(__lambdaHookProcLambda_, __LINE__);                                                                 \
        static Self& Instance()                                                                                                                      \
        {                                                                                                                                            \
            static Self inst;                                                                                                                        \
            return inst;                                                                                                                             \
        }                                                                                                                                            \
        void* ReturnAddress() const                                                                                                                  \
        {                                                                                                                                            \
            return s_returnAddress;                                                                                                                  \
        }                                                                                                                                            \
        bool HasVarArgs() const                                                                                                                      \
        {                                                                                                                                            \
            return s_hasVaList;                                                                                                                      \
        }                                                                                                                                            \
        va_list* VarArgs()                                                                                                                           \
        {                                                                                                                                            \
            assert(s_hasVaList && "VarArgs() called without active varargs");                                                                        \
            return &s_vaList;                                                                                                                        \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Invoke(Args... args)                                                                                     \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_lambda(Instance(), args...);                                                                                                       \
            else                                                                                                                                     \
                return s_lambda(Instance(), args...);                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT InvokeVarargs(Args... args)                                                                              \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_lambda(Instance(), args...);                                                                                                       \
            else                                                                                                                                     \
                return s_lambda(Instance(), args...);                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Original(Args... args)                                                                                   \
        {                                                                                                                                            \
            if (!s_original)                                                                                                                         \
            {                                                                                                                                        \
                spdlog::error("Original function for hook {} is null", DebugName());                                                                 \
                if constexpr (!std::is_void_v<ReturnT>)                                                                                              \
                    return ReturnT{};                                                                                                                \
                else                                                                                                                                 \
                    return;                                                                                                                          \
            }                                                                                                                                        \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_original(args...);                                                                                                                 \
            else                                                                                                                                     \
                return s_original(args...);                                                                                                          \
        }                                                                                                                                            \
        bool Dispatch() override                                                                                                                     \
        {                                                                                                                                            \
            const uintptr_t addrResolved = ResolveAddress();                                                                                         \
            if (!addrResolved)                                                                                                                       \
            {                                                                                                                                        \
                spdlog::error("Address for hook {} is invalid", DebugName());                                                                        \
                return false;                                                                                                                        \
            }                                                                                                                                        \
            if (MH_CreateHook(reinterpret_cast<LPVOID>(addrResolved), reinterpret_cast<LPVOID>(&HelperT::Detour),                                    \
                              reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                      \
            {                                                                                                                                        \
                if (MH_EnableHook(reinterpret_cast<LPVOID>(addrResolved)) == MH_OK)                                                                  \
                {                                                                                                                                    \
                    spdlog::info("Enabling hook {}", DebugName());                                                                                   \
                    return true;                                                                                                                     \
                }                                                                                                                                    \
                spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                                  \
            }                                                                                                                                        \
            else                                                                                                                                     \
                spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                                  \
            return false;                                                                                                                            \
        }                                                                                                                                            \
        void* GetOriginalRaw() const override                                                                                                        \
        {                                                                                                                                            \
            return reinterpret_cast<void*>(s_original);                                                                                              \
        }                                                                                                                                            \
        static void OnModuleLoaded(CModule)                                                                                                          \
        {                                                                                                                                            \
            Instance().Dispatch();                                                                                                                   \
        }                                                                                                                                            \
    };                                                                                                                                               \
    HookSys::LambdaHookRegistrationProc<                                                                                                             \
        CONCAT2(__lambdaHookProc_, __LINE__)<CONCAT2(__lambdaHookProcDebugName_, __LINE__)>> CONCAT2(__lambdaHookProcReg_, __LINE__)(               \
        HookSys::GetOrCreateFileHookModule(__FILE__), CONCAT2(__lambdaHookProcDebugName_, __LINE__), __STR(moduleName), __STR(procName));            \
    }

#define DECLARE_HOOK_PROC(debugName, moduleName, procName, lambda) DECLARE_HOOK_PROC_CC(debugName, moduleName, procName, HOOKSYS_CALLCONV, lambda)

// Hook using a regular free function resolved via GetProcAddress
#define DECLARE_HOOK_PROC_FN_CC(debugName, moduleName, procName, callingConvention, func)                                                            \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    inline constexpr char CONCAT2(__funcProcHookDebugName_, __LINE__)[] = __STR(debugName);                                                         \
    template <const char* HookDebugName> struct CONCAT2(__funcProcHook_, __LINE__) : public HookSys::LambdaHookBase                                  \
    {                                                                                                                                                \
        using Self = CONCAT2(__funcProcHook_, __LINE__)<HookDebugName>;                                                                              \
        using FnPtr = std::remove_reference_t<decltype(&func)>;                                                                                      \
        using Traits = HookSys::function_traits<FnPtr>;                                                                                              \
        using ReturnT = typename Traits::return_type;                                                                                                \
        using ArgsTuple = typename Traits::args_tuple;                                                                                                \
        static constexpr bool kVarargs = Traits::is_variadic;                                                                                        \
        using FixedArgsTuple = ArgsTuple;                                                                                                            \
        template <typename... Args> struct Helper                                                                                                    \
        {                                                                                                                                            \
            using OriginalFn = ReturnT(callingConvention*)(Args...);                                                                                 \
            static ReturnT callingConvention Detour(Args... args)                                                                                    \
            {                                                                                                                                        \
                if constexpr (std::is_void_v<ReturnT>)                                                                                               \
                    Self::Instance().Invoke(args...);                                                                                                \
                else                                                                                                                                 \
                    return Self::Instance().Invoke(args...);                                                                                         \
            }                                                                                                                                        \
        };                                                                                                                                           \
        template <typename... Args> struct VarargHelper                                                                                              \
        {                                                                                                                                            \
            static_assert(sizeof...(Args) > 0, "Varargs hooks require at least one fixed argument");                                                 \
            static_assert(sizeof...(Args) <= 8, "Varargs hooks support up to 8 fixed args");                                                         \
        };                                                                                                                                           \
        HOOKSYS_DEFINE_VARARG_HELPERS_NESTED_CC(callingConvention)                                                                                   \
        using HelperT = std::conditional_t<kVarargs, typename HookSys::apply_tuple<VarargHelper, FixedArgsTuple>::type,                              \
                                           typename HookSys::apply_tuple<Helper, ArgsTuple>::type>;                                                  \
        using OriginalFn = typename HelperT::OriginalFn;                                                                                             \
        inline static OriginalFn s_original = nullptr;                                                                                               \
        inline static thread_local void* s_returnAddress = nullptr;                                                                                  \
        inline static thread_local bool s_hasVaList = false;                                                                                         \
        inline static thread_local va_list s_vaList;                                                                                                 \
        static Self& Instance()                                                                                                                      \
        {                                                                                                                                            \
            static Self inst;                                                                                                                        \
            return inst;                                                                                                                             \
        }                                                                                                                                            \
        void* ReturnAddress() const                                                                                                                  \
        {                                                                                                                                            \
            return s_returnAddress;                                                                                                                  \
        }                                                                                                                                            \
        bool HasVarArgs() const                                                                                                                      \
        {                                                                                                                                            \
            return s_hasVaList;                                                                                                                      \
        }                                                                                                                                            \
        va_list* VarArgs()                                                                                                                           \
        {                                                                                                                                            \
            assert(s_hasVaList && "VarArgs() called without active varargs");                                                                        \
            return &s_vaList;                                                                                                                        \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Invoke(Args... args)                                                                                     \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                func(args...);                                                                                                                       \
            else                                                                                                                                     \
                return func(args...);                                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT InvokeVarargs(Args... args)                                                                              \
        {                                                                                                                                            \
            HookSys::HookInvocationScope<HookDebugName> hookInvocationScope;                                                                         \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                func(args...);                                                                                                                       \
            else                                                                                                                                     \
                return func(args...);                                                                                                                \
        }                                                                                                                                            \
        template <typename... Args> ReturnT Original(Args... args)                                                                                   \
        {                                                                                                                                            \
            if (!s_original)                                                                                                                         \
            {                                                                                                                                        \
                spdlog::error("Original function for hook {} is null", DebugName());                                                                 \
                if constexpr (!std::is_void_v<ReturnT>)                                                                                              \
                    return ReturnT{};                                                                                                                \
                else                                                                                                                                 \
                    return;                                                                                                                          \
            }                                                                                                                                        \
            if constexpr (std::is_void_v<ReturnT>)                                                                                                   \
                s_original(args...);                                                                                                                 \
            else                                                                                                                                     \
                return s_original(args...);                                                                                                          \
        }                                                                                                                                            \
        bool Dispatch() override                                                                                                                     \
        {                                                                                                                                            \
            const uintptr_t addrResolved = ResolveAddress();                                                                                         \
            if (!addrResolved)                                                                                                                       \
            {                                                                                                                                        \
                spdlog::error("Address for hook {} is invalid", DebugName());                                                                        \
                return false;                                                                                                                        \
            }                                                                                                                                        \
            if (MH_CreateHook(reinterpret_cast<LPVOID>(addrResolved), reinterpret_cast<LPVOID>(&HelperT::Detour),                                    \
                              reinterpret_cast<LPVOID*>(&s_original)) == MH_OK)                                                                      \
            {                                                                                                                                        \
                if (MH_EnableHook(reinterpret_cast<LPVOID>(addrResolved)) == MH_OK)                                                                  \
                {                                                                                                                                    \
                    spdlog::info("Enabling hook {}", DebugName());                                                                                   \
                    return true;                                                                                                                     \
                }                                                                                                                                    \
                spdlog::error("MH_EnableHook failed for function {}", DebugName());                                                                  \
            }                                                                                                                                        \
            else                                                                                                                                     \
                spdlog::error("MH_CreateHook failed for function {}", DebugName());                                                                  \
            return false;                                                                                                                            \
        }                                                                                                                                            \
        void* GetOriginalRaw() const override                                                                                                        \
        {                                                                                                                                            \
            return reinterpret_cast<void*>(s_original);                                                                                              \
        }                                                                                                                                            \
        static void OnModuleLoaded(CModule)                                                                                                          \
        {                                                                                                                                            \
            Instance().Dispatch();                                                                                                                   \
        }                                                                                                                                            \
    };                                                                                                                                               \
    HookSys::LambdaHookRegistrationProc<                                                                                                             \
        CONCAT2(__funcProcHook_, __LINE__)<CONCAT2(__funcProcHookDebugName_, __LINE__)>> CONCAT2(__funcProcHookReg_, __LINE__)(                     \
        HookSys::GetOrCreateFileHookModule(__FILE__), CONCAT2(__funcProcHookDebugName_, __LINE__), __STR(moduleName), __STR(procName));              \
    }

#define DECLARE_HOOK_PROC_FN(debugName, moduleName, procName, func) DECLARE_HOOK_PROC_FN_CC(debugName, moduleName, procName, HOOKSYS_CALLCONV, func)

// Hook using a non-static member function. instanceExpr should be a pointer or reference to the instance.
#define DECLARE_HOOK_METHOD_CC(debugName, addrString, callingConvention, methodPtr, instanceExpr)                                                    \
    DECLARE_HOOK_CC(debugName, addrString, callingConvention, [](auto& hook, auto... args) -> decltype(auto)                                         \
    {                                                                                                                                                \
        if constexpr (std::is_void_v<decltype(HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...))>)                                   \
        {                                                                                                                                            \
            HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                                       \
            return;                                                                                                                                  \
        }                                                                                                                                            \
        else                                                                                                                                         \
        {                                                                                                                                            \
            return HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                                \
        }                                                                                                                                            \
    })

#define DECLARE_HOOK_METHOD(debugName, addrString, methodPtr, instanceExpr)                                                                          \
    DECLARE_HOOK_METHOD_CC(debugName, addrString, HOOKSYS_CALLCONV, methodPtr, instanceExpr)

#define DECLARE_HOOK_ABSOLUTE_METHOD_CC(debugName, addr, callingConvention, methodPtr, instanceExpr)                                                 \
    DECLARE_HOOK_ABSOLUTE_CC(debugName, addr, callingConvention, [](auto& hook, auto... args) -> decltype(auto)                                      \
    {                                                                                                                                                \
        if constexpr (std::is_void_v<decltype(HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...))>)                                   \
        {                                                                                                                                            \
            HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                                       \
            return;                                                                                                                                  \
        }                                                                                                                                            \
        else                                                                                                                                         \
        {                                                                                                                                            \
            return HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                                \
        }                                                                                                                                            \
    })

#define DECLARE_HOOK_ABSOLUTE_METHOD(debugName, addr, methodPtr, instanceExpr)                                                                       \
    DECLARE_HOOK_ABSOLUTE_METHOD_CC(debugName, addr, HOOKSYS_CALLCONV, methodPtr, instanceExpr)

#define DECLARE_HOOK_PROC_METHOD_CC(debugName, moduleName, procName, callingConvention, methodPtr, instanceExpr)                                     \
    DECLARE_HOOK_PROC_CC(debugName, moduleName, procName, callingConvention, [](auto& hook, auto... args) -> decltype(auto)                          \
    {                                                                                                                                                \
        if constexpr (std::is_void_v<decltype(HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...))>)                                   \
        {                                                                                                                                            \
            HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                                       \
            return;                                                                                                                                  \
        }                                                                                                                                            \
        else                                                                                                                                         \
        {                                                                                                                                            \
            return HookSys::InvokeMethodHook(methodPtr, instanceExpr, hook, args...);                                                                \
        }                                                                                                                                            \
    })

#define DECLARE_HOOK_PROC_METHOD(debugName, moduleName, procName, methodPtr, instanceExpr)                                                           \
    DECLARE_HOOK_PROC_METHOD_CC(debugName, moduleName, procName, HOOKSYS_CALLCONV, methodPtr, instanceExpr)
