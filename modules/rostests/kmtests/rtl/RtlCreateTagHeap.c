/*
 * PROJECT:     ReactOS kernel-mode tests
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     Test for RtlCreateTagHeap and RtlQueryTagHeap
 * COPYRIGHT:   Copyright 2026 ReactOS Lab
 */

#include <kmt_test.h>

/* Blocks the counter test works with */
#define TAG_BLOCK_COUNT 5
#define TAG_BLOCK_SIZE 0x100

START_TEST(RtlCreateTagHeap)
{
    HANDLE Heap;
    HANDLE EmptyHeap;
    ULONG Tag;
    ULONG Tag1;
    ULONG Tag2;
    ULONG TagBig;
    ULONG TagNeighbour;
    ULONG TagShift;
    PVOID Blocks[TAG_BLOCK_COUNT];
    PVOID Block;
    RTL_HEAP_TAG_INFO Info;
    PWSTR Name;
    SIZE_T SizeAllocated;
    BOOLEAN Allocated;
    ULONG i;

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

    if (Tag1 == 0 || Tag2 == 0)
    {
        RtlDestroyHeap(Heap);
        return;
    }

    ok(Tag2 != Tag1, "RtlCreateTagHeap returned the same tag 0x%lx twice\n", Tag1);

    /* The shift which RtlCreateTagHeap uses to put the index of the tag into
       the flags of the heap functions is not part of the user mode API and the
       headers disagree about its value, so derive it instead of guessing: the
       first tag of a fresh heap is 1 << shift */
    TagShift = 0;
    while (TagShift < 32 && !(Tag1 & (1UL << TagShift)))
        TagShift++;

    ok(TagShift != 0, "the first tag of a fresh heap is not 1 << shift\n");
    ok(TagShift < 32 && Tag1 == (1UL << TagShift),
       "the first tag 0x%lx is not a single shifted index\n",
       Tag1);

    /* A tag which was just created has a name and no counters */
    RtlZeroMemory(&Info, sizeof(Info));
    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap failed for a valid tag\n");
    ok_eq_ulong(Info.NumberOfAllocations, 0);
    ok_eq_ulong(Info.NumberOfFrees, 0);
    ok_eq_size(Info.BytesAllocated, 0);

    /* Allocate a few tagged blocks, all of them must be counted */
    RtlZeroMemory(Blocks, sizeof(Blocks));
    Allocated = TRUE;
    for (i = 0; i < TAG_BLOCK_COUNT; i++)
    {
        Blocks[i] = RtlAllocateHeap(Heap, Tag1, TAG_BLOCK_SIZE);
        if (Blocks[i] == NULL)
            Allocated = FALSE;
        ok(Blocks[i] != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", Tag1);
    }

    if (!Allocated)
    {
        for (i = 0; i < TAG_BLOCK_COUNT; i++)
            if (Blocks[i] != NULL)
                RtlFreeHeap(Heap, Tag1, Blocks[i]);

        RtlDestroyHeap(Heap);
        return;
    }

    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap failed after the allocations\n");
    ok_eq_ulong(Info.NumberOfAllocations, TAG_BLOCK_COUNT);
    ok_eq_ulong(Info.NumberOfFrees, 0);
    ok(Info.BytesAllocated > 0, "the tag did not account any bytes\n");
    SizeAllocated = Info.BytesAllocated;

    /* Freeing a block reduces the counters of its tag */
    for (i = 0; i < 2; i++)
    {
        ok_bool_true(RtlFreeHeap(Heap, Tag1, Blocks[i]), "RtlFreeHeap with tag");
        Blocks[i] = NULL;
    }

    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap failed after the frees\n");
    ok_eq_ulong(Info.NumberOfAllocations, TAG_BLOCK_COUNT);
    ok_eq_ulong(Info.NumberOfFrees, 2);
    ok(Info.BytesAllocated < SizeAllocated, "the freed blocks were not subtracted\n");

    /* A block is accounted to its own tag, so a free without a tag counts too */
    ok_bool_true(RtlFreeHeap(Heap, 0, Blocks[2]), "RtlFreeHeap without a tag");
    Blocks[2] = NULL;

    ok_bool_true(RtlFreeHeap(Heap, Tag1, Blocks[3]), "RtlFreeHeap with tag");
    Blocks[3] = NULL;
    ok_bool_true(RtlFreeHeap(Heap, Tag1, Blocks[4]), "RtlFreeHeap with tag");
    Blocks[4] = NULL;

    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap failed after freeing everything\n");
    ok_eq_ulong(Info.NumberOfAllocations, TAG_BLOCK_COUNT);
    ok_eq_ulong(Info.NumberOfFrees, TAG_BLOCK_COUNT);
    ok_eq_size(Info.BytesAllocated, 0);

    /* The counters are returned and reset by the same call */
    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), TRUE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap with a reset failed\n");
    ok_eq_ulong(Info.NumberOfAllocations, TAG_BLOCK_COUNT);
    ok_eq_ulong(Info.NumberOfFrees, TAG_BLOCK_COUNT);

    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap after the reset failed\n");
    ok_eq_ulong(Info.NumberOfAllocations, 0);
    ok_eq_ulong(Info.NumberOfFrees, 0);
    ok_eq_size(Info.BytesAllocated, 0);

    /* Nothing above touched the second tag */
    Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag2 >> TagShift), FALSE, &Info);
    ok(Name != NULL, "RtlQueryTagHeap failed for the second tag\n");
    ok_eq_ulong(Info.NumberOfAllocations, 0);
    ok_eq_ulong(Info.NumberOfFrees, 0);
    ok_eq_size(Info.BytesAllocated, 0);

    /* A tag travels through the Flags parameter of the heap functions */
    Block = RtlAllocateHeap(Heap, Tag2, TAG_BLOCK_SIZE);
    ok(Block != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", Tag2);
    if (Block != NULL)
    {
        ok_bool_true(RtlFreeHeap(Heap, Tag2, Block), "RtlFreeHeap with tag");

        /* The first tag is still not affected by the second one */
        Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
        ok(Name != NULL, "RtlQueryTagHeap failed after the second tag was used\n");
        ok_eq_ulong(Info.NumberOfAllocations, 0);
        ok_eq_ulong(Info.NumberOfFrees, 0);
        ok_eq_size(Info.BytesAllocated, 0);
    }

    /* A block which shrinks in place keeps its tag, and the tag size follows */
    Block = RtlAllocateHeap(Heap, Tag1, 0x1000);
    ok(Block != NULL, "RtlAllocateHeap of a big block with tag 0x%lx failed\n", Tag1);
    if (Block != NULL)
    {
        Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
        ok(Name != NULL, "RtlQueryTagHeap failed after the big allocation\n");
        ok_eq_ulong(Info.NumberOfAllocations, 1);
        SizeAllocated = Info.BytesAllocated;
        ok(SizeAllocated > 0, "the big block did not account any bytes\n");

        Block = RtlReAllocateHeap(Heap,
                                  Tag1 | HEAP_REALLOC_IN_PLACE_ONLY,
                                  Block,
                                  TAG_BLOCK_SIZE);
        ok(Block != NULL, "in place reallocation failed\n");

        Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
        ok(Name != NULL, "RtlQueryTagHeap failed after the reallocation\n");
        ok_eq_ulong(Info.NumberOfAllocations, 1);
        ok_eq_ulong(Info.NumberOfFrees, 0);
        ok(Info.BytesAllocated < SizeAllocated, "the reallocation did not update the tag\n");
        ok(Info.BytesAllocated > 0, "the reallocated block lost its tag\n");

        ok_bool_true(RtlFreeHeap(Heap, Tag1, Block), "RtlFreeHeap with tag");

        Name = RtlQueryTagHeap(Heap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info);
        ok(Name != NULL, "RtlQueryTagHeap failed after the reallocated block was freed\n");
        ok_eq_ulong(Info.NumberOfFrees, 1);
        ok_eq_size(Info.BytesAllocated, 0);
    }

    /* Index 0 is not a tag, and neither is an index which was never handed out */
    ok(RtlQueryTagHeap(Heap, 0, 0, FALSE, &Info) == NULL,
       "RtlQueryTagHeap accepted tag index 0\n");
    ok(RtlQueryTagHeap(Heap, 0, 63, FALSE, &Info) == NULL,
       "RtlQueryTagHeap accepted an unused tag index\n");

    /* A tag with an index below 0x100 is small: it is kept in the header of the
       block itself. The bigger indexes do not fit there and need an extra
       header at the end of the block, so both representations are tested here.
       RtlCreateTagHeap hands the indexes out in order, so keep calling it until
       it passes the small ones. */
    TagBig = 0;
    TagNeighbour = 0;
    for (i = 0; i < 300 && !TagBig; i++)
    {
        Tag = RtlCreateTagHeap(Heap, 0, NULL, NULL);
        if (Tag == 0)
            break;

        if ((USHORT)(Tag >> TagShift) >= 0x100)
            TagBig = Tag;
        else
            TagNeighbour = Tag;
    }

    ok(TagBig != 0, "RtlCreateTagHeap did not hand out a tag above 0xff\n");
    if (TagBig != 0)
    {
        ok((TagBig >> TagShift) >= 0x100,
           "the tag 0x%lx does not need an extra header\n", TagBig);

        /* Two blocks of the big tag are counted, nothing else is */
        Blocks[0] = RtlAllocateHeap(Heap, TagBig, TAG_BLOCK_SIZE);
        Blocks[1] = RtlAllocateHeap(Heap, TagBig, TAG_BLOCK_SIZE);
        ok(Blocks[0] != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", TagBig);
        ok(Blocks[1] != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", TagBig);

        if (Blocks[0] != NULL && Blocks[1] != NULL)
        {
            Name = RtlQueryTagHeap(Heap, 0, (USHORT)(TagBig >> TagShift), FALSE, &Info);
            ok(Name != NULL, "RtlQueryTagHeap failed for the big tag\n");
            ok_eq_ulong(Info.NumberOfAllocations, 2);
            ok_eq_ulong(Info.NumberOfFrees, 0);
            ok(Info.BytesAllocated > 0, "the big tag did not account any bytes\n");
            SizeAllocated = Info.BytesAllocated;

            ok_bool_true(RtlFreeHeap(Heap, TagBig, Blocks[0]), "RtlFreeHeap with tag");
            Blocks[0] = NULL;

            Name = RtlQueryTagHeap(Heap, 0, (USHORT)(TagBig >> TagShift), FALSE, &Info);
            ok(Name != NULL, "RtlQueryTagHeap failed after the first big free\n");
            ok_eq_ulong(Info.NumberOfFrees, 1);
            ok(Info.BytesAllocated < SizeAllocated,
               "the freed big block was not subtracted\n");

            ok_bool_true(RtlFreeHeap(Heap, TagBig, Blocks[1]), "RtlFreeHeap with tag");
            Blocks[1] = NULL;

            Name = RtlQueryTagHeap(Heap, 0, (USHORT)(TagBig >> TagShift), FALSE, &Info);
            ok(Name != NULL, "RtlQueryTagHeap failed after the second big free\n");
            ok_eq_ulong(Info.NumberOfAllocations, 2);
            ok_eq_ulong(Info.NumberOfFrees, 2);
            ok_eq_size(Info.BytesAllocated, 0);

            /* Asking again without a reset returns the same counters */
            Name = RtlQueryTagHeap(Heap, 0, (USHORT)(TagBig >> TagShift), FALSE, &Info);
            ok(Name != NULL, "RtlQueryTagHeap failed for the big tag again\n");
            ok_eq_ulong(Info.NumberOfAllocations, 2);
            ok_eq_ulong(Info.NumberOfFrees, 2);
            ok_eq_size(Info.BytesAllocated, 0);
        }

        /* The tag which was handed out before stayed without counters */
        if (TagNeighbour != 0)
        {
            Name = RtlQueryTagHeap(Heap, 0, (USHORT)(TagNeighbour >> TagShift), FALSE, &Info);
            ok(Name != NULL, "RtlQueryTagHeap failed for the neighbour tag\n");
            ok_eq_ulong(Info.NumberOfAllocations, 0);
            ok_eq_ulong(Info.NumberOfFrees, 0);
            ok_eq_size(Info.BytesAllocated, 0);
        }

        /* A big block freed without a tag still counts on its own tag */
        Block = RtlAllocateHeap(Heap, TagBig, TAG_BLOCK_SIZE);
        ok(Block != NULL, "RtlAllocateHeap with tag 0x%lx failed\n", TagBig);
        if (Block != NULL)
        {
            ok_bool_true(RtlFreeHeap(Heap, 0, Block), "RtlFreeHeap without a tag");

            Name = RtlQueryTagHeap(Heap, 0, (USHORT)(TagBig >> TagShift), FALSE, &Info);
            ok(Name != NULL, "RtlQueryTagHeap failed after a free without a tag\n");
            ok_eq_ulong(Info.NumberOfFrees, 3);
            ok_eq_size(Info.BytesAllocated, 0);
        }
    }

    /* A heap without a tag table has no tags */
    EmptyHeap = RtlCreateHeap(HEAP_GROWABLE, NULL, 0, 0x10000, NULL, NULL);
    ok(EmptyHeap != NULL, "RtlCreateHeap failed\n");
    if (EmptyHeap != NULL)
    {
        ok(RtlQueryTagHeap(EmptyHeap, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info) == NULL,
           "RtlQueryTagHeap accepted a heap without tags\n");
        RtlDestroyHeap(EmptyHeap);
    }

    /* An invalid heap handle is rejected instead of crashing */
    ok(RtlQueryTagHeap(NULL, 0, (USHORT)(Tag1 >> TagShift), FALSE, &Info) == NULL,
       "RtlQueryTagHeap accepted a null heap\n");
    Tag1 = RtlCreateTagHeap(NULL, 0, L"TagNone", NULL);
    ok_eq_ulong(Tag1, 0);

    RtlDestroyHeap(Heap);
}
