#pragma once

#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

#include <cstddef>

struct alignas(4) SQVector
{
	SQObjectType _Type;
	float x;
	float y;
	float z;
};

static_assert(sizeof(SQVector) == 0x10);
static_assert(alignof(SQVector) == 0x4);
static_assert(offsetof(SQVector, _Type) == 0x0);
static_assert(offsetof(SQVector, x) == 0x4);
static_assert(offsetof(SQVector, y) == 0x8);
static_assert(offsetof(SQVector, z) == 0xC);
