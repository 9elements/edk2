/** @file
  Platform Hooks file

  @copyright
  INTEL CONFIDENTIAL
  Copyright 1999 - 2020 Intel Corporation. <BR>

  The source code contained or described herein and all documents related to the
  source code ("Material") are owned by Intel Corporation or its suppliers or
  licensors. Title to the Material remains with Intel Corporation or its suppliers
  and licensors. The Material may contain trade secrets and proprietary    and
  confidential information of Intel Corporation and its suppliers and licensors,
  and is protected by worldwide copyright and trade secret laws and treaty
  provisions. No part of the Material may be used, copied, reproduced, modified,
  published, uploaded, posted, transmitted, distributed, or disclosed in any way
  without Intel's prior express written permission.

  No license under any patent, copyright, trade secret or other intellectual
  property right is granted to or conferred upon you by disclosure or delivery
  of the Materials, either expressly, by implication, inducement, estoppel or
  otherwise. Any license under such intellectual property rights must be
  express and approved by Intel in writing.

  Unless otherwise agreed by Intel in writing, you may not remove or alter
  this notice or any other notice embedded in Materials by Intel or
  Intel's suppliers or licensors in any way.
**/

#ifndef BASE_PUSH_POP_BUFFER_LIB_H__
#define BASE_PUSH_POP_BUFFER_LIB_H__

#include <Base.h>
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>

typedef struct {
  UINT8    *Data;
  UINTN    Offset;
  UINTN    Size;
} PUSH_POP_BUFFER;

/**
  Initializes a new PushPopBuffer.

  @retval EFI_SUCCESS           The operation was successful.
  @retval EFI_INVALID_PARAMETER One of the parameters was invalid.

**/
EFI_STATUS
InitPushPopBuffer (
  IN     VOID             *Data,
  IN     UINTN            Size,
  IN OUT PUSH_POP_BUFFER  *Buffer
  );

/**
  Pushes a slice of memory to the end of the buffer.
  The buffer must at least have the requested amount of bytes free.

  @retval EFI_SUCCESS           The operation was successful.
  @retval EFI_INVALID_PARAMETER One of the parameters was invalid.
  @retval EFI_BUFFER_TOO_SMALL   Requested more data than stored in the buffer.

**/
EFI_STATUS
BufferPush (
  IN PUSH_POP_BUFFER  *Buffer,
  IN VOID             *Data,
  IN UINTN            Size
  );

/**
  Pops a slice of memory from the end of the buffer.
  The buffer must at least hold the requested amount of bytes.
  The destination must be large enough to store the poped data.

  @retval EFI_SUCCESS           The operation was successful.
  @retval EFI_INVALID_PARAMETER One of the parameters was invalid.
  @retval EFI_BUFFER_TOO_SMALL   Requested more data than stored in the buffer.

**/
EFI_STATUS
BufferPop (
  IN  PUSH_POP_BUFFER  *Buffer,
  OUT VOID             *Data,
  IN  UINTN            Size
  );

/**
  Return the mount of data in bytes currently stored in the buffer.

  @retval Number of bytes
**/
UINTN
BufferLength (
  IN  PUSH_POP_BUFFER  *Buffer
  );

/**
  Return the maximum amount of data in bytes than can be stored in the buffer.

  @retval Number of bytes
**/
UINTN
BufferSize (
  IN  PUSH_POP_BUFFER  *Buffer
  );

/**
  Drops a slice of memory from the begining of the buffer.

  @retval EFI_SUCCESS            The operation was successful.
  @retval EFI_INVALID_PARAMETER  One of the parameters was invalid.
  @retval EFI_BUFFER_TOO_SMALL   Requested more data than stored in the buffer.
**/
EFI_STATUS
BufferDiscardFront (
  IN  PUSH_POP_BUFFER  *Buffer,
  IN  UINTN            Size
  );

/**
  Deletes the internal contents.

**/
VOID
BufferClear (
  IN  PUSH_POP_BUFFER  *Buffer
  );

/**
  Updates the buffer length after manually filling it's storage backend.

**/
VOID
BufferUpdate (
  IN  PUSH_POP_BUFFER  *Buffer,
  IN  UINTN            Size
  );

/**
  Return a pointer to the memory and returns it's size.

  @retval Pointer to memory backend
**/
VOID *
BufferData (
  IN  PUSH_POP_BUFFER  *Buffer
  );

/**
  Pops a slice of memory from the begining of the buffer.

  @retval EFI_SUCCESS           The operation was successful.
  @retval EFI_INVALID_PARAMETER One of the parameters was invalid.
  @retval EFI_BUFFER_TOO_SMALL   Requested more data than stored in the buffer.
**/
EFI_STATUS
BufferPopFront (
  IN  PUSH_POP_BUFFER  *Buffer,
  OUT VOID             *Data,
  IN  UINTN            Size
  );

/**
  Pops a slice of memory from the begining of the buffer and pushes it
  to another buffer.

  @retval EFI_SUCCESS           The operation was successful.
  @retval EFI_INVALID_PARAMETER One of the parameters was invalid.
  @retval EFI_BUFFER_TOO_SMALL   Requested more data than stored in the buffer.
**/
EFI_STATUS
BufferPopFrontPush (
  IN     PUSH_POP_BUFFER  *SrcBuffer,
  IN     UINTN            Size,
  IN OUT PUSH_POP_BUFFER  *DestBuffer
  );

/**
  Pops a NULL terminated WCHAR string from the begining of the buffer.

  @retval EFI_SUCCESS           The operation was successful.
  @retval EFI_INVALID_PARAMETER One of the parameters was invalid.
  @retval EFI_BUFFER_TOO_SMALL   The string in the buffer is bigger than the output buffer.
**/
EFI_STATUS
BufferPopWCHARFront (
  IN      PUSH_POP_BUFFER  *Buffer,
  OUT     CHAR16           *Str,
  OUT IN  UINTN            *Size
  );

#endif //BASE_PUSH_POP_BUFFER_LIB_H__
