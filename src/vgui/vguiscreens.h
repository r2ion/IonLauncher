#pragma once

// Re-parses scripts/vgui_screens.txt (including mod KeyValues patches) so screens
// added by mods become known PanelMetaClasses. No-op until the game's own initial load happened.
void ReloadVGuiScreens();
