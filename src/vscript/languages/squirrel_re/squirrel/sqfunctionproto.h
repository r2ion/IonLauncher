#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

// NOTE [Fifty]: Variable sized struct
struct alignas(8) SQFunctionProto : public SQCollectable
{
	void* pointer_30;
	SQObjectType _fileNameType;
	SQString* _fileName;
	SQObjectType _funcNameType;
	SQString* _funcName;
	SQObject obj_58;
	unsigned char gap_68[12];
	int _stacksize;
	unsigned char gap_78[48];
	int nParameters;
	unsigned char gap_AC[60];
	int nDefaultParams;
	unsigned char gap_EC[200];
};
static_assert(std::is_base_of_v<SQCollectable, SQFunctionProto>);
static_assert(sizeof(SQFunctionProto) == 0x1B8);
static_assert(offsetof(SQFunctionProto, pointer_30) == 0x30);
static_assert(offsetof(SQFunctionProto, _fileNameType) == 0x38);
static_assert(offsetof(SQFunctionProto, _fileName) == 0x40);
static_assert(offsetof(SQFunctionProto, _funcNameType) == 0x48);
static_assert(offsetof(SQFunctionProto, _funcName) == 0x50);
static_assert(offsetof(SQFunctionProto, obj_58) == 0x58);
static_assert(offsetof(SQFunctionProto, gap_68) == 0x68);
static_assert(offsetof(SQFunctionProto, _stacksize) == 0x74);
static_assert(offsetof(SQFunctionProto, gap_78) == 0x78);
static_assert(offsetof(SQFunctionProto, nParameters) == 0xA8);
static_assert(offsetof(SQFunctionProto, gap_AC) == 0xAC);
static_assert(offsetof(SQFunctionProto, nDefaultParams) == 0xE8);
static_assert(offsetof(SQFunctionProto, gap_EC) == 0xEC);
