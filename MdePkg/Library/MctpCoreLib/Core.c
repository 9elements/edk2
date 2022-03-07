#include <Base.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BasePushPopBufferLib.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpTransportLib.h>
#include <Library/DebugLib.h>

#include "Requests.h"

EFI_STATUS
MctpCoreInit (
  OUT MCTP_BINDING  *Binding
  )
{
  if (Binding == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: Called...\n", __FUNCTION__));

  ZeroMem (Binding, sizeof (*Binding));
  Binding->SupportedMessageTypes[0] = MCTP_TYPE_CONTROL_MSG;
  Binding->NumSupportedMessageTypes = 1;

  return MctpTransportConnect (&Binding->TransportBinding);
}

STATIC
EFI_STATUS
MctpCoreValidateMessageType (
  IN UINT8  Type
  )
{
  switch (Type) {
    case MCTP_TYPE_CONTROL_MSG:
    case MCTP_TYPE_PLDM_MSG:
    case MCTP_TYPE_NCSI_MSG:
    case MCTP_TYPE_ETHERNET_MSG:
    case MCTP_TYPE_NVME_MSG:
    case MCTP_TYPE_SPDM_MSG:
    case MCTP_TYPE_PCI_VENDOR:
    case MCTP_TYPE_IANA_VENDOR:
      return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
MctpCoreRegisterMessageClass (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             MessageType
  )
{
  UINTN  Index;

  if (Binding == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (EFI_ERROR (MctpCoreValidateMessageType (MessageType))) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < Binding->NumSupportedMessageTypes; Index++) {
    if (Binding->SupportedMessageTypes[Index] == MessageType) {
      return EFI_SUCCESS;
    }
  }

  if (Binding->NumSupportedMessageTypes == MCTP_MAX_DEFINED_MESSAGE_TYPES) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Binding->SupportedMessageTypes[Binding->NumSupportedMessageTypes++] = MessageType;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreRegisterPCIVendor (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT16            VendorId,
  IN UINT16            BitField
  )
{
  UINTN                   Index;
  MCTP_CONTROL_VENDOR_ID  Vendor;
  BOOLEAN                 Supported;

  if (Binding == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Supported = FALSE;
  for (Index = 0; Index < Binding->NumSupportedMessageTypes; Index++) {
    if (Binding->SupportedMessageTypes[Index] == MCTP_TYPE_PCI_VENDOR) {
      Supported = TRUE;
      break;
    }
  }

  if (!Supported) {
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < Binding->NumSupportedVendorDefinedMessages; Index++) {
    if (Binding->SupportedVendorMessage[Index].VendorIDFormat != MCTP_VENDOR_ID_FORMAT_PCI) {
      continue;
    }

    if (Binding->SupportedVendorMessage[Index].Pci.PCIVendorID == VendorId) {
      return EFI_SUCCESS;
    }
  }

  if (Binding->NumSupportedVendorDefinedMessages == MCTP_MAX_SUPPORTED_VENDOR_MSG) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Vendor.VendorIDFormat     = MCTP_VENDOR_ID_FORMAT_PCI;
  Vendor.Pci.PCIVendorID    = VendorId;
  Vendor.Pci.VendorBitField = BitField;

  Binding->SupportedVendorMessage[Binding->NumSupportedVendorDefinedMessages++] = Vendor;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreRegisterIANAVendor (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT32            IANAEnterpriseID,
  IN UINT16            BitField
  )
{
  UINTN                   Index;
  MCTP_CONTROL_VENDOR_ID  Vendor;
  BOOLEAN                 Supported;

  if (Binding == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Supported = FALSE;
  for (Index = 0; Index < Binding->NumSupportedMessageTypes; Index++) {
    if (Binding->SupportedMessageTypes[Index] == MCTP_TYPE_IANA_VENDOR) {
      Supported = TRUE;
      break;
    }
  }

  if (!Supported) {
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < Binding->NumSupportedVendorDefinedMessages; Index++) {
    if (Binding->SupportedVendorMessage[Index].VendorIDFormat != MCTP_VENDOR_ID_FORMAT_IANA) {
      continue;
    }

    if (Binding->SupportedVendorMessage[Index].Iana.IANAEnterpriseID == IANAEnterpriseID) {
      return EFI_SUCCESS;
    }
  }

  if (Binding->NumSupportedVendorDefinedMessages == MCTP_MAX_SUPPORTED_VENDOR_MSG) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Vendor.VendorIDFormat        = MCTP_VENDOR_ID_FORMAT_IANA;
  Vendor.Iana.IANAEnterpriseID = IANAEnterpriseID;
  Vendor.Iana.VendorBitField   = BitField;

  Binding->SupportedVendorMessage[Binding->NumSupportedVendorDefinedMessages++] = Vendor;
  return EFI_SUCCESS;
}

/**  Checks if the remote endpoint supports the specified message type.

  @param  [in]  DestinationEID  The Destination EID.
  @param  [in]  MessageType     The message type to look for.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval other                 The message type is not supported or another error happened.
**/
EFIAPI
EFI_STATUS
MctpCoreRemoteSupportMessageType (
  IN OUT MCTP_BINDING  *Binding,
  IN  UINT8            DestinationEID,
  IN  UINT8            RequestedMessageType,
  IN  UINTN            TimeoutUsec
  )
{
  EFI_STATUS  Status;
  UINT8       MessageType;
  UINTN       Count;
  UINTN       Index;

  if (Binding == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Index = 0;
  do {
    Count = 1;

    Status = MctpControlGetMCTPMessageTypeSupport (
               Binding,
               DestinationEID,
               Index,
               &MessageType,
               &Count,
               TimeoutUsec
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((Count == 1) && (MessageType == RequestedMessageType)) {
      break;
    }

    Index++;
  } while (1);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreReceiveMessage (
  IN OUT MCTP_BINDING  *Binding,
  OUT MCTP_MSG         **ExternalMessage,
  OUT UINTN            *ExternalMessageLength,
  IN  UINTN            TimeoutUsec
  )
{
  EFI_STATUS  Status;
  MCTP_MSG    *Msg;
  UINTN       Length;

  if ((Binding == NULL) || (ExternalMessage == NULL) || (ExternalMessageLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Length = 0;
  Status = MctpTransportReceiveMessage (
             &Binding->TransportBinding,
             &Msg,
             &Length,
             TimeoutUsec
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!MctpIsTargetEndpointID (Binding, Msg->Header.DestEID)) {
    DEBUG ((DEBUG_INFO, "%a: Received message for another EID\n", __FUNCTION__));
    return EFI_NO_MAPPING;
  }

  if (Length >= sizeof (MCTP_CONTROL_MSG)) {
    if (((Msg->Body.ControlMsg.Header.ICMsgType & MCTP_MSG_TYPE_MASK) == MCTP_TYPE_CONTROL_MSG) &&
        (Msg->Body.ControlMsg.Header.Rq == 1))
    {
      MctpHandleControlMsg (Binding, Msg, Length, TimeoutUsec);
      return EFI_NOT_READY;
    }
  }

  *ExternalMessage       = Msg;
  *ExternalMessageLength = Length;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreReceiveMessageChunks (
  IN OUT MCTP_BINDING        *Binding,
  IN OUT LIST_ENTRY          *MessageBodyChunk,
  OUT MCTP_TRANSPORT_HEADER  *Hdr,
  IN OUT UINTN               *Remaining,
  IN  UINTN                  TimeoutUsec
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  Tmp;
  UINTN       Retries = 3;

  if ((Binding == NULL) || (MessageBodyChunk == NULL) || (Hdr == NULL) || (Remaining == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  while (Retries > 0) {
    Tmp = MctpTransportReceiveMessageChunks (
            &Binding->TransportBinding,
            MessageBodyChunk,
            Hdr,
            Remaining,
            TimeoutUsec
            );
    if (EFI_ERROR (Tmp)) {
      Status = Tmp;
      Retries--;
    } else {
      if (!MctpIsTargetEndpointID (Binding, Hdr->DestEID)) {
        DEBUG ((DEBUG_INFO, "%a: Received message for another EID\n", __FUNCTION__));
      } else {
        if (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_EOM) {
          break;
        }
      }
    }

    // FIXME: Use timer to get real time
    if (TimeoutUsec > 100) {
      TimeoutUsec -= 100;
    } else {
      Retries = 0;
    }
  }

  if (Retries == 0) {
    Status = EFI_TIMEOUT;
  }

  return Status;
}

/**  Send a message using the Binding to the Destionation EID.

  @param  [in]  Binding         The binding to use, created with MctpCoreInit().
  @param  [in]  DestinationEID  The Destination EID.
  @param  [in]  TagOwner        Sets if the TAG is owned by self.
  @param  [in]  Tag             The message TAG to use.
  @param  [in]  Data            The data to transmit.
  @param  [in]  DataLength      The length of data to transmit.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval other                 The message type is not supported or another error happened.
**/
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
  )
{
  MCTP_TRANSPORT_HEADER     Header;
  EFI_MCTP_DATA_DESCRIPTOR  MctpDataDescriptor;
  LIST_ENTRY                MessageBodyChunks;

  if ((Binding == NULL) || (Data == NULL) || (DataLength == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Header, sizeof (Header));

  Header.DestEID   = DestinationEID;
  Header.SourceEID = MctpGetOwnEndpointID (Binding);
  if (TagOwner == TRUE) {
    Header.FlagsSeqTag |= MCTP_TRANSPORT_HDR_TO_MASK << MCTP_TRANSPORT_HDR_TO_SHIFT;
    Header.FlagsSeqTag |= (Tag & MCTP_TRANSPORT_HDR_TAG_MASK) << MCTP_TRANSPORT_HDR_TAG_SHIFT;
  }

  MctpDataDescriptor.Length    = DataLength;
  MctpDataDescriptor.Size      = DataLength;
  MctpDataDescriptor.DataBlock = Data;

  InitializeListHead (&MessageBodyChunks);
  InsertTailList (&MessageBodyChunks, &MctpDataDescriptor.Link);

  return MctpTransportSendMessage (
           &Binding->TransportBinding,
           Header,
           &MessageBodyChunks,
           TimeoutUsec
           );
}

/**  Send a multiple chunk message using the Binding to the Destionation EID.
  The chunks are assembled into one or multiple MCTP packets according to the MTU.

  @param  [in]  Binding          The binding to use, created with MctpCoreInit().
  @param  [in]  DestinationEID   The Destination EID.
  @param  [in]  TagOwner         Sets if the TAG is owned by self.
  @param  [in]  Tag              The message TAG to use.
  @param  [in]  MessageBodyChunk Linked list of EFI_MCTP_DATA_DESCRIPTOR to transmit.
  @param  [in]  TimeoutUsec      The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval other                 The message type is not supported or another error happened.
**/
EFI_STATUS
EFIAPI
MctpCoreSendMessage (
  IN OUT MCTP_BINDING  *Binding,
  IN     UINT8         DestinationEID,
  IN     BOOLEAN       TagOwner,
  IN     UINT8         Tag,
  IN     LIST_ENTRY    *MessageBodyChunk,
  IN     UINTN         TimeoutUsec
  )
{
  MCTP_TRANSPORT_HEADER  Header;

  if ((Binding == NULL) || (MessageBodyChunk == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Header, sizeof (Header));

  Header.DestEID   = DestinationEID;
  Header.SourceEID = MctpGetOwnEndpointID (Binding);
  if (TagOwner == TRUE) {
    Header.FlagsSeqTag |= MCTP_TRANSPORT_HDR_TO_MASK << MCTP_TRANSPORT_HDR_TO_SHIFT;
    Header.FlagsSeqTag |= (Tag & MCTP_TRANSPORT_HDR_TAG_MASK) << MCTP_TRANSPORT_HDR_TAG_SHIFT;
  }

  return MctpTransportSendMessage (
           &Binding->TransportBinding,
           Header,
           MessageBodyChunk,
           TimeoutUsec
           );
}

EFI_STATUS
EFIAPI
MctpCoreLibConstructor (
  VOID
  )
{
  return EFI_SUCCESS;
}
