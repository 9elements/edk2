/** @file
  CBMEM console CbmemLib instance

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Coreboot.h>
#include <Uefi/UefiSpec.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Library/BaseLib.h>
#include <Library/BlParseLib.h>
#include <Library/CbmemConsoleLib.h>

//
// We can't use DebugLib due to a constructor dependency cycle between DebugLib
// and ourselves.
//
#define ASSERT(Expression)      \
  do {                          \
    if (!(Expression)) {        \
      CpuDeadLoop ();           \
    }                           \
  } while (FALSE)

#define CBMC_CURSOR_MASK ((1 << 28) - 1)
#define CBMC_OVERFLOW (1 << 31)

STATIC struct cbmem_console  *gCbConsole = NULL;
STATIC UINT32 STM_cursor = 0;

/**
  Write data from buffer to serial device.

  Writes NumberOfBytes data bytes from Buffer to the serial device.
  The number of bytes actually written to the serial device is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.
  If Buffer is NULL, then ASSERT().
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           Pointer to the data buffer to be written.
  @param  NumberOfBytes    Number of bytes to written to the serial device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the write operation failed.

**/
UINTN
EFIAPI
CbmemWrite (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  UINTN             Sent;
  UINT32            cursor;
  UINT32            flags;

  ASSERT (Buffer != NULL);

  if (NumberOfBytes == 0) {
    return 0;
  }

  if (gCbConsole == NULL) {
    return 0;
  }

  Sent = 0;
  do {
    cursor = gCbConsole->cursor & CBMC_CURSOR_MASK;
    flags = gCbConsole->cursor & ~CBMC_CURSOR_MASK;

    if (cursor >= gCbConsole->size) {
      return 0;	// Old coreboot version with legacy overflow mechanism.
    }

    gCbConsole->body[cursor++] = Buffer[Sent++];

    if (cursor >= gCbConsole->size) {
      cursor = STM_cursor;
      flags |= CBMC_OVERFLOW;
    }

    gCbConsole->cursor = flags | cursor;
  } while (Sent < NumberOfBytes);

  return Sent;
}

/**
  Locate the cbmem buffer in memory used for logging.

  @retval RETURN_SUCCESS        If console was successfully initialized.
  @retval RETURN_UNSUPPORTED    On error initializing the console or if not present.

**/
RETURN_STATUS
EFIAPI
CbmemConsoleInitialize (
  VOID
  )
{
  VOID           *CbmemPhysAddress;
  RETURN_STATUS  Status;

  Status = ParseCbMemConsoleLogBufferInfo(&CbmemPhysAddress);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  if (CbmemPhysAddress == NULL) {
    return RETURN_UNSUPPORTED;
  }

  gCbConsole = CbmemPhysAddress;

  // set the cursor such that the STM console will not overwrite the
  // coreboot console output
  STM_cursor = gCbConsole->cursor & CBMC_CURSOR_MASK;

  return RETURN_SUCCESS;
}