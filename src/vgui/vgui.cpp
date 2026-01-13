#include "vgui/vgui.h"

vgui::Panel* (*vgui_Panel_FindChildByName)(vgui::Panel* thisptr, const char* childName, bool recurseDown);
void (*vgui_Label_SetText)(vgui::Label* thisptr, const char* text);

ON_DLL_LOAD_CLIENT("client.dll", VGuiOffsets, (CModule module))
{
	vgui_Panel_FindChildByName = module.Offset(0x759F50).RCast<vgui::Panel* (*)(vgui::Panel * thisptr, const char*, bool)>();
	vgui_Label_SetText = module.Offset(0x780940).RCast<void (*)(vgui::Label* thisptr, const char*)>();
}
