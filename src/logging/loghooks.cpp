#include "logging.h"
#include "loghooks.h"
#include "core/convar/convar.h"
#include "core/convar/concommand.h"
#include "engine/bitbuf.h"
#include "config/profile.h"
#include "core/tier0.h"
#include "vscript/squirrel/squirrel.h"
#include <iomanip>
#include <sstream>

ConVar* Cvar_spewlog_enable;
ConVar* Cvar_cl_showtextmsg;

enum class TextMsgPrintType_t
{
	HUD_PRINTNOTIFY = 1,
	HUD_PRINTCONSOLE,
	HUD_PRINTTALK,
	HUD_PRINTCENTER
};

class ICenterPrint
{
public:
	virtual void ctor() = 0;
	virtual void Clear(void) = 0;
	virtual void ColorPrint(int r, int g, int b, int a, wchar_t* text) = 0;
	virtual void ColorPrint(int r, int g, int b, int a, char* text) = 0;
	virtual void Print(wchar_t* text) = 0;
	virtual void Print(char* text) = 0;
	virtual void SetTextColor(int r, int g, int b, int a) = 0;
};

enum class SpewType_t
{
	SPEW_MESSAGE = 0,

	SPEW_WARNING,
	SPEW_ASSERT,
	SPEW_ERROR,
	SPEW_LOG,

	SPEW_TYPE_COUNT
};

const std::unordered_map<SpewType_t, const char*> PrintSpewTypes = {
	{SpewType_t::SPEW_MESSAGE, "SPEW_MESSAGE"},
	{SpewType_t::SPEW_WARNING, "SPEW_WARNING"},
	{SpewType_t::SPEW_ASSERT, "SPEW_ASSERT"},
	{SpewType_t::SPEW_ERROR, "SPEW_ERROR"},
	{SpewType_t::SPEW_LOG, "SPEW_LOG"}};

// these are used to define the base text colour for these things
const std::unordered_map<SpewType_t, spdlog::level::level_enum> PrintSpewLevels = {
	{SpewType_t::SPEW_MESSAGE, spdlog::level::level_enum::info},
	{SpewType_t::SPEW_WARNING, spdlog::level::level_enum::warn},
	{SpewType_t::SPEW_ASSERT, spdlog::level::level_enum::err},
	{SpewType_t::SPEW_ERROR, spdlog::level::level_enum::err},
	{SpewType_t::SPEW_LOG, spdlog::level::level_enum::info}};

const std::unordered_map<SpewType_t, const char> PrintSpewTypes_Short = {
	{SpewType_t::SPEW_MESSAGE, 'M'},
	{SpewType_t::SPEW_WARNING, 'W'},
	{SpewType_t::SPEW_ASSERT, 'A'},
	{SpewType_t::SPEW_ERROR, 'E'},
	{SpewType_t::SPEW_LOG, 'L'}};

ICenterPrint* pInternalCenterPrint = NULL;

DECLARE_MODULE(LogHooks)

DECLARE_HOOK_CC(TextMsg, client.dll + 0x198710, __cdecl, [](auto& hook, bf_read* msg)
{
	NOTE_UNUSED(hook);

	TextMsgPrintType_t msg_dest = (TextMsgPrintType_t)msg->ReadByte();

	char text[256];
	msg->ReadString(text, sizeof(text));

	if (!Cvar_cl_showtextmsg->GetBool())
		return;

	switch (msg_dest)
	{
	case TextMsgPrintType_t::HUD_PRINTCENTER:
		pInternalCenterPrint->Print(text);
		break;

	default:
		spdlog::warn("Unimplemented TextMsg type {}! printing to console", static_cast<int>(msg_dest));
		[[fallthrough]];

	case TextMsgPrintType_t::HUD_PRINTCONSOLE:
		auto endpos = strlen(text);
		if (text[endpos - 1] == '\n')
			text[endpos - 1] = '\0'; // cut off repeated newline

		spdlog::info(text);
		break;
	}
})

DECLARE_HOOK_CC(fprintf, engine.dll + 0x51B1F0, __cdecl, [](auto& hook, void* const stream, const char* const format, ...)
{
	NOTE_UNUSED(stream);

	va_list* va = hook.VarArgs();

	SQChar buf[1024];
	int charsWritten = vsnprintf_s(buf, sizeof(buf), _TRUNCATE, format, *va);

	if (charsWritten > 0)
	{
		if (buf[charsWritten - 1] == '\n')
			buf[charsWritten - 1] = '\0';
		NS::log::NATIVE_EN->info("{}", buf);
	}

	return 0;
})

DECLARE_HOOK_CC(ConCommand_echo, engine.dll + 0x123680, __cdecl, [](auto& hook, const CCommand& arg)
{
	NOTE_UNUSED(hook);

	if (arg.ArgC() >= 2)
		NS::log::echo->info("{}", arg.ArgS());
})

DECLARE_HOOK_CC(EngineSpewFunc, engine.dll + 0x11CA80, __fastcall, [](auto& hook, void* pEngineServer, SpewType_t type, const char* format, va_list args)
{
	NOTE_UNUSED(hook);

	NOTE_UNUSED(pEngineServer);
	if (!Cvar_spewlog_enable->GetBool())
		return;

	const char* typeStr = PrintSpewTypes.at(type);
	char formatted[2048] = {0};
	bool bShouldFormat = true;

	// because titanfall 2 is quite possibly the worst thing to yet exist, it sometimes gives invalid specifiers which will crash
	// ttf2sdk had a way to prevent them from crashing but it doesnt work in debug builds
	// so we use this instead
	for (int i = 0; format[i]; i++)
	{
		if (format[i] == '%')
		{
			switch (format[i + 1])
			{
			// this is fucking awful lol
			case 'd':
			case 'i':
			case 'u':
			case 'x':
			case 'X':
			case 'f':
			case 'F':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
			case 'c':
			case 's':
			case 'p':
			case 'n':
			case '%':
			case '-':
			case '+':
			case ' ':
			case '#':
			case '*':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				break;

			default:
			{
				bShouldFormat = false;
				break;
			}
			}
		}
	}

	if (bShouldFormat)
		vsnprintf(formatted, sizeof(formatted), format, args);
	else
		spdlog::warn("Failed to format {} \"{}\"", typeStr, format);

	auto endpos = strlen(formatted);
	if (formatted[endpos - 1] == '\n')
		formatted[endpos - 1] = '\0'; // cut off repeated newline

	NS::log::NATIVE_SV->log(PrintSpewLevels.at(type), "{}", formatted);
})

// used for printing the output of status
DECLARE_HOOK_CC(Status_ConMsg, engine.dll + 0x15ABD0, __cdecl, [](auto& hook, const char* text, ...)
{
	char formatted[2048];
	va_list* list = hook.VarArgs();

	vsprintf_s(formatted, text, *list);

	auto endpos = strlen(formatted);
	if (formatted[endpos - 1] == '\n')
		formatted[endpos - 1] = '\0'; // cut off repeated newline

	spdlog::info(formatted);
})

DECLARE_HOOK_CC(CClientState_ProcessPrint, engine.dll + 0x1A1530, __cdecl, [](auto& hook, void* thisptr, uintptr_t msg)
{
	NOTE_UNUSED(hook);

	NOTE_UNUSED(thisptr);

	char* text = *(char**)(msg + 0x20);

	auto endpos = strlen(text);
	if (text[endpos - 1] == '\n')
		text[endpos - 1] = '\0'; // cut off repeated newline

	spdlog::info(text);
	return true;
})

ON_DLL_LOAD_RELIESON("engine.dll", EngineSpewFuncHooks, ConVar, [](CModule module)
{
	NOTE_UNUSED(module);
	DISPATCH_HOOK(LogHooks, fprintf)
	DISPATCH_HOOK(LogHooks, ConCommand_echo)
	DISPATCH_HOOK(LogHooks, EngineSpewFunc)
	DISPATCH_HOOK(LogHooks, Status_ConMsg)
	DISPATCH_HOOK(LogHooks, CClientState_ProcessPrint)

	Cvar_spewlog_enable = new ConVar("spewlog_enable", "0", FCVAR_NONE, "Enables/disables whether the engine spewfunc should be logged");
})

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ClientPrintHooks, ConVar, [](CModule module)
{
	DISPATCH_HOOK(LogHooks, TextMsg)

	Cvar_cl_showtextmsg = new ConVar("cl_showtextmsg", "1", FCVAR_NONE, "Enable/disable text messages printing on the screen.");
	pInternalCenterPrint = module.Offset(0x216E940).RCast<ICenterPrint*>();
})
