#pragma once

#include <datamap.h>
#include "engine/basehandle.h"
#include "mathlib/color.h"
#include "mathlib/vector.h"

#ifdef variant_t
#undef variant_t
#endif

class CBaseEntity;

class variant_t
{
  public:
    variant_t() : vecVal{}, fieldType(FIELD_VOID) {}

    fieldtype_t FieldType() const { return fieldType; }
    bool Bool() const { return fieldType == FIELD_BOOLEAN ? bVal : false; }
    int Int() const { return fieldType == FIELD_INTEGER ? iVal : 0; }
    float Float() const { return fieldType == FIELD_FLOAT ? flVal : 0.0f; }
    const char* StringID() const { return fieldType == FIELD_STRING ? iszVal : nullptr; }
    CBaseHandle Entity() const { return fieldType == FIELD_EHANDLE ? eVal : CBaseHandle{}; }
    color32 Color32() const { return rgbaVal; }
    void Vector3D(::Vector3D& value) const
    {
        value = fieldType == FIELD_VECTOR || fieldType == FIELD_POSITION_VECTOR
            ? ::Vector3D(vecVal[0], vecVal[1], vecVal[2]) : ::Vector3D{};
    }

    void SetBool(bool value) { bVal = value; fieldType = FIELD_BOOLEAN; }
    void SetInt(int value) { iVal = value; fieldType = FIELD_INTEGER; }
    void SetFloat(float value) { flVal = value; fieldType = FIELD_FLOAT; }
    void SetString(const char* value) { iszVal = value; fieldType = FIELD_STRING; }
    void SetEntity(CBaseHandle value) { eVal = value; fieldType = FIELD_EHANDLE; }
    void SetColor32(color32 value) { rgbaVal = value; fieldType = FIELD_COLOR32; }
    void SetVector3D(const ::Vector3D& value)
    {
        vecVal[0] = value.x;
        vecVal[1] = value.y;
        vecVal[2] = value.z;
        fieldType = FIELD_VECTOR;
    }
    void SetPositionVector3D(const ::Vector3D& value)
    {
        SetVector3D(value);
        fieldType = FIELD_POSITION_VECTOR;
    }

  private:
    union
    {
        bool bVal;
        const char* iszVal;
        int iVal;
        float flVal;
        float vecVal[3];
        color32 rgbaVal;
    };
    CBaseHandle eVal;
    fieldtype_t fieldType;
};

struct inputdata_t
{
    CBaseEntity* pActivator;
    CBaseEntity* pCaller;
    variant_t value;
    int nOutputID;
};

static_assert(sizeof(variant_t) == 0x18);
static_assert(alignof(variant_t) == 0x8);
static_assert(offsetof(inputdata_t, pActivator) == 0x0);
static_assert(offsetof(inputdata_t, pCaller) == 0x8);
static_assert(offsetof(inputdata_t, value) == 0x10);
static_assert(offsetof(inputdata_t, nOutputID) == 0x28);
static_assert(sizeof(inputdata_t) == 0x30);
