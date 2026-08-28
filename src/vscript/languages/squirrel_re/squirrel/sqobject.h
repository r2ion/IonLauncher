#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include <cstddef>
#include <type_traits>

struct SQObjectPtr;
struct SQTable;
struct SQVM;
struct SQWeakRef;

enum SQObjectType : int;

struct SQRefCounted
{
	SQUnsignedInteger _uiRef;
	SQWeakRef* _weakref;

	virtual ~SQRefCounted();
	virtual void Release() = 0;
};

struct SQCollectable : public SQRefCounted
{
	SQCollectable* _next;
	SQCollectable* _prev;
	SQSharedState* _sharedstate;

	virtual void Release() override = 0;
	virtual void Mark(SQCollectable** chain) = 0;
	virtual void Finalize() = 0;
	virtual void DumpToString(SQChar* buffer, SQUnsignedInteger length, SQInteger start, SQInteger end) = 0;
};

struct SQDelegable : public SQCollectable
{
	virtual bool GetMetaMethod(SQVM* vm, int method, SQObjectPtr& result) = 0;

	SQTable* _delegate;
};

static_assert(std::is_polymorphic_v<SQRefCounted>);
static_assert(std::has_virtual_destructor_v<SQRefCounted>);
static_assert(std::is_abstract_v<SQRefCounted>);
static_assert(sizeof(SQRefCounted) == 0x18);
static_assert(offsetof(SQRefCounted, _uiRef) == 0x8);
static_assert(offsetof(SQRefCounted, _weakref) == 0x10);

static_assert(std::is_base_of_v<SQRefCounted, SQCollectable>);
static_assert(std::is_abstract_v<SQCollectable>);
static_assert(sizeof(SQCollectable) == 0x30);
static_assert(offsetof(SQCollectable, _next) == 0x18);
static_assert(offsetof(SQCollectable, _prev) == 0x20);
static_assert(offsetof(SQCollectable, _sharedstate) == 0x28);

static_assert(std::is_base_of_v<SQCollectable, SQDelegable>);
static_assert(std::is_abstract_v<SQDelegable>);
static_assert(sizeof(SQDelegable) == 0x38);
static_assert(offsetof(SQDelegable, _delegate) == 0x30);

enum SQObjectType : int
{
	_RT_NULL = 0x1,
	_RT_INTEGER = 0x2,
	_RT_FLOAT = 0x4,
	_RT_BOOL = 0x8,
	_RT_STRING = 0x10,
	_RT_TABLE = 0x20,
	_RT_ARRAY = 0x40,
	_RT_USERDATA = 0x80,
	_RT_CLOSURE = 0x100,
	_RT_NATIVECLOSURE = 0x200,
	_RT_GENERATOR = 0x400,
	OT_USERPOINTER = 0x800,
	_RT_USERPOINTER = 0x800,
	_RT_THREAD = 0x1000,
	_RT_FUNCPROTO = 0x2000,
	_RT_CLASS = 0x4000,
	_RT_INSTANCE = 0x8000,
	_RT_WEAKREF = 0x10000,
	OT_VECTOR = 0x40000,
	SQOBJECT_CANBEFALSE = 0x1000000,
	OT_NULL = 0x1000001,
	OT_BOOL = 0x1000008,
	SQOBJECT_DELEGABLE = 0x2000000,
	SQOBJECT_NUMERIC = 0x4000000,
	OT_INTEGER = 0x5000002,
	OT_FLOAT = 0x5000004,
	SQOBJECT_REF_COUNTED = 0x8000000,
	OT_STRING = 0x8000010,
	OT_ARRAY = 0x8000040,
	OT_CLOSURE = 0x8000100,
	OT_NATIVECLOSURE = 0x8000200,
	OT_ASSET = 0x8000400,
	OT_THREAD = 0x8001000,
	OT_FUNCPROTO = 0x8002000,
	OT_CLASS = 0x8004000,
	OT_STRUCT = 0x8200000,
	OT_WEAKREF = 0x8010000,
    OT_TABLE = 0xA000020,
    OT_USERDATA = 0xA000080,
    OT_INSTANCE = 0xA008000,
    OT_ENTITY = 0xA400000,
};
#define ISREFCOUNTED(t) (t & SQOBJECT_REF_COUNTED)

union SQObjectValue
{
    SQString* asString;
    SQRefCounted* asRefCounted;
    SQTable* asTable;
    SQClosure* asClosure;
    SQFunctionProto* asFuncProto;
    SQStructDef* asStructDef;
    long long as64Integer;
    SQNativeClosure* asNativeClosure;
    SQArray* asArray;
    HSQUIRRELVM asThread;
    float asFloat;
    int asInteger;
    SQUserData* asUserdata;
    SQStructInstance* asStructInstance;
};

struct SQObject
{
    SQObjectType _Type;
    int structNumber;
    SQObjectValue _VAL;
};
#define _integer(obj) ((obj)._VAL.asInteger)
#define _float(obj) ((obj)._VAL.asFloat)
#define _bool(obj) ((obj)._VAL.asInteger)
#define _string(obj) ((obj)._VAL.asString)
#define _refcounted(obj) ((obj)._VAL.asRefCounted)
#define _rawval(obj) ((obj)._VAL.as64Integer)
#define _stringval(obj) ((obj)._VAL.asString->_val)
#define _vector(obj) (reinterpret_cast<const SQFloat*>(&(obj).structNumber))

static_assert(sizeof(SQObjectType) == 0x4);
static_assert(sizeof(SQObjectValue) == 0x8);
static_assert(sizeof(SQObject) == 0x10);
static_assert(offsetof(SQObject, _Type) == 0x0);
static_assert(offsetof(SQObject, structNumber) == 0x4);
static_assert(offsetof(SQObject, _VAL) == 0x8);
