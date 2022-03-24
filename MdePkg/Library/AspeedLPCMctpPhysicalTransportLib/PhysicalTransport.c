#include "AspeedLPCMctpPhysicalTransportLib.h"

EFI_STATUS
EFIAPI
MctpPhysicalSend (
  IN MCTP_PHYSICAL_MSG  *Msg,
  IN UINTN              TimeoutUsec
  )
{
  if (Msg == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpPhysicalReadyToSend ()) {
    return EFI_NOT_READY;
  }

  return MctpAstLpcChannelTx (Msg, TimeoutUsec);
}

EFI_STATUS
EFIAPI
MctpPhysicalReceive (
  IN MCTP_PHYSICAL_MSG  *Msg,
  IN UINTN              TimeoutUsec
  )
{
  if (Msg == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpPhysicalHasMessage ()) {
    return EFI_NOT_READY;
  }

  return MctpAstLpcRxStart (Msg, TimeoutUsec);
}

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend (
  VOID
  )
{
  UINT8       DataReg;
  EFI_STATUS  Status;

  Status = MctpChannelDataReg (&DataReg);

  return !EFI_ERROR (Status) && (DataReg == 0x02);
}

BOOLEAN
EFIAPI
MctpPhysicalHasMessage (
  VOID
  )
{
  UINT8       DataReg;
  EFI_STATUS  Status;

  Status = MctpChannelDataReg (&DataReg);

  return !EFI_ERROR (Status) && (DataReg == 0x01);
}

UINT8
EFIAPI
MctpPhysicalGetEndpointInformation (
  VOID
  )
{
  return 0;
}

EFI_STATUS
EFIAPI
MctpPhysicalConnect (
  VOID
  )
{
  return MctpInitHost ();
}
