#include "vscript/squirrel/squirrel.h"
#include "tier0/hooks.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace
{
// client.dll stores player .set records in a fixed inline array. Ion reconstructs every record
// into stable heap storage and routes all consumers through this vector instead.
constexpr size_t PLAYER_SETTINGS_RECORD_SIZE = 0x68E8;
constexpr size_t PLAYER_SETTINGS_POSE_OFFSET = 0x2730;
constexpr size_t PLAYER_SETTINGS_POSE_SIZE = 0x110;
constexpr size_t PLAYER_SETTINGS_KEYVALUES_OFFSET = 0x68E0;
constexpr size_t PLAYER_SETTINGS_NAME_SIZE = 32;

constexpr size_t PLAYER_SETTINGS_MODEL_0_OFFSET = 0x2484;
constexpr size_t PLAYER_SETTINGS_MODEL_1_OFFSET = 0x24C4;
constexpr size_t PLAYER_SETTINGS_MODEL_2_OFFSET = 0x2504;
constexpr size_t PLAYER_SETTINGS_MODEL_3_OFFSET = 0x2544;
constexpr size_t PLAYER_SETTINGS_MODEL_INDEX_0_OFFSET = 0x2584;
constexpr size_t PLAYER_SETTINGS_MODEL_INDEX_1_OFFSET = 0x2588;
constexpr size_t PLAYER_SETTINGS_MODEL_INDEX_2_OFFSET = 0x258C;
constexpr size_t PLAYER_SETTINGS_MODEL_INDEX_3_OFFSET = 0x2590;
constexpr size_t PLAYER_SETTINGS_RUI_0_OFFSET = 0x2594;
constexpr size_t PLAYER_SETTINGS_ASSET_0_OFFSET = 0x25D4;
constexpr size_t PLAYER_SETTINGS_ASSET_1_OFFSET = 0x2614;
constexpr size_t PLAYER_SETTINGS_RUI_HANDLE_OFFSET = 0x2658;
constexpr size_t PLAYER_SETTINGS_ASSET_HANDLE_0_OFFSET = 0x2660;
constexpr size_t PLAYER_SETTINGS_ASSET_HANDLE_1_OFFSET = 0x2668;

constexpr size_t PLAYER_SETTINGS_HEALTH_OFFSET = 0x2B70;
constexpr size_t PLAYER_SETTINGS_HEALTH_PER_SEGMENT_OFFSET = 0x2B74;
constexpr size_t PLAYER_SETTINGS_HEALTH_SHIELD_OFFSET = 0x2B78;
constexpr size_t PLAYER_SETTINGS_HEALTH_DOOMED_OFFSET = 0x2B7C;

constexpr uint32_t PLAYER_SETTINGS_NEGATIVE_EPSILON = 0xBC23D70A;
constexpr uint32_t PLAYER_SETTINGS_POSITIVE_EPSILON = 0x3C23D70A;

struct alignas(8) PlayerSettingsRecord
{
	std::array<std::byte, PLAYER_SETTINGS_RECORD_SIZE> data;
};

static_assert(sizeof(PlayerSettingsRecord) == PLAYER_SETTINGS_RECORD_SIZE);

int* g_playerSettingsCount;
int* g_currentPlayerSettingsIndex;
int* g_defaultPlayerSettingsIndex;
void (*g_releasePlayerSettingsKeyValues)(void* keyValues);
void* (*g_getFirstKeyValuesChild)(void* keyValues);
void* (*g_getNextKeyValuesSibling)(void* keyValues);
const char* (*g_getKeyValuesName)(void* keyValues);
void* (*g_findKeyValuesChild)(void* keyValues, const char* name, int create);
void (*g_parsePlayerSettingsFields)(const void* fieldTable, void* record, void* keyValues, bool required);
int64_t (*g_parsePlayerSettingsGlobal)(void* record, void* keyValues, bool required);
int64_t (*g_parsePlayerSettingsModifier)(
	void* result, const char* name, const char* defaultValue, uint8_t type, void* context);
int64_t (*g_applyPlayerSettingsModifiers)(int64_t* target, void* modifiers);
int64_t (*g_parsePlayerSettingsPoseMods)(void* record, void* keyValues, void* poseRecord);
int (*g_resolvePlayerSettingsModel)(const char* path);
uint64_t (*g_loadPlayerSettingsRui)(const char* path);
uint64_t (*g_loadPlayerSettingsAsset)(const char* path);
bool (*g_findPlayerSettingsField)(
	const void* fieldTable, const void* record, const char* fieldName, int* fieldType, const void** fieldValue);
void (*g_pushPlayerSettingsField)(HSQUIRRELVM sqvm, bool found, int fieldType, const void* fieldValue);
bool (*g_pushPlayerSettingsGlobalField)(HSQUIRRELVM sqvm, const void* record, const char* fieldName);
int (*g_getSquirrelArgumentCount)(HSQUIRRELVM sqvm);
int64_t (*g_getSquirrelInteger)(HSQUIRRELVM sqvm, int argument, int* value);
void (*g_playerSettingsLog)(const char* format, ...);

const void* g_playerSettingsGlobalFieldTable;
const void* g_playerSettingsSecondaryFieldTable;
const void* g_playerSettingsPoseFieldTable;

std::vector<std::unique_ptr<PlayerSettingsRecord>> g_playerSettings;
std::recursive_mutex g_playerSettingsMutex;

std::byte* PlayerSettingsAt(size_t index)
{
	return index < g_playerSettings.size() ? g_playerSettings[index]->data.data() : nullptr;
}

template <typename T> T ReadPlayerSettingsValue(const std::byte* record, size_t offset)
{
	T value;
	std::memcpy(&value, record + offset, sizeof(value));
	return value;
}

template <typename T> void WritePlayerSettingsValue(std::byte* record, size_t offset, const T& value)
{
	std::memcpy(record + offset, &value, sizeof(value));
}

void InitialisePlayerSettingsRecord(std::byte* record)
{
	std::memset(record, 0, PLAYER_SETTINGS_RECORD_SIZE);

	constexpr std::array<int, 4> poseSpeeds = {300, 400, 300, 300};
	for (size_t pose = 0; pose < poseSpeeds.size(); ++pose)
	{
		const size_t poseOffset = PLAYER_SETTINGS_POSE_OFFSET + PLAYER_SETTINGS_POSE_SIZE * pose;
		WritePlayerSettingsValue(record, poseOffset + 0x18, PLAYER_SETTINGS_NEGATIVE_EPSILON);
		WritePlayerSettingsValue(record, poseOffset + 0x1C, PLAYER_SETTINGS_NEGATIVE_EPSILON);
		WritePlayerSettingsValue(record, poseOffset + 0x20, PLAYER_SETTINGS_NEGATIVE_EPSILON);
		WritePlayerSettingsValue(record, poseOffset + 0x24, PLAYER_SETTINGS_POSITIVE_EPSILON);
		WritePlayerSettingsValue(record, poseOffset + 0x28, PLAYER_SETTINGS_POSITIVE_EPSILON);
		WritePlayerSettingsValue(record, poseOffset + 0x2C, PLAYER_SETTINGS_POSITIVE_EPSILON);
		WritePlayerSettingsValue(record, poseOffset + 0x30, poseSpeeds[pose]);
	}
}

int FindPlayerSettingsIndex(const char* className)
{
	if (!className || !className[0])
		return -1;

	for (size_t index = 0; index < g_playerSettings.size(); ++index)
	{
		if (_stricmp(reinterpret_cast<const char*>(g_playerSettings[index]->data.data()), className) == 0)
			return static_cast<int>(index);
	}

	return -1;
}

void ReleasePlayerSettings()
{
	for (const std::unique_ptr<PlayerSettingsRecord>& record : g_playerSettings)
	{
		void* keyValues = ReadPlayerSettingsValue<void*>(record->data.data(), PLAYER_SETTINGS_KEYVALUES_OFFSET);
		if (keyValues)
			g_releasePlayerSettingsKeyValues(keyValues);
	}

	g_playerSettings.clear();
}

int FindPlayerSettingsPose(const char* name)
{
	constexpr std::array<const char*, 4> poseNames = {"stand", "crouch", "dead", "observe"};
	for (size_t pose = 0; pose < poseNames.size(); ++pose)
	{
		if (_stricmp(name, poseNames[pose]) == 0)
			return static_cast<int>(pose);
	}

	return -1;
}

struct alignas(8) PlayerSettingsParseContext
{
	void* record;
	void* keyValues;
	uint64_t unknown;
	bool required;
	std::array<std::byte, 7> padding;
};
static_assert(sizeof(PlayerSettingsParseContext) == 0x20);

struct alignas(8) PlayerSettingsParsedPoseSpeeds
{
	uint64_t header;
	std::array<std::byte, 0x10> speed;
	std::array<std::byte, 0x10> sprintSpeed;
};
static_assert(sizeof(PlayerSettingsParsedPoseSpeeds) == 0x28);

struct alignas(8) PlayerSettingsModifierList
{
	std::array<void*, 4> entries;
	uint64_t count;
	uint64_t capacity;
};
static_assert(sizeof(PlayerSettingsModifierList) == 0x30);

void ParsePlayerSettingsPose(std::byte* record, void* keyValues, int pose)
{
	std::byte* poseRecord = record + PLAYER_SETTINGS_POSE_OFFSET + PLAYER_SETTINGS_POSE_SIZE * pose;
	g_parsePlayerSettingsFields(g_playerSettingsPoseFieldTable, poseRecord, keyValues, true);

	PlayerSettingsParseContext context = {record, keyValues, 0, true, {}};
	PlayerSettingsParsedPoseSpeeds parsedSpeeds = {};
	g_parsePlayerSettingsModifier(parsedSpeeds.speed.data(), "speed", "120", 2, &context);
	g_parsePlayerSettingsModifier(parsedSpeeds.sprintSpeed.data(), "sprintspeed", "0", 2, &context);

	PlayerSettingsModifierList modifiers = {};
	modifiers.entries[0] = &parsedSpeeds;
	modifiers.count = 1;
	g_applyPlayerSettingsModifiers(reinterpret_cast<int64_t*>(poseRecord + 0x60), &modifiers);

	if (void* poseMods = g_findKeyValuesChild(keyValues, "PoseMods", 0))
		g_parsePlayerSettingsPoseMods(record, poseMods, poseRecord);
}

void ResolvePlayerSettingsAssets(std::byte* record)
{
	constexpr std::array<size_t, 4> modelOffsets = {
		PLAYER_SETTINGS_MODEL_0_OFFSET,
		PLAYER_SETTINGS_MODEL_1_OFFSET,
		PLAYER_SETTINGS_MODEL_2_OFFSET,
		PLAYER_SETTINGS_MODEL_3_OFFSET,
	};
	constexpr std::array<size_t, 4> modelIndexOffsets = {
		PLAYER_SETTINGS_MODEL_INDEX_0_OFFSET,
		PLAYER_SETTINGS_MODEL_INDEX_1_OFFSET,
		PLAYER_SETTINGS_MODEL_INDEX_2_OFFSET,
		PLAYER_SETTINGS_MODEL_INDEX_3_OFFSET,
	};

	for (size_t model = 0; model < modelOffsets.size(); ++model)
	{
		const char* path = reinterpret_cast<const char*>(record + modelOffsets[model]);
		const int modelIndex = path[0] ? g_resolvePlayerSettingsModel(path) : -1;
		WritePlayerSettingsValue(record, modelIndexOffsets[model], modelIndex);
	}

	const char* ruiPath = reinterpret_cast<const char*>(record + PLAYER_SETTINGS_RUI_0_OFFSET);
	WritePlayerSettingsValue(record, PLAYER_SETTINGS_RUI_HANDLE_OFFSET,
		ruiPath[0] ? g_loadPlayerSettingsRui(ruiPath) : uint64_t{0});

	const char* assetPath0 = reinterpret_cast<const char*>(record + PLAYER_SETTINGS_ASSET_0_OFFSET);
	WritePlayerSettingsValue(record, PLAYER_SETTINGS_ASSET_HANDLE_0_OFFSET,
		assetPath0[0] ? g_loadPlayerSettingsAsset(assetPath0) : uint64_t{0});

	const char* assetPath1 = reinterpret_cast<const char*>(record + PLAYER_SETTINGS_ASSET_1_OFFSET);
	WritePlayerSettingsValue(record, PLAYER_SETTINGS_ASSET_HANDLE_1_OFFSET,
		assetPath1[0] ? g_loadPlayerSettingsAsset(assetPath1) : uint64_t{0});
}

bool IsStringArgument(HSQUIRRELVM sqvm, size_t argument)
{
	return sqvm && sqvm->_stackOfCurrentFunction && sqvm->_stackOfCurrentFunction[argument]._Type == OT_STRING;
}

int GetPlayerSettingsIndexArgument(HSQUIRRELVM sqvm)
{
	const char* className = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
	return FindPlayerSettingsIndex(className);
}

template <size_t ValueOffset> SQRESULT PushPlayerSettingsFloat(HSQUIRRELVM sqvm)
{
	if (!IsStringArgument(sqvm, 1))
	{
		g_playerSettingsLog("Classname must be a string");
		return static_cast<SQRESULT>(0);
	}

	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	const int index = GetPlayerSettingsIndexArgument(sqvm);
	const std::byte* record = index >= 0 ? PlayerSettingsAt(static_cast<size_t>(index)) : nullptr;
	if (!record)
	{
		const char* className = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
		g_playerSettingsLog("Given classname '%s' is invalid", className);
		return static_cast<SQRESULT>(0);
	}

	g_pSquirrel[ScriptContext::CLIENT]->pushfloat(sqvm, ReadPlayerSettingsValue<float>(record, ValueOffset));
	return static_cast<SQRESULT>(1);
}
} // namespace

DECLARE_MODULE(PlayerSettingsHooks)

DECLARE_HOOK(PlayerSettings_Reset, client.dll + 0x19E060, [](auto&) -> void*
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	ReleasePlayerSettings();
	*g_playerSettingsCount = 0;
	*g_currentPlayerSettingsIndex = -1;
	*g_defaultPlayerSettingsIndex = -1;
	return nullptr;
})

DECLARE_HOOK(PlayerSettings_LoadRecord, client.dll + 0x19E230, [](auto&, void* keyValues) -> int64_t
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	const size_t index = g_playerSettings.size();
	auto record = std::make_unique<PlayerSettingsRecord>();
	std::byte* recordData = record->data.data();
	InitialisePlayerSettingsRecord(recordData);
	WritePlayerSettingsValue(recordData, PLAYER_SETTINGS_KEYVALUES_OFFSET, keyValues);
	if (const char* name = g_getKeyValuesName(keyValues))
		strncpy_s(reinterpret_cast<char*>(recordData), PLAYER_SETTINGS_NAME_SIZE, name, _TRUNCATE);

	g_playerSettings.push_back(std::move(record));
	*g_playerSettingsCount = static_cast<int>(g_playerSettings.size());
	*g_currentPlayerSettingsIndex = static_cast<int>(index);

	for (void* section = g_getFirstKeyValuesChild(keyValues); section; section = g_getNextKeyValuesSibling(section))
	{
		const char* sectionName = g_getKeyValuesName(section);
		if (_stricmp(sectionName, "global") == 0)
			g_parsePlayerSettingsGlobal(recordData, section, true);
		else if (const int pose = FindPlayerSettingsPose(sectionName); pose >= 0)
			ParsePlayerSettingsPose(recordData, section, pose);
	}

	ResolvePlayerSettingsAssets(recordData);
	*g_currentPlayerSettingsIndex = -1;
	return 0;
})

DECLARE_HOOK(PlayerSettings_Count, client.dll + 0x19EC10, [](auto&) -> uint32_t
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	return static_cast<uint32_t>(g_playerSettings.size());
})

DECLARE_HOOK(PlayerSettings_NameToIndex, client.dll + 0x19EC20, [](auto&, const char* className) -> int
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	return FindPlayerSettingsIndex(className);
})

DECLARE_HOOK(PlayerSettings_GetPose, client.dll + 0x19ED50, [](auto&, int index, int pose) -> char*
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	std::byte* record = index >= 0 ? PlayerSettingsAt(static_cast<size_t>(index)) : nullptr;
	if (!record || pose < 0 || pose >= 4)
		return nullptr;
	return reinterpret_cast<char*>(record + PLAYER_SETTINGS_POSE_OFFSET + PLAYER_SETTINGS_POSE_SIZE * pose);
})

DECLARE_HOOK(PlayerSettings_GetRecord, client.dll + 0x19ED80, [](auto&, int index) -> char*
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	std::byte* record = index >= 0 ? PlayerSettingsAt(static_cast<size_t>(index)) : nullptr;
	return reinterpret_cast<char*>(record);
})

DECLARE_HOOK(PlayerSettings_IndexToName, client.dll + 0x19B2C0, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	const SQInteger index = g_pSquirrel[ScriptContext::CLIENT]->getinteger(sqvm, 1);
	std::byte* record = index >= 0 ? PlayerSettingsAt(static_cast<size_t>(index)) : nullptr;
	if (!record)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "Player settings index is out of range");
		return SQRESULT_ERROR;
	}

	g_pSquirrel[ScriptContext::CLIENT]->pushstring(sqvm, reinterpret_cast<const char*>(record));
	return static_cast<SQRESULT>(1);
})

DECLARE_HOOK(PlayerSettings_GetModelField, client.dll + 0x19DA20,
	[](auto&, const char** result, const char* className, const char* fieldName) -> const char**
{
	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	const int index = FindPlayerSettingsIndex(className);
	std::byte* record = index >= 0 ? PlayerSettingsAt(static_cast<size_t>(index)) : nullptr;
	if (!record)
	{
		g_playerSettingsLog("Given classname '%s' is invalid", className);
		*result = "";
		return result;
	}

	int fieldType = 0;
	const void* fieldValue = nullptr;
	const bool found = g_findPlayerSettingsField(
		g_playerSettingsGlobalFieldTable, record, fieldName, &fieldType, &fieldValue)
		|| g_findPlayerSettingsField(
			g_playerSettingsSecondaryFieldTable, record, fieldName, &fieldType, &fieldValue);

	if (!found)
	{
		g_playerSettingsLog("Field not found: %s", fieldName);
		*result = "";
	}
	else if (fieldType != 2)
	{
		g_playerSettingsLog("Field %s not a MODELNAME...type %d", fieldName, fieldType);
		*result = "";
	}
	else
	{
		*result = static_cast<const char*>(fieldValue);
	}

	return result;
})

DECLARE_HOOK(PlayerSettings_GetField, client.dll + 0x19FC20, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT
{
	if (!IsStringArgument(sqvm, 1))
	{
		g_playerSettingsLog("Classname must be a string");
		return static_cast<SQRESULT>(0);
	}

	std::lock_guard<std::recursive_mutex> lock(g_playerSettingsMutex);
	const int index = GetPlayerSettingsIndexArgument(sqvm);
	std::byte* record = index >= 0 ? PlayerSettingsAt(static_cast<size_t>(index)) : nullptr;
	if (!record)
	{
		const char* className = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
		g_playerSettingsLog("Given classname '%s' is invalid", className);
		return static_cast<SQRESULT>(0);
	}

	const char* fieldName = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 2);
	if (g_getSquirrelArgumentCount(sqvm) >= 4)
	{
		int pose = 0;
		if (g_getSquirrelInteger(sqvm, 4, &pose) < 0 || static_cast<unsigned int>(pose) >= 4)
		{
			g_playerSettingsLog("Optional parameter must be a valid PLAYERPOSE_ constant.");
			return static_cast<SQRESULT>(0);
		}

		const std::byte* poseRecord = record + PLAYER_SETTINGS_POSE_OFFSET + PLAYER_SETTINGS_POSE_SIZE * pose;
		int fieldType = 0;
		const void* fieldValue = nullptr;
		const bool found = g_findPlayerSettingsField(
			g_playerSettingsPoseFieldTable, poseRecord, fieldName, &fieldType, &fieldValue);
		if (found)
		{
			g_pushPlayerSettingsField(sqvm, true, fieldType, fieldValue);
			return static_cast<SQRESULT>(1);
		}
	}

	if (!g_pushPlayerSettingsGlobalField(sqvm, record, fieldName))
		g_playerSettingsLog("Field not found: %s", fieldName);
	return static_cast<SQRESULT>(1);
})

DECLARE_HOOK(PlayerSettings_GetHealth, client.dll + 0x19FDB0, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT
{
	return PushPlayerSettingsFloat<PLAYER_SETTINGS_HEALTH_OFFSET>(sqvm);
})

DECLARE_HOOK(PlayerSettings_GetHealthShield, client.dll + 0x19FE40, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT
{
	return PushPlayerSettingsFloat<PLAYER_SETTINGS_HEALTH_SHIELD_OFFSET>(sqvm);
})

DECLARE_HOOK(PlayerSettings_GetHealthDoomed, client.dll + 0x19FED0, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT
{
	return PushPlayerSettingsFloat<PLAYER_SETTINGS_HEALTH_DOOMED_OFFSET>(sqvm);
})

DECLARE_HOOK(PlayerSettings_GetHealthPerSegment, client.dll + 0x19FF60, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT
{
	return PushPlayerSettingsFloat<PLAYER_SETTINGS_HEALTH_PER_SEGMENT_OFFSET>(sqvm);
})

ON_DLL_LOAD_CLIENT("client.dll", PlayerSettings, [](CModule module)
{
	g_playerSettingsCount = module.Offset(0xDC97B8).RCast<int*>();
	g_currentPlayerSettingsIndex = module.Offset(0xDC97BC).RCast<int*>();
	g_defaultPlayerSettingsIndex = module.Offset(0xDC97C0).RCast<int*>();
	g_releasePlayerSettingsKeyValues = module.Offset(0x7305B0).RCast<void (*)(void*)>();
	g_getFirstKeyValuesChild = module.Offset(0x72AAC0).RCast<void* (*)(void*)>();
	g_getNextKeyValuesSibling = module.Offset(0x72AD50).RCast<void* (*)(void*)>();
	g_getKeyValuesName = module.Offset(0x72ACD0).RCast<const char* (*)(void*)>();
	g_findKeyValuesChild = module.Offset(0x72A3F0).RCast<void* (*)(void*, const char*, int)>();
	g_parsePlayerSettingsFields = module.Offset(0x56E020).RCast<void (*)(const void*, void*, void*, bool)>();
	g_parsePlayerSettingsGlobal = module.Offset(0x19E530).RCast<int64_t (*)(void*, void*, bool)>();
	g_parsePlayerSettingsModifier = module.Offset(0x19D0F0)
		.RCast<int64_t (*)(void*, const char*, const char*, uint8_t, void*)>();
	g_applyPlayerSettingsModifiers = module.Offset(0x19D2B0).RCast<int64_t (*)(int64_t*, void*)>();
	g_parsePlayerSettingsPoseMods = module.Offset(0x19D6B0).RCast<int64_t (*)(void*, void*, void*)>();
	g_resolvePlayerSettingsModel = module.Offset(0x195CD0).RCast<int (*)(const char*)>();
	g_loadPlayerSettingsRui = module.Offset(0x190E60).RCast<uint64_t (*)(const char*)>();
	g_loadPlayerSettingsAsset = module.Offset(0x190B40).RCast<uint64_t (*)(const char*)>();
	g_findPlayerSettingsField = module.Offset(0x56DFA0)
		.RCast<bool (*)(const void*, const void*, const char*, int*, const void**)>();
	g_pushPlayerSettingsField = module.Offset(0x19F000).RCast<void (*)(HSQUIRRELVM, bool, int, const void*)>();
	g_pushPlayerSettingsGlobalField = module.Offset(0x19EB80).RCast<bool (*)(HSQUIRRELVM, const void*, const char*)>();
	g_getSquirrelArgumentCount = module.Offset(0x6F60).RCast<int (*)(HSQUIRRELVM)>();
	g_getSquirrelInteger = module.Offset(0x5D50).RCast<int64_t (*)(HSQUIRRELVM, int, int*)>();
	g_playerSettingsLog = module.Offset(0x39DAA0).RCast<void (*)(const char*, ...)>();
	g_playerSettingsGlobalFieldTable = module.Offset(0xB099C0).RCast<const void*>();
	g_playerSettingsSecondaryFieldTable = module.Offset(0xB0E490).RCast<const void*>();
	g_playerSettingsPoseFieldTable = module.Offset(0xB0E610).RCast<const void*>();
	DISPATCH_MODULE(PlayerSettingsHooks)
})
