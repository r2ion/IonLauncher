#include "convar.h"
#include "tier1/cvar.h"
#include "core/tier1.h"

#include <float.h>

#include <utility>

using ConVarRegisterType = void (*)(
	ConVar* conVar,
	const char* name,
	const char* defaultValue,
	int flags,
	const char* helpString,
	bool hasMin,
	float minValue,
	bool hasMax,
	float maxValue,
	FnChangeCallback_t callback);
static ConVarRegisterType s_ConVarRegister;

using ConVarCallbacksConstructorType = void (*)(CUtlVector<ConVar::CVChange_t>* callbacks, int growSize, int initialCapacity);
static ConVarCallbacksConstructorType s_ConVarCallbacksConstructor;

static void* s_ConVarVTable;
static void* s_IConVarVTable;

static constexpr std::pair<int, const char*> s_PrintCommandFlags[] = {
	{FCVAR_UNREGISTERED, "UNREGISTERED"},
	{FCVAR_DEVELOPMENTONLY, "DEVELOPMENTONLY"},
	{FCVAR_GAMEDLL, "GAMEDLL"},
	{FCVAR_CLIENTDLL, "CLIENTDLL"},
	{FCVAR_HIDDEN, "HIDDEN"},
	{FCVAR_PROTECTED, "PROTECTED"},
	{FCVAR_SPONLY, "SPONLY"},
	{FCVAR_ARCHIVE, "ARCHIVE"},
	{FCVAR_NOTIFY, "NOTIFY"},
	{FCVAR_USERINFO, "USERINFO"},
	{FCVAR_PRINTABLEONLY, "PRINTABLEONLY"},
	{FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS, "GAMEDLL_FOR_REMOTE_CLIENTS"},
	{FCVAR_UNLOGGED, "UNLOGGED"},
	{FCVAR_NEVER_AS_STRING, "NEVER_AS_STRING"},
	{FCVAR_REPLICATED, "REPLICATED"},
	{FCVAR_CHEAT, "CHEAT"},
	{FCVAR_SS, "SS"},
	{FCVAR_DEMO, "DEMO"},
	{FCVAR_DONTRECORD, "DONTRECORD"},
	{FCVAR_SS_ADDED, "SS_ADDED"},
	{FCVAR_RELEASE, "RELEASE"},
	{FCVAR_RELOAD_MATERIALS, "RELOAD_MATERIALS"},
	{FCVAR_RELOAD_TEXTURES, "RELOAD_TEXTURES"},
	{FCVAR_NOT_CONNECTED, "NOT_CONNECTED"},
	{FCVAR_MATERIAL_SYSTEM_THREAD, "MATERIAL_SYSTEM_THREAD"},
	{FCVAR_ARCHIVE_PLAYERPROFILE, "ARCHIVE_PLAYERPROFILE"},
	{FCVAR_ACCESSIBLE_FROM_THREADS, "ACCESSIBLE_FROM_THREADS"},
	{FCVAR_STUDIO_SYSTEM, "STUDIO_SYSTEM"},
	{FCVAR_SERVER_FRAME_THREAD, "SERVER_FRAME_THREAD"},
	{FCVAR_SERVER_CAN_EXECUTE, "SERVER_CAN_EXECUTE"},
	{FCVAR_SERVER_CANNOT_QUERY, "SERVER_CANNOT_QUERY"},
	{FCVAR_CLIENTCMD_CAN_EXECUTE, "CLIENTCMD_CAN_EXECUTE"},
	{static_cast<int>(FCVAR_PLATFORM_SYSTEM), "PLATFORM_SYSTEM"},
};

std::span<const std::pair<int, const char*>> GetConVarFlagNames()
{
	return s_PrintCommandFlags;
}

//-----------------------------------------------------------------------------
// Purpose: ConVar interface initialization
//-----------------------------------------------------------------------------
ON_DLL_LOAD("engine.dll", ConVar, [](CModule module)
{
	s_ConVarCallbacksConstructor = module.Offset(0x415C20).RCast<ConVarCallbacksConstructorType>();
	s_ConVarRegister = module.Offset(0x417230).RCast<ConVarRegisterType>();
	s_ConVarVTable = module.Offset(0x67FD28).RCast<void*>();
	s_IConVarVTable = module.Offset(0x67FDC8).RCast<void*>();

	g_pCVar = Sys_GetFactoryPtr("vstdlib.dll", CVAR_INTERFACE_VERSION).RCast<CCvar*>();
})

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
ConVar::ConVar(const char* pszName, const char* pszDefaultValue, int nFlags, const char* pszHelpString)
{
	spdlog::info("Registering Convar {}", pszName);

	*reinterpret_cast<void**>(static_cast<ConCommandBase*>(this)) = s_ConVarVTable;
	s_ConVarCallbacksConstructor(&m_fnChangeCallbacks, 0, 0);
	s_ConVarRegister(this, pszName, pszDefaultValue, nFlags, pszHelpString, false, 0.0f, false, 0.0f, nullptr);
}

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
ConVar::ConVar(
	const char* pszName,
	const char* pszDefaultValue,
	int nFlags,
	const char* pszHelpString,
	bool bMin,
	float fMin,
	bool bMax,
	float fMax,
	FnChangeCallback_t pCallback)
{
	spdlog::info("Registering Convar {}", pszName);

	*reinterpret_cast<void**>(static_cast<ConCommandBase*>(this)) = s_ConVarVTable;
	s_ConVarCallbacksConstructor(&m_fnChangeCallbacks, 0, 0);
	s_ConVarRegister(this, pszName, pszDefaultValue, nFlags, pszHelpString, bMin, fMin, bMax, fMax, pCallback);
}

// Purpose: destructor
//-----------------------------------------------------------------------------
ConVar::~ConVar()
{
	if (m_pParent == this && m_Value.m_pszString)
		delete[] m_Value.m_pszString;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the base ConVar name.
// Output : const char*
//-----------------------------------------------------------------------------
const char* ConVar::GetName() const
{
	return m_pParent->m_pszName;
}

const char* ConVar::GetBaseName() const
{
	return m_pParent->m_pszName;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the ConVar help text.
// Output : const char*
//-----------------------------------------------------------------------------
const char* ConVar::GetHelpText() const
{
	return m_pParent->m_pszHelpString;
}

//-----------------------------------------------------------------------------
// Purpose: Add's flags to ConVar.
// Input  : nFlags -
//-----------------------------------------------------------------------------
void ConVar::AddFlags(int flags)
{
	m_pParent->m_nFlags |= flags;
}

//-----------------------------------------------------------------------------
// Purpose: Removes flags from ConVar.
// Input  : nFlags -
//-----------------------------------------------------------------------------
void ConVar::RemoveFlags(int flags)
{
	m_pParent->m_nFlags &= ~flags;
}

int ConVar::GetFlags() const
{
	return m_pParent->m_nFlags;
}

int ConVar::GetSplitScreenPlayerSlot() const
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a boolean.
// Output : bool
//-----------------------------------------------------------------------------
bool ConVar::GetBool(void) const
{
	return !!GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float.
// Output : float
//-----------------------------------------------------------------------------
float ConVar::GetFloat() const
{
	return m_pParent->m_Value.m_fValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an integer.
// Output : int
//-----------------------------------------------------------------------------
int ConVar::GetInt() const
{
	return m_pParent->m_Value.m_nValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a color.
// Output : Color
//-----------------------------------------------------------------------------
Color ConVar::GetColor() const
{
	const unsigned char* color = reinterpret_cast<const unsigned char*>(&m_pParent->m_Value.m_nValue);
	return Color(color[0], color[1], color[2], color[3]);
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string.
// Output : const char *
//-----------------------------------------------------------------------------
const char* ConVar::GetString() const
{
	if (m_pParent->m_nFlags & FCVAR_NEVER_AS_STRING)
		return "FCVAR_NEVER_AS_STRING";

	const char* value = m_pParent->m_Value.m_pszString;
	return value ? value : "";
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : flMinVal -
// Output : true if there is a min set.
//-----------------------------------------------------------------------------
bool ConVar::GetMin(float& minValue) const
{
	minValue = m_pParent->m_fMinVal;
	return m_pParent->m_bHasMin;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : flMaxVal -
// Output : true if there is a max set.
//-----------------------------------------------------------------------------
bool ConVar::GetMax(float& maxValue) const
{
	maxValue = m_pParent->m_fMaxVal;
	return m_pParent->m_bHasMax;
}

//-----------------------------------------------------------------------------
// Purpose: returns the min value.
// Output : float
//-----------------------------------------------------------------------------
float ConVar::GetMinValue() const
{
	return m_pParent->m_fMinVal;
}

//-----------------------------------------------------------------------------
// Purpose: returns the max value.
// Output : float
//-----------------------------------------------------------------------------
float ConVar::GetMaxValue() const
{
	return m_pParent->m_fMaxVal;
}

//-----------------------------------------------------------------------------
// Purpose: checks if ConVar has min value.
// Output : bool
//-----------------------------------------------------------------------------
bool ConVar::HasMin() const
{
	return m_pParent->m_bHasMin;
}

//-----------------------------------------------------------------------------
// Purpose: checks if ConVar has max value.
// Output : bool
//-----------------------------------------------------------------------------
bool ConVar::HasMax() const
{
	return m_pParent->m_bHasMax;
}

//-----------------------------------------------------------------------------
// Purpose: sets the ConVar int value.
// Input  : nValue -
//-----------------------------------------------------------------------------
void ConVar::SetValue(int value)
{
	ConVar* const parent = m_pParent;
	if (value == parent->m_Value.m_nValue)
		return;

	float floatValue = static_cast<float>(value);
	if (ClampValue(floatValue))
		value = static_cast<int>(floatValue);

	const float oldValue = parent->m_Value.m_fValue;
	parent->m_Value.m_fValue = floatValue;
	parent->m_Value.m_nValue = value;

	if (!(parent->m_nFlags & FCVAR_NEVER_AS_STRING))
	{
		char text[32];
		snprintf(text, sizeof(text), "%d", parent->m_Value.m_nValue);
		ChangeStringValue(text, oldValue);
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the ConVar float value.
// Input  : flValue -
//-----------------------------------------------------------------------------
void ConVar::SetValue(float value)
{
	ConVar* const parent = m_pParent;
	if (value == parent->m_Value.m_fValue)
		return;

	ClampValue(value);

	const float oldValue = parent->m_Value.m_fValue;
	parent->m_Value.m_fValue = value;
	parent->m_Value.m_nValue = static_cast<int>(value);

	if (!(parent->m_nFlags & FCVAR_NEVER_AS_STRING))
	{
		char text[32];
		snprintf(text, sizeof(text), "%f", value);
		ChangeStringValue(text, oldValue);
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the ConVar string value.
// Input  : *szValue -
//-----------------------------------------------------------------------------
void ConVar::SetValue(const char* value)
{
	ConVar* const parent = m_pParent;
	if (value && parent->m_Value.m_pszString && strcmp(parent->m_Value.m_pszString, value) == 0)
		return;

	char clampedText[32] {};
	const char* newValue = value ? value : "";
	const float oldValue = parent->m_Value.m_fValue;

	if (!SetColorFromString(newValue))
	{
		float floatValue = static_cast<float>(atof(newValue));
		if (!std::isfinite(floatValue))
		{
			spdlog::warn("Warning: ConVar '{}' = '{}' is infinite, clamping value.\n", GetBaseName(), newValue);
			floatValue = FLT_MAX;
		}

		if (ClampValue(floatValue))
		{
			snprintf(clampedText, sizeof(clampedText), "%f", floatValue);
			newValue = clampedText;
		}

		parent->m_Value.m_fValue = floatValue;
		parent->m_Value.m_nValue = static_cast<int>(floatValue);
	}

	if (!(parent->m_nFlags & FCVAR_NEVER_AS_STRING))
		ChangeStringValue(newValue, oldValue);
}

//-----------------------------------------------------------------------------
// Purpose: sets the ConVar color value.
// Input  : clValue -
//-----------------------------------------------------------------------------
void ConVar::SetValue(Color value)
{
	std::string text;
	for (int i = 0; i < 4; ++i)
	{
		if (value.GetValue(i) != 0 || !text.empty())
		{
			text += std::to_string(value.GetValue(i));
			text.push_back(' ');
		}
	}
	SetValue(text.c_str());
}

//-----------------------------------------------------------------------------
// Purpose: changes the ConVar string value.
// Input  : *pszTempVal - flOldValue
//-----------------------------------------------------------------------------
void ConVar::ChangeStringValue(const char* value, float oldValue)
{
	NOTE_UNUSED(oldValue);
	ConVar* const parent = m_pParent;
	assert(!(parent->m_nFlags & FCVAR_NEVER_AS_STRING));

	char* oldString = static_cast<char*>(_malloca(parent->m_Value.m_iStringLength));
	if (oldString)
		memcpy(oldString, parent->m_Value.m_pszString, parent->m_Value.m_iStringLength);

	if (value)
	{
		const size_t length = strlen(value) + 1;
		if (length > parent->m_Value.m_iStringLength)
		{
			if (parent->m_Value.m_pszString)
				delete[] parent->m_Value.m_pszString;

			parent->m_Value.m_pszString = new char[length];
			parent->m_Value.m_iStringLength = length;
		}

		memcpy(const_cast<char*>(parent->m_Value.m_pszString), value, length);
	}
	else
	{
		parent->m_Value.m_pszString = nullptr;
	}

	_freea(oldString);
}

//-----------------------------------------------------------------------------
// Purpose: sets the ConVar color value from string.
// Input  : *pszValue -
//-----------------------------------------------------------------------------
bool ConVar::SetColorFromString(const char* value)
{
	int rgba[4] {};
	const int count = sscanf_s(value, "%i %i %i %i", &rgba[0], &rgba[1], &rgba[2], &rgba[3]);
	if (count < 3)
		return false;

	if (count == 3)
		rgba[3] = 255;

	for (int component : rgba)
	{
		if (component < 0 || component > 255)
			return false;
	}

	ConVar* const parent = m_pParent;
	unsigned char* color = reinterpret_cast<unsigned char*>(&parent->m_Value.m_nValue);
	color[0] = static_cast<unsigned char>(rgba[0]);
	color[1] = static_cast<unsigned char>(rgba[1]);
	color[2] = static_cast<unsigned char>(rgba[2]);
	color[3] = static_cast<unsigned char>(rgba[3]);
	parent->m_Value.m_fValue = static_cast<float>(parent->m_Value.m_nValue);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if ConVar is registered.
// Output : bool
//-----------------------------------------------------------------------------
bool ConVar::IsRegistered() const
{
	return m_pParent->m_bRegistered;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this is a command
// Output : bool
//-----------------------------------------------------------------------------
bool ConVar::IsCommand() const
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Test each ConVar query before setting the value.
// Input  : nFlags
// Output : False if change is permitted, true if not.
//-----------------------------------------------------------------------------
bool ConVar::IsFlagSet(int flags) const
{
	return (m_pParent->m_nFlags & flags) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Check whether to clamp and then perform clamp.
// Input  : flValue -
// Output : Returns true if value changed.
//-----------------------------------------------------------------------------
bool ConVar::ClampValue(float& value)
{
	ConVar* const parent = m_pParent;
	if (parent->m_bHasMin && value < parent->m_fMinVal)
	{
		value = parent->m_fMinVal;
		return true;
	}

	if (parent->m_bHasMax && value > parent->m_fMaxVal)
	{
		value = parent->m_fMaxVal;
		return true;
	}

	return false;
}

int ParseConVarFlagsString(std::string modName, std::string sFlags)
{
	int iFlags = 0;
	std::stringstream stFlags(sFlags);
	std::string sFlag;

	while (std::getline(stFlags, sFlag, '|'))
	{
		// trim the flag
		sFlag.erase(sFlag.find_last_not_of(" \t\n\f\v\r") + 1);
		sFlag.erase(0, sFlag.find_first_not_of(" \t\n\f\v\r"));

		// skip if empty
		if (sFlag.empty())
			continue;

		// find the matching flag value
		bool ok = false;
		for (const auto& flagPair : s_PrintCommandFlags)
		{
			if (sFlag == flagPair.second)
			{
				iFlags |= flagPair.first;
				ok = true;
				break;
			}
		}
		if (!ok)
		{
			spdlog::warn("Mod ConCommand {} has unknown flag {}", modName, sFlag);
		}
	}

	return iFlags;
}
