#include "core/tier0.h"
#include "tier0/hooks.h"
#include "tier0/module.h"
#include "logging/logging.h"
#include "core/convar/cvar.h"
#include "core/convar/convar.h"
#include "util/strtools.h"

DECLARE_MODULE(ConsoleLoggingFixHooks)

// Console filter convars
ConVar* Cvar_con_filter_enable;
ConVar* Cvar_con_filter_text;
ConVar* Cvar_con_filter_text_out;

// Global flags
static bool g_bInColorPrint = false;
static bool g_fColorPrintf = false;

// Con_ColorPrintf - Hook the completely gutted R2 version and restore CSGO/Portal 2 functionality
// This is the varargs version that formats and then prints
DECLARE_HOOK_CC(Con_ColorPrintf, engine.dll + 0x0A8830, __cdecl, [](auto& hook, const Color& clr, const char* fmt, ...)
{
    va_list* args = hook.VarArgs();

    // Format the message (matching CSGO Q_vsnprintf behavior)
    char msg[4096];
    va_list argptr;
    va_copy(argptr, *args);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    // HandleRedirectAndDebugLog would go here in CSGO, but R2 handles this in caller

    // Now do the Con_ColorPrint logic (filtering + output)
    // Prevent recursion
    if (g_bInColorPrint)
        return;

    // Apply console filters (matching CSGO implementation)
    int nFilterEnable = Cvar_con_filter_enable ? Cvar_con_filter_enable->GetInt() : 0;

    if (nFilterEnable > 0)
    {
        const char* pszText = Cvar_con_filter_text ? Cvar_con_filter_text->GetString() : "";
        const char* pszIgnoreText = Cvar_con_filter_text_out ? Cvar_con_filter_text_out->GetString() : "";

        switch (nFilterEnable)
        {
        case 1:
            // Complete filtering: only show lines containing filter text and not containing ignore text
            if (pszText && (*pszText != '\0') && V_stristr(msg, pszText) == nullptr)
                return;
            if (pszIgnoreText && *pszIgnoreText && V_stristr(msg, pszIgnoreText) != nullptr)
                return;
            break;

        case 2:
            // Soft filtering: hide lines with ignore text, dim lines without filter text
            if (pszIgnoreText && *pszIgnoreText && V_stristr(msg, pszIgnoreText) != nullptr)
                return;
            if (pszText && (*pszText != '\0') && V_stristr(msg, pszText) == nullptr)
            {
                // Line doesn't contain filter text - print in darker color
                Color dimColor(200, 200, 200, 150);

                g_bInColorPrint = true;
                typedef void (*ConsoleColorPrintfFn)(CCvar*, const Color*, const char*, ...);
                ConsoleColorPrintfFn consoleColorPrintf = *(ConsoleColorPrintfFn*)((uintptr_t*)*(uintptr_t*)g_pCVar + 26);
                if (consoleColorPrintf)
                    consoleColorPrintf(g_pCVar, &dimColor, "%s", msg);
                g_bInColorPrint = false;
                return;
            }
            break;

        default:
            // No filtering
            break;
        }
    }

    g_bInColorPrint = true;

    // Call the appropriate console output function via vtable
    // In CSGO this checks g_fColorPrintf, g_fIsDebugPrint flags
    // For now, always use ConsoleColorPrintf since we have the color
    typedef void (*ConsoleColorPrintfFn)(CCvar*, const Color*, const char*, ...);
    typedef void (*ConsolePrintfFn)(CCvar*, const char*, ...);
    typedef void (*ConsoleDPrintfFn)(CCvar*, const char*, ...);

    ConsoleColorPrintfFn consoleColorPrintf = *(ConsoleColorPrintfFn*)((uintptr_t*)*(uintptr_t*)g_pCVar + 26);
    ConsolePrintfFn consolePrintf = *(ConsolePrintfFn*)((uintptr_t*)*(uintptr_t*)g_pCVar + 27);

    if (consoleColorPrintf)
    {
        consoleColorPrintf(g_pCVar, &clr, "%s", msg);
    }
    else if (consolePrintf)
    {
        consolePrintf(g_pCVar, "%s", msg);
    }

    g_bInColorPrint = false;
})

ON_DLL_LOAD_RELIESON("engine.dll", ConsoleLoggingFix, ConVar, [](CModule module)
{
    DISPATCH_MODULE(ConsoleLoggingFixHooks)

    // Create console filter convars (matching Source engine implementation)
    Cvar_con_filter_enable = new ConVar("con_filter_enable", "0", FCVAR_NONE, "Filters console output based on the setting of con_filter_text. 1 filters completely, 2 displays filtered text brighter than other text.");
    Cvar_con_filter_text = new ConVar("con_filter_text", "", FCVAR_NONE, "Text with which to filter console spew. Set con_filter_enable 1 or 2 to activate.");
    Cvar_con_filter_text_out = new ConVar("con_filter_text_out", "", FCVAR_NONE, "Text with which to filter OUT of console spew. Set con_filter_enable 1 or 2 to activate.");
})
