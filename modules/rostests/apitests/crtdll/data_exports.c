/*
 * PROJECT:     ReactOS API tests
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Test for the crtdll data exports
 * PROGRAMMERS: Andrey Markin
 */

#include <apitest.h>
#include <windows.h>

static void TestDataExport(HMODULE Module, PCSTR Name)
{
    int *Value;
    int OriginalValue;

    Value = (int *)GetProcAddress(Module, Name);
    ok(Value != NULL, "GetProcAddress(%s) failed\n", Name);
    if (Value == NULL)
        return;

    /* Write a temporary value and restore it, so the CRT state survives */
    OriginalValue = *Value;
    *Value = ~OriginalValue;
    ok(*Value == ~OriginalValue, "%s: write/read mismatch\n", Name);
    *Value = OriginalValue;
    ok(*Value == OriginalValue, "%s: failed to restore the value\n", Name);
}

START_TEST(data_exports)
{
    HMODULE Module;

    Module = GetModuleHandleA("crtdll.dll");
    ok(Module != NULL, "crtdll.dll is not loaded\n");
    if (Module == NULL)
    {
        skip("crtdll.dll is not loaded\n");
        return;
    }

    /* Control: a neighbouring data export that always had the suffix */
    ok(GetProcAddress(Module, "__argc_dll") != NULL,
       "__argc_dll is not exported\n");

    /* These two lost the _dll suffix when crtdll moved to msvcrt sources */
    TestDataExport(Module, "_fmode_dll");
    TestDataExport(Module, "_commode_dll");

    /* The crtdll contract only uses the suffixed names */
    ok(GetProcAddress(Module, "_fmode") == NULL, "_fmode is exported\n");
    ok(GetProcAddress(Module, "_commode") == NULL, "_commode is exported\n");
}
