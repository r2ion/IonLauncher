
bool* bIsOriginOverlayEnabled;

DECLARE_MODULE(ScriptBrowserHooks)

DECLARE_HOOK(OpenExternalWebBrowser, engine.dll + 0x184E40, [](auto& hook, char* pUrl, char flags)
{
	bool bIsOriginOverlayEnabledOriginal = *bIsOriginOverlayEnabled;
	bool isHttp = !strncmp(pUrl, "http://", 7) || !strncmp(pUrl, "https://", 8);
	if (flags & 2 && isHttp) // custom force external browser flag
		*bIsOriginOverlayEnabled = false; // if this bool is false, game will use an external browser rather than the origin overlay one

	hook.Original(pUrl, flags);
	*bIsOriginOverlayEnabled = bIsOriginOverlayEnabledOriginal;
})

ON_DLL_LOAD_CLIENT("engine.dll", ScriptExternalBrowserHooks, [](CModule module)
{
	DISPATCH_MODULE(ScriptBrowserHooks)

	bIsOriginOverlayEnabled = module.Offset(0x13978255).RCast<bool*>();
})
