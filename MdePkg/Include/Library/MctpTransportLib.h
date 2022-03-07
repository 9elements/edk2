/** @file
  Provide services to MCTP transport mechanism.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_TRANSPORT_LIB_H__
#define __MCTP_TRANSPORT_LIB_H__

#include <IndustryStandard/Mctp.h>
#include <Library/BasePushPopBufferLib.h>
#include <Library/PcdLib.h>

typedef struct {
  BOOLEAN    Connected;
  BOOLEAN    ExpectSOM;
  UINT8      ExpectSeq;
  UINT8      ExpectDestinationEID;
  UINT8      ExpectSourceEID;
  UINT8      RxBuffer[FixedPcdGet32 (PcdMctpPhysicalMtu)];
  UINT8      PhysicalContext[FixedPcdGet32 (PcdMctpPhysicalContextSize)];
} MCTP_TRANSPORT_BINDING;

EFI_STATUS
EFIAPI
MctpTransportConnect (
  OUT MCTP_TRANSPORT_BINDING  *Binding
  );

UINTN
EFIAPI
MctpGetMTU (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding
  );

EFI_STATUS
EFIAPI
MctpTransportSendMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN MCTP_TRANSPORT_HEADER       Hdr,
  IN LIST_ENTRY                  *MessageBodyChunk,
  IN UINTN                       TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpTransportReceiveMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  OUT  MCTP_MSG                  **Msg,
  OUT  UINTN                     *MsgLength,
  IN   UINTN                     TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpTransportReceiveMessageChunks (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN OUT LIST_ENTRY              *MessageBodyChunk,
  OUT    MCTP_TRANSPORT_HEADER   *Hdr,
  IN OUT UINTN                   *Remaining,
  IN     UINTN                   TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpTransportWaitForReadyToSend (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN     UINTN                   TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpTransportReadyToSend (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding
  );

EFI_STATUS
EFIAPI
MctpTransportWaitForMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN     UINTN                   TimeoutUsec
  );

BOOLEAN
EFIAPI
MctpTransportHasMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding
  );

#endif // __MCTP_TRANSPORT_LIB_H__
