/*
 * PROJECT:         ReactOS API tests
 * LICENSE:         GPL - See COPYING in the top level directory
 * PURPOSE:         Test for the reference counting of SHARED_FACE
 * PROGRAMMERS:     Andrey Markin
 */

#include "precomp.h"

#include <ndk/umtypes.h>
#include <ndk/extypes.h>
#include <ndk/exfuncs.h>
#include <ndk/rtlfuncs.h>

/* Pool tag of the FreeType glue code (TAG_FREETYPE in
 * win32ss/drivers/font/ftfd/rosglue.c); stored as 'PYTF', so it is
 * listed as "FTYP" in the pool statistics. */
#define TAG_FREETYPE 'PYTF'

#define SHAREDFACE_CYCLES 100
#define SHAREDFACE_GROWTH_LIMIT (64 * 1024)

/* Returns the amount of paged pool used by the FreeType tag, or
 * (SIZE_T)-1 when the pool tag information is not available. */
static SIZE_T
GetFontPagedPool(VOID)
{
    NTSTATUS Status;
    PSYSTEM_POOLTAG_INFORMATION PoolInfo;
    PVOID Buffer;
    ULONG Size = 0x10000;
    ULONG Returned;
    ULONG Attempt, i;
    SIZE_T Used;

    for (Attempt = 0; Attempt < 3; ++Attempt)
    {
        Buffer = HeapAlloc(GetProcessHeap(), 0, Size);
        if (!Buffer)
            return (SIZE_T)-1;

        Returned = 0;
        Status = NtQuerySystemInformation(SystemPoolTagInformation,
                                          Buffer, Size, &Returned);
        if (Status == STATUS_INFO_LENGTH_MISMATCH)
        {
            HeapFree(GetProcessHeap(), 0, Buffer);
            if (Returned > Size)
                Size = Returned;
            Size += 0x1000;
            continue;
        }

        if (!NT_SUCCESS(Status))
        {
            HeapFree(GetProcessHeap(), 0, Buffer);
            return (SIZE_T)-1;
        }

        PoolInfo = Buffer;
        if (PoolInfo->Count == 0)
        {
            HeapFree(GetProcessHeap(), 0, Buffer);
            return (SIZE_T)-1;
        }

        Used = 0;
        for (i = 0; i < PoolInfo->Count; ++i)
        {
            if (PoolInfo->TagInfo[i].TagUlong == TAG_FREETYPE)
                Used += PoolInfo->TagInfo[i].PagedUsed;
        }

        HeapFree(GetProcessHeap(), 0, Buffer);
        return Used;
    }

    return (SIZE_T)-1;
}

START_TEST(SharedFaceRefCount)
{
    HMODULE Module;
    HRSRC Resource;
    HGLOBAL Template;
    PVOID Buffer;
    DWORD Size, NumFaces;
    HANDLE hFont;
    SIZE_T Baseline, After;
    LONG Growth;
    UINT i;

    Module = GetModuleHandleW(NULL);
    Resource = FindResourceA(Module, "ReactOSTestTahoma.ttf", MAKEINTRESOURCE(RT_RCDATA));
    if (!Resource)
    {
        skip("ReactOSTestTahoma.ttf resource not found\n");
        return;
    }

    Size = SizeofResource(Module, Resource);
    Template = LoadResource(Module, Resource);
    Buffer = LockResource(Template);
    if (!Buffer || !Size)
    {
        skip("ReactOSTestTahoma.ttf resource cannot be loaded\n");
        return;
    }

    /* The first add/remove cycle initialises the lazy FreeType allocations */
    NumFaces = 0;
    hFont = AddFontMemResourceEx(Buffer, Size, NULL, &NumFaces);
    if (!hFont)
    {
        skip("AddFontMemResourceEx failed\n");
        return;
    }
    RemoveFontMemResourceEx(hFont);

    Baseline = GetFontPagedPool();
    if (Baseline == (SIZE_T)-1)
    {
        skip("SystemPoolTagInformation is not available\n");
        return;
    }

    for (i = 0; i < SHAREDFACE_CYCLES; ++i)
    {
        NumFaces = 0;
        hFont = AddFontMemResourceEx(Buffer, Size, NULL, &NumFaces);
        if (!hFont)
        {
            ok(0, "AddFontMemResourceEx failed on cycle %u\n", i);
            break;
        }

        if (!RemoveFontMemResourceEx(hFont))
        {
            ok(0, "RemoveFontMemResourceEx failed on cycle %u\n", i);
            break;
        }
    }

    After = GetFontPagedPool();
    if (After == (SIZE_T)-1)
    {
        skip("SystemPoolTagInformation is not available\n");
        return;
    }

    /* A negative difference means that the pool shrank, which passes as well */
    Growth = (After >= Baseline) ? (LONG)(After - Baseline) : -(LONG)(Baseline - After);
    trace("FTYP paged pool: baseline=%lu after=%lu growth=%ld bytes over %u cycles\n",
          (unsigned long)Baseline, (unsigned long)After, (long)Growth, SHAREDFACE_CYCLES);
    ok(Growth <= SHAREDFACE_GROWTH_LIMIT,
       "FTYP paged pool grew by %ld bytes over %u add/remove cycles (limit %u)\n",
       (long)Growth, (unsigned)SHAREDFACE_CYCLES, (unsigned)SHAREDFACE_GROWTH_LIMIT);
}
