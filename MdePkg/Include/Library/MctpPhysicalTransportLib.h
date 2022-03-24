/** @file
  Provide services to MCTP physical message transceiving.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_PHYSICAL_TRANSPORT_LIB_H__
#define __MCTP_PHYSICAL_TRANSPORT_LIB_H__

#include <IndustryStandard/Mctp.h>

typedef struct {
  // Msg points to a buffer where the message is (to be) placed
  MCTP_MSG    *Msg;
  // Length is the actual message length
  UINTN       Length;
  // Mtu holds the maximum available message size. This is the
  // buffer size.
  UINTN       Mtu;
} MCTP_PHYSICAL_MSG;

EFI_STATUS
EFIAPI
MctpPhysicalSend (
  IN MCTP_PHYSICAL_MSG  *Msg,
  IN UINTN              TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpPhysicalReceive (
  IN MCTP_PHYSICAL_MSG  *Msg,
  IN UINTN              TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpPhysicalReadyToSend (
  VOID
  );

BOOLEAN
EFIAPI
MctpPhysicalHasMessage (
  VOID
  );

UINT8
EFIAPI
MctpPhysicalGetEndpointInformation (
  VOID
  );

EFI_STATUS
EFIAPI
MctpPhysicalConnect (
  VOID
  );

#endif // __MCTP_PHYSICAL_TRANSPORT_LIB_H__
