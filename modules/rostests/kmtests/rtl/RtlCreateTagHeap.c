/*
 * PROJECT:     ReactOS kernel-mode tests
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     Test for RtlCreateTagHeap
 * COPYRIGHT:   Copyright 2026 ReactOS Lab
 */

#include <kmt_test.h>

START_TEST(RtlCreateTagHeap)
{
    HANDLE Heap;
    ULONG Tag1;
    ULONG Tag2;
    PVOID Block;

    /* Create a private heap so the test does not touch the process heap */
    Heap = RtlCreateHeap(HEAP_GROWABLE, NULL, 0, 0x10000, NULL, NULL);
    ok(Heap != NULL, "RtlCreateHeap failed\n");
    if (Heap == NULL)
        return;

    /* The first call creates the tag table and hands out a tag */
    Tag1 = RtlCreateTagHeap(Heap, 0, L"TagOne", L"SubOne");
    ok(Tag1 != 0, "RtlCreateTagHeap returned no tag\n");

    /* Every call returns a new tag, index 0 means "no tag" */
    Tag2 = RtlCreateTagHeap(Heap, 0, L"TagTwo", NULL);
    ok(Tag2 != 0, "RtlCreateTagHeap returned no tag for the second tag\n");

    /* A tag travels through the Flags parameter of the heap functions */
    if (Tag1 != 0 && Tag2 != 0)
    {
        ok(Tag2 != Tag1, "RtlCreateTagHeap returned the same tag 0x%lx twice\n", Tag1);

        Block = RtlAllocateHeap(Heap, Tag1, 0x100);
        ok(Block != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", Tag1);
        if (Block != NULL)
            ok_bool_true(RtlFreeHeap(Heap, Tag1, Block), "RtlFreeHeap with tag");

        Block = RtlAllocateHeap(Heap, Tag2, 0x100);
        ok(Block != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", Tag2);
        if (Block != NULL)
            ok_bool_true(RtlFreeHeap(Heap, Tag2, Block), "RtlFreeHeap with tag");
    }

    /* An invalid heap handle is rejected instead of crashing */
    Tag1 = RtlCreateTagHeap(NULL, 0, L"TagNone", NULL);
    ok_eq_ulong(Tag1, 0);

    RtlDestroyHeap(Heap);
}
