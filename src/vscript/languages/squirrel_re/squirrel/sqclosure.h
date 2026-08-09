#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

struct alignas(8) SQClosure : public SQCollectable
{
	SQObject obj_30;
	SQObject _function;
	SQObject* _outervalues;
	unsigned char gap_58[8];
};
static_assert(std::is_base_of_v<SQCollectable, SQClosure>);
static_assert(sizeof(SQClosure) == 0x60);
static_assert(offsetof(SQClosure, obj_30) == 0x30);
static_assert(offsetof(SQClosure, _function) == 0x40);
static_assert(offsetof(SQClosure, _outervalues) == 0x50);
static_assert(offsetof(SQClosure, gap_58) == 0x58);

struct alignas(8) SQNativeClosure : public SQCollectable
{
	char unknown_30;
	unsigned char padding_34[7];
	long long value_38;
	long long value_40;
	long long value_48;
	long long value_50;
	long long value_58;
	SQObjectType _nameType;
	SQString* _name;
	long long value_70;
	long long value_78;
};
static_assert(std::is_base_of_v<SQCollectable, SQNativeClosure>);
static_assert(sizeof(SQNativeClosure) == 0x80);
static_assert(offsetof(SQNativeClosure, unknown_30) == 0x30);
static_assert(offsetof(SQNativeClosure, padding_34) == 0x31);
static_assert(offsetof(SQNativeClosure, value_38) == 0x38);
static_assert(offsetof(SQNativeClosure, value_40) == 0x40);
static_assert(offsetof(SQNativeClosure, value_48) == 0x48);
static_assert(offsetof(SQNativeClosure, value_50) == 0x50);
static_assert(offsetof(SQNativeClosure, value_58) == 0x58);
static_assert(offsetof(SQNativeClosure, _nameType) == 0x60);
static_assert(offsetof(SQNativeClosure, _name) == 0x68);
static_assert(offsetof(SQNativeClosure, value_70) == 0x70);
static_assert(offsetof(SQNativeClosure, value_78) == 0x78);
