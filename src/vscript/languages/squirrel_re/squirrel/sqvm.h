#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"
#include "vscript/languages/squirrel_re/squirrel/sqstate.h"


enum class ScriptContext : int
{
	INVALID = -1,
	SERVER,
	CLIENT,
	UI,
};

struct alignas(8) SQVM : public SQCollectable
{
	struct alignas(8) CallInfo
	{
		long long ip;
		SQObject* _literals;
		SQObject obj10;
		SQObject closure;
		int _etraps[4];
		int _root;
		short _vargs_size;
		short _vargs_base;
		unsigned char gap[16];
	};

	CallInfo* ci;
	CallInfo* _callstack;
	int _callstacksize;
	int _stackbase;
	SQObject* _stackOfCurrentFunction;
	SQSharedState* sharedState;
	void* pointer_58;
	void* pointer_60;
	int _top;
	SQObject* _stack;
	unsigned char gap_78[8];
	SQObject* _vargvstack;
	unsigned char gap_88[8];
	SQObject temp_reg;
	unsigned char gapA0[8];
	void* pointer_A8;
	unsigned char gap_B0[8];
	SQObject _roottable_object;
	SQObject _lasterror;
	SQObject _errorHandler;
	long long field_E8;
	int traps;
	unsigned char gap_F4[12];
	int _nnativecalls;
	int _suspended;
	int _suspended_root;
	int _unk;
	int _suspended_target;
	int trapAmount;
	int _suspend_varargs;
	int unknown_field_11C;
	SQObject object_120;
};

static_assert(std::is_base_of_v<SQCollectable, SQVM>);
static_assert(sizeof(SQVM::CallInfo) == 0x58);
static_assert(offsetof(SQVM::CallInfo, ip) == 0x0);
static_assert(offsetof(SQVM::CallInfo, _literals) == 0x8);
static_assert(offsetof(SQVM::CallInfo, obj10) == 0x10);
static_assert(offsetof(SQVM::CallInfo, closure) == 0x20);
static_assert(offsetof(SQVM::CallInfo, _etraps) == 0x30);
static_assert(offsetof(SQVM::CallInfo, _root) == 0x40);
static_assert(offsetof(SQVM::CallInfo, _vargs_size) == 0x44);
static_assert(offsetof(SQVM::CallInfo, _vargs_base) == 0x46);

static_assert(sizeof(SQVM) == 0x130);
static_assert(offsetof(SQVM, ci) == 0x30);
static_assert(offsetof(SQVM, _callstack) == 0x38);
static_assert(offsetof(SQVM, _callstacksize) == 0x40);
static_assert(offsetof(SQVM, _stackbase) == 0x44);
static_assert(offsetof(SQVM, _stackOfCurrentFunction) == 0x48);
static_assert(offsetof(SQVM, sharedState) == 0x50);
static_assert(offsetof(SQVM, pointer_58) == 0x58);
static_assert(offsetof(SQVM, pointer_60) == 0x60);
static_assert(offsetof(SQVM, _top) == 0x68);
static_assert(offsetof(SQVM, _stack) == 0x70);
static_assert(offsetof(SQVM, _vargvstack) == 0x80);
static_assert(offsetof(SQVM, temp_reg) == 0x90);
static_assert(offsetof(SQVM, pointer_A8) == 0xA8);
static_assert(offsetof(SQVM, _roottable_object) == 0xB8);
static_assert(offsetof(SQVM, _lasterror) == 0xC8);
static_assert(offsetof(SQVM, _errorHandler) == 0xD8);
static_assert(offsetof(SQVM, field_E8) == 0xE8);
static_assert(offsetof(SQVM, traps) == 0xF0);
static_assert(offsetof(SQVM, _nnativecalls) == 0x100);
static_assert(offsetof(SQVM, _suspended) == 0x104);
static_assert(offsetof(SQVM, _suspended_root) == 0x108);
static_assert(offsetof(SQVM, _unk) == 0x10C);
static_assert(offsetof(SQVM, _suspended_target) == 0x110);
static_assert(offsetof(SQVM, trapAmount) == 0x114);
static_assert(offsetof(SQVM, _suspend_varargs) == 0x118);
static_assert(offsetof(SQVM, unknown_field_11C) == 0x11C);
static_assert(offsetof(SQVM, object_120) == 0x120);
