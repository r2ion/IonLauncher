#include "tier0/hooks.h"
#include "tier1/keyvalues.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

DECLARE_MODULE(PlayerSettingsHooks)

static int* s_pPlayerSettingsCount;
static int* s_pCurrentPlayerSettingsIndex;
static int* s_pDefaultPlayerSettingsIndex;
static void (*s_ReleasePlayerSettingsKeyValues)(void* keyValues);
static void* (*s_GetFirstPlayerSettingsKeyValuesChild)(void* keyValues);
static void* (*s_GetNextPlayerSettingsKeyValuesSibling)(void* keyValues);
static const char* (*s_GetPlayerSettingsKeyValuesName)(void* keyValues);
static void* (*s_FindPlayerSettingsKeyValuesChild)(void* keyValues, const char* name, int create);
static void (*s_ParsePlayerSettingsFields)(const void* fieldTable, void* record, void* keyValues, bool required);
static int64_t (*s_ParsePlayerSettingsGlobal)(void* record, void* keyValues, bool required);
static int64_t (*s_ParsePlayerSettingsModifier)(void* result, const char* name, const char* defaultValue, uint8_t type, void* context);
static int64_t (*s_ApplyPlayerSettingsModifiers)(int64_t* target, void* modifiers);
static int64_t (*s_ParsePlayerSettingsPoseMods)(void* record, void* keyValues, void* poseRecord);
static int (*s_ResolvePlayerSettingsModel)(const char* path);
static uint64_t (*s_LoadPlayerSettingsRui)(const char* path);
static uint64_t (*s_LoadPlayerSettingsAsset)(const char* path);
static bool (*s_FindPlayerSettingsField)(const void* fieldTable, const void* record, const char* fieldName, int* fieldType, const void** fieldValue);
static void (*s_PushPlayerSettingsField)(HSQUIRRELVM sqvm, bool found, int fieldType, const void* fieldValue);
static bool (*s_PushPlayerSettingsGlobalField)(HSQUIRRELVM sqvm, const void* record, const char* fieldName);
static int (*s_GetSquirrelArgumentCount)(HSQUIRRELVM sqvm);
static int64_t (*s_GetSquirrelInteger)(HSQUIRRELVM sqvm, int argument, int* value);
static void (*s_PlayerSettingsLog)(const char* format, ...);
static const void* s_pPlayerSettingsGlobalFieldTable;
static const void* s_pPlayerSettingsSecondaryFieldTable;
static const void* s_pPlayerSettingsPoseFieldTable;

// The compiled .set loader can preserve small integer values using KeyValues'
// compact integer tags. The legacy script bridge only handles TYPE_STRING,
// TYPE_INT, and TYPE_FLOAT, so normalize the equivalent compact encodings
// before exposing the record through Ion's dynamic storage.
static void NormalizeCompiledIntegerKeyValues(KeyValues* keyValues)
{
    for (KeyValues* node = keyValues; node; node = node->m_pPeer)
    {
        NormalizeCompiledIntegerKeyValues(node->m_pSub);

        switch (node->m_iDataType)
        {
        case TYPE_COMPILED_INT_BYTE:
            node->m_iDataType = TYPE_INT;
            break;
        case TYPE_COMPILED_INT_0:
            node->m_iValue = 0;
            node->m_iDataType = TYPE_INT;
            break;
        case TYPE_COMPILED_INT_1:
            node->m_iValue = 1;
            node->m_iDataType = TYPE_INT;
            break;
        }
    }
}

// client.dll keeps player .set records in a fixed inline array. This replacement
// uses stable heap allocations so the native consumers can address every record
// without inheriting the engine's fixed capacity.
class CPlayerSettings
{
  public:
    void* Reset();
    int64_t LoadRecord(void* keyValues);
    uint32_t Count();
    int NameToIndex(const char* className);
    char* GetPose(int index, int pose);
    char* GetRecord(int index);
    SQRESULT IndexToName(HSQUIRRELVM sqvm);
    const char** GetModelField(const char** result, const char* className, const char* fieldName);
    SQRESULT GetField(HSQUIRRELVM sqvm);
    SQRESULT GetHealth(HSQUIRRELVM sqvm);
    SQRESULT GetHealthShield(HSQUIRRELVM sqvm);
    SQRESULT GetHealthDoomed(HSQUIRRELVM sqvm);
    SQRESULT GetHealthPerSegment(HSQUIRRELVM sqvm);

  private:
    static constexpr size_t RECORD_SIZE = 0x68E8;
    static constexpr size_t POSE_OFFSET = 0x2730;
    static constexpr size_t POSE_SIZE = 0x110;
    static constexpr size_t KEYVALUES_OFFSET = 0x68E0;
    static constexpr size_t NAME_SIZE = 32;

    static constexpr std::array<size_t, 4> MODEL_PATH_OFFSETS = {0x2484, 0x24C4, 0x2504, 0x2544};
    static constexpr std::array<size_t, 4> MODEL_INDEX_OFFSETS = {0x2584, 0x2588, 0x258C, 0x2590};
    static constexpr size_t RUI_PATH_OFFSET = 0x2594;
    static constexpr std::array<size_t, 2> ASSET_PATH_OFFSETS = {0x25D4, 0x2614};
    static constexpr size_t RUI_HANDLE_OFFSET = 0x2658;
    static constexpr std::array<size_t, 2> ASSET_HANDLE_OFFSETS = {0x2660, 0x2668};

    static constexpr size_t HEALTH_OFFSET = 0x2B70;
    static constexpr size_t HEALTH_PER_SEGMENT_OFFSET = 0x2B74;
    static constexpr size_t HEALTH_SHIELD_OFFSET = 0x2B78;
    static constexpr size_t HEALTH_DOOMED_OFFSET = 0x2B7C;

    static constexpr uint32_t NEGATIVE_EPSILON = 0xBC23D70A;
    static constexpr uint32_t POSITIVE_EPSILON = 0x3C23D70A;
    static constexpr std::array<int, 4> DEFAULT_POSE_SPEEDS = {300, 400, 300, 300};
    static constexpr std::array<const char*, 4> POSE_NAMES = {"stand", "crouch", "dead", "observe"};

    struct alignas(8) PlayerSettingsRecord
    {
        std::array<std::byte, RECORD_SIZE> m_Data;
    };
    static_assert(sizeof(PlayerSettingsRecord) == RECORD_SIZE);

    struct alignas(8) ParseContext
    {
        void* m_Record;
        void* m_KeyValues;
        uint64_t m_Unknown;
        bool m_Required;
        std::array<std::byte, 7> m_Padding;
    };
    static_assert(sizeof(ParseContext) == 0x20);

    struct alignas(8) ParsedPoseSpeeds
    {
        uint64_t m_Header;
        std::array<std::byte, 0x10> m_Speed;
        std::array<std::byte, 0x10> m_SprintSpeed;
    };
    static_assert(sizeof(ParsedPoseSpeeds) == 0x28);

    struct alignas(8) ModifierList
    {
        std::array<void*, 4> m_Entries;
        uint64_t m_Count;
        uint64_t m_Capacity;
    };
    static_assert(sizeof(ModifierList) == 0x30);

    std::byte* At(size_t index) const;
    int FindIndex(const char* className) const;
    void ReleaseRecords();
    static int FindPose(const char* name);
    static bool IsStringArgument(HSQUIRRELVM sqvm, size_t argument);
    int GetIndexArgument(HSQUIRRELVM sqvm) const;
    void InitialiseRecord(std::byte* record) const;
    void ParsePose(std::byte* record, void* keyValues, int pose) const;
    void ResolveAssets(std::byte* record) const;

    template <typename T> static T ReadValue(const std::byte* record, size_t offset);
    template <typename T> static void WriteValue(std::byte* record, size_t offset, const T& value);
    template <size_t ValueOffset> SQRESULT PushFloat(HSQUIRRELVM sqvm);

    std::vector<std::unique_ptr<PlayerSettingsRecord>> m_Records;
    std::recursive_mutex m_Mutex;
};

CPlayerSettings g_PlayerSettings;

std::byte* CPlayerSettings::At(size_t index) const
{
    return index < m_Records.size() ? m_Records[index]->m_Data.data() : nullptr;
}

template <typename T> T CPlayerSettings::ReadValue(const std::byte* record, size_t offset)
{
    T value;
    std::memcpy(&value, record + offset, sizeof(value));
    return value;
}

template <typename T> void CPlayerSettings::WriteValue(std::byte* record, size_t offset, const T& value)
{
    std::memcpy(record + offset, &value, sizeof(value));
}

void CPlayerSettings::InitialiseRecord(std::byte* record) const
{
    std::memset(record, 0, RECORD_SIZE);

    for (size_t pose = 0; pose < DEFAULT_POSE_SPEEDS.size(); ++pose)
    {
        const size_t poseOffset = POSE_OFFSET + POSE_SIZE * pose;
        WriteValue(record, poseOffset + 0x18, NEGATIVE_EPSILON);
        WriteValue(record, poseOffset + 0x1C, NEGATIVE_EPSILON);
        WriteValue(record, poseOffset + 0x20, NEGATIVE_EPSILON);
        WriteValue(record, poseOffset + 0x24, POSITIVE_EPSILON);
        WriteValue(record, poseOffset + 0x28, POSITIVE_EPSILON);
        WriteValue(record, poseOffset + 0x2C, POSITIVE_EPSILON);
        WriteValue(record, poseOffset + 0x30, DEFAULT_POSE_SPEEDS[pose]);
    }
}

int CPlayerSettings::FindIndex(const char* className) const
{
    if (!className || !className[0])
        return -1;

    for (size_t index = 0; index < m_Records.size(); ++index)
    {
        if (_stricmp(reinterpret_cast<const char*>(m_Records[index]->m_Data.data()), className) == 0)
            return static_cast<int>(index);
    }

    return -1;
}

void CPlayerSettings::ReleaseRecords()
{
    for (const std::unique_ptr<PlayerSettingsRecord>& record : m_Records)
    {
        void* keyValues = ReadValue<void*>(record->m_Data.data(), KEYVALUES_OFFSET);
        if (keyValues)
            s_ReleasePlayerSettingsKeyValues(keyValues);
    }

    m_Records.clear();
}

int CPlayerSettings::FindPose(const char* name)
{
    for (size_t pose = 0; pose < POSE_NAMES.size(); ++pose)
    {
        if (_stricmp(name, POSE_NAMES[pose]) == 0)
            return static_cast<int>(pose);
    }

    return -1;
}

void CPlayerSettings::ParsePose(std::byte* record, void* keyValues, int pose) const
{
    std::byte* poseRecord = record + POSE_OFFSET + POSE_SIZE * pose;
    s_ParsePlayerSettingsFields(s_pPlayerSettingsPoseFieldTable, poseRecord, keyValues, true);

    ParseContext context = {record, keyValues, 0, true, {}};
    ParsedPoseSpeeds parsedSpeeds = {};
    s_ParsePlayerSettingsModifier(parsedSpeeds.m_Speed.data(), "speed", "120", 2, &context);
    s_ParsePlayerSettingsModifier(parsedSpeeds.m_SprintSpeed.data(), "sprintspeed", "0", 2, &context);

    ModifierList modifiers = {};
    modifiers.m_Entries[0] = &parsedSpeeds;
    modifiers.m_Count = 1;
    s_ApplyPlayerSettingsModifiers(reinterpret_cast<int64_t*>(poseRecord + 0x60), &modifiers);

    if (void* poseMods = s_FindPlayerSettingsKeyValuesChild(keyValues, "PoseMods", 0))
        s_ParsePlayerSettingsPoseMods(record, poseMods, poseRecord);
}

void CPlayerSettings::ResolveAssets(std::byte* record) const
{
    for (size_t model = 0; model < MODEL_PATH_OFFSETS.size(); ++model)
    {
        const char* path = reinterpret_cast<const char*>(record + MODEL_PATH_OFFSETS[model]);
        const int modelIndex = path[0] ? s_ResolvePlayerSettingsModel(path) : -1;
        WriteValue(record, MODEL_INDEX_OFFSETS[model], modelIndex);
    }

    const char* ruiPath = reinterpret_cast<const char*>(record + RUI_PATH_OFFSET);
    WriteValue(record, RUI_HANDLE_OFFSET, ruiPath[0] ? s_LoadPlayerSettingsRui(ruiPath) : uint64_t{0});

    for (size_t asset = 0; asset < ASSET_PATH_OFFSETS.size(); ++asset)
    {
        const char* path = reinterpret_cast<const char*>(record + ASSET_PATH_OFFSETS[asset]);
        WriteValue(record, ASSET_HANDLE_OFFSETS[asset], path[0] ? s_LoadPlayerSettingsAsset(path) : uint64_t{0});
    }
}

bool CPlayerSettings::IsStringArgument(HSQUIRRELVM sqvm, size_t argument)
{
    return sqvm && sqvm->_stackOfCurrentFunction && sqvm->_stackOfCurrentFunction[argument]._Type == OT_STRING;
}

int CPlayerSettings::GetIndexArgument(HSQUIRRELVM sqvm) const
{
    const char* className = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
    return FindIndex(className);
}

template <size_t ValueOffset> SQRESULT CPlayerSettings::PushFloat(HSQUIRRELVM sqvm)
{
    if (!IsStringArgument(sqvm, 1))
    {
        s_PlayerSettingsLog("Classname must be a string");
        return static_cast<SQRESULT>(0);
    }

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const int index = GetIndexArgument(sqvm);
    const std::byte* record = index >= 0 ? At(static_cast<size_t>(index)) : nullptr;
    if (!record)
    {
        const char* className = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
        s_PlayerSettingsLog("Given classname '%s' is invalid", className);
        return static_cast<SQRESULT>(0);
    }

    g_pSquirrel[ScriptContext::CLIENT]->pushfloat(sqvm, ReadValue<float>(record, ValueOffset));
    return static_cast<SQRESULT>(1);
}

void* CPlayerSettings::Reset()
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    ReleaseRecords();
    *s_pPlayerSettingsCount = 0;
    *s_pCurrentPlayerSettingsIndex = -1;
    *s_pDefaultPlayerSettingsIndex = -1;
    return nullptr;
}

int64_t CPlayerSettings::LoadRecord(void* keyValues)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    NormalizeCompiledIntegerKeyValues(static_cast<KeyValues*>(keyValues));
    const size_t index = m_Records.size();
    auto record = std::make_unique<PlayerSettingsRecord>();
    std::byte* recordData = record->m_Data.data();
    InitialiseRecord(recordData);
    WriteValue(recordData, KEYVALUES_OFFSET, keyValues);
    if (const char* name = s_GetPlayerSettingsKeyValuesName(keyValues))
        strncpy_s(reinterpret_cast<char*>(recordData), NAME_SIZE, name, _TRUNCATE);

    m_Records.push_back(std::move(record));
    *s_pPlayerSettingsCount = static_cast<int>(m_Records.size());
    *s_pCurrentPlayerSettingsIndex = static_cast<int>(index);

    for (void* section = s_GetFirstPlayerSettingsKeyValuesChild(keyValues); section; section = s_GetNextPlayerSettingsKeyValuesSibling(section))
    {
        const char* sectionName = s_GetPlayerSettingsKeyValuesName(section);
        if (_stricmp(sectionName, "global") == 0)
            s_ParsePlayerSettingsGlobal(recordData, section, true);
        else if (const int pose = FindPose(sectionName); pose >= 0)
            ParsePose(recordData, section, pose);
    }

    ResolveAssets(recordData);
    *s_pCurrentPlayerSettingsIndex = -1;
    return 0;
}

uint32_t CPlayerSettings::Count()
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return static_cast<uint32_t>(m_Records.size());
}

int CPlayerSettings::NameToIndex(const char* className)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return FindIndex(className);
}

char* CPlayerSettings::GetPose(int index, int pose)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    std::byte* record = index >= 0 ? At(static_cast<size_t>(index)) : nullptr;
    if (!record || pose < 0 || pose >= static_cast<int>(POSE_NAMES.size()))
        return nullptr;
    return reinterpret_cast<char*>(record + POSE_OFFSET + POSE_SIZE * pose);
}

char* CPlayerSettings::GetRecord(int index)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    std::byte* record = index >= 0 ? At(static_cast<size_t>(index)) : nullptr;
    return reinterpret_cast<char*>(record);
}

SQRESULT CPlayerSettings::IndexToName(HSQUIRRELVM sqvm)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const SQInteger index = g_pSquirrel[ScriptContext::CLIENT]->getinteger(sqvm, 1);
    std::byte* record = index >= 0 ? At(static_cast<size_t>(index)) : nullptr;
    if (!record)
    {
        g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "Player settings index is out of range");
        return SQRESULT_ERROR;
    }

    g_pSquirrel[ScriptContext::CLIENT]->pushstring(sqvm, reinterpret_cast<const char*>(record));
    return static_cast<SQRESULT>(1);
}

const char** CPlayerSettings::GetModelField(const char** result, const char* className, const char* fieldName)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const int index = FindIndex(className);
    std::byte* record = index >= 0 ? At(static_cast<size_t>(index)) : nullptr;
    if (!record)
    {
        s_PlayerSettingsLog("Given classname '%s' is invalid", className);
        *result = "";
        return result;
    }

    int fieldType = 0;
    const void* fieldValue = nullptr;
    const bool found = s_FindPlayerSettingsField(s_pPlayerSettingsGlobalFieldTable, record, fieldName, &fieldType, &fieldValue) ||
                       s_FindPlayerSettingsField(s_pPlayerSettingsSecondaryFieldTable, record, fieldName, &fieldType, &fieldValue);

    if (!found)
    {
        s_PlayerSettingsLog("Field not found: %s", fieldName);
        *result = "";
    }
    else if (fieldType != 2)
    {
        s_PlayerSettingsLog("Field %s not a MODELNAME...type %d", fieldName, fieldType);
        *result = "";
    }
    else
    {
        *result = static_cast<const char*>(fieldValue);
    }

    return result;
}

SQRESULT CPlayerSettings::GetField(HSQUIRRELVM sqvm)
{
    if (!IsStringArgument(sqvm, 1))
    {
        s_PlayerSettingsLog("Classname must be a string");
        return static_cast<SQRESULT>(0);
    }

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const int index = GetIndexArgument(sqvm);
    std::byte* record = index >= 0 ? At(static_cast<size_t>(index)) : nullptr;
    if (!record)
    {
        const char* className = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
        s_PlayerSettingsLog("Given classname '%s' is invalid", className);
        return static_cast<SQRESULT>(0);
    }

    const char* fieldName = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 2);
    if (s_GetSquirrelArgumentCount(sqvm) >= 4)
    {
        int pose = 0;
        if (s_GetSquirrelInteger(sqvm, 4, &pose) < 0 || static_cast<unsigned int>(pose) >= POSE_NAMES.size())
        {
            s_PlayerSettingsLog("Optional parameter must be a valid PLAYERPOSE_ constant.");
            return static_cast<SQRESULT>(0);
        }

        const std::byte* poseRecord = record + POSE_OFFSET + POSE_SIZE * pose;
        int fieldType = 0;
        const void* fieldValue = nullptr;
        const bool found = s_FindPlayerSettingsField(s_pPlayerSettingsPoseFieldTable, poseRecord, fieldName, &fieldType, &fieldValue);
        if (found)
        {
            s_PushPlayerSettingsField(sqvm, true, fieldType, fieldValue);
            return static_cast<SQRESULT>(1);
        }
    }

    if (!s_PushPlayerSettingsGlobalField(sqvm, record, fieldName))
        s_PlayerSettingsLog("Field not found: %s", fieldName);
    return static_cast<SQRESULT>(1);
}

SQRESULT CPlayerSettings::GetHealth(HSQUIRRELVM sqvm)
{
    return PushFloat<HEALTH_OFFSET>(sqvm);
}

SQRESULT CPlayerSettings::GetHealthShield(HSQUIRRELVM sqvm)
{
    return PushFloat<HEALTH_SHIELD_OFFSET>(sqvm);
}

SQRESULT CPlayerSettings::GetHealthDoomed(HSQUIRRELVM sqvm)
{
    return PushFloat<HEALTH_DOOMED_OFFSET>(sqvm);
}

SQRESULT CPlayerSettings::GetHealthPerSegment(HSQUIRRELVM sqvm)
{
    return PushFloat<HEALTH_PER_SEGMENT_OFFSET>(sqvm);
}

DECLARE_HOOK(PlayerSettings_Reset, client.dll + 0x19E060, [](auto&) -> void* { return g_PlayerSettings.Reset(); })

DECLARE_HOOK(PlayerSettings_LoadRecord, client.dll + 0x19E230,
             [](auto&, void* keyValues) -> int64_t { return g_PlayerSettings.LoadRecord(keyValues); })

DECLARE_HOOK(PlayerSettings_Count, client.dll + 0x19EC10, [](auto&) -> uint32_t { return g_PlayerSettings.Count(); })

DECLARE_HOOK(PlayerSettings_NameToIndex, client.dll + 0x19EC20,
             [](auto&, const char* className) -> int { return g_PlayerSettings.NameToIndex(className); })

DECLARE_HOOK(PlayerSettings_GetPose, client.dll + 0x19ED50, [](auto&, int index, int pose) -> char* { return g_PlayerSettings.GetPose(index, pose); })

DECLARE_HOOK(PlayerSettings_GetRecord, client.dll + 0x19ED80, [](auto&, int index) -> char* { return g_PlayerSettings.GetRecord(index); })

DECLARE_HOOK(PlayerSettings_IndexToName, client.dll + 0x19B2C0,
             [](auto&, HSQUIRRELVM sqvm) -> SQRESULT { return g_PlayerSettings.IndexToName(sqvm); })

DECLARE_HOOK(PlayerSettings_GetModelField, client.dll + 0x19DA20,
             [](auto&, const char** result, const char* className, const char* fieldName) -> const char**
{ return g_PlayerSettings.GetModelField(result, className, fieldName); })

DECLARE_HOOK(PlayerSettings_GetField, client.dll + 0x19FC20, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT { return g_PlayerSettings.GetField(sqvm); })

DECLARE_HOOK(PlayerSettings_GetHealth, client.dll + 0x19FDB0, [](auto&, HSQUIRRELVM sqvm) -> SQRESULT { return g_PlayerSettings.GetHealth(sqvm); })

DECLARE_HOOK(PlayerSettings_GetHealthShield, client.dll + 0x19FE40,
             [](auto&, HSQUIRRELVM sqvm) -> SQRESULT { return g_PlayerSettings.GetHealthShield(sqvm); })

DECLARE_HOOK(PlayerSettings_GetHealthDoomed, client.dll + 0x19FED0,
             [](auto&, HSQUIRRELVM sqvm) -> SQRESULT { return g_PlayerSettings.GetHealthDoomed(sqvm); })

DECLARE_HOOK(PlayerSettings_GetHealthPerSegment, client.dll + 0x19FF60,
             [](auto&, HSQUIRRELVM sqvm) -> SQRESULT { return g_PlayerSettings.GetHealthPerSegment(sqvm); })

ON_DLL_LOAD_CLIENT("client.dll", PlayerSettings, [](CModule module)
{
    s_pPlayerSettingsCount = module.Offset(0xDC97B8).RCast<int*>();
    s_pCurrentPlayerSettingsIndex = module.Offset(0xDC97BC).RCast<int*>();
    s_pDefaultPlayerSettingsIndex = module.Offset(0xDC97C0).RCast<int*>();
    s_ReleasePlayerSettingsKeyValues = module.Offset(0x7305B0).RCast<decltype(s_ReleasePlayerSettingsKeyValues)>();
    s_GetFirstPlayerSettingsKeyValuesChild = module.Offset(0x72AAC0).RCast<decltype(s_GetFirstPlayerSettingsKeyValuesChild)>();
    s_GetNextPlayerSettingsKeyValuesSibling = module.Offset(0x72AD50).RCast<decltype(s_GetNextPlayerSettingsKeyValuesSibling)>();
    s_GetPlayerSettingsKeyValuesName = module.Offset(0x72ACD0).RCast<decltype(s_GetPlayerSettingsKeyValuesName)>();
    s_FindPlayerSettingsKeyValuesChild = module.Offset(0x72A3F0).RCast<decltype(s_FindPlayerSettingsKeyValuesChild)>();
    s_ParsePlayerSettingsFields = module.Offset(0x56E020).RCast<decltype(s_ParsePlayerSettingsFields)>();
    s_ParsePlayerSettingsGlobal = module.Offset(0x19E530).RCast<decltype(s_ParsePlayerSettingsGlobal)>();
    s_ParsePlayerSettingsModifier = module.Offset(0x19D0F0).RCast<decltype(s_ParsePlayerSettingsModifier)>();
    s_ApplyPlayerSettingsModifiers = module.Offset(0x19D2B0).RCast<decltype(s_ApplyPlayerSettingsModifiers)>();
    s_ParsePlayerSettingsPoseMods = module.Offset(0x19D6B0).RCast<decltype(s_ParsePlayerSettingsPoseMods)>();
    s_ResolvePlayerSettingsModel = module.Offset(0x195CD0).RCast<decltype(s_ResolvePlayerSettingsModel)>();
    s_LoadPlayerSettingsRui = module.Offset(0x190E60).RCast<decltype(s_LoadPlayerSettingsRui)>();
    s_LoadPlayerSettingsAsset = module.Offset(0x190B40).RCast<decltype(s_LoadPlayerSettingsAsset)>();
    s_FindPlayerSettingsField = module.Offset(0x56DFA0).RCast<decltype(s_FindPlayerSettingsField)>();
    s_PushPlayerSettingsField = module.Offset(0x19F000).RCast<decltype(s_PushPlayerSettingsField)>();
    s_PushPlayerSettingsGlobalField = module.Offset(0x19EB80).RCast<decltype(s_PushPlayerSettingsGlobalField)>();
    s_GetSquirrelArgumentCount = module.Offset(0x6F60).RCast<decltype(s_GetSquirrelArgumentCount)>();
    s_GetSquirrelInteger = module.Offset(0x5D50).RCast<decltype(s_GetSquirrelInteger)>();
    s_PlayerSettingsLog = module.Offset(0x39DAA0).RCast<decltype(s_PlayerSettingsLog)>();
    s_pPlayerSettingsGlobalFieldTable = module.Offset(0xB099C0).RCast<const void*>();
    s_pPlayerSettingsSecondaryFieldTable = module.Offset(0xB0E490).RCast<const void*>();
    s_pPlayerSettingsPoseFieldTable = module.Offset(0xB0E610).RCast<const void*>();
    DISPATCH_MODULE(PlayerSettingsHooks)
})
