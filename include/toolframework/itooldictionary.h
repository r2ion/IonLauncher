#pragma once

#include "appframework/IAppSystem.h"

class IToolSystem;

inline constexpr char VTOOLDICTIONARY_INTERFACE_VERSION[] = "VTOOLDICTIONARY003";

class IToolDictionary : public IAppSystem
{
public:
	virtual void CreateTools() = 0;
	virtual int GetToolCount() const = 0;
	virtual IToolSystem* GetTool(int index) = 0;
};

static_assert(sizeof(IToolDictionary) == sizeof(void*));
