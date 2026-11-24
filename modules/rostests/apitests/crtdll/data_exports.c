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

    /* crtdll.dll is an import of this test, so a missing module is a failure */
    Module = GetModuleHandleA(TEST_DLL_NAME);
    ok(Module != NULL, "Failed to get the handle of %s\n", TEST_DLL_NAME);
    if (Module == NULL)
        return;

    /* Control: a neighbouring data export that always had the suffix */
    ok(GetProcAddress(Module, "__argc_dll") != NULL,
       "__argc_dll is not exported\n");

    /* These two lost the _dll suffix when crtdll moved to msvcrt sources */
    TestDataExport(Module, "_fmode_dll");
    TestDataExport(Module, "_commode_dll");

    /*
     * The unsuffixed names are a by-product of fc55d38e0ed ("[CRTDLL] Build
     * from msvcrt sources"), which renamed them in crtdll.spec, not a part of
     * the crtdll contract: Windows does not export them either.
     */
    ok(GetProcAddress(Module, "_fmode") == NULL, "_fmode is exported\n");
    ok(GetProcAddress(Module, "_commode") == NULL, "_commode is exported\n");
}
