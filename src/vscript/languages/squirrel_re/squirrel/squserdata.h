#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

// NOTE [Fifty]: Variable sized struct
struct SQUserData : public SQDelegable
{
	int size;
	char padding1[4];
	void* (*releasehook)(void* val, int size);
	long long typeId;
	char data[1];
};
static_assert(std::is_base_of_v<SQDelegable, SQUserData>);
static_assert(sizeof(SQUserData) == 0x58); // Game allocates 0x57 + payload size.
static_assert(offsetof(SQUserData, size) == 0x38);
static_assert(offsetof(SQUserData, padding1) == 0x3C);
static_assert(offsetof(SQUserData, releasehook) == 0x40);
static_assert(offsetof(SQUserData, typeId) == 0x48);
static_assert(offsetof(SQUserData, data) == 0x50);
