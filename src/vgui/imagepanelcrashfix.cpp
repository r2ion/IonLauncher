DECLARE_MODULE(ImagePanelCrashFixHooks)

DECLARE_HOOK(sub_1E5F0, client.dll + 0x1E5F0, ([](auto& hook, __int64 a1) -> __int64
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

    return hook.Original(a1);
}))

ON_DLL_LOAD_CLIENT("client.dll", ImagePanelCrashFix, [](CModule module)
{
    DISPATCH_MODULE(ImagePanelCrashFixHooks);
})
