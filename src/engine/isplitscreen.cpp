#include "engine/isplitscreen.h"

ISplitScreen* g_pSplitScreenMgr;

ON_DLL_LOAD_CLIENT("engine.dll", SplitScreenManager, [](CModule module)
{
	g_pSplitScreenMgr = module.Offset(0x7A64B0).RCast<ISplitScreen*>();
})
