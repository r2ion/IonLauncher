#pragma once

#include <cstddef>
#include <cstdint>

enum class SendPropType : int
{
	Int = 0,
	Float,
	Vector,
	VectorXY,
	String,
	Array,
	Quaternion,
	Int64,
	Ticks,
	Time,
	DataTable,
	Count,
};

enum class RecvPropFlags : int
{
	None = 0,
	Exclude = 1 << 1,
	NestedDataTable = 1 << 4,
};

union DVariantValue
{
	std::int32_t m_Int;
	float m_Float;
	const char* m_String;
	float m_Vector[3];
	float m_Quaternion[4];
	std::int64_t m_Int64;
};

struct DVariant
{
	DVariantValue m_Value;
	SendPropType m_Type;
};

struct CRecvPropMetadata;
struct CRecvProp;
struct CRecvTable;
struct CRecvProxyData;

using RecvVarProxyFn = void (*)(const CRecvProxyData* data, void* struct_base, void* output);
using ArrayLengthRecvProxyFn = void (*)(void* struct_base, int object_id, int array_length);
using DataTableRecvVarProxyFn =
	void (*)(const CRecvProp* prop, void** output, void* data, int object_id);

using ArrayLengthSendProxyFn = int (*)(const void* struct_base, int object_id);
using SendVarProxyFn = void (*)(const CRecvPropMetadata* prop, const void* struct_base,
	const void* data, DVariant* output, int element_index, int object_id);
using SendTableProxyFn = void* (*)(const CRecvPropMetadata* prop, const void* struct_base,
	const void* data, void* recipients, int object_id);

// The receive decoder retains the matching send-property metadata needed by the
// type-specific codecs. Retail records are 0x80 bytes.
struct CRecvPropMetadata
{
	SendPropType m_Type;
	int m_NumBits;
	std::uint32_t m_Unknown0008;
	std::uint32_t m_Unknown000C;
	float m_LowValue;
	float m_HighValue;
	CRecvPropMetadata* m_ArrayElementMetadata;
	ArrayLengthSendProxyFn m_ArrayLengthProxy;
	int m_ArrayMaxElements;
	int m_ElementStride;
	const char* m_ExcludeTableName;
	const char* m_ParentArrayPropName;
	const char* m_VarName;
	std::uint32_t m_Unknown0048;
	float m_HighLowMultiplier;
	std::uint32_t m_Unknown0050;
	std::uint32_t m_Flags;
	SendVarProxyFn m_ProxyFn;
	SendTableProxyFn m_DataTableProxyFn;
	void* m_DataTable;
	int m_Offset;
	std::uint32_t m_Unknown0074;
	int m_FlattenedIndex;
	std::uint32_t m_Unknown007C;
};

struct CRecvProp
{
	SendPropType m_Type;
	int m_Offset;
	int m_NestedOffset;
	int m_DataSize;
	std::uint32_t m_Unknown0010;
	RecvPropFlags m_Flags;
	int m_StringBufferSize;
	std::uint32_t m_Pad001C;
	CRecvTable* m_DataTable;
	const char* m_VarName;
	bool m_InsideArray;
	std::uint8_t m_Pad0031[7];
	CRecvProp* m_ArrayProp;
	ArrayLengthRecvProxyFn m_ArrayLengthProxy;
	RecvVarProxyFn m_ProxyFn;
	DataTableRecvVarProxyFn m_DataTableProxyFn;
	int m_ElementStride;
	int m_ElementCount;
	const char* m_ParentArrayPropName;
};

template <typename T> struct CRecvTableVector
{
	T* m_Memory;
	int m_AllocationCount;
	int m_GrowSize;
	T* m_Elements;
	int m_Size;
	std::uint32_t m_Pad001C;
};

class CSendTablePrecalc
{
public:
	virtual ~CSendTablePrecalc() = default;
	int m_Unknown0008;
	std::uint32_t m_Pad000C;
	CRecvPropMetadata** m_FlatPropMetadata;
	int m_FlatPropMetadataCount;
	std::uint32_t m_Pad001C;
	CRecvPropMetadata** m_DataTableProps;
	int m_DataTablePropCount;
	std::uint32_t m_Pad002C;
	CRecvPropMetadata** m_ProxyProps;
	int m_ProxyPropCount;
	std::uint32_t m_Pad003C;
	void* m_SendTable;
	void* m_Unknown0048;
	CRecvTableVector<int> m_PropProxyIndices;
	int m_FlattenedPropProxyIndices[4094];
};

struct CRecvTableDecoder
{
	CRecvTable* m_RecvTable;
	void* m_SendTable;
	CSendTablePrecalc m_Precalc;
	CRecvTableVector<CRecvProp*> m_FlatRecvProps;
	CRecvTableVector<int*> m_FlatPropStackIndices;
	void* m_DebugInfo;
};

struct CRecvTable
{
	int m_DecodeStackIndex;
	std::uint32_t m_Pad0004;
	CRecvProp** m_Props;
	int m_PropCount;
	std::uint32_t m_Pad0014;
	CRecvProp* m_ChildTableProps[32];
	int m_ChildTableCount;
	std::uint32_t m_Pad011C;
	CRecvTableDecoder* m_Decoder;
	const char* m_NetTableName;
	bool m_Initialized;
	bool m_InMainList;
	std::uint8_t m_Pad0132[6];
	const CRecvTable* m_SourceTable;
};

struct CRecvProxyData
{
	std::uint8_t m_Unknown0000[0x10];
	SendPropType m_ValueType;
	std::uint8_t m_Unknown0014[0xC];
	DVariant m_Value;
	const CRecvProp* m_RecvProp;
	int m_ElementIndex;
	std::uint32_t m_ObjectID;
	std::uint8_t m_Unknown0048[8];
	void* m_StructBase;
	void* m_PropOutput;
	const CRecvPropMetadata* m_PropMetadata;
	void* m_ArrayContext;
};

struct CRecvTableStack
{
	bool m_Active;
	std::uint8_t m_Pad0001[7];
	void* m_EntityBase;
	CRecvTable* m_RecvTable;
};

static_assert(sizeof(DVariant) == 0x18);
static_assert(offsetof(DVariant, m_Type) == 0x10);

static_assert(sizeof(CRecvPropMetadata) == 0x80);
static_assert(offsetof(CRecvPropMetadata, m_ArrayElementMetadata) == 0x18);
static_assert(offsetof(CRecvPropMetadata, m_ArrayMaxElements) == 0x28);
static_assert(offsetof(CRecvPropMetadata, m_VarName) == 0x40);
static_assert(offsetof(CRecvPropMetadata, m_Flags) == 0x54);
static_assert(offsetof(CRecvPropMetadata, m_DataTable) == 0x68);
static_assert(offsetof(CRecvPropMetadata, m_FlattenedIndex) == 0x78);

static_assert(sizeof(CRecvProp) == 0x68);
static_assert(offsetof(CRecvProp, m_Flags) == 0x14);
static_assert(offsetof(CRecvProp, m_DataTable) == 0x20);
static_assert(offsetof(CRecvProp, m_VarName) == 0x28);
static_assert(offsetof(CRecvProp, m_InsideArray) == 0x30);
static_assert(offsetof(CRecvProp, m_ArrayProp) == 0x38);
static_assert(offsetof(CRecvProp, m_ProxyFn) == 0x48);
static_assert(offsetof(CRecvProp, m_DataTableProxyFn) == 0x50);
static_assert(offsetof(CRecvProp, m_ElementStride) == 0x58);
static_assert(offsetof(CRecvProp, m_ParentArrayPropName) == 0x60);

static_assert(sizeof(CRecvTableVector<void*>) == 0x20);
static_assert(offsetof(CRecvTableVector<void*>, m_Elements) == 0x10);
static_assert(offsetof(CRecvTableVector<void*>, m_Size) == 0x18);

static_assert(sizeof(CSendTablePrecalc) == 0x4068);
static_assert(offsetof(CSendTablePrecalc, m_Unknown0008) == 0x8);
static_assert(offsetof(CSendTablePrecalc, m_FlatPropMetadata) == 0x10);
static_assert(offsetof(CSendTablePrecalc, m_PropProxyIndices) == 0x50);
static_assert(offsetof(CSendTablePrecalc, m_FlattenedPropProxyIndices) == 0x70);

static_assert(sizeof(CRecvTableDecoder) == 0x40C0);
static_assert(offsetof(CRecvTableDecoder, m_Precalc) == 0x10);
static_assert(offsetof(CRecvTableDecoder, m_FlatRecvProps) == 0x4078);
static_assert(offsetof(CRecvTableDecoder, m_FlatPropStackIndices) == 0x4098);
static_assert(offsetof(CRecvTableDecoder, m_DebugInfo) == 0x40B8);

static_assert(sizeof(CRecvTable) == 0x140);
static_assert(offsetof(CRecvTable, m_Props) == 0x8);
static_assert(offsetof(CRecvTable, m_ChildTableProps) == 0x18);
static_assert(offsetof(CRecvTable, m_Decoder) == 0x120);
static_assert(offsetof(CRecvTable, m_NetTableName) == 0x128);
static_assert(offsetof(CRecvTable, m_Initialized) == 0x130);
static_assert(offsetof(CRecvTable, m_SourceTable) == 0x138);

static_assert(sizeof(CRecvProxyData) == 0x70);
static_assert(offsetof(CRecvProxyData, m_Value) == 0x20);
static_assert(offsetof(CRecvProxyData, m_RecvProp) == 0x38);
static_assert(offsetof(CRecvProxyData, m_PropMetadata) == 0x60);

static_assert(sizeof(CRecvTableStack) == 0x18);
static_assert(offsetof(CRecvTableStack, m_EntityBase) == 0x8);
static_assert(offsetof(CRecvTableStack, m_RecvTable) == 0x10);
