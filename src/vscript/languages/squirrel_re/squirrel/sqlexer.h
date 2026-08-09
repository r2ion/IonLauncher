#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"

#include <cstddef>
#include <cstdarg>

struct SQLexerBuffer
{
	SQChar* _data;
	SQInteger _size;
	SQInteger _allocated;
};

using SQLexReadFunc = SQInteger (*)(void* userData);
using SQLexResetFunc = void (*)(void* userData);
using SQLexErrorFunc = void (*)(void* target, const SQChar* format, va_list args);

struct alignas(8) SQLexer
{
	SQInteger _curtoken;
	std::byte _padding04[4];
	SQTable* _keywords;
	SQBool _reachedEof;
	SQInteger _prevtoken;
	SQInteger _currentline;
	SQInteger _lasttokenline;
	SQInteger _currentcolumn;
	std::byte _padding24[4];
	const SQChar* _svalue;
	SQInteger _nvalue;
	SQFloat _fvalue;
	SQLexReadFunc _readf;

	// Retail client.dll leaves these bounded pointer-sized ranges untouched in
	// SQLexer_Init and every lexer helper at 0x83C40-0x86370.
	std::byte _reserved40[8];
	SQLexResetFunc _resetf;
	void* _up;
	std::byte _reserved58[8];

	SQChar _currdata;
	std::byte _padding61[7];
	SQSharedState* _sharedstate;
	SQLexerBuffer _longstr;
	SQLexErrorFunc _errfunc;
	void* _errtarget;
};

static_assert(sizeof(SQLexerBuffer) == 0x10);
static_assert(alignof(SQLexerBuffer) == 0x8);
static_assert(offsetof(SQLexerBuffer, _data) == 0x0);
static_assert(offsetof(SQLexerBuffer, _size) == 0x8);
static_assert(offsetof(SQLexerBuffer, _allocated) == 0xC);

static_assert(sizeof(SQLexer) == 0x90);
static_assert(alignof(SQLexer) == 0x8);
static_assert(offsetof(SQLexer, _keywords) == 0x8);
static_assert(offsetof(SQLexer, _reachedEof) == 0x10);
static_assert(offsetof(SQLexer, _currentline) == 0x18);
static_assert(offsetof(SQLexer, _currentcolumn) == 0x20);
static_assert(offsetof(SQLexer, _svalue) == 0x28);
static_assert(offsetof(SQLexer, _nvalue) == 0x30);
static_assert(offsetof(SQLexer, _fvalue) == 0x34);
static_assert(offsetof(SQLexer, _readf) == 0x38);
static_assert(offsetof(SQLexer, _resetf) == 0x48);
static_assert(offsetof(SQLexer, _up) == 0x50);
static_assert(offsetof(SQLexer, _currdata) == 0x60);
static_assert(offsetof(SQLexer, _sharedstate) == 0x68);
static_assert(offsetof(SQLexer, _longstr) == 0x70);
static_assert(offsetof(SQLexer, _errfunc) == 0x80);
static_assert(offsetof(SQLexer, _errtarget) == 0x88);
