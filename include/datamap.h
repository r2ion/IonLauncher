#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

typedef enum _fieldtypes : std::int32_t
{
	FIELD_VOID = 0,			// No type or value
	FIELD_FLOAT,			// Any floating point value
	FIELD_STRING,			// A string ID (return from ALLOC_STRING)
	FIELD_VECTOR,			// Any vector, QAngle, or AngularImpulse
	FIELD_QUATERNION,		// A quaternion
	FIELD_INTEGER,			// Any integer or enum
	FIELD_BOOLEAN,			// boolean, implemented as an int, I may use this as a hint for compression
	FIELD_SHORT,			// 2 byte integer
	FIELD_CHARACTER,		// a byte
	FIELD_COLOR32,			// 8-bit per channel r,g,b,a (32bit color)
	FIELD_EMBEDDED,			// an embedded object with a datadesc, recursively traverse and embedded class/structure based on an additional typedescription
	FIELD_CUSTOM,			// special type that contains function pointers to it's read/write/parse functions

	FIELD_CLASSPTR,			// CBaseEntity*
	FIELD_EHANDLE,			// Entity handle
	FIELD_EDICT,			// edict_t

	FIELD_POSITION_VECTOR,	// A world coordinate (these are fixed up across level transitions automagically)
	FIELD_TIME,				// a floating point time (these are fixed up automatically too!)
	FIELD_TICK,				// an integer tick count (fixed up similarly to time)
	FIELD_MODELNAME,		// Engine string that is a model name (needs precache)
	FIELD_SOUNDNAME,		// Engine string that is a sound name (needs precache)

	FIELD_INPUT,			// a list of inputed data fields (all derived from CMultiInputVar)
	FIELD_FUNCTION,			// A class function pointer (Think, Use, etc)

	FIELD_VMATRIX,			// a vmatrix (output coords are NOT worldspace)

	// NOTE: Use float arrays for local transformations that don't need to be fixed up.
	FIELD_VMATRIX_WORLDSPACE,// A VMatrix that maps some local space to world space (translation is fixed up on level transitions)
	FIELD_MATRIX3X4_WORLDSPACE,	// matrix3x4_t that maps some local space to world space (translation is fixed up on level transitions)

	FIELD_INTERVAL,			// a start and range floating point interval ( e.g., 3.2->3.6 == 3.2 and 0.4 )
	FIELD_MODELINDEX,		// a model index
	FIELD_MATERIALINDEX,	// a material index (using the material precache string table)

	FIELD_VECTOR2D,			// 2 floats
	FIELD_INTEGER64,		// 64bit integer

	FIELD_VECTOR4D,			// 4 floats
	FIELD_UNTYPED,			// Untyped field, usually engine class pointers, intermediate types like memhandle_t, interface classes like IClientNetworkable*.

	FIELD_TYPECOUNT,		// MUST BE LAST
} fieldtype_t;

struct datamap_t;
struct typedescription_t;
class ISaveRestoreOps;

inline constexpr std::size_t TD_OFFSET_NORMAL = 0;
inline constexpr std::size_t TD_OFFSET_PACKED = 1;
inline constexpr std::size_t TD_OFFSET_COUNT = 2;

struct typedescription_t
{
	fieldtype_t fieldType;
	const char* fieldName;
	std::int32_t fieldOffset;
	std::uint16_t fieldSize;
	std::int16_t flags;
	const char* externalName;
	ISaveRestoreOps* pSaveRestoreOps;
	std::byte inputFunc[0x18];
	datamap_t* td;
	std::int32_t fieldSizeInBytes;
	std::uint32_t reserved4C;
	std::int32_t fieldAlignment;
	typedescription_t* override_field;
	std::int32_t override_count;
	float fieldTolerance;
	std::int32_t flatOffset[TD_OFFSET_COUNT];
	std::uint16_t flatGroup;
	std::byte reserved72[0x6];
};

struct datamap_t
{
	typedescription_t* dataDesc;
	std::int32_t dataNumFields;
	const char* dataClassName;
	std::int32_t dataSize;
	std::int32_t dataAlignment;
	std::uint64_t reserved20;
	datamap_t* baseMap;
};

static_assert(sizeof(fieldtype_t) == 0x4);
static_assert(FIELD_TYPECOUNT == 32);

static_assert(sizeof(typedescription_t) == 0x78);
static_assert(alignof(typedescription_t) == 0x8);
static_assert(offsetof(typedescription_t, fieldType) == 0x0);
static_assert(offsetof(typedescription_t, fieldName) == 0x8);
static_assert(offsetof(typedescription_t, fieldOffset) == 0x10);
static_assert(offsetof(typedescription_t, fieldSize) == 0x14);
static_assert(offsetof(typedescription_t, flags) == 0x16);
static_assert(offsetof(typedescription_t, externalName) == 0x18);
static_assert(offsetof(typedescription_t, pSaveRestoreOps) == 0x20);
static_assert(offsetof(typedescription_t, inputFunc) == 0x28);
static_assert(offsetof(typedescription_t, td) == 0x40);
static_assert(offsetof(typedescription_t, fieldSizeInBytes) == 0x48);
static_assert(offsetof(typedescription_t, reserved4C) == 0x4C);
static_assert(offsetof(typedescription_t, fieldAlignment) == 0x50);
static_assert(offsetof(typedescription_t, override_field) == 0x58);
static_assert(offsetof(typedescription_t, override_count) == 0x60);
static_assert(offsetof(typedescription_t, fieldTolerance) == 0x64);
static_assert(offsetof(typedescription_t, flatOffset) == 0x68);
static_assert(offsetof(typedescription_t, flatGroup) == 0x70);
static_assert(offsetof(typedescription_t, reserved72) == 0x72);

static_assert(sizeof(datamap_t) == 0x30);
static_assert(alignof(datamap_t) == 0x8);
static_assert(offsetof(datamap_t, dataDesc) == 0x0);
static_assert(offsetof(datamap_t, dataNumFields) == 0x8);
static_assert(offsetof(datamap_t, dataClassName) == 0x10);
static_assert(offsetof(datamap_t, dataSize) == 0x18);
static_assert(offsetof(datamap_t, dataAlignment) == 0x1C);
static_assert(offsetof(datamap_t, reserved20) == 0x20);
static_assert(offsetof(datamap_t, baseMap) == 0x28);

namespace DataMapHandler
{
	std::int32_t FindOffsetForField(datamap_t* map, std::string_view fieldName);
	typedescription_t* FindFieldInDataMap(datamap_t* map, std::string_view fieldName);
}

#define DATAMAP_VAR(type, name, datamap, varname) \
	type& name() { \
		static const std::int32_t _##name = DataMapHandler::FindOffsetForField(datamap, varname); \
		return *reinterpret_cast<type*>(reinterpret_cast<std::uintptr_t>(this) + _##name); \
	}
