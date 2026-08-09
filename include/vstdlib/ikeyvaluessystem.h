#pragma once

#include <cstddef>

using HKeySymbol = int;
inline constexpr HKeySymbol INVALID_KEY_SYMBOL = -1;

class IKeyValuesSystem
{
public:
	virtual void RegisterSizeofKeyValues(std::ptrdiff_t size) = 0; // 0
	virtual void* AllocKeyValuesMemory(std::ptrdiff_t size) = 0; // 1
	virtual void FreeKeyValuesMemory(void* memory) = 0; // 2
	virtual HKeySymbol GetSymbolForString(const char* name, bool create = true) = 0; // 3
	virtual const char* GetStringForSymbol(HKeySymbol symbol) = 0; // 4
	virtual void AddKeyValuesToMemoryLeakList(const void* memory, HKeySymbol name) = 0; // 5
	virtual void RemoveKeyValuesFromMemoryLeakList(const void* memory) = 0; // 6
	virtual void* GetKeyValuesMemory() = 0; // 7
	virtual void SetExpressionSymbol(const char* name, bool value) = 0; // 8
	virtual bool GetExpressionSymbol(const char* name) = 0; // 9
	virtual HKeySymbol GetSymbolForStringCaseSensitive(
		HKeySymbol& caseInsensitiveSymbol, const char* name, bool create = true) = 0; // 10
};

using KeyValuesSystemFn = IKeyValuesSystem* (*)();
extern KeyValuesSystemFn KeyValuesSystem;

static_assert(sizeof(IKeyValuesSystem) == sizeof(void*));
