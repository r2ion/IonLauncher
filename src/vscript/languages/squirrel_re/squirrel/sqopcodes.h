#pragma once

#include <cstddef>

struct alignas(4) SQInstruction
{
	int op;
	int arg1;
	int output;
	short arg2;
	short arg3;
};

static_assert(sizeof(SQInstruction) == 0x10);
static_assert(alignof(SQInstruction) == 0x4);
static_assert(offsetof(SQInstruction, op) == 0x0);
static_assert(offsetof(SQInstruction, arg1) == 0x4);
static_assert(offsetof(SQInstruction, output) == 0x8);
static_assert(offsetof(SQInstruction, arg2) == 0xC);
static_assert(offsetof(SQInstruction, arg3) == 0xE);
