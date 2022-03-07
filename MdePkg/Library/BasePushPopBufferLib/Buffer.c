/** @file
  Helper to append or drop data from the start/end of a memory buffer.

  @copyright
  Copyright 2022 9elements GmbH <BR>

**/

#include <Library/BasePushPopBufferLib.h>

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
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer->Data   = (UINT8 *)Data;
  Buffer->Size   = Size;
  Buffer->Offset = 0;

  return EFI_SUCCESS;
}

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
  )
{
  if ((Buffer == NULL) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Size + Buffer->Offset > Buffer->Size) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (&Buffer->Data[Buffer->Offset], Data, Size);
  Buffer->Offset += Size;

  return EFI_SUCCESS;
}

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
  )
{
  if ((Buffer == NULL) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Size > Buffer->Offset) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Data, &Buffer->Data[Buffer->Offset - Size], Size);
  Buffer->Offset -= Size;

  return EFI_SUCCESS;
}

/**
  Return the mount of data in bytes currently stored in the buffer.

  @retval Number of bytes
**/
UINTN
BufferLength (
  IN  PUSH_POP_BUFFER  *Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return Buffer->Offset;
}

/**
  Return the maximum amount of data in bytes than can be stored in the buffer.

  @retval Number of bytes
**/
UINTN
BufferSize (
  IN  PUSH_POP_BUFFER  *Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return Buffer->Size;
}

/**
  Return a pointer to the memory and returns it's size.

  @retval Pointer to memory backend
**/
VOID *
BufferData (
  IN  PUSH_POP_BUFFER  *Buffer
  )
{
  return Buffer->Data;
}

/**
  Deletes the internal contents.

**/
VOID
BufferClear (
  IN  PUSH_POP_BUFFER  *Buffer
  )
{
  if (Buffer == NULL) {
    return;
  }

  Buffer->Offset = 0;
}

/**
  Updates the buffer length after manually filling it's storage backend.

**/
VOID
BufferUpdate (
  IN  PUSH_POP_BUFFER  *Buffer,
  IN  UINTN            Size
  )
{
  if (Buffer == NULL) {
    return;
  }

  Buffer->Offset = Size;
}

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
  )
{
  if ((Buffer == NULL) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Size > Buffer->Offset) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Data, Buffer->Data, Size);
  // Shift buffer
  CopyMem (Buffer->Data, &Buffer->Data[Size], Buffer->Offset - Size);

  Buffer->Offset -= Size;

  return EFI_SUCCESS;
}

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
  )
{
  if ((SrcBuffer == NULL) || (DestBuffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Size > SrcBuffer->Offset) || (Size + DestBuffer->Offset > DestBuffer->Size)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (&DestBuffer->Data[DestBuffer->Offset], SrcBuffer->Data, Size);
  DestBuffer->Offset += Size;

  // Shift buffer
  if (Size != SrcBuffer->Offset) {
    CopyMem (SrcBuffer->Data, &SrcBuffer->Data[Size], SrcBuffer->Offset - Size);
  }

  SrcBuffer->Offset -= Size;

  return EFI_SUCCESS;
}

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
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Size > Buffer->Offset) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Shift buffer
  CopyMem (Buffer->Data, &Buffer->Data[Size], Buffer->Offset - Size);

  Buffer->Offset -= Size;

  return EFI_SUCCESS;
}

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
  )
{
  CHAR16  Glyph;
  UINTN   Index;
  UINTN   Max;

  if ((Buffer == NULL) || (Str == NULL) || (Size == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Max = Buffer->Offset < *Size ? Buffer->Offset : *Size;

  for (Index = 0; Index + 1 < Max; Index += sizeof (Glyph)) {
    CopyMem (&Glyph, &Buffer->Data[Index], sizeof (Glyph));
    if (Glyph == 0) {
      if (*Size < Index + sizeof (Glyph)) {
        return EFI_BUFFER_TOO_SMALL;
      }

      return BufferPopFront (Buffer, Str, Index + sizeof (Glyph));
    }
  }

  return EFI_BUFFER_TOO_SMALL;
}
