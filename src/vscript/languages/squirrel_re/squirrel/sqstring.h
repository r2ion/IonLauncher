#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

// NOTE [Fifty]: Variable sized struct
struct alignas(8) SQString : public SQRefCounted
{
	SQSharedState* sharedState;
	int length;
	unsigned char gap_24[4];
	char _hash[8];
	char _val[1];
};
static_assert(std::is_base_of_v<SQRefCounted, SQString>);
static_assert(sizeof(SQString) == 0x38); // Game allocates 0x38 + string length.
static_assert(offsetof(SQString, sharedState) == 0x18);
static_assert(offsetof(SQString, length) == 0x20);
static_assert(offsetof(SQString, gap_24) == 0x24);
static_assert(offsetof(SQString, _hash) == 0x28);
static_assert(offsetof(SQString, _val) == 0x30);
