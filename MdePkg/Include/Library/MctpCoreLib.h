/** @file
  Provide services to MCTP core services.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_CORE_LIB_H__
#define __MCTP_CORE_LIB_H__

#include <IndustryStandard/Mctp.h>
#include <Library/MctpTransportLib.h>

#define MCTP_MAX_SUPPORTED_VENDOR_MSG  8

typedef struct {
  MCTP_TRANSPORT_BINDING    TransportBinding;
  UINT8                     SupportedMessageTypes[MCTP_MAX_DEFINED_MESSAGE_TYPES];
  UINT8                     NumSupportedMessageTypes;

  MCTP_CONTROL_VENDOR_ID    SupportedVendorMessage[MCTP_MAX_SUPPORTED_VENDOR_MSG];
  UINT8                     NumSupportedVendorDefinedMessages;
  UINT8                     OwnEID;
  UINT8                     BusOwnerEID;
  BOOLEAN                   IsBusOwner;
  BOOLEAN                   SupportsStaticEID;
  UINT8                     StaticEID;
} MCTP_BINDING;

EFI_STATUS
MctpCoreInit (
  OUT MCTP_BINDING  *Binding
  );

EFIAPI
BOOLEAN
MctpIsAssignableEndpointID (
  IN UINT8  ID
  );

EFIAPI
BOOLEAN
MctpIsValidEndpointID (
  IN UINT8  ID
  );

EFIAPI
BOOLEAN
MctpIsTargetEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             ID
  );

EFIAPI
UINT8
MctpGetOwnEndpointID (
  IN OUT MCTP_BINDING  *Binding
  );

EFIAPI
VOID
MctpSetOwnEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             ID
  );

EFIAPI
VOID
MctpSetBusOwnerEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             ID
  );

EFIAPI
UINT8
MctpGetBusOwnerEndpointID (
  IN OUT MCTP_BINDING  *Binding
  );

EFIAPI
BOOLEAN
MctpIsBusOwner (
  IN OUT MCTP_BINDING  *Binding
  );

EFIAPI
VOID
MctpSetBusOwner (
  IN OUT MCTP_BINDING  *Binding,
  BOOLEAN              Value
  );

EFIAPI
BOOLEAN
MctpSupportsStaticEID (
  IN OUT MCTP_BINDING  *Binding
  );

EFIAPI
EFI_STATUS
MctpResetEID (
  IN OUT MCTP_BINDING  *Binding
  );

EFIAPI
EFI_STATUS
MctpSetStaticEID (
  IN OUT MCTP_BINDING  *Binding,
  UINT8                EndpointID
  );

EFIAPI
UINT8
MctpGetStaticEID (
  IN OUT MCTP_BINDING  *Binding
  );

EFI_STATUS
EFIAPI
MctpControlSetEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             EndpointID,
  IN UINT8             DestinationEID,
  IN UINTN             TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN  UINT8            DestinationEID,
  OUT UINT8            *EndpointID,
  OUT UINT8            *EndpointType,
  IN  UINTN            TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPBaseVersionSupport (
  IN OUT MCTP_BINDING                *Binding,
  IN     UINT8                       DestinationEID,
  OUT    MCTP_CONTROL_VERSION_ENTRY  *Entry,
  IN OUT UINTN                       *EntryCount,
  IN     UINTN                       TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPMessageTypeSupport (
  IN OUT MCTP_BINDING  *Binding,
  IN     UINT8         DestinationEID,
  IN     UINTN         FirstEntry,
  OUT    UINT8         *Entry,
  IN OUT UINTN         *EntryCount,
  IN     UINTN         TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPVendorMessageSupport (
  IN OUT MCTP_BINDING         *Binding,
  IN  UINT8                   DestinationEID,
  IN  UINT8                   IDSelector,
  OUT MCTP_CONTROL_VENDOR_ID  *Entry,
  IN  UINTN                   TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpControlGetMCTPVendorMessageSupport (
  IN OUT MCTP_BINDING         *Binding,
  IN  UINT8                   DestinationEID,
  IN  UINT8                   IDSelector,
  OUT MCTP_CONTROL_VENDOR_ID  *Entry,
  IN  UINTN                   TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpCoreRegisterMessageClass (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             MessageType
  );

EFI_STATUS
EFIAPI
MctpCoreRegisterPCIVendor (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT16            VendorId,
  IN UINT16            BitField
  );

EFI_STATUS
EFIAPI
MctpCoreRegisterIANAVendor (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT32            IANAEnterpriseID,
  IN UINT16            BitField
  );

EFI_STATUS
EFIAPI
MctpCoreReceiveMessage (
  IN OUT MCTP_BINDING  *Binding,
  OUT MCTP_MSG         **ExternalMessage,
  OUT UINTN            *ExternalMessageLength,
  IN  UINTN            TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpCoreReceiveMessageChunks (
  IN OUT MCTP_BINDING           *Binding,
  IN OUT LIST_ENTRY             *MessageBodyChunk,
  OUT    MCTP_TRANSPORT_HEADER  *Hdr,
  IN OUT UINTN                  *Remaining,
  IN     UINTN                  TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpCoreSendMessage (
  IN OUT MCTP_BINDING  *Binding,
  IN  UINT8            DestinationEID,
  IN  BOOLEAN          TagOwner,
  IN  UINT8            Tag,
  IN  LIST_ENTRY       *MessageBodyChunk,
  IN  UINTN            TimeoutUsec
  );

EFI_STATUS
EFIAPI
MctpCoreSendMessageSimple (
  IN OUT MCTP_BINDING  *Binding,
  IN  UINT8            DestinationEID,
  IN  BOOLEAN          TagOwner,
  IN  UINT8            Tag,
  IN  VOID             *Data,
  IN  UINTN            DataLength,
  IN  UINTN            TimeoutUsec
  );

EFIAPI
EFI_STATUS
MctpCoreRemoteSupportMessageType (
  IN OUT MCTP_BINDING  *Binding,
  IN  UINT8            DestinationEID,
  IN  UINT8            RequestedMessageType,
  IN  UINTN            TimeoutUsec
  );

#endif // __MCTP_CORE_LIB_H__
