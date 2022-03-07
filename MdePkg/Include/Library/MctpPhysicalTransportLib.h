/** @file
  Provide services to MCTP physical message transceiving.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_PHYSICAL_TRANSPORT_LIB_H__
#define __MCTP_PHYSICAL_TRANSPORT_LIB_H__

#include <IndustryStandard/Mctp.h>
#include <Library/BasePushPopBufferLib.h>

EFI_STATUS
EFIAPI
MctpPhysicalSend (
  IN OUT   VOID       *Context,
  IN PUSH_POP_BUFFER  *Msg,
  IN UINTN            TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpPhysicalReceive (
  IN OUT   VOID       *Context,
  IN PUSH_POP_BUFFER  *Msg,
  IN UINTN            TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend (
  IN OUT   VOID  *Context
  );

BOOLEAN
EFIAPI
MctpPhysicalHasMessage (
  IN OUT   VOID  *Context
  );

UINT8
EFIAPI
MctpPhysicalGetEndpointInformation (
  IN OUT   VOID  *Context
  );

UINT8
EFIAPI
MctpPhysicalGetTransportHeaderVersion (
  IN OUT   VOID  *Context
  );

EFI_STATUS
EFIAPI
MctpPhysicalConnect (
  OUT   VOID  *Context
  );

UINTN
EFIAPI
MctpPhysicalMTU (
  IN OUT   VOID  *Context
  );

#endif // __MCTP_PHYSICAL_TRANSPORT_LIB_H__
