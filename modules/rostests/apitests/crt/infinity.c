/*
 * PROJECT:     ReactOS API tests
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Tests that INFINITY is a constant and that msxml3 loads when
 *              the x87 exceptions are unmasked
 * COPYRIGHT:   Copyright 2026 Andrey Markin <py6on@yandex.ru>
 */

/* Turns DEFINE_GUID into definitions, so that IID_IUnknown (unknown.h) and
 * CLSID_DOMDocument30 together with the msxml2.h IIDs are defined here */
#include <initguid.h>
#include <windows.h>
#include <math.h>
#include <ole2.h>
#include <msxml2.h>
#include <apitest.h>

/* Defined in fpcontrol.c */
unsigned int get_native_fpcw(void);
void set_native_fpcw(unsigned int value);

/* Overflow (OE) flag in the x87 status word */
#define X87_SW_OE 0x0008

/* x87 control word with all exceptions masked, precision control 00b */
#define X87_CW_MASKED_ALL 0x027F
/* x87 control word of Delphi / Free Pascal hosts: IM, ZM, OM unmasked */
#define X87_CW_DELPHI 0x1332

#ifdef _M_IX86

static unsigned short get_x87_status(void)
{
    unsigned short status = 0;
#if defined(_MSC_VER)
    __asm fnstsw[status];
#else
    __asm__ __volatile__("fnstsw %0" : "=m" (status));
#endif
    return status;
}

static void clear_x87_status(void)
{
#if defined(_MSC_VER)
    __asm fnclex;
#else
    __asm__ __volatile__("fnclex");
#endif
}

#endif

START_TEST(infinity)
{
#ifdef _M_IX86
    unsigned int fpcw;
    unsigned short status;
    volatile float value;
    HMODULE module;
    HRESULT hr;
    IUnknown *unknown;
    DWORD error;

    /*
     * INFINITY has to be a constant. While it was defined as
     * _HUGE_ENUF * _HUGE_ENUF, evaluating it produced an overflow, so the
     * status word must stay clean even though all exceptions are masked.
     */
    fpcw = get_native_fpcw();
    set_native_fpcw(X87_CW_MASKED_ALL);
    clear_x87_status();

    value = INFINITY;
    (void)value;

    status = get_x87_status();
    ok(!(status & X87_SW_OE), "INFINITY raised x87 overflow, status word 0x%04x\n", status);

    set_native_fpcw(fpcw);

    /*
     * User visible symptom: a host that leaves the x87 overflow exception
     * unmasked (0x1332) cannot load msxml3.dll. xmlInitParser() is called
     * from DllMain and sets xmlXPathPINF = INFINITY (xpath.c, CORE-20494),
     * which raised STATUS_FLOAT_OVERFLOW (0xC0000091) and made
     * LoadLibraryW() fail with ERROR_DLL_INIT_FAILED (CORE-20862).
     */
    fpcw = get_native_fpcw();
    set_native_fpcw(X87_CW_DELPHI);
    module = LoadLibraryW(L"msxml3.dll");
    /* Restore the FPU state before any other floating point operation */
    set_native_fpcw(fpcw);
    error = GetLastError();

    ok(module != NULL, "LoadLibraryW(msxml3.dll) failed with unmasked x87 exceptions, error %lu\n", error);
    if (!module)
        return;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ok(SUCCEEDED(hr), "CoInitializeEx failed, hr = 0x%08lx\n", hr);
    if (SUCCEEDED(hr))
    {
        unknown = NULL;
        hr = CoCreateInstance(&CLSID_DOMDocument30, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unknown);
        ok(hr == S_OK, "CoCreateInstance(CLSID_DOMDocument30) failed, hr = 0x%08lx\n", hr);
        if (SUCCEEDED(hr))
            unknown->lpVtbl->Release(unknown);
        CoUninitialize();
    }

    FreeLibrary(module);
#else
    skip("x87 specific test\n");
#endif
}
