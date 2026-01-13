#include "vscript/squirrel/squirrel.h"
#include "vgui/basemodui.h"

using CClientScriptHudElement = void*;
using CUtlString = const char*;
using CGameUIConVarRef__Init_t = __int64 (*)(__int64, const char*, bool);

CGameUIConVarRef__Init_t CGameUIConVarRef__Init = nullptr;

class CDialogListButton
{
public:
	struct DialogListItem_t
	{
		// CUtlString m_String;
		// CUtlString m_StringParm1;
		// CUtlString m_CommandString;
		// bool m_bEnabled;
		char pad[72];
	};
};

// static_assert(sizeof(CDialogListButton::DialogListItem_t) == 72, "Wrong size");

ADD_SQFUNC("void", Hud_DialogList_RemoveListItems, "var elem", "", ScriptContext::UI)
{
	CClientScriptHudElement* hudElement = g_pSquirrel[context]->gethudelement<CClientScriptHudElement>(sqvm, 1);

	if (!hudElement)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

	uintptr_t dialogListButton = *(uintptr_t*)((uintptr_t)hudElement + 40);

	using VFuncType = uintptr_t(__fastcall*)(uintptr_t);
	VFuncType vfunc = *(VFuncType*)(*(uintptr_t*)dialogListButton + 1520);
	uintptr_t isDialogListButton = vfunc(dialogListButton);

	using NameFuncType = const char*(__fastcall*)(uintptr_t);
	NameFuncType nameFunc = *(NameFuncType*)(*(uintptr_t*)dialogListButton + 168);
	const char* name = nameFunc(dialogListButton);

	if (!isDialogListButton)
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
		return SQRESULT_ERROR;
	}

    int count = *(int*)(dialogListButton + 1328);
    CDialogListButton::DialogListItem_t* vectorBase = (CDialogListButton::DialogListItem_t*)(*(uintptr_t*)(dialogListButton + 1304));

	// epic memory leak probably
	*(int*)(dialogListButton + 1328) = 0;

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

	uintptr_t dialogListButton = *(uintptr_t*)((uintptr_t)hudElement + 40);

	using VFuncType = uintptr_t(__fastcall*)(uintptr_t);
	VFuncType vfunc = *(VFuncType*)(*(uintptr_t*)dialogListButton + 1520);
	uintptr_t isDialogListButton = vfunc(dialogListButton);

	using NameFuncType = const char*(__fastcall*)(uintptr_t);
	NameFuncType nameFunc = *(NameFuncType*)(*(uintptr_t*)dialogListButton + 168);
	const char* name = nameFunc(dialogListButton);

	if (!isDialogListButton)
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
		return SQRESULT_ERROR;
	}

	void* convarRef = malloc(0x10);
	__int64 result = CGameUIConVarRef__Init((uintptr_t)convarRef, convarName, false);

	*(uintptr_t**)(dialogListButton + 1256) = (uintptr_t*)convarRef;

	return SQRESULT_NULL;
}

using sub_738940_t = __int64 (__fastcall *)(uint64_t);
sub_738940_t sub_738940 = nullptr;
using sub_4A3620_t = int (__fastcall *)(void);
sub_4A3620_t sub_4A3620 = nullptr;
using sub_733BB0_t = unsigned int (__fastcall *)(unsigned __int8 *, unsigned __int8 *);
sub_733BB0_t sub_733BB0 = nullptr;
using sub_73A300_t = int (__fastcall *)(__int64);
sub_73A300_t sub_73A300 = nullptr;
using sub_739B90_t = __int64 (__fastcall *)(__int64);
sub_739B90_t sub_739B90 = nullptr;

HMODULE clientBase = 0;

AUTOHOOK_INIT()

AUTOHOOK(DialogListButton__IsDefaultValue, client.dll + 0x4C96A0, bool, __fastcall, (__int64 a1))
{
  __int64 v3; // rcx
  __int64 v4; // rax
  __int64 v5; // rbx
  unsigned __int8 *v6; // rdi
  int v7; // eax
  __int64 v8; // rbx
  __int64 v9; // rax
  __int64 v10; // rax
  __int64 v11; // rbx
  unsigned __int8 *v12; // rdi
  __int64 v13; // rax
  __int64 v14; // r9
  unsigned __int8 *v15; // rax

  uintptr_t base = (uintptr_t)clientBase;
  auto qword_C3D940 = *(uint64_t *)(base + 0xC3D940);

  if ( *(DWORD *)(a1 + 1132) != 3 )
    return 0;
  v3 = *(uint64_t *)(a1 + 1256);
  if ( v3 )
  {
    v4 = sub_738940(*(uint64_t *)(v3 + 8));
    v5 = *(uint64_t *)(a1 + 1256);
    v6 = (unsigned __int8 *)v4;
    v7 = sub_4A3620();
    if ( (unsigned int)sub_733BB0(v6, *(unsigned __int8 **)(*(uint64_t *)(v5 + 16LL * v7 + 8) + 72LL)) )
      return 1;
  }
  if ( sub_73A300(a1 + 1264) )
  {
    v8 = *(uint64_t *)qword_C3D940;
    v9 = sub_739B90(a1 + 1264);
    v10 = (*(__int64 (__fastcall **)(__int64, uint64_t, __int64, uint64_t))(v8 + 808))(
            qword_C3D940,
            *(unsigned __int8 *)(a1 + 1296),
            v9,
            0LL);
    v11 = *(uint64_t *)qword_C3D940;
    v12 = (unsigned __int8 *)v10;
    v13 = sub_739B90(a1 + 1264);
    v15 = (unsigned __int8 *)(*(__int64 (__fastcall **)(__int64, uint64_t, __int64, __int64))(v11 + 808))(
                               qword_C3D940,
                               *(unsigned __int8 *)(a1 + 1296),
                               v13,
                               1);

	// this can be null when we use Hud_DialogList_RemoveListItems
	if( !v12 )
		return 0;
    if ( (unsigned int)sub_733BB0(v12, v15) )
      return 1;
  }
  return 0;
}


ON_DLL_LOAD_CLIENT_RELIESON("client.dll", CGameUIConVarRef, ConVar, (CModule module))
{
	sub_738940 = module.Offset(0x738940).RCast<sub_738940_t>();
	sub_4A3620 = module.Offset(0x4A3620).RCast<sub_4A3620_t>();
	sub_733BB0 = module.Offset(0x733BB0).RCast<sub_733BB0_t>();
	sub_73A300 = module.Offset(0x73A300).RCast<sub_73A300_t>();
	sub_739B90 = module.Offset(0x739B90).RCast<sub_739B90_t>();
    clientBase = (HMODULE)module.GetModuleBase();

	AUTOHOOK_DISPATCH()
	CGameUIConVarRef__Init = module.Offset(0x4A34A0).RCast<CGameUIConVarRef__Init_t>();
}
