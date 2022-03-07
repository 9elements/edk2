#include <Base.h>
#include <Uefi.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpPCIVendorLib.h>
#include <Library/BasePushPopBufferLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

#ifndef NDEBUG
#define DEBUG_MCTP(x)  DEBUG(x)
#else
#define DEBUG_MCTP(x)
#endif

#if defined (__LITTLE_ENDIAN__) \
  || defined (__ARMEL__) \
  || defined (__THUMBEL__) \
  || defined (__AARCH64EL__) \
  || defined (__MIPSEL__) \
  || defined (__MIPSEL) \
  || defined (_MIPSEL) \
  || defined (__BFIN__) \
  || (defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#define HostToBigEndian16  SwapBytes16
#define BigEndian16ToHost  SwapBytes16
#else
#define HostToBigEndian16(x)  x
#define BigEndian16ToHost(x)  x
#endif

/**  Receive a vendor PCI packet targeted to this endpoint. Broadcast and Null EID
     packets are being ignored. The provided buffer must be at least the size of the
     data to be received.

  @param  [in]     This               The connection to operate on.
  @param  [in]     MessageBodyChunks  A scatter gather list of buffers to place the result into.
                                      Length and Data field must be sane.
  @param  [in out] Remaining          The number of bytes received but not added to MessageBodyChunks.
                                      Must be initialized by the caller to 0.
  @param  [in]     TimeoutUsec        The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/
EFI_STATUS
MctpVendorPCIReceiveChunks (
  IN     MCTP_PCI_VENDOR_PROTOCOL  *This,
  IN OUT LIST_ENTRY                *MessageBodyChunks,
  IN OUT UINTN                     *Remaining,
  IN     UINTN                     TimeoutUsec
  )
{
  EFI_STATUS                  Status;
  MCTP_VENDOR_PCI_MSG_HEADER  VendorHeader;
  EFI_MCTP_DATA_DESCRIPTOR    MctpDataDescriptor;
  MCTP_TRANSPORT_HEADER       Hdr;
  UINTN                       Index;

  if ((This == NULL) || (MessageBodyChunks == NULL) || (Remaining == NULL) ||
      IsNull (MessageBodyChunks, GetFirstNode (MessageBodyChunks)))
  {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Append the PCI header
  //
  MctpDataDescriptor.Signature = MCTP_INFO_SIGNATURE;
  MctpDataDescriptor.Length    = 0;
  MctpDataDescriptor.Size      = sizeof (VendorHeader);
  MctpDataDescriptor.DataBlock = (VOID *)&VendorHeader;
  InsertHeadList (MessageBodyChunks, &MctpDataDescriptor.Link);

  Status = MctpCoreReceiveMessageChunks (This->Binding, MessageBodyChunks, &Hdr, Remaining, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MctpCoreReceiveMessageChunks failed with %r\n", __FUNCTION__));
    return Status;
  }

  // FIXME: Transport layer should drop packets based on Source EID and Dest EID
  if (Hdr.SourceEID != This->DestinationEID) {
    DEBUG ((DEBUG_INFO, "%a: Received message from unknown EID: %x\n", __FUNCTION__, Hdr.SourceEID));
    return EFI_PROTOCOL_ERROR;
  }

  //
  // Check if the PCI header is correct
  //

  if (MctpDataDescriptor.Length != sizeof (VendorHeader)) {
    DEBUG ((DEBUG_ERROR, "%a: VendorHeader size missmatch: %d\n", __FUNCTION__, MctpDataDescriptor.Length));
    return EFI_PROTOCOL_ERROR;
  }

  if ((VendorHeader.ICMsgType & MCTP_MSG_TYPE_MASK) != MCTP_TYPE_PCI_VENDOR) {
    DEBUG ((DEBUG_ERROR, "%a: VendorHeader message type not PCI: 0x%x\n", __FUNCTION__, VendorHeader.ICMsgType & MCTP_MSG_TYPE_MASK));
    return EFI_PROTOCOL_ERROR;
  }

  if (BigEndian16ToHost (VendorHeader.PCIVendorID) != This->VendorID) {
    for (Index = 0; Index < sizeof (VendorHeader); Index++) {
      DEBUG ((DEBUG_ERROR, "0x%02x ", ((UINT8 *)&VendorHeader)[Index]));
    }

    DEBUG ((DEBUG_ERROR, "%a: VendorHeader VendorID missmatch: 0x%x\n", __FUNCTION__, BigEndian16ToHost (VendorHeader.PCIVendorID)));
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}

/**  Receive a vendor PCI packet targeted to this endpoint. Broadcast and Null EID
     packets are being ignored. The provided buffer must be at least the size of the
     data to be received.
     Discard data not fitting into the destination buffer and doesn't return the amount of
     data that has been discarded.

  @param  [in]  This             The connection to operate on.
  @param  [in]  DestBuffer       The buffer where to place the data.
  @param  [in]  DestBufferLength The length of the buffer. Reads in a single packet not bigger
                                 than the specified size.
  @param  [in]  TimeoutUsec      The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval other                 The message type is not supported or another error happened.
**/
EFI_STATUS
MctpVendorPCIReceiveMsg (
  IN MCTP_PCI_VENDOR_PROTOCOL  *This,
  OUT UINT8                    *DestBuffer,
  IN OUT UINTN                 *DestBufferLength,
  IN UINTN                     TimeoutUsec
  )
{
  EFI_STATUS                Status;
  UINTN                     Remaining = 0;
  EFI_MCTP_DATA_DESCRIPTOR  MctpDataDescriptor;
  LIST_ENTRY                MessageBodyChunks;

  if ((This == NULL) || (DestBufferLength == NULL) || (DestBuffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  InitializeListHead (&MessageBodyChunks);

  MctpDataDescriptor.Signature = MCTP_INFO_SIGNATURE;
  MctpDataDescriptor.Length    = 0;
  MctpDataDescriptor.Size      = *DestBufferLength;
  MctpDataDescriptor.DataBlock = (VOID *)DestBuffer;
  InsertTailList (&MessageBodyChunks, &MctpDataDescriptor.Link);

  Status = MctpVendorPCIReceiveChunks (This, &MessageBodyChunks, &Remaining, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *DestBufferLength = MctpDataDescriptor.Length;

  return Status;
}

/**  Send a vendor PCI packet using the specified connection.

  @param  [in]  This              The connection to operate on.
  @param  [in]  Data              Pointer to data to transmit.
  @param  [in]  DataLength        Length of data to transmit.
  @param  [in]  TimeoutUsec       The timeout in microseconds for this command to take.
  @param  [in]  Tag               MCTP transport header TAG

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/
EFI_STATUS
MctpVendorPCISend (
  IN MCTP_PCI_VENDOR_PROTOCOL  *This,
  IN VOID                      *Data,
  IN UINTN                     DataLength,
  IN UINTN                     TimeoutUsec,
  IN UINTN                     Tag
  )
{
  EFI_MCTP_DATA_DESCRIPTOR  MctpDataDescriptor;
  LIST_ENTRY                MessageBodyChunks;

  if ((This == NULL) || (Data == NULL) || (DataLength == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  InitializeListHead (&MessageBodyChunks);

  MctpDataDescriptor.Signature = MCTP_INFO_SIGNATURE;
  MctpDataDescriptor.Length    = DataLength;
  MctpDataDescriptor.Size      = DataLength;
  MctpDataDescriptor.DataBlock = Data;
  InsertTailList (&MessageBodyChunks, &MctpDataDescriptor.Link);

  return MctpVendorPCISendChunks (
           This,
           &MessageBodyChunks,
           TimeoutUsec,
           Tag
           );
}

/**  Send a vendor PCI packet consisting out of a chunk list using the specified connection.

  @param  [in]  This              The connection to operate on.
  @param  [in]  MessageBodyChunks Linked list of EFI_MCTP_DATA_DESCRIPTOR pointing to data to transmit.
  @param  [in]  TimeoutUsec       The timeout in microseconds for this command to take.
  @param  [in]  Tag               MCTP transport header TAG

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/
EFI_STATUS
MctpVendorPCISendChunks (
  IN MCTP_PCI_VENDOR_PROTOCOL  *This,
  IN LIST_ENTRY                *MessageBodyChunks,
  IN UINTN                     TimeoutUsec,
  IN UINTN                     Tag
  )
{
  MCTP_VENDOR_PCI_MSG_HEADER  Header;
  EFI_MCTP_DATA_DESCRIPTOR    MctpDataDescriptor;

  if ((This == NULL) || (MessageBodyChunks == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Connected == FALSE) {
    return EFI_NOT_STARTED;
  }

  Header.ICMsgType   = MCTP_TYPE_PCI_VENDOR;
  Header.PCIVendorID = HostToBigEndian16 (This->VendorID);

  //
  // Prepend the PCI header
  //
  MctpDataDescriptor.Signature = MCTP_INFO_SIGNATURE;
  MctpDataDescriptor.Length    = sizeof (Header);
  MctpDataDescriptor.Size      = sizeof (Header);
  MctpDataDescriptor.DataBlock = (VOID *)&Header;
  InsertHeadList (MessageBodyChunks, &MctpDataDescriptor.Link);

  return MctpCoreSendMessage (
           This->Binding,
           This->DestinationEID,
           TRUE,
           Tag,
           MessageBodyChunks,
           TimeoutUsec
           );
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
MctpVendorPCIConnect (
  IN MCTP_BINDING               *Binding,
  IN  UINT8                     DestinationEID,
  IN  UINT16                    VendorID,
  IN  UINT16                    BitField,
  IN  UINTN                     TimeoutUsec,
  OUT MCTP_PCI_VENDOR_PROTOCOL  *Protocol
  )
{
  EFI_STATUS              Status;
  MCTP_CONTROL_VENDOR_ID  RemoteVendorID;
  UINTN                   Index;

  if (Protocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  MctpCoreRegisterMessageClass (Binding, MCTP_TYPE_PCI_VENDOR);

  ZeroMem (Protocol, sizeof (MCTP_PCI_VENDOR_PROTOCOL));
  // FIXME: Not supported by some vendors
  if (0) {
    Status = MctpCoreRegisterPCIVendor (Binding, VendorID, BitField);
    if (EFI_ERROR (Status)) {
      DEBUG_MCTP ((DEBUG_ERROR, "MctpCoreRegisterPCIVendor failed with %r\n", Status));
      return Status;
    }

    Status = MctpCoreRemoteSupportMessageType (
               Binding,
               DestinationEID,
               MCTP_TYPE_PCI_VENDOR,
               TimeoutUsec
               );
    if (EFI_ERROR (Status)) {
      DEBUG_MCTP ((DEBUG_ERROR, "MctpCoreRemoteSupportMessageType failed with %r\n", Status));
      return Status;
    }

    Index = 0;
    do {
      Status = MctpControlGetMCTPVendorMessageSupport (
                 Binding,
                 DestinationEID,
                 Index,
                 &RemoteVendorID,
                 TimeoutUsec
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if ((RemoteVendorID.VendorIDFormat == MCTP_VENDOR_ID_FORMAT_PCI) &&
          (RemoteVendorID.Pci.PCIVendorID == VendorID))
      {
        break;
      }

      Index++;
    } while (1);
  }

  Protocol->Connected      = TRUE;
  Protocol->DestinationEID = DestinationEID;
  Protocol->VendorID       = VendorID;
  Protocol->Binding        = Binding;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MctpVendorPCILibConstructor (
  VOID
  )
{
  return EFI_SUCCESS;
}
