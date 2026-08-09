#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

struct SQStructDef : public SQCollectable
{
	SQObjectType _nameType;
	SQString* _name;
	unsigned char gap_38[16];
	SQObjectType _variableNamesType;
	SQTable* _variableNames;
	unsigned char gap_[32];
};
static_assert(std::is_base_of_v<SQCollectable, SQStructDef>);
static_assert(sizeof(SQStructDef) == 0x80);
static_assert(offsetof(SQStructDef, _nameType) == 0x30);
static_assert(offsetof(SQStructDef, _name) == 0x38);
static_assert(offsetof(SQStructDef, gap_38) == 0x40);
static_assert(offsetof(SQStructDef, _variableNamesType) == 0x50);
static_assert(offsetof(SQStructDef, _variableNames) == 0x58);
static_assert(offsetof(SQStructDef, gap_) == 0x60);

// NOTE [Fifty]: Variable sized struct
struct SQStructInstance : public SQCollectable
{
	unsigned int size;
	BYTE gap_34[4];
	SQObject data[1];
};
static_assert(std::is_base_of_v<SQCollectable, SQStructInstance>);
static_assert(sizeof(SQStructInstance) == 0x48);
static_assert(offsetof(SQStructInstance, size) == 0x30);
static_assert(offsetof(SQStructInstance, gap_34) == 0x34);
static_assert(offsetof(SQStructInstance, data) == 0x38);
