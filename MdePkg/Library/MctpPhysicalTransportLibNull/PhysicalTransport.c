#include <Base.h>
#include <Uefi.h>

#include <Library/MctpPhysicalTransportLib.h>

EFI_STATUS
EFIAPI
MctpPhysicalSend (
  IN MCTP_PHYSICAL_MSG  *Msg,
  IN UINTN              TimeoutUsec
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpPhysicalReceive (
  IN MCTP_PHYSICAL_MSG  *Msg,
  IN UINTN              TimeoutUsec
  )
{
  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend (
  VOID
  )
{
  return TRUE;
}

BOOLEAN
EFIAPI
MctpPhysicalHasMessage (
  VOID
  )
{
  return FALSE;
}

UINT8
EFIAPI
MctpPhysicalGetEndpointInformation (
  VOID
  )
{
  return 0;
}
