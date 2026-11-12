/*
 * PROJECT:     ReactOS kernel-mode tests
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     Kernel-mode test for the i8042prt keyboard input queue
 */

#include <kmt_test.h>

#define NDEBUG
#include <debug.h>

#include "i8042queue.h"

#define QUEUE_SIZE          8
#define SCAN_CODES_TO_FEED  64
#define CANARY_BYTE         0xA5

typedef struct _SCAN_CODE_PATTERN
{
    UCHAR ScanCode;
    USHORT ScanFlags;
} SCAN_CODE_PATTERN;

/* Make/break codes, plain and extended (E0), plus the E1 Pause prefix */
static const SCAN_CODE_PATTERN Patterns[] =
{
    { 0x1e, 0 },        /* A make */
    { 0x9e, 0 },        /* A break */
    { 0x48, KEY_E0 },   /* Up make, extended */
    { 0xc8, KEY_E0 },   /* Up break, extended */
    { 0x1d, 0 },        /* Left Ctrl make */
    { 0x9d, 0 },        /* Left Ctrl break */
    { 0x45, KEY_E1 },   /* Pause prefix */
    { 0x2a, 0 }         /* Left Shift make */
};

static
BOOLEAN
CanaryIntact(
    _In_reads_(QUEUE_SIZE + 1) PKEYBOARD_INPUT_DATA Buffer)
{
    PUCHAR Bytes = (PUCHAR)&Buffer[QUEUE_SIZE];
    ULONG i;

    for (i = 0; i < sizeof(KEYBOARD_INPUT_DATA); i++)
    {
        if (Bytes[i] != CANARY_BYTE)
            return FALSE;
    }

    return TRUE;
}

START_TEST(I8042KbdQueue)
{
    KEYBOARD_INPUT_DATA Buffer[QUEUE_SIZE + 1];
    ULONG KeysInBuffer = 0;
    ULONG Stored = 0;
    ULONG Dropped = 0;
    ULONG i;
    USHORT ExpectedFlags;
    const SCAN_CODE_PATTERN *Pattern;

    /* The queue holds QUEUE_SIZE entries; Buffer[QUEUE_SIZE] is a canary that
       catches a write at KeysInBuffer == QUEUE_SIZE */
    RtlFillMemory(Buffer, sizeof(Buffer), CANARY_BYTE);

    /* Feed more scan codes than the queue can hold */
    for (i = 0; i < SCAN_CODES_TO_FEED && KeysInBuffer <= QUEUE_SIZE; i++)
    {
        Pattern = &Patterns[i % RTL_NUMBER_OF(Patterns)];

        if (I8042KbdStoreScanCode(Buffer, QUEUE_SIZE, KeysInBuffer, Pattern->ScanCode, Pattern->ScanFlags))
        {
            KeysInBuffer++;
            Stored++;
        }
        else
        {
            Dropped++;
        }
    }

    /* Parse the counters, but keep going: the checks below report the details */
    ok_eq_ulong(KeysInBuffer, (ULONG)QUEUE_SIZE);
    ok_eq_ulong(Stored, (ULONG)QUEUE_SIZE);
    ok_eq_ulong(Dropped, (ULONG)(SCAN_CODES_TO_FEED - QUEUE_SIZE));
    ok(CanaryIntact(Buffer), "Keyboard buffer write went past the queue end\n");

    /* Every stored entry must be formatted from its scan code */
    for (i = 0; i < QUEUE_SIZE; i++)
    {
        Pattern = &Patterns[i % RTL_NUMBER_OF(Patterns)];
        ExpectedFlags = Pattern->ScanFlags | ((Pattern->ScanCode & 0x80) ? KEY_BREAK : KEY_MAKE);

        ok_eq_ulong((ULONG)Buffer[i].MakeCode, (ULONG)(Pattern->ScanCode & 0x7f));
        ok_eq_ulong((ULONG)Buffer[i].Flags, (ULONG)ExpectedFlags);
        ok_eq_ulong((ULONG)Buffer[i].Reserved, 0);
    }

    /* A queue without room must never accept a scan code */
    ok_bool_false(I8042KbdStoreScanCode(Buffer, QUEUE_SIZE, QUEUE_SIZE, 0x1e, 0),
                  "store into a full queue\n");

    /* Keep the verdict in the kernel debug log: the test runs on a LiveCD
       where the user-mode console output is not captured */
    DbgPrint("[I8042KbdQueue] stored=%lu dropped=%lu keys=%lu canary=%s full-queue-store=%s VERDICT=%s\n",
             Stored, Dropped, KeysInBuffer,
             CanaryIntact(Buffer) ? "intact" : "SMASHED",
             I8042KbdStoreScanCode(Buffer, QUEUE_SIZE, QUEUE_SIZE, 0x1e, 0) ? "accepted" : "dropped",
             (KeysInBuffer == QUEUE_SIZE && Stored == QUEUE_SIZE &&
              Dropped == SCAN_CODES_TO_FEED - QUEUE_SIZE && CanaryIntact(Buffer)) ? "PASS" : "FAIL");
}
