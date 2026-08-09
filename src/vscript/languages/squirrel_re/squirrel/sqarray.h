#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

struct SQArray : public SQCollectable
{
	SQObject* _values;
	int _usedSlots;
	int _allocated;
};
static_assert(std::is_base_of_v<SQCollectable, SQArray>);
static_assert(sizeof(SQArray) == 0x40);
static_assert(offsetof(SQArray, _values) == 0x30);
static_assert(offsetof(SQArray, _usedSlots) == 0x38);
static_assert(offsetof(SQArray, _allocated) == 0x3C);
