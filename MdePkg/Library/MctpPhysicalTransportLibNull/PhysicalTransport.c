#include <Base.h>
#include <Uefi.h>

#include <Library/MctpPhysicalTransportLib.h>

UINTN
EFIAPI
MctpPhysicalMTU (
  IN OUT   VOID  *Context
  )
{
  return MCTP_BTU;
}

EFI_STATUS
EFIAPI
MctpPhysicalSend (
  IN OUT   VOID       *Context,
  IN PUSH_POP_BUFFER  *Msg,
  IN UINTN            TimeoutUsec
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpPhysicalReceive (
  IN OUT   VOID       *Context,
  IN PUSH_POP_BUFFER  *Msg,
  IN UINTN            TimeoutUsec
  )
{
  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend (
  IN OUT   VOID  *Context
  )
{
  return TRUE;
}

BOOLEAN
EFIAPI
MctpPhysicalHasMessage (
  IN OUT   VOID  *Context
  )
{
  return FALSE;
}

UINT8
EFIAPI
MctpPhysicalGetEndpointInformation (
  IN OUT   VOID  *Context
  )
{
  return 0;
}

UINT8
EFIAPI
MctpPhysicalGetTransportHeaderVersion (
  IN OUT   VOID  *Context
  )
{
  return 1;
}

EFI_STATUS
EFIAPI
MctpPhysicalConnect (
  OUT   VOID  *Context
  )
{
  return EFI_SUCCESS;
}
