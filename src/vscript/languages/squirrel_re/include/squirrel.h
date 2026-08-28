#pragma once

class Mod;
struct SQBufferState;
class CBaseEntity;

struct SQVM;
struct SQObject;
struct SQTable;
struct SQArray;
struct SQString;
struct SQClosure;
struct SQFunctionProto;
struct SQStructDef;
struct SQNativeClosure;
struct SQStructInstance;
struct SQUserData;
struct SQSharedState;

typedef float SQFloat;
typedef long SQInteger;
typedef unsigned long SQUnsignedInteger;
typedef char SQChar;
typedef SQUnsignedInteger SQBool;
#define _SC(a) a
#define SQTrue (1)
#define SQFalse (0)

typedef SQVM* HSQUIRRELVM;

enum SQRESULT : SQInteger
{
    SQRESULT_ERROR = -1,
    SQRESULT_NULL = 0,
    SQRESULT_NOTNULL = 1,
};
#define sq_type(o) ((o)._Type)
#define SQ_FAILED(res) (res < 0)
#define SQ_SUCCEEDED(res) (res >= 0)

enum class eSQReturnType
{
	Float = 0x1,
	Vector = 0x3,
	Integer = 0x5,
	Boolean = 0x6,
	Entity = 0xD,
	String = 0x21,
	Default = 0x20,
	Arrays = 0x25,
	Asset = 0x28,
	Table = 0x26,
};

struct SQBufferState
{
	const SQChar* buffer;
	const SQChar* bufferPlusLength;
	const SQChar* bufferAgain;

	SQBufferState(const SQChar* pszCode)
	{
		buffer = pszCode;
		bufferPlusLength = pszCode + strlen(pszCode);
		bufferAgain = pszCode;
	}
};

typedef SQRESULT (*SQFunction)(HSQUIRRELVM sqvm);

struct SQFuncRegistration
{
	const char* squirrelFuncName;
	const char* cppFuncName;
	const char* helpText;
	const char* returnTypeString;
	const char* argTypes;
	uint32_t unknown1;
	uint32_t devLevel;
	const char* shortNameMaybe;
	uint32_t unknown2;
	eSQReturnType returnType;
	uint32_t* externalBufferPointer;
	uint64_t externalBufferSize;
	uint64_t unknown3;
	uint64_t unknown4;
	SQFunction funcPtr;

	SQFuncRegistration()
	{
		memset(this, 0, sizeof(SQFuncRegistration));
		this->returnType = eSQReturnType::Default;
	}
};

struct alignas(8) SQStackInfos
{
    const SQChar* funcname;
    const SQChar* source;
    SQInteger line;
};
