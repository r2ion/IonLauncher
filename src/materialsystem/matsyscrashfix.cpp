#include <winternl.h>

// I trust claude sonnet (sonny) 4.5 with my life

AUTOHOOK_INIT()

uint64_t qword_1BBB080;
DWORD TlsIndex;

// Type definitions for function pointers
typedef void(__fastcall* sub_878D0_t)(unsigned int, unsigned int);
typedef __int64(__fastcall* sub_83C10_t)(__int64);
typedef __int64(__fastcall* sub_86DC0_t)(__int64);

// Function pointers
sub_878D0_t sub_878D0_func;
sub_83C10_t sub_83C10;
sub_86DC0_t sub_86DC0;

// Global variables/pointers referenced in the code
void** off_2229C0;
int dword_21E0E0;
uintptr_t sub_44D80;
uintptr_t sub_5B20;


AUTOHOOK(sub_87A50, materialsystem_dx11.dll + 0x87A50, __int64*, __fastcall, (__int64 a1, int a2, int a3))
{
    __int64 v4 = *((__int64*)*((void**)NtCurrentTeb() + 11) + (unsigned int)TlsIndex);
    int v5 = a2 + 8;
    __int64 v6 = *(int*)(v4 + 4136);

    // Call sub_878D0
    sub_878D0_func((unsigned int)v6, (unsigned int)v5);

    // Get pointer to the structure array
    __int64* v8 = (__int64*)(qword_1BBB080) + (8 * v6);

    // Validate v8 before accessing
    __try
    {
        volatile __int64 test = v8[0];
        volatile __int64 test2 = v8[1];
        volatile int test3 = *((int*)v8 + 2);
        volatile int test4 = *((int*)v8 + 4);
        volatile int test5 = *((int*)v8 + 6);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        spdlog::error("Invalid v8 pointer in materialsystem_dx11 sub_87A50");
        return nullptr;
    }

    int v9 = a3 & (-8 - *((int*)v8 + 4) - *((int*)v8 + 2));

    if (v9)
    {
        sub_878D0_func((unsigned int)v6, (unsigned int)(v9 + v5));
    }

    // Get v10 and v11
    unsigned int v10 = *((unsigned int*)v8 + 6);
    unsigned int v11 = *((unsigned int*)v8 + 4);

    // Critical validation: check if the write destination is valid
    if (v10 != v11)
    {
        __int64 baseAddr = v8[1];

        // Validate base address
        if (!baseAddr || baseAddr < 0x10000)
        {
            spdlog::error("Invalid base address v8[1] = 0x{:x}", baseAddr);
            return nullptr;
        }

        __int64 writeAddress = baseAddr + v10 + 8;

        // Validate the write address with bounds checking
        __try
        {
            // Test if we can access this address
            volatile int testRead = *(DWORD*)writeAddress;

            // If we got here, address is valid - do the actual write
            *(DWORD*)writeAddress = v11;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            spdlog::error("Invalid write destination 0x{:x} in materialsystem_dx11 sub_87A50, v10={}, v11={}",
                writeAddress, v10, v11);
            return nullptr;
        }
    }

    // Complete the rest of the original function logic
    __try
    {
        int64_t* v12 = (int64_t*)(v8[1] + v11);
        *v12 = a1;
        v12[1] = 0LL;
        *((int*)v8 + 4) += v9 + 8;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        spdlog::error("Exception while completing sub_87A50 logic");
        return nullptr;
    }

    return v8;
}

AUTOHOOK(sub_499E0, materialsystem_dx11.dll + 0x499E0, __int64, __fastcall, (__int64 a1, __int64 a2, int a3))
{
    __int64 v4 = a1 + 56;

    (*((void (__fastcall **)(void **, uint64_t, __int64, uint64_t, DWORD, __int64, __int64, int))*off_2229C0 + 10))(
        off_2229C0,
        *(uint64_t *)(a1 + 16),
        a1 + 56,
        *(uint64_t *)(a1 + 88),
        *(DWORD *)(a1 + 48),
        a1 + 112,
        a2,
        a3);

    if (dword_21E0E0 < 2)
    {
        uint64_t *v5 = (uint64_t *)(a1 + 128);
        __int64 v6 = *(uint64_t *)(a1 + 128);

        if (v6)
        {
            (*(void (__fastcall **)(__int64))(*(uint64_t *)v6 + 16LL))(v6);
            *v5 = 0LL;
        }

        int v7 = (*(__int64 (__fastcall **)(uint64_t, __int64, __int64))(**(uint64_t **)(a1 + 16) + 64LL))(
            *(uint64_t *)(a1 + 16),
            a1 + 56,
            a1 + 112);

        if (v7)
        {
            __int64 *v8 = sub_87A50((__int64)sub_44D80, 24, 7);

            // Validate v8 pointer before dereferencing
            if (!v8)
            {
                spdlog::error("sub_87A50 returned nullptr in sub_499E0");
                return sub_83C10(v4);
            }

            // Validate v8[1] before using it
            __int64 baseAddr = v8[1];
            if (!baseAddr || baseAddr < 0x10000)
            {
                spdlog::error("Invalid base address v8[1] = 0x{:x} in sub_499E0", baseAddr);
                return sub_83C10(v4);
            }

            // Validate the offset
            unsigned int offset = *((unsigned int *)v8 + 4);
            __int64 v9Addr = baseAddr + offset;

            // Validate v9 address before accessing
            __try
            {
                volatile __int64 testRead = *(uint64_t*)v9Addr;

                uint64_t *v9 = (uint64_t*)v9Addr;
                v9[2] = 0LL;
                v9[1] = (uint64_t)v5;
                *v9 = sub_5B20;
                *((DWORD *)v9 + 4) = v7;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                spdlog::error("Invalid v9 address 0x{:x} in sub_499E0", v9Addr);
                return sub_83C10(v4);
            }

            sub_86DC0(24LL);
        }
    }

    return sub_83C10(v4);
}

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", MaterialSystemCrashFix, (CModule module))
{
    sub_878D0_func = module.Offset(0x878D0).RCast<sub_878D0_t>();
    sub_83C10 = module.Offset(0x83C10).RCast<sub_83C10_t>();
    sub_86DC0 = module.Offset(0x86DC0).RCast<sub_86DC0_t>();

    qword_1BBB080 = module.Offset(0x1BBB080);
    TlsIndex = *module.Offset(0x1DE5A74).RCast<DWORD*>();

    off_2229C0 = module.Offset(0x2229C0).RCast<void**>();
    dword_21E0E0 = *module.Offset(0x21E0E0).RCast<int*>();
    sub_44D80 = module.Offset(0x44D80);
    sub_5B20 = module.Offset(0x5B20);
}
