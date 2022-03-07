#include "AspeedLPCMctpPhysicalTransportLib.h"

EFI_STATUS
EFIAPI
MctpPhysicalSend (
  IN OUT   VOID       *Context,
  IN PUSH_POP_BUFFER  *Msg,
  IN UINTN            TimeoutUsec
  )
{
  MCTP_BINDING_ASTLPC  *Binding = (MCTP_BINDING_ASTLPC *)Context;

  if ((Binding == NULL) || (Msg == NULL) || (BufferSize (Msg) < PcdGet32 (PcdMctpPhysicalMtu))) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpPhysicalReadyToSend (Context)) {
    return EFI_NOT_READY;
  }

  return MctpAstLpcChannelTx (Binding, Msg, TimeoutUsec);
}

EFI_STATUS
EFIAPI
MctpPhysicalReceive (
  IN OUT   VOID       *Context,
  IN PUSH_POP_BUFFER  *Msg,
  IN UINTN            TimeoutUsec
  )
{
  MCTP_BINDING_ASTLPC  *Binding = (MCTP_BINDING_ASTLPC *)Context;

  if ((Binding == NULL) || (Msg == NULL) || (BufferSize (Msg) < PcdGet32 (PcdMctpPhysicalMtu))) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpPhysicalHasMessage (Context)) {
    return EFI_NOT_READY;
  }

  return MctpAstLpcRxStart (Binding, Msg, TimeoutUsec);
}

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend (
  IN OUT   VOID  *Context
  )
{
  EFI_STATUS           Status;
  MCTP_BINDING_ASTLPC  *Binding = (MCTP_BINDING_ASTLPC *)Context;

  if (MctpChannelReadyToTx (Binding)) {
    return TRUE;
  }

  Status = MctpChannelPoll (Binding);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return MctpChannelReadyToTx (Binding);
}

BOOLEAN
EFIAPI
MctpPhysicalHasMessage (
  IN OUT   VOID  *Context
  )
{
  EFI_STATUS           Status;
  MCTP_BINDING_ASTLPC  *Binding = (MCTP_BINDING_ASTLPC *)Context;

  if (MctpChannelReadyToRx (Binding)) {
    return TRUE;
  }

  Status = MctpChannelPoll (Binding);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return MctpChannelReadyToRx (Binding);
}

UINTN
EFIAPI
MctpPhysicalMTU (
  IN OUT   VOID  *Context
  )
{
  MCTP_BINDING_ASTLPC  *Binding = (MCTP_BINDING_ASTLPC *)Context;

  //
  // Update channel status
  //
  if (EFI_ERROR (MctpChannelPoll (Binding))) {
    return 0;
  }

  return MctpAstLpcChannelMTU (Binding);
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
  MCTP_BINDING_ASTLPC  *Binding = (MCTP_BINDING_ASTLPC *)Context;

  DEBUG ((DEBUG_INFO, "%a: Called...\n", __FUNCTION__));

  return MctpInitHost (Binding, 100000);
}
