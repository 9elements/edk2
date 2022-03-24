#include <Base.h>
#include <Uefi.h>

#include "AspeedLPCMctpPhysicalTransportLib.h"

EFI_STATUS
MctpKcsRead (
  IN  UINT8  Offset,
  OUT UINT8  *Data
  )
{
  if ((Offset > MCTP_ASTLPC_KCS_REG_STATUS) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (FixedPcdGet8 (PcdMctpKcsType) == 0) {
    *Data = MmioRead8 (PcdGet32 (PcdMctpKcsPort) + Offset);
  } else if (FixedPcdGet8 (PcdMctpKcsType) == 1) {
    *Data = IoRead8 (PcdGet32 (PcdMctpKcsPort) + Offset);
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MctpKcsWrite (
  IN UINT8  Offset,
  IN UINT8  Data
  )
{
  if (Offset > MCTP_ASTLPC_KCS_REG_STATUS) {
    return EFI_INVALID_PARAMETER;
  }

  if (FixedPcdGet8 (PcdMctpKcsType) == 0) {
    MmioWrite8 (PcdGet32 (PcdMctpKcsPort) + Offset, Data);
  } else if (FixedPcdGet8 (PcdMctpKcsType) == 1) {
    IoWrite8 (PcdGet32 (PcdMctpKcsPort) + Offset, Data);
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
MctpKcsReady (
  IN UINT8    Status,
  IN BOOLEAN  IsWrite
  )
{
  if (IsWrite) {
    return !!(Status & KCS_STATUS_IBF) == 0;
  } else {
    return !!(Status & KCS_STATUS_OBF) == 1;
  }
}

BOOLEAN
MctpKcsReadReady (
  IN UINT8  Status
  )
{
  return MctpKcsReady (Status, FALSE);
}

BOOLEAN
MctpKcsWriteReady (
  IN UINT8  Status
  )
{
  return MctpKcsReady (Status, TRUE);
}

EFI_STATUS
MctpKcsSend (
  IN UINT8  Data
  )
{
  UINT8       StatusReg;
  EFI_STATUS  Status;

  do {
    Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_STATUS, &StatusReg);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (MctpKcsWriteReady (StatusReg)) {
      break;
    }

    // TODO: Timeout
  } while (1);

  return MctpKcsWrite (MCTP_ASTLPC_KCS_REG_DATA, Data);
}

EFI_STATUS
MctpAstLpcKcsSetStatus (
  UINT8  StatusReg
  )
{
  UINT8       Data;
  EFI_STATUS  Status;

  /* Since we're setting the status register, we want the other endpoint
   * to be interrupted. However, some hardware may only raise a host-side
   * interrupt on an ODR event.
   * So, write a dummy value of 0xff to ODR, which will ensure that an
   * interrupt is triggered, and can be ignored by the host.
   */
  Data = 0xff;

  Status = MctpKcsWrite (MCTP_ASTLPC_KCS_REG_STATUS, StatusReg);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MctpKcsWrite (MCTP_ASTLPC_KCS_REG_DATA, Data);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
