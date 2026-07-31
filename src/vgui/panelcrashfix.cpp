DECLARE_MODULE(PanelCrashFixHooks)

static void** s_ppMatSystemSurface = nullptr;

static bool IsMatSystemSurfaceAvailable()
{
	if (!s_ppMatSystemSurface)
		return false;

	__try
	{
		void* pMatSystemSurface = *s_ppMatSystemSurface;
		return pMatSystemSurface && *reinterpret_cast<void**>(pMatSystemSurface);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

DECLARE_HOOK(Panel__Repaint, client.dll + 0x76DDB0, [](auto& hook, void* pPanel) -> __int64
{
	if (!IsMatSystemSurfaceAvailable())
	{
		// Preserve the repaint flag without calling into a surface that has already shut down.
		if (pPanel)
			*reinterpret_cast<unsigned int*>(reinterpret_cast<uintptr_t>(pPanel) + 0x98) |= 2;

		return 0;
	}

	return hook.Original(pPanel);
})

ON_DLL_LOAD_CLIENT("client.dll", PanelCrashFix, [](CModule module)
{
	s_ppMatSystemSurface = module.Offset(0x2E44020).RCast<void**>();
	DISPATCH_MODULE(PanelCrashFixHooks);
})
