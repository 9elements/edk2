
#include <Base.h>
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpTransportLib.h>

#include "Requests.h"
#include "Core.h"

UINT8 SupportedMessageTypes[MCTP_MAX_DEFINED_MESSAGE_TYPES];
UINT8 NumSupportedMessageTypes;

MCTP_CONTROL_VENDOR_ID SupportedVendorMessage[MCTP_MAX_SUPPORTED_VENDOR_MSG];
UINT8 NumSupportedVendorDefinedMessages;

STATIC
EFI_STATUS
MctpCoreValidateMessageType(
  IN UINT8 Type
  )
{
  switch (Type) {
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
  IN UINT8 MessageType
  )
{
  UINTN Index;

  if (EFI_ERROR(MctpCoreValidateMessageType(MessageType))) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < NumSupportedMessageTypes; Index++) {
    if (SupportedMessageTypes[Index] == MessageType) {
      return EFI_SUCCESS;
    }
  }
  if (NumSupportedMessageTypes == MCTP_MAX_DEFINED_MESSAGE_TYPES) {
    return EFI_BAD_BUFFER_SIZE;
  }

  SupportedMessageTypes[NumSupportedMessageTypes++] = MessageType;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreRegisterPCIVendor (
  IN UINT16 VendorId,
  IN UINT16 BitField
  )
{
  UINTN                  Index;
  MCTP_CONTROL_VENDOR_ID Vendor;
  BOOLEAN                Supported;

  Supported = FALSE;
  for (Index = 0; Index < NumSupportedMessageTypes; Index++) {
    if (SupportedMessageTypes[Index] == MCTP_TYPE_PCI_VENDOR) {
      Supported = TRUE;
      break;
    }
  }
  if (!Supported) {
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < NumSupportedVendorDefinedMessages; Index++) {
    if (SupportedVendorMessage[Index].VendorIDFormat != MCTP_VENDOR_ID_FORMAT_PCI) {
      continue;
    }
    if (SupportedVendorMessage[Index].Pci.PCIVendorID == VendorId) {
      return EFI_SUCCESS;
    }
  }
  if (NumSupportedVendorDefinedMessages == MCTP_MAX_SUPPORTED_VENDOR_MSG) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Vendor.VendorIDFormat = MCTP_VENDOR_ID_FORMAT_PCI;
  Vendor.Pci.PCIVendorID = VendorId;
  Vendor.Pci.VendorBitField = BitField;

  SupportedVendorMessage[NumSupportedVendorDefinedMessages++] = Vendor;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreRegisterIANAVendor (
  IN UINT32 IANAEnterpriseID,
  IN UINT16 BitField
  )
{
  UINTN                  Index;
  MCTP_CONTROL_VENDOR_ID Vendor;
  BOOLEAN                Supported;

  Supported = FALSE;
  for (Index = 0; Index < NumSupportedMessageTypes; Index++) {
    if (SupportedMessageTypes[Index] == MCTP_TYPE_IANA_VENDOR) {
      Supported = TRUE;
      break;
    }
  }
  if (!Supported) {
    return EFI_UNSUPPORTED;
  }

  for (Index = 0; Index < NumSupportedVendorDefinedMessages; Index++) {
    if (SupportedVendorMessage[Index].VendorIDFormat != MCTP_VENDOR_ID_FORMAT_IANA) {
      continue;
    }
    if (SupportedVendorMessage[Index].Iana.IANAEnterpriseID == IANAEnterpriseID) {
      return EFI_SUCCESS;
    }
  }
  if (NumSupportedVendorDefinedMessages == MCTP_MAX_SUPPORTED_VENDOR_MSG) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Vendor.VendorIDFormat = MCTP_VENDOR_ID_FORMAT_IANA;
  Vendor.Iana.IANAEnterpriseID = IANAEnterpriseID;
  Vendor.Iana.VendorBitField = BitField;

  SupportedVendorMessage[NumSupportedVendorDefinedMessages++] = Vendor;
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
MctpCoreRemoteSupportMessageType(
  IN  UINT8  DestinationEID,
  IN  UINT8  RequestedMessageType,
  IN  UINTN  TimeoutUsec
)
{
  EFI_STATUS Status;
  UINT8      MessageType;
  UINTN      Count;
  UINTN      Index;

  Index = 0;
  do {
    Count = 1;
    Status = MctpControlGetMCTPMessageTypeSupport(DestinationEID,
        Index,
        &MessageType,
        &Count,
        TimeoutUsec);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    if (Count == 1 && MessageType == RequestedMessageType) {
      break;
    }
    Index++;
  } while(1);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreReceiveMessage (
  OUT MCTP_MSG     **ExternalMessage,
  OUT UINTN        *ExternalMessageLength,
  IN  UINTN        TimeoutUsec
  )
{
  EFI_STATUS Status;
  MCTP_MSG   *Msg;
  UINTN      Length;

  if (ExternalMessage == NULL || ExternalMessageLength == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Length = 0;
  Status = MctpTransportReceiveMessage(&Msg,
      &Length,
      TimeoutUsec);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  
  if (!MctpIsTargetEndpointID(Msg->Header.DestEID)) {
    return EFI_NO_MAPPING;
  }

  if (Length >= sizeof(MCTP_CONTROL_MSG)) {
    if (Msg->Body.ControlMsg.Header.MsgType == MCTP_TYPE_CONTROL_MSG &&
        Msg->Body.ControlMsg.Header.Rq == 1) {
      return MctpHandleControlMsg(Msg, Length, TimeoutUsec);
    }
  }
  *ExternalMessage = Msg;
  *ExternalMessageLength = Length;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpCoreSendMessage (
  IN  UINT8  DestinationEID,
  IN  UINT8  *MessageBodyHeader,
  IN  UINTN  MessageBodyHeaderLength,
  IN  UINT8  *MessageBody,
  IN  UINTN  MessageBodyLength,
  IN  UINTN  TimeoutUsec
  )
{
  MCTP_TRANSPORT_HEADER   Header;

  if (MessageBody == NULL || MessageBodyLength == 0) {
    return EFI_INVALID_PARAMETER;
  }
  ZeroMem(&Header, sizeof(Header));

  Header.DestEID = DestinationEID;
  Header.SourceEID = MctpGetOwnEndpointID();

  return MctpTransportSendMessage(&Header,
      MessageBodyHeader,
      MessageBodyHeaderLength,
      MessageBody,
      MessageBodyLength,
      TimeoutUsec);
}

EFI_STATUS
EFIAPI
MctpCoreLibConstructor (
  VOID
  )
{
  SupportedMessageTypes[0] = MCTP_TYPE_CONTROL_MSG;
  NumSupportedMessageTypes = 1;

  return EFI_SUCCESS;
}