#pragma once
#include "appframework/IAppSystem.h"
#include "tier1/convar.h"

#include <cstddef>
#include <string>
#include <unordered_map>

inline constexpr char CVAR_INTERFACE_VERSION[] = "VEngineCvar007";

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IConsoleDisplayFunc;
class Color;
class ICvarQuery;

using CVarDLLIdentifier_t = int;
//-----------------------------------------------------------------------------
// Internals for ICVarIterator
//-----------------------------------------------------------------------------
class CCVarIteratorInternal
{
public:
	virtual void SetFirst() = 0; // 0
	virtual void Next() = 0; // 1
	virtual bool IsValid() = 0; // 2
	virtual ConCommandBase* Get() = 0; // 3
	virtual ~CCVarIteratorInternal() = default; // 4
};

//-----------------------------------------------------------------------------
// Default implementation
//-----------------------------------------------------------------------------
class CCvar : public IAppSystem
{
public:
	virtual CVarDLLIdentifier_t AllocateDLLIdentifier() = 0; // 8
	virtual void RegisterConCommand(ConCommandBase* command) = 0; // 9
	virtual void UnregisterConCommand(ConCommandBase* command) = 0; // 10
	virtual void UnregisterConCommands(CVarDLLIdentifier_t identifier) = 0; // 11
	virtual const char* GetCommandLineValue(const char* name) = 0; // 12
	virtual ConCommandBase* FindCommandBase(const char* name) = 0; // 13
	virtual const ConCommandBase* FindCommandBase(const char* name) const = 0; // 14
	virtual ConVar* FindVar(const char* name) = 0; // 15
	virtual const ConVar* FindVar(const char* name) const = 0; // 16
	virtual ConCommand* FindCommand(const char* name) = 0; // 17
	virtual const ConCommand* FindCommand(const char* name) const = 0; // 18
	virtual void PrintConCommandDescriptionsExcept(
		const char* const* exclusions, int exclusionCount) = 0; // 19
	virtual void RevertConVarsExcept(const char* const* exclusions, int exclusionCount) = 0; // 20
	virtual void InstallGlobalChangeCallback(FnChangeCallback_t callback) = 0; // 21
	virtual void RemoveGlobalChangeCallback(FnChangeCallback_t callback) = 0; // 22
	virtual void CallGlobalChangeCallbacks(ConVar* var, const char* oldValue) = 0; // 23
	virtual void InstallConsoleDisplayFunc(IConsoleDisplayFunc* display) = 0; // 24
	virtual void RemoveConsoleDisplayFunc(IConsoleDisplayFunc* display) = 0; // 25
	virtual void ConsoleColorPrintf(const Color& color, const char* format, ...) const = 0; // 26
	virtual void ConsolePrintf(const char* format, ...) const = 0; // 27
	virtual void ConsoleDPrintf(const char* format, ...) const = 0; // 28
	virtual void RevertFlaggedConVars(int flags) = 0; // 29
	virtual void InstallCVarQuery(ICvarQuery* query) = 0; // 30
	virtual void SetMaxSplitScreenSlots(int slots) = 0; // 31
	virtual int GetMaxSplitScreenSlots() const = 0; // 32
	virtual int GetConsoleDisplayFuncCount() const = 0; // 33
	virtual void GetConsoleText(int displayIndex, char* text, std::size_t textSize) const = 0; // 34
	virtual bool IsMaterialThreadSetAllowed() const = 0; // 35
	virtual void QueueMaterialThreadSetValue(ConVar* var, float value) = 0; // 36
	virtual void QueueMaterialThreadSetValue(ConVar* var, int value) = 0; // 37
	virtual void QueueMaterialThreadSetValue(ConVar* var, const char* value) = 0; // 38
	virtual bool HasQueuedMaterialThreadConVarSets() const = 0; // 39
	virtual int ProcessQueuedMaterialThreadConVarSets() = 0; // 40
	virtual CCVarIteratorInternal* FactoryInternalIterator() = 0; // 41

	std::unordered_map<std::string, ConCommandBase*> DumpToMap();
};

extern CCvar* g_pCVar;
