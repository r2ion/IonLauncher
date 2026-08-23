#pragma once
#include "tier1/uniqueid.h"

using DmObjectId_t = UniqueId_t;


// Source DMX attribute type identifiers. These values are serialized directly by
// binary PCF v2 files and therefore must retain the retail ordering.
enum DmAttributeType_t
{
	AT_UNKNOWN = 0,

	AT_FIRST_VALUE_TYPE,

	AT_ELEMENT = AT_FIRST_VALUE_TYPE,
	AT_INT,
	AT_FLOAT,
	AT_BOOL,
	AT_STRING,
	AT_VOID,
	AT_TIME,
	AT_COLOR,
	AT_VECTOR2,
	AT_VECTOR3,
	AT_VECTOR4,
	AT_QANGLE,
	AT_QUATERNION,
	AT_VMATRIX,

	AT_FIRST_ARRAY_TYPE,

	AT_ELEMENT_ARRAY = AT_FIRST_ARRAY_TYPE,
	AT_INT_ARRAY,
	AT_FLOAT_ARRAY,
	AT_BOOL_ARRAY,
	AT_STRING_ARRAY,
	AT_VOID_ARRAY,
	AT_TIME_ARRAY,
	AT_COLOR_ARRAY,
	AT_VECTOR2_ARRAY,
	AT_VECTOR3_ARRAY,
	AT_VECTOR4_ARRAY,
	AT_QANGLE_ARRAY,
	AT_QUATERNION_ARRAY,
	AT_VMATRIX_ARRAY,
	AT_TYPE_COUNT,

	AT_TYPE_INVALID,
};

inline bool IsValueType(DmAttributeType_t type)
{
	return type >= AT_FIRST_VALUE_TYPE && type < AT_FIRST_ARRAY_TYPE;
}

inline bool IsArrayType(DmAttributeType_t type)
{
	return type >= AT_FIRST_ARRAY_TYPE && type < AT_TYPE_COUNT;
}

inline DmAttributeType_t ValueTypeToArrayType(DmAttributeType_t type)
{
	return static_cast<DmAttributeType_t>((type - AT_FIRST_VALUE_TYPE) + AT_FIRST_ARRAY_TYPE);
}

inline DmAttributeType_t ArrayTypeToValueType(DmAttributeType_t type)
{
	return static_cast<DmAttributeType_t>((type - AT_FIRST_ARRAY_TYPE) + AT_FIRST_VALUE_TYPE);
}

inline bool IsTopological(DmAttributeType_t type)
{
	return type == AT_ELEMENT || type == AT_ELEMENT_ARRAY;
}

inline int NumComponents(DmAttributeType_t type)
{
	switch (type)
	{
	case AT_BOOL:
	case AT_INT:
	case AT_FLOAT:
	case AT_TIME:
		return 1;
	case AT_VECTOR2:
		return 2;
	case AT_VECTOR3:
	case AT_QANGLE:
		return 3;
	case AT_COLOR:
	case AT_VECTOR4:
	case AT_QUATERNION:
		return 4;
	case AT_VMATRIX:
		return 16;
	default:
		return 0;
	}
}
