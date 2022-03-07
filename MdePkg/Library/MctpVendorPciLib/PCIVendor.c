
#include <Base.h>
#include <Uefi.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpPCIVendorLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

/**  Recevied a vendor PCI packet target to this endpoint. Broadcast and Null EID
     packets are being ignored. The provided buffer must be at least the size of the
     data to be received.

  @param  [in]  This            The connection to operate on.
  @param  [in]  Buffer          The buffer where to place the data.
  @param  [in]  BufferLength    The length of the buffer. Reads in a single packet not bigger
                                than the specified size.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/

EFI_STATUS
MctpVendorPCIReceiveMsg(
  IN MCTP_PCI_VENDOR_PROTOCOL *This,
  OUT UINT8                  *Buffer,
  IN OUT UINTN               *BufferLength,
  IN UINTN                    TimeoutUsec
  )
{
  EFI_STATUS Status;
  MCTP_MSG     *Msg;
  UINTN        Length;
  UINTN        Received;

  if (This == NULL || BufferLength == NULL || Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Connected == FALSE) {
    return EFI_NOT_STARTED;
  }

  for (Received = 0, Length = 0; Received < *BufferLength; ) {
    Status = MctpCoreReceiveMessage(&Msg, &Length, TimeoutUsec);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Only accept direct messages
    if (Msg->Header.DestEID != MctpGetOwnEndpointID()) {
      continue;
    }
    if (Msg->Header.SourceEID != This->DestinationEID) {
      continue;
    }
    if (Received == 0) {
      if (Length < sizeof(MCTP_VENDOR_PCI_MSG_HEADER)) {
        continue;
      }   
      if (Msg->Body.PCIVendorMsg.Header.MsgType != MCTP_TYPE_PCI_VENDOR) {
        continue;
      }
      if (Msg->Body.PCIVendorMsg.Header.PCIVendorID != This->VendorID) {
        continue;
      }
    }
    if (Length + Received > *BufferLength) {
      break;
    }
    CopyMem(&Buffer[Received], Msg->Body.Raw, Length);
    Received += Length;

    if (Msg->Header.EOM == 1) {
      *BufferLength = Received;
      return EFI_SUCCESS;
    }
  };
 return EFI_BAD_BUFFER_SIZE;
}


/**  Send a vendor PCI paket using the specified connection.

  @param  [in]  This            The connection to operate on.
  @param  [in]  Buffer          The buffer where to read the data.
  @param  [in]  BufferLength    The length of the buffer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/

EFI_STATUS
MctpVendorPCISend(
  IN MCTP_PCI_VENDOR_PROTOCOL *This,
  IN UINT8                    *Buffer,
  IN UINTN                    BufferLength,
  IN UINTN                    TimeoutUsec
  )
{
  MCTP_BODY   Body;

  if (This == NULL || Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Connected == FALSE) {
    return EFI_NOT_STARTED;
  }

  Body.PCIVendorMsg.Header.MsgType = MCTP_TYPE_PCI_VENDOR;
  Body.PCIVendorMsg.Header.PCIVendorID = This->VendorID;

  return MctpCoreSendMessage(This->DestinationEID,
      Body.Raw,
      sizeof(MCTP_VENDOR_PCI_MSG_HEADER),
      Buffer,
      BufferLength,
      TimeoutUsec);
}

/**  Establish a connection to the remote machine.

  @param  [in]  DestinationEID  The Destination EID.
  @param  [in]  VendorID        The PCI VendorID to use for custom messages.
  @param  [in]  BitField        The vendor defined bitfield as returned by MCTP control messages.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval other                 The message type is not supported or another error happened.
*/
EFI_STATUS
MctpVendorPCIConnect(
  IN  UINT8  DestinationEID,
  IN  UINT16 VendorID,
  IN  UINT16 BitField,
  IN  UINTN  TimeoutUsec,
  OUT MCTP_PCI_VENDOR_PROTOCOL *Protocol
  )
{
  EFI_STATUS             Status;
  MCTP_CONTROL_VENDOR_ID RemoteVendorID;
  UINTN                  Index;
  DEBUG ((DEBUG_ERROR, "MctpVendorPCIConnect\n"));

  if (Protocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  ZeroMem(Protocol, sizeof(MCTP_PCI_VENDOR_PROTOCOL));
  DEBUG ((DEBUG_ERROR, "zerod protocol\n"));

  Status = MctpCoreRegisterPCIVendor(VendorID, BitField);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  DEBUG ((DEBUG_ERROR, "MctpCoreRegisterPCIVendor done\n"));

  Status = MctpCoreRemoteSupportMessageType(
      DestinationEID,
      MCTP_TYPE_PCI_VENDOR,
      TimeoutUsec);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  DEBUG ((DEBUG_ERROR, "MctpCoreRemoteSupportMessageType done\n"));

  Index = 0;
  do {
    Status = MctpControlGetMCTPVendorMessageSupport(DestinationEID,
        Index,
        &RemoteVendorID,
        TimeoutUsec);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    if ((RemoteVendorID.VendorIDFormat == MCTP_VENDOR_ID_FORMAT_PCI) && 
        (RemoteVendorID.Pci.PCIVendorID == VendorID)) {
      break;
    }
    Index++;
    DEBUG ((DEBUG_ERROR, "MctpControlGetMCTPVendorMessageSupport done\n"));

  } while(1);

  Protocol->Connected = TRUE;
  Protocol->DestinationEID = DestinationEID;
  Protocol->VendorID = VendorID;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpVendorPCILibConstructor (
  VOID
  )
{
  DEBUG ((DEBUG_ERROR, "MctpVendorPCILibConstructor\n"));

  return MctpCoreRegisterMessageClass(MCTP_TYPE_PCI_VENDOR);
}
