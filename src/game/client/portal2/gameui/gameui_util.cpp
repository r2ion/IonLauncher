#include "game/client/portal2/gameui/gameui_util.h"
#include "util/utils.h"

using CGameUIConVarRef_Init_t = CGameUIConVarRef* (*)(CGameUIConVarRef*, const char*);
static CGameUIConVarRef_Init_t CGameUIConVarRef__Init = nullptr;

CGameUIConVarRef* CGameUIConVarRef::Init(const char* convarName)
{
	return CGameUIConVarRef__Init(this, convarName);
}

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", CGameUIConVarRef, ConVar, [](CModule module)
{
	CGameUIConVarRef__Init = module.Offset(0x4A34A0).RCast<CGameUIConVarRef_Init_t>();
})
