
/** @file
  Provide services to MCTP transport mechanism.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_TRANSPORT_LIB_H__
#define __MCTP_TRANSPORT_LIB_H__

#include <IndustryStandard/Mctp.h>

EFI_STATUS
EFIAPI
MctpSetNewMTU(
  IN UINTN Size,
  IN UINT8 *Buffer
  );

UINTN
EFIAPI
MctpGetMTU(
  VOID
  );

EFI_STATUS
MctpTransportSendMessage(
  IN MCTP_TRANSPORT_HEADER *Hdr,
  IN UINT8                 *MessageBodyHeader,
  IN UINTN                 MessageBodyHeaderLength,
  IN UINT8                 *MessageBody,
  IN UINTN                  MessageBodyLength,
  IN UINTN                 TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpTransportReceiveMessage(
  OUT  MCTP_MSG              **Msg,
  OUT  UINTN                 *MsgLength,
  IN   UINTN                 TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpTransportWaitForReadyToSend(
  IN     UINTN                 TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpTransportReadyToSend(
  VOID
  );

EFI_STATUS
EFIAPI
MctpTransportWaitForMessage(
  IN     UINTN                 TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpTransportHasMessage(
  VOID
  );
#endif // __MCTP_TRANSPORT_LIB_H__