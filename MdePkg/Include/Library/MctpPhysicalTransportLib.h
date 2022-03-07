
/** @file
  Provide services to MCTP physical message transceiving.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_PHYSICAL_TRANSPORT_LIB_H__
#define __MCTP_PHYSICAL_TRANSPORT_LIB_H__

#include <IndustryStandard/Mctp.h>

EFI_STATUS
EFIAPI
MctpPhysicalSend(
  IN MCTP_MSG *Msg,
  IN UINTN Length,
  IN UINTN TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpPhysicalReceive(
  IN MCTP_MSG *Msg,
  IN UINTN *Length,
  IN UINTN TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend(
  VOID
  );

BOOLEAN
EFIAPI
MctpPhysicalHasMessage(
  VOID
  );

UINT8
EFIAPI
MctpPhysicalGetEndpointInformation(
  VOID
  );

#endif // __MCTP_PHYSICAL_TRANSPORT_LIB_H__