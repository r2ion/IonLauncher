//====== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. =======
#ifndef IVSCRIPT_H
#define IVSCRIPT_H
#ifdef _WIN32
#pragma once
#endif

#include "datamap.h"
#include "mathlib/vector.h"
#include "tier0/dbg.h"
#include "tier0/memstd.h"
#include "tier1/utlvector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

struct HSCRIPT__;
typedef HSCRIPT__* HSCRIPT;
#define INVALID_HSCRIPT ((HSCRIPT)-1)
#define GAME_SCRIPT_PATH "scripts/vscripts/"
#define GAME_SCRIPT_COMPILELIST GAME_SCRIPT_PATH "scripts.rson"

inline bool IsValid(HSCRIPT hScript) { return hScript && hScript != INVALID_HSCRIPT; }

typedef int ScriptDataType_t;
enum ScriptStatus_t
{
    SCRIPT_ERROR = -1,
    SCRIPT_DONE,
    SCRIPT_RUNNING
};

enum ExtendedFieldType
{
    FIELD_TYPEUNKNOWN = FIELD_TYPECOUNT,
    FIELD_CSTRING,
    FIELD_HSCRIPT,
    FIELD_VARIANT,
    FIELD_TYPEUNKNOWN3,
    FIELD_ARRAY,
    FIELD_TABLE,
    FIELD_TYPEUNKNOWN6,
    FIELD_ASSET
};

inline const char* ScriptFieldTypeName(int type)
{
    switch (type)
    {
    case FIELD_VOID: return "void";
    case FIELD_FLOAT: return "float";
    case FIELD_VECTOR: return "vector";
    case FIELD_INTEGER: return "integer";
    case FIELD_BOOLEAN: return "boolean";
    case FIELD_CHARACTER: return "character";
    case FIELD_EHANDLE: return "entity";
    case FIELD_CSTRING: return "cstring";
    case FIELD_HSCRIPT: return "hscript";
    case FIELD_VARIANT: return "variant";
    case FIELD_ARRAY: return "array";
    case FIELD_TABLE: return "table";
    case FIELD_ASSET: return "asset";
    default: return "unknown_script_type";
    }
}

struct ScriptAsset
{
    ScriptAsset() : _str(nullptr) {}
    explicit ScriptAsset(const char* str) : _str(str) {}
    operator const char*() const { return _str; }
    const char* _str;
};

struct ScriptStringOrNull
{
    ScriptStringOrNull() : _str(nullptr) {}
    explicit ScriptStringOrNull(const char* str) : _str(str) {}
    operator const char*() const { return _str; }
    const char* _str;
};

struct ScriptVariant_t
{
    enum SVFlags_t
    {
        SV_FREE = 0x01,
        SV_RELEASE = 0x02
    };

    ScriptVariant_t() : m_vector{0, 0, 0}, m_flags(0), m_type(FIELD_VOID) {}
    ScriptVariant_t(int value) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(float value) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(char value) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(bool value) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(HSCRIPT value) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(ScriptAsset value) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(const Vector3D& value, bool copy = false) : ScriptVariant_t() { *this = value; }
    ScriptVariant_t(const Vector3D* value, bool copy = false) : ScriptVariant_t() { *this = *value; }
    ScriptVariant_t(const char* value, bool copy = false) : ScriptVariant_t()
    {
        m_type = FIELD_CSTRING;
        m_pszString = copy && value ? _strdup_base(value) : value;
        if (copy && value)
            m_flags = SV_FREE;
    }

    ScriptVariant_t(const ScriptVariant_t& value) : ScriptVariant_t() { value.AssignTo(this); }
    ScriptVariant_t(ScriptVariant_t&& value) noexcept : ScriptVariant_t()
    {
        std::memcpy(this, &value, sizeof(*this));
        value.m_flags = 0;
        value.m_type = FIELD_VOID;
        value.m_dataPointer = nullptr;
    }
    ~ScriptVariant_t() { FreeVariantMemory(); }

    ScriptVariant_t& operator=(const ScriptVariant_t& value)
    {
        if (this != &value)
            value.AssignTo(this);
        return *this;
    }
    ScriptVariant_t& operator=(ScriptVariant_t&& value) noexcept
    {
        if (this != &value)
        {
            FreeVariantMemory();
            std::memcpy(this, &value, sizeof(*this));
            value.m_flags = 0;
            value.m_type = FIELD_VOID;
            value.m_dataPointer = nullptr;
        }
        return *this;
    }

    bool IsNull() const { return m_type == FIELD_VOID; }
    ScriptDataType_t GetType() const { return m_type; }

    operator int() const { Assert(m_type == FIELD_INTEGER); return m_int; }
    operator std::int64_t() const { Assert(m_type == FIELD_INTEGER); return m_int; }
    operator float() const { Assert(m_type == FIELD_FLOAT); return m_float; }
    operator char() const { Assert(m_type == FIELD_CHARACTER); return m_char; }
    operator bool() const { Assert(m_type == FIELD_BOOLEAN); return m_bool; }
    operator HSCRIPT() const { Assert(m_type == FIELD_HSCRIPT); return m_hScript; }
    operator const char*() const { Assert(m_type == FIELD_CSTRING); return m_pszString ? m_pszString : ""; }
    operator const Vector3D&() const { Assert(m_type == FIELD_VECTOR); return m_Vec3D; }
    operator ScriptAsset() const { Assert(m_type == FIELD_ASSET); return ScriptAsset(m_pszString); }

    void operator=(int value) { FreeVariantMemory(); m_type = FIELD_INTEGER; m_int = value; }
    void operator=(float value) { FreeVariantMemory(); m_type = FIELD_FLOAT; m_float = value; }
    void operator=(char value) { FreeVariantMemory(); m_type = FIELD_CHARACTER; m_char = value; }
    void operator=(bool value) { FreeVariantMemory(); m_type = FIELD_BOOLEAN; m_bool = value; }
    void operator=(HSCRIPT value) { FreeVariantMemory(); m_type = FIELD_HSCRIPT; m_hScript = value; }
    void operator=(ScriptAsset value) { FreeVariantMemory(); m_type = FIELD_ASSET; m_pszString = value._str; }
    void operator=(const Vector3D& value) { FreeVariantMemory(); m_type = FIELD_VECTOR; m_Vec3D = value; }
    void operator=(const Vector3D* value) { *this = *value; }
    void operator=(const char* value) { FreeVariantMemory(); m_type = FIELD_CSTRING; m_pszString = value; }

    void FreeVariantMemory()
    {
        if (m_flags & SV_FREE)
        {
            if (m_type == FIELD_CSTRING || m_type == FIELD_HSCRIPT || m_type == FIELD_ASSET)
                _free_base(m_dataPointer);
            m_flags &= ~SV_FREE;
            m_dataPointer = nullptr;
        }
    }
    void Free() { FreeVariantMemory(); }

    template <typename T> T Get() const
    {
        T value{};
        AssignTo(&value);
        return value;
    }

    bool AssignTo(float* dest) const
    {
        switch (m_type)
        {
        case FIELD_VOID: *dest = 0; return false;
        case FIELD_INTEGER: *dest = static_cast<float>(m_int); return true;
        case FIELD_FLOAT: *dest = m_float; return true;
        case FIELD_BOOLEAN: *dest = static_cast<float>(m_bool); return true;
        default: return false;
        }
    }
    bool AssignTo(int* dest) const
    {
        switch (m_type)
        {
        case FIELD_VOID: *dest = 0; return false;
        case FIELD_INTEGER: *dest = m_int; return true;
        case FIELD_FLOAT: *dest = static_cast<int>(m_float); return true;
        case FIELD_BOOLEAN: *dest = m_bool; return true;
        default: return false;
        }
    }
    bool AssignTo(bool* dest) const
    {
        switch (m_type)
        {
        case FIELD_VOID: *dest = false; return false;
        case FIELD_INTEGER: *dest = m_int != 0; return true;
        case FIELD_FLOAT: *dest = m_float != 0; return true;
        case FIELD_BOOLEAN: *dest = m_bool; return true;
        default: return false;
        }
    }
    bool AssignTo(Vector3D* dest) const
    {
        if (m_type != FIELD_VECTOR)
            return false;
        *dest = m_Vec3D;
        return true;
    }
    bool AssignTo(const char** dest) const
    {
        if (m_type != FIELD_CSTRING)
            return false;
        *dest = m_pszString;
        return true;
    }
    bool AssignTo(char** dest) const
    {
        if (m_type != FIELD_CSTRING)
            return false;
        *dest = m_pszString ? _strdup_base(m_pszString) : nullptr;
        return true;
    }
    bool AssignTo(HSCRIPT* dest) const
    {
        if (m_type != FIELD_HSCRIPT)
            return false;
        *dest = m_hScript;
        return true;
    }
    bool AssignTo(ScriptAsset* dest) const
    {
        if (m_type != FIELD_ASSET)
            return false;
        *dest = ScriptAsset(m_pszString);
        return true;
    }
    void AssignTo(ScriptVariant_t* dest) const
    {
        if (dest == this)
            return;
        const char* copy = (m_type == FIELD_CSTRING || m_type == FIELD_ASSET) && m_pszString
            ? _strdup_base(m_pszString) : nullptr;
        dest->FreeVariantMemory();
        std::memcpy(dest, this, sizeof(*this));
        dest->m_flags = 0;
        if (copy)
        {
            dest->m_pszString = copy;
            dest->m_flags = SV_FREE;
        }
    }

    union
    {
        int m_int;
        float m_float;
        const char* m_pszString;
        float m_vector[3];
        Vector3D m_Vec3D;
        char m_char;
        bool m_bool;
        HSCRIPT m_hScript;
        void* m_dataPointer;
    };
    std::int16_t m_flags;
    std::int16_t m_type;
};

struct SQVM;
struct ScriptFuncDescriptor_t
{
    ScriptFuncDescriptor_t()
        : m_pszScriptName(nullptr), m_pszFunction(nullptr), m_pszDescription(nullptr),
          m_signatureReturn(nullptr), m_signatureParams(nullptr), m_oldStyle(true),
          m_varParams(false), m_devOnlyLevel(0), m_paramMask(nullptr),
          m_defaultParamCount(0), m_ReturnType(FIELD_TYPEUNKNOWN)
    {
    }

    void Init(const char* scriptName, const char* nativeName, const char* description,
              const char* returnType, const char* parameters)
    {
        m_pszScriptName = scriptName;
        m_pszFunction = nativeName;
        m_pszDescription = description;
        m_signatureReturn = returnType;
        m_signatureParams = parameters;
        m_oldStyle = false;
    }

    const char* m_pszScriptName;
    const char* m_pszFunction;
    const char* m_pszDescription;
    const char* m_signatureReturn;
    const char* m_signatureParams;
    bool m_oldStyle;
    bool m_varParams;
    int m_devOnlyLevel;
    const char* m_paramMask;
    int m_defaultParamCount;
    ScriptDataType_t m_ReturnType;
    CUtlVector<ScriptDataType_t> m_Parameters;
};

struct ScriptFunctionBinding_t
{
    void Init(const char* scriptName, const char* nativeName, const char* description,
              const char* returnType, const char* parameters, bool isVariadic, int (*function)(SQVM*))
    {
        m_desc.Init(scriptName, nativeName, description, returnType, parameters);
        m_desc.m_varParams = isVariadic;
        m_func = function;
    }

    ScriptFuncDescriptor_t m_desc;
    int (*m_func)(SQVM*);
};

class IScriptInstanceHelper
{
public:
    virtual void* GetProxied(void* instance, ScriptFunctionBinding_t* binding) { return instance; }
    virtual bool ToString(void* instance, char* buffer, int bufferSize) { return false; }
    virtual void* BindOnRead(HSCRIPT script, void* oldInstance, const char* id) { return nullptr; }
};

#define SCRIPT_HIDE "@"
#define SCRIPT_SINGLETON "!"
#define SCRIPT_ALIAS(alias, description) "#" alias ":" description

struct ScriptClassDesc_t
{
    explicit ScriptClassDesc_t(void (*pfnInitializer)())
        : m_pszScriptName(nullptr), m_pszClassname(nullptr), m_pszDescription(nullptr),
          m_pBaseDesc(nullptr), m_initialized(true), m_initializerFunc(pfnInitializer), pHelper(nullptr)
    {
        (*pfnInitializer)();
        ScriptClassDesc_t** ppHead = GetDescList();
        m_pNextDesc = *ppHead;
        *ppHead = this;
    }

    static ScriptClassDesc_t** GetDescList()
    {
        static ScriptClassDesc_t* pHead;
        return &pHead;
    }

    const char* m_pszScriptName;
    const char* m_pszClassname;
    const char* m_pszDescription;
    ScriptClassDesc_t* m_pBaseDesc;
    CUtlVector<ScriptFunctionBinding_t> m_FunctionBindings;
    CUtlVector<ScriptFunctionBinding_t> m_NativeFunctionBindings;
    bool m_initialized;
    void (*m_initializerFunc)();
    IScriptInstanceHelper* pHelper;
    ScriptClassDesc_t* m_pNextDesc;
};

static_assert(sizeof(ScriptAsset) == 8);
static_assert(sizeof(ScriptStringOrNull) == 8);
static_assert(sizeof(ScriptVariant_t) == 0x18);
static_assert(alignof(ScriptVariant_t) == 8);
static_assert(offsetof(ScriptVariant_t, m_flags) == 0x10);
static_assert(offsetof(ScriptVariant_t, m_type) == 0x12);
static_assert(FIELD_CSTRING == 33 && FIELD_HSCRIPT == 34 && FIELD_ASSET == 40);
static_assert(sizeof(ScriptFuncDescriptor_t) == 0x60);
static_assert(sizeof(ScriptFunctionBinding_t) == 0x68);
static_assert(sizeof(ScriptClassDesc_t) == 0x80);

#endif // IVSCRIPT_H
