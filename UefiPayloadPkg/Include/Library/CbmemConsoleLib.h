/** @file
  This library class provides common console I/O port functions.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2012 - 2014, ARM Ltd. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONSOLE_LIB__
#define __CONSOLE_LIB__

#include <Uefi/UefiBaseType.h>

/**
  Locate the cbmem buffer in memory used for logging.

  @retval RETURN_SUCCESS        If console was successfully initialized.
  @retval RETURN_UNSUPPORTED    On error initializing the console or if not present.

**/
RETURN_STATUS
EFIAPI
CbmemConsoleInitialize (
  VOID
  );

/**
  Write data from buffer to cbmem console.

  Writes NumberOfBytes data bytes from Buffer to the cbmem console.
  The number of bytes actually written to the cbmem console is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.
  If Buffer is NULL, then ASSERT().
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           Pointer to the data buffer to be written.
  @param  NumberOfBytes    Number of bytes to written to the cbmem console.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the cbmem console.
                           If this value is less than NumberOfBytes, then the write operation failed.

**/
UINTN
EFIAPI
CbmemWrite (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
);

#endif
