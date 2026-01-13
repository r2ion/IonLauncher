AUTOHOOK_INIT()

AUTOHOOK(sub_1E5F0, client.dll + 0x1E5F0, __int64, __fastcall, (__int64 a1))
{
    if (!a1)
        return 0;

    __int64** pPtr = (__int64**)(a1 + 616);

    __try
    {
        __int64* v8 = *pPtr;
        if (v8)
        {
            volatile __int64 test = *v8;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        spdlog::error("Invalid pointer at offset a1+616, setting to nullptr");
        *pPtr = nullptr;
        return 0;
    }

    return sub_1E5F0(a1);
}

ON_DLL_LOAD_CLIENT("client.dll", ImagePanelCrashFix, (CModule module))
{
	AUTOHOOK_DISPATCH()
}
