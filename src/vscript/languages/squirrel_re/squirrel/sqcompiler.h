#pragma once

#include "vscript/languages/squirrel_re/squirrel/sqlexer.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

#include <cstddef>
#include <cstdint>

struct SQCompiler
{
	std::byte gap0[4];
	int _token;
	std::byte gap_8[8];
	SQObject object_10;
	SQLexer lexer;
	std::byte gapB0[264];
	bool bFatalError;
	std::byte gap1B9[143];
	std::int64_t qword248;
	std::int64_t qword250;
	std::int64_t qword258;
	std::int64_t qword260;
	std::byte gap268[280];
	HSQUIRRELVM pSQVM;
	std::byte gap_388[8];
};
static_assert(sizeof(SQCompiler) == 0x390);
static_assert(alignof(SQCompiler) == 0x8);
static_assert(offsetof(SQCompiler, _token) == 0x4);
static_assert(offsetof(SQCompiler, object_10) == 0x10);
static_assert(offsetof(SQCompiler, lexer) == 0x20);
static_assert(offsetof(SQCompiler, bFatalError) == 0x1B8);
static_assert(offsetof(SQCompiler, qword248) == 0x248);
static_assert(offsetof(SQCompiler, pSQVM) == 0x380);
