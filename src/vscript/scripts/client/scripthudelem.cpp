#include "vscript/squirrel/squirrel.h"
#include "engine/cdll_int.h"
#include "vgui/basemodui.h"
#include "vgui_controls/Panel.h"

#include <cstddef>
#include <cstdlib>

class CClientScriptHudElement;
class CDialogListButton;
class ConVar;
class IConVar;

// Retail CGameUIConVarRef stores the IConVar subobject and its owning ConVar.
// The resolver at client.dll + 0x4A3650 proves the +0x30 base adjustment.
struct CGameUIConVarRef
{
	IConVar* m_pIConVar;
	ConVar* m_pConVar;
};

static_assert(sizeof(CGameUIConVarRef) == 0x10);
static_assert(alignof(CGameUIConVarRef) == 0x8);
static_assert(offsetof(CGameUIConVarRef, m_pIConVar) == 0x0);
static_assert(offsetof(CGameUIConVarRef, m_pConVar) == 0x8);

using CGameUIConVarRef__Init_t = CGameUIConVarRef* (*)(CGameUIConVarRef*, const char*);

CGameUIConVarRef__Init_t CGameUIConVarRef__Init = nullptr;

static vgui::Panel* GetHudElementPanel(CClientScriptHudElement* hudElement)
{
	return *reinterpret_cast<vgui::Panel**>(reinterpret_cast<std::byte*>(hudElement) + 0x28);
}

ADD_SQFUNC("void", Hud_DialogList_RemoveListItems, "var elem", "", ScriptContext::UI)
{
	CClientScriptHudElement* hudElement = g_pSquirrel[context]->gethudelement<CClientScriptHudElement>(sqvm, 1);

	if (!hudElement)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

	vgui::Panel* dialogListButton = GetHudElementPanel(hudElement);
	const char* name = dialogListButton->GetName();

	if (!dialogListButton->IsDialogListButton())
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
		return SQRESULT_ERROR;
	}

	// The retail type has no recoverable RTTI or static vtable, so only mutate the proven CUtlVector count field.
	*reinterpret_cast<int*>(reinterpret_cast<std::byte*>(dialogListButton) + 0x530) = 0;

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", Hud_ChangeDialogListConVar, "var elem, string convarName", "", ScriptContext::UI)
{
	CClientScriptHudElement* hudElement = g_pSquirrel[context]->gethudelement<CClientScriptHudElement>(sqvm, 1);
	const char* convarName = g_pSquirrel[context]->getstring(sqvm, 2);

	if (!hudElement)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

	vgui::Panel* dialogListButton = GetHudElementPanel(hudElement);
	const char* name = dialogListButton->GetName();

	if (!dialogListButton->IsDialogListButton())
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
		return SQRESULT_ERROR;
	}

	auto* convarRef = static_cast<CGameUIConVarRef*>(std::malloc(sizeof(CGameUIConVarRef)));
	CGameUIConVarRef__Init(convarRef, convarName);

	*reinterpret_cast<CGameUIConVarRef**>(reinterpret_cast<std::byte*>(dialogListButton) + 0x4E8) = convarRef;

	return SQRESULT_NULL;
}

using sub_738940_t = __int64 (__fastcall *)(uint64_t);
sub_738940_t sub_738940 = nullptr;
using sub_4A3620_t = int (__fastcall *)(void);
sub_4A3620_t sub_4A3620 = nullptr;
using sub_733BB0_t = unsigned int (__fastcall *)(const unsigned __int8 *, const unsigned __int8 *);
sub_733BB0_t sub_733BB0 = nullptr;
using sub_73A300_t = int (__fastcall *)(__int64);
sub_73A300_t sub_73A300 = nullptr;
using sub_739B90_t = const char* (__fastcall *)(__int64);
sub_739B90_t sub_739B90 = nullptr;

HMODULE clientBase = 0;

DECLARE_MODULE(ScriptHudElemHooks)

DECLARE_HOOK(DialogListButton__IsDefaultValue, client.dll + 0x4C96A0, ([](auto& hook, CDialogListButton* dialogListButton) -> bool
{
	NOTE_UNUSED(hook);
	uintptr_t a1 = reinterpret_cast<uintptr_t>(dialogListButton);
  __int64 v3; // rcx
  __int64 v4; // rax
  __int64 v5; // rbx
  unsigned __int8 *v6; // rdi
  int v7; // eax
  const char* v9; // rax
  const char* v12; // rdi
  const char* v13; // rax
  const char* v15; // rax

  uintptr_t base = (uintptr_t)clientBase;
  IVEngineClient* engineClient = *reinterpret_cast<IVEngineClient**>(base + 0xC3D940);

  if ( *(DWORD *)(a1 + 1132) != 3 )
    return 0;
  v3 = *(uint64_t *)(a1 + 1256);
  if ( v3 )
  {
    v4 = sub_738940(*(uint64_t *)(v3 + 8));
    v5 = *(uint64_t *)(a1 + 1256);
    v6 = (unsigned __int8 *)v4;
    v7 = sub_4A3620();
    if ( (unsigned int)sub_733BB0(v6, *reinterpret_cast<const unsigned __int8 **>(*reinterpret_cast<uint64_t *>(v5 + 16LL * v7 + 8) + 72LL)) )
      return 1;
  }
  if ( sub_73A300(a1 + 1264) )
  {
    v9 = sub_739B90(a1 + 1264);
    v12 = engineClient->GetCurrentPlaylistGameModeVar(
            *reinterpret_cast<unsigned __int8*>(a1 + 1296),
            v9,
            false);
    v13 = sub_739B90(a1 + 1264);
    v15 = engineClient->GetCurrentPlaylistGameModeVar(
            *reinterpret_cast<unsigned __int8*>(a1 + 1296),
            v13,
            true);

	// this can be null when we use Hud_DialogList_RemoveListItems
	if( !v12 )
		return 0;
    if ( (unsigned int)sub_733BB0(reinterpret_cast<const unsigned __int8*>(v12), reinterpret_cast<const unsigned __int8*>(v15)) )
      return 1;
  }
  return 0;
}))


ON_DLL_LOAD_CLIENT_RELIESON("client.dll", CGameUIConVarRef, ConVar, [](CModule module)
{
	sub_738940 = module.Offset(0x738940).RCast<sub_738940_t>();
	sub_4A3620 = module.Offset(0x4A3620).RCast<sub_4A3620_t>();
	sub_733BB0 = module.Offset(0x733BB0).RCast<sub_733BB0_t>();
	sub_73A300 = module.Offset(0x73A300).RCast<sub_73A300_t>();
	sub_739B90 = module.Offset(0x739B90).RCast<sub_739B90_t>();
    clientBase = (HMODULE)module.GetModuleBase();

	DISPATCH_MODULE(ScriptHudElemHooks);
	CGameUIConVarRef__Init = module.Offset(0x4A34A0).RCast<CGameUIConVarRef__Init_t>();
})
