#include "AspeedLPCMctpPhysicalTransportLib.h"

EFI_STATUS
MctpLpcWrite (
  IN VOID   *Data,
  IN UINTN  Offset,
  IN UINTN  Length
  )
{
  UINTN  Index;
  UINT8  *DataPtr = Data;

  if ((Offset + Length) > PcdGet32 (PcdAspeedLPCWindowSize)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < Length; Index++) {
    MmioWrite8 (PcdGet32 (PcdAspeedLPCWindowBaseAddress) + Index + Offset, DataPtr[Index]);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MctpLpcRead (
  OUT VOID   *Data,
  IN  UINTN  Offset,
  IN  UINTN  Length
  )
{
  UINTN  Index;
  UINT8  *DataPtr = Data;

  if ((Offset + Length) > PcdGet32 (PcdAspeedLPCWindowSize)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < Length; Index++) {
    DataPtr[Index] = MmioRead8 (PcdGet32 (PcdAspeedLPCWindowBaseAddress) + Index + Offset);
  }

  return EFI_SUCCESS;
}
