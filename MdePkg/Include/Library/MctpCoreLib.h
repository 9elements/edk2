/** @file
  Provide services to MCTP core services.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_CORE_LIB_H__
#define __MCTP_CORE_LIB_H__

#include <IndustryStandard/Mctp.h>

EFIAPI
BOOLEAN
MctpIsAssignableEndpointID(
  IN UINT8 ID
  );

EFIAPI
BOOLEAN
MctpIsValidEndpointID(
  IN UINT8 ID
  );

EFIAPI
BOOLEAN
MctpIsTargetEndpointID(
  IN UINT8 ID
  );

EFIAPI
UINT8
MctpGetOwnEndpointID(
  VOID
  );

EFIAPI
VOID
MctpSetOwnEndpointID(
  IN UINT8 ID
  );

EFIAPI
VOID
MctpSetBusOwnerEndpointID(
  IN UINT8 ID
  );

EFIAPI
UINT8
MctpGetBusOwnerEndpointID(
  VOID
  );

EFIAPI
BOOLEAN
MctpIsBusOwner(
  VOID
  );

EFIAPI
VOID
MctpSetBusOwner(
  BOOLEAN Value
  );

EFIAPI
BOOLEAN
MctpSupportsStaticEID(
  VOID
  );

EFIAPI
EFI_STATUS
MctpResetEID(
  VOID
  );

EFIAPI
EFI_STATUS
MctpSetStaticEID(
  UINT8 EndpointID
  );

EFIAPI
UINT8
MctpGetStaticEID(
  VOID
  );

EFI_STATUS
EFIAPI
MctpControlSetEndpointID(
  IN UINT8 EndpointID,
  IN UINT8 DestinationEID,
  IN UINTN TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetEndpointID(
  IN  UINT8  DestinationEID,
  OUT UINT8  *EndpointID,
  OUT UINT8  *EndpointType,
  IN  UINTN  TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPBaseVersionSupport(
  IN     UINT8                      DestinationEID,
  OUT    MCTP_CONTROL_VERSION_ENTRY *Entry,
  IN OUT UINTN                      *EntryCount,
  IN     UINTN                      TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPMessageTypeSupport(
  IN     UINT8  DestinationEID,
  IN     UINTN  FirstEntry,
  OUT    UINT8  *Entry,
  IN OUT UINTN  *EntryCount,
  IN     UINTN  TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPVendorMessageSupport(
  IN  UINT8                   DestinationEID,
  IN  UINT8                   IDSelector,
  OUT MCTP_CONTROL_VENDOR_ID  *Entry,
  IN  UINTN                   TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPVendorMessageSupport(
  IN  UINT8                   DestinationEID,
  IN  UINT8                   IDSelector,
  OUT MCTP_CONTROL_VENDOR_ID  *Entry,
  IN  UINTN                   TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpCoreRegisterMessageClass (
  IN UINT8 MessageType
  );

EFI_STATUS
EFIAPI
MctpCoreRegisterPCIVendor (
  IN UINT16 VendorId,
  IN UINT16 BitField
  );

EFI_STATUS
EFIAPI
MctpCoreReceiveMessage (
  OUT MCTP_MSG     **ExternalMessage,
  OUT UINTN        *ExternalMessageLength,
  IN  UINTN        TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpCoreSendMessage (
  IN  UINT8  DestinationEID,
  IN  UINT8  *MessageBodyHeader,
  IN  UINTN  MessageBodyHeaderLength,
  IN  UINT8  *MessageBody,
  IN  UINTN  MessageBodyLength,
  IN  UINTN  TimeoutUsec
  );

EFIAPI
EFI_STATUS
MctpCoreRemoteSupportMessageType(
  IN  UINT8  DestinationEID,
  IN  UINT8  RequestedMessageType,
  IN  UINTN  TimeoutUsec
);

#endif // __MCTP_CORE_LIB_H__