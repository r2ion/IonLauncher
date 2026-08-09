#pragma once

class CCommand;
class IConVar;

// Console command and variable flags.
#define FCVAR_NONE 0
#define FCVAR_UNREGISTERED (1 << 0)
#define FCVAR_DEVELOPMENTONLY (1 << 1)
#define FCVAR_GAMEDLL (1 << 2)
#define FCVAR_CLIENTDLL (1 << 3)
#define FCVAR_HIDDEN (1 << 4)
#define FCVAR_PROTECTED (1 << 5)
#define FCVAR_SPONLY (1 << 6)
#define FCVAR_ARCHIVE (1 << 7)
#define FCVAR_NOTIFY (1 << 8)
#define FCVAR_USERINFO (1 << 9)
#define FCVAR_PRINTABLEONLY (1 << 10)
#define FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS (1 << 10)
#define FCVAR_UNLOGGED (1 << 11)
#define FCVAR_NEVER_AS_STRING (1 << 12)
#define FCVAR_REPLICATED (1 << 13)
#define FCVAR_CHEAT (1 << 14)
#define FCVAR_SS (1 << 15)
#define FCVAR_DEMO (1 << 16)
#define FCVAR_DONTRECORD (1 << 17)
#define FCVAR_SS_ADDED (1 << 18)
#define FCVAR_RELEASE (1 << 19)
#define FCVAR_RELOAD_MATERIALS (1 << 20)
#define FCVAR_RELOAD_TEXTURES (1 << 21)
#define FCVAR_NOT_CONNECTED (1 << 22)
#define FCVAR_MATERIAL_SYSTEM_THREAD (1 << 23)
#define FCVAR_ARCHIVE_PLAYERPROFILE (1 << 24)
#define FCVAR_ACCESSIBLE_FROM_THREADS (1 << 25)
#define FCVAR_STUDIO_SYSTEM (1 << 26)
#define FCVAR_SERVER_FRAME_THREAD (1 << 27)
#define FCVAR_SERVER_CAN_EXECUTE (1 << 28)
#define FCVAR_SERVER_CANNOT_QUERY (1 << 29)
#define FCVAR_CLIENTCMD_CAN_EXECUTE (1 << 30)
#define FCVAR_PLATFORM_SYSTEM (1U << 31)

#define FCVAR_MATERIAL_THREAD_MASK (FCVAR_RELOAD_MATERIALS | FCVAR_RELOAD_TEXTURES | FCVAR_MATERIAL_SYSTEM_THREAD)

using FnCommandCallback_t = void (*)(const CCommand& command);

#define COMMAND_COMPLETION_MAXITEMS 64
#define COMMAND_COMPLETION_ITEM_LENGTH 128
using FnCommandCompletionCallback =
	int (*)(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH]);

using ChangeUserData_t = void*;
using FnChangeCallback_t = void (*)(IConVar* var, const char* oldValue, float oldFloatValue, ChangeUserData_t userData);

class IConVar
{
public:
	virtual ~IConVar() = default; // 0
	virtual void SetValue(const char* value) = 0; // 1
	virtual void SetValue(float value) = 0; // 2
	virtual void SetValue(int value) = 0; // 3
	virtual const char* GetName() const = 0; // 4
	virtual const char* GetBaseName() const = 0; // 5
	virtual bool IsFlagSet(int flag) const = 0; // 6
	virtual int GetSplitScreenPlayerSlot() const = 0; // 7
};

static_assert(sizeof(IConVar) == 0x8);
