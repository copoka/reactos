/*
 * PROJECT:     ReactOS i8042prt PS/2 port driver
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Keyboard input queue helper, shared with the kernel-mode tests
 */

#ifndef _I8042QUEUE_H_
#define _I8042QUEUE_H_

#include <ntddkbd.h>

/*
 * Store a scan code in the keyboard input queue.
 *
 * The keyboard buffer holds KeyboardDataQueueSize entries and the interrupt
 * service routine runs at DIRQL, while the DPC that drains the queue runs
 * asynchronously. A scan code can therefore arrive when the queue is full:
 * writing it at index KeysInBuffer == KeyboardDataQueueSize would go past the
 * end of the non-paged buffer, so such a scan code is dropped instead.
 *
 * Returns TRUE when the scan code was stored, FALSE when it was dropped.
 */
static __inline
BOOLEAN
I8042KbdStoreScanCode(
	_Out_writes_(QueueSize) PKEYBOARD_INPUT_DATA Buffer,
	_In_ ULONG QueueSize,
	_In_ ULONG KeysInBuffer,
	_In_ UCHAR ScanCode,
	_In_ USHORT ScanFlags)
{
	PKEYBOARD_INPUT_DATA InputData;

	if (KeysInBuffer >= QueueSize)
		return FALSE;

	InputData = &Buffer[KeysInBuffer];
	InputData->Flags = ScanFlags;
	if (ScanCode & 0x80)
		InputData->Flags |= KEY_BREAK;
	else
		InputData->Flags |= KEY_MAKE;
	InputData->MakeCode = ScanCode & 0x7f;
	InputData->Reserved = 0;

	return TRUE;
}

#endif /* _I8042QUEUE_H_ */
