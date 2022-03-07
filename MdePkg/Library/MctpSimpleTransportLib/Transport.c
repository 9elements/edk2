#include <Base.h>
#include <Uefi.h>
#include <Library/MctpTransportLib.h>
#include <Library/MctpCoreLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/TimerLib.h>
#include <Library/MctpPhysicalTransportLib.h>
#include <Library/DebugLib.h>

/** Connects the physical layer.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpTransportConnect (
  OUT MCTP_TRANSPORT_BINDING  *Binding
  )
{
  ZeroMem (Binding, sizeof (*Binding));
  Binding->ExpectSOM = TRUE;

  return MctpPhysicalConnect (Binding->PhysicalContext);
}

/** Returns the current Maximum Transmission Unit.
  @param  [in]  Binding                  The transport context as initialized with MctpTransportConnect.

  @retval The current MTU. By default the BTU is used.

**/
UINTN
EFIAPI
MctpGetMTU (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding
  )
{
  return MctpPhysicalMTU (Binding->PhysicalContext);
}

/** Fragments the message, fills in missing bits into the header and transmits
    the message over the underlaying physical MCTP layer.

  @param  [in]  Binding                  The transport context as initialized with MctpTransportConnect.
  @param  [in]  Hdr                      Prefilled MCTP header. Some fields are updated before tx.
  @param  [in]  MessageBodyChunk         Pointer to linked list of data to transmit.
  @param  [in]  TimeoutUsec              The time in microseconds for this function to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
  @retval EFI_TIMEOUT            The command timed out.
**/
EFI_STATUS
EFIAPI
MctpTransportSendMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN MCTP_TRANSPORT_HEADER       Hdr,
  IN LIST_ENTRY                  *MessageBodyChunk,
  IN UINTN                       TimeoutUsec
  )
{
  EFI_STATUS                Status;
  UINTN                     Length;
  UINTN                     Seq;
  PUSH_POP_BUFFER           PhysicalMsg;
  PUSH_POP_BUFFER           Chunk;
  UINT8                     Raw[FixedPcdGet32 (PcdMctpPhysicalMtu)];
  BOOLEAN                   First;
  BOOLEAN                   Last;
  MCTP_TRANSPORT_HEADER     *PatchHdr;
  LIST_ENTRY                *Link;
  EFI_MCTP_DATA_DESCRIPTOR  *MessageBody;

  Link = GetFirstNode (MessageBodyChunk);

  // Sanity checks
  if ((MessageBodyChunk == NULL) || IsNull (MessageBodyChunk, Link)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = MctpTransportWaitForReadyToSend (Binding, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Transport not ready to accept new message: %r\n", __FUNCTION__, Status));
    return Status;
  }

  InitPushPopBuffer (Raw, sizeof (Raw), &PhysicalMsg);
  First = TRUE;
  Last  = FALSE;
  Seq   = 0;

  MessageBody = MCTP_INFO_FROM_LINK (Link);
  InitPushPopBuffer (MessageBody->DataBlock, MessageBody->Size, &Chunk);
  BufferUpdate (&Chunk, MessageBody->Length);

  while (TRUE) {
    if (BufferLength (&Chunk) == 0) {
      if (IsNodeAtEnd (MessageBodyChunk, Link)) {
        break;
      }

      //
      // Get data from linked list
      //
      Link        = GetNextNode (MessageBodyChunk, Link);
      MessageBody = MCTP_INFO_FROM_LINK (Link);

      //
      // Create PushPopBuffer
      //
      InitPushPopBuffer (MessageBody->DataBlock, MessageBody->Size, &Chunk);
      BufferUpdate (&Chunk, MessageBody->Length);
    }

    if (BufferLength (&PhysicalMsg) == 0) {
      //
      // Every message has a transport header
      //
      Status = BufferPush (&PhysicalMsg, &Hdr, sizeof (Hdr));

      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    //
    // Get the maximum length of data that is possible to pop
    //
    Length = MIN (BufferLength (&Chunk), MctpGetMTU (Binding) - BufferLength (&PhysicalMsg));

    //
    // Add data to PhysicalMsg
    //
    Status = BufferPopFrontPush (&Chunk, Length, &PhysicalMsg);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Last = IsNodeAtEnd (MessageBodyChunk, Link) && (BufferLength (&Chunk) == 0);

    if ((BufferLength (&PhysicalMsg) < MctpGetMTU (Binding)) && !Last) {
      //
      // Fill in more data
      //
      continue;
    }

 #if 0
    if (BufferLength (&PhysicalMsg) == 0) {
      //
      // Every message has a transport header
      //
      Status = BufferPush (&PhysicalMsg, &Hdr, sizeof (Hdr));

      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    if (BufferLength (MessageBodyHeader) > 0) {
      Length = MIN (BufferLength (MessageBodyHeader), MctpGetMTU (Binding) - BufferLength (&PhysicalMsg));

      Status = BufferPopFrontPush (MessageBodyHeader, Length, &PhysicalMsg);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    } else if (BufferLength (MessageBody) > 0) {
      Length = MIN (BufferLength (MessageBody), MctpGetMTU (Binding) - BufferLength (&PhysicalMsg));

      Status = BufferPopFrontPush (MessageBody, Length, &PhysicalMsg);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    Last = BufferLength (MessageBodyHeader) == 0 && BufferLength (MessageBody) == 0;

    if ((BufferLength (&PhysicalMsg) < MctpGetMTU (Binding)) && !Last) {
      continue;
    }

    //
    // Set resource to zero for nodes where allocation fails
    //
    for (Link = GetFirstNode (&HostBridge->RootBridges)
         ; !IsNull (&HostBridge->RootBridges, Link)
         ; Link = GetNextNode (&HostBridge->RootBridges, Link)
         )
    {
      RootBridge = ROOT_BRIDGE_FROM_LINK (Link);
      for (Index = TypeIo; Index < TypeBus; Index++) {
        if (RootBridge->ResAllocNode[Index].Status != ResAllocated) {
          RootBridge->ResAllocNode[Index].Length = 0;
        }
      }
    }

 #endif

    PatchHdr = BufferData (&PhysicalMsg);

    //
    // Update transport header with medium specific version number
    //
    PatchHdr->Hdr = MctpPhysicalGetTransportHeaderVersion (Binding->PhysicalContext);

    // Fill transport header
    if (First == TRUE) {
      PatchHdr->FlagsSeqTag |= MCTP_TRANSPORT_HDR_FLAG_SOM;
      First                  = FALSE;
    } else {
      PatchHdr->FlagsSeqTag &= ~MCTP_TRANSPORT_HDR_FLAG_SOM;
    }

    if (Last) {
      PatchHdr->FlagsSeqTag |= MCTP_TRANSPORT_HDR_FLAG_EOM;
    } else {
      PatchHdr->FlagsSeqTag &= ~MCTP_TRANSPORT_HDR_FLAG_EOM;
    }

    PatchHdr->FlagsSeqTag &= ~(MCTP_TRANSPORT_HDR_SEQ_MASK << MCTP_TRANSPORT_HDR_SEQ_SHIFT);
    PatchHdr->FlagsSeqTag |= (Seq & MCTP_TRANSPORT_HDR_SEQ_MASK) << MCTP_TRANSPORT_HDR_SEQ_SHIFT;
    Seq++;

    //
    // Send Message
    //
    Status = MctpPhysicalSend (Binding->PhysicalContext, &PhysicalMsg, TimeoutUsec);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to send the message: %r\n", __FUNCTION__, Status));
      return Status;
    }

    BufferClear (&PhysicalMsg);

    // Wait until remote read it
    if (!Last) {
      Status = MctpTransportWaitForReadyToSend (Binding, TimeoutUsec);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Transport not ready to accept new message: %r\n", __FUNCTION__, Status));
        return Status;
      }
    }
  }

  return EFI_SUCCESS;
}

// check the transport layer for pending rx
BOOLEAN
EFIAPI
MctpTransportHasMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding
  )
{
  return MctpPhysicalHasMessage (Binding->PhysicalContext);
}

EFI_STATUS
EFIAPI
MctpTransportWaitForMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN     UINTN                   TimeoutUsec
  )
{
  UINTN  Timeout = 0;

  while (!MctpPhysicalHasMessage (Binding->PhysicalContext)) {
    MicroSecondDelay (100);
    Timeout += 100;
    if (Timeout >= TimeoutUsec) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to receive message within timeout\n", __FUNCTION__));
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

// Ready to accept a new message?
BOOLEAN
EFIAPI
MctpTransportReadyToSend (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding
  )
{
  return MctpPhysicalReadyToSend (Binding->PhysicalContext);
}

EFI_STATUS
EFIAPI
MctpTransportWaitForReadyToSend (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN     UINTN                   TimeoutUsec
  )
{
  UINTN  Timeout = 0;

  while (!MctpPhysicalReadyToSend (Binding->PhysicalContext)) {
    MicroSecondDelay (100);
    Timeout += 100;
    if (Timeout >= TimeoutUsec) {
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/** Simple receiver function. Checks multi packet MCTP messages, which must be transmitted from
    the same source to the same destination in one series. Interleaving multiple multi packet MCTP
    messages isn't supported and will be reported as error.
    The multi packet stream might be interleaved by single packet MCTP messages.

  @param  [in]  Binding     The transport context as initialized with MctpTransportConnect.
  @param  [in]  Msg         Pointer to received MCTP message.
  @param  [in]  MsgLength   The length of the received MCTP message.
  @param  [in]  TimeoutUsec The time in microseconds for this function to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_NOT_STARTED        Received no multi packet start message.
  @retval EFI_ALREADY_STARTED    Received a new start message, but didn't expect it.
  @retval EFI_PROTOCOL_ERROR     The MCTP packet was malformed or not expected.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
  @retval EFI_TIMEOUT            The command timed out.
**/
EFI_STATUS
EFIAPI
MctpTransportReceiveMessage (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  OUT  MCTP_MSG                  **Msg,
  OUT  UINTN                     *MsgLength,
  IN   UINTN                     TimeoutUsec
  )
{
  EFI_STATUS             Status;
  PUSH_POP_BUFFER        PhysicalMsg;
  MCTP_TRANSPORT_HEADER  *Hdr;

  // Sanity checks
  if ((Msg == NULL) || (MsgLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: Entering...\n", __FUNCTION__));

  Status = MctpTransportWaitForMessage (Binding, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to wait for message arrival: %r\n", __FUNCTION__, Status));
    return Status;
  }

  InitPushPopBuffer (Binding->RxBuffer, sizeof (Binding->RxBuffer), &PhysicalMsg);

  // Underlaying layer will know the exact packet length
  Status = MctpPhysicalReceive (Binding->PhysicalContext, &PhysicalMsg, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to receive message: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Got packet...\n", __FUNCTION__));

  if (BufferLength (&PhysicalMsg) < sizeof (MCTP_TRANSPORT_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: Received malformed message\n", __FUNCTION__));
    return EFI_PROTOCOL_ERROR;
  }

  Hdr = BufferData (&PhysicalMsg);

  if ((Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_SOM) && (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_EOM)) {
    //
    // Single fragment packet
    //
    Status = EFI_SUCCESS;
  } else {
    //
    // Multi fragment packet
    //
    if (!(Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_SOM) && Binding->ExpectSOM) {
      // Not a start packet
      Status = EFI_NOT_STARTED;
    } else if (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_SOM) {
      if (!Binding->ExpectSOM) {
        // repeated start
        Status = EFI_ALREADY_STARTED;
      }

      DEBUG ((DEBUG_INFO, "%a: Multipacket start\n", __FUNCTION__));

      Binding->ExpectSOM            = FALSE;
      Binding->ExpectSourceEID      = Hdr->SourceEID;
      Binding->ExpectDestinationEID = Hdr->DestEID;
      Binding->ExpectSeq            = (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_SEQ_MASK) >> MCTP_TRANSPORT_HDR_SEQ_SHIFT;
    } else {
      Binding->ExpectSeq = (Binding->ExpectSeq + 1) % 4;
      if (Binding->ExpectSeq != ((Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_SEQ_MASK) >> MCTP_TRANSPORT_HDR_SEQ_SHIFT)) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Expected Seq %x, but got %x\n",
          __FUNCTION__,
          Binding->ExpectSeq,
          (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_SEQ_MASK) >> MCTP_TRANSPORT_HDR_SEQ_SHIFT
          ));
        // Status = EFI_PROTOCOL_ERROR;
      } else if (Binding->ExpectSourceEID != Hdr->SourceEID) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Expected Source EID %02x, but received from EID %02x\n",
          __FUNCTION__,
          Binding->ExpectSourceEID,
          Hdr->SourceEID
          ));

        Status = EFI_PROTOCOL_ERROR;
      } else if (Binding->ExpectDestinationEID != Hdr->DestEID) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Expected Dest EID %02x, but target EID is %02x\n",
          __FUNCTION__,
          Binding->ExpectDestinationEID,
          Hdr->DestEID
          ));
        Status = EFI_PROTOCOL_ERROR;
      }
    }
  }

  if (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_EOM) {
    Binding->ExpectSOM = TRUE;
    DEBUG ((DEBUG_INFO, "%a: Multipacket end\n", __FUNCTION__));
  }

  *Msg       = BufferData (&PhysicalMsg);
  *MsgLength = BufferLength (&PhysicalMsg);
  DEBUG ((DEBUG_ERROR, "%a: Status %r\n", __FUNCTION__, Status));

  return Status;
}

/** Simple receiver function. Checks multi packet MCTP messages, which must be transmitted from
    the same source to the same destination in one series. Interleaving multiple multi packet MCTP
    messages isn't supported and will be reported as error.
    Check SOM and EOM as well as the sequence number to be ascending.
    Dest EID and source EID are not checked and must be verified by the caller.
    The transport header is removed and only data is placed in the LIST_ENTRY buffers.

  @param  [in]      Binding                  The transport context as initialized with MctpTransportConnect.
  @param  [out]     MessageBodyChunk         Pointer to linked list of buffers to place received data into.
  @param  [out]     Hdr                      Pointer for the current transport header received.
  @param  [in out]  Remaining                The number of bytes not added to MessageBodyChunk.
                                             The caller must initialize this field to 0.
  @param  [in]      TimeoutUsec              The time in microseconds for this function to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_NOT_STARTED        Received no multi packet start message.
  @retval EFI_ALREADY_STARTED    Received a new start message, but didn't expect it.
  @retval EFI_PROTOCOL_ERROR     The MCTP packet was malformed or not expected.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
  @retval EFI_TIMEOUT            The command timed out.
**/
EFI_STATUS
EFIAPI
MctpTransportReceiveMessageChunks (
  IN OUT MCTP_TRANSPORT_BINDING  *Binding,
  IN OUT LIST_ENTRY              *MessageBodyChunk,
  OUT    MCTP_TRANSPORT_HEADER   *Hdr,
  IN OUT UINTN                   *Remaining,
  IN     UINTN                   TimeoutUsec
  )
{
  EFI_STATUS                Status;
  PUSH_POP_BUFFER           PhysicalMsg;
  PUSH_POP_BUFFER           Chunk;
  UINTN                     Length;
  LIST_ENTRY                *Link;
  EFI_MCTP_DATA_DESCRIPTOR  *MessageBody;

  // Sanity checks
  if ((Binding == NULL) || (MessageBodyChunk == NULL) || (Hdr == NULL) || (Remaining == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = MctpTransportWaitForMessage (Binding, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to wait for message arrival: %r\n", __FUNCTION__, Status));
    return Status;
  }

  InitPushPopBuffer (Binding->RxBuffer, sizeof (Binding->RxBuffer), &PhysicalMsg);

  // Underlaying layer will know the exact packet length
  Status = MctpPhysicalReceive (Binding->PhysicalContext, &PhysicalMsg, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to receive message: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (BufferLength (&PhysicalMsg) < sizeof (MCTP_TRANSPORT_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: Received malformed message\n", __FUNCTION__));
    return EFI_PROTOCOL_ERROR;
  }

  Status = BufferPopFront (&PhysicalMsg, Hdr, sizeof (MCTP_TRANSPORT_HEADER));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to receive message: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if ((Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_SOM) && (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_EOM)) {
    //
    // Single fragment packet
    //
    Status = EFI_SUCCESS;
  } else {
    //
    // Multi fragment packet
    //
    if (!(Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_SOM) && Binding->ExpectSOM) {
      // Not a start packet
      Status = EFI_NOT_STARTED;
    } else if (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_SOM) {
      if (!Binding->ExpectSOM) {
        // repeated start
        Status = EFI_ALREADY_STARTED;
      }

      Binding->ExpectSOM            = FALSE;
      Binding->ExpectSourceEID      = Hdr->SourceEID;
      Binding->ExpectDestinationEID = Hdr->DestEID;
      Binding->ExpectSeq            = (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_SEQ_MASK) >> MCTP_TRANSPORT_HDR_SEQ_SHIFT;
    } else {
      Binding->ExpectSeq = (Binding->ExpectSeq + 1) % 4;
      if (Binding->ExpectSeq != ((Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_SEQ_MASK) >> MCTP_TRANSPORT_HDR_SEQ_SHIFT)) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Expected Seq %x, but got %x\n",
          __FUNCTION__,
          Binding->ExpectSeq,
          (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_SEQ_MASK) >> MCTP_TRANSPORT_HDR_SEQ_SHIFT
          ));
        // Status = EFI_PROTOCOL_ERROR;
      } else if (Binding->ExpectSourceEID != Hdr->SourceEID) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Expected Source EID %02x, but received from EID %02x\n",
          __FUNCTION__,
          Binding->ExpectSourceEID,
          Hdr->SourceEID
          ));

        Status = EFI_PROTOCOL_ERROR;
      } else if (Binding->ExpectDestinationEID != Hdr->DestEID) {
        DEBUG ((
          DEBUG_WARN,
          "%a: Expected Dest EID %02x, but target EID is %02x\n",
          __FUNCTION__,
          Binding->ExpectDestinationEID,
          Hdr->DestEID
          ));
        Status = EFI_PROTOCOL_ERROR;
      }
    }
  }

  if (Hdr->FlagsSeqTag & MCTP_TRANSPORT_HDR_FLAG_EOM) {
    Binding->ExpectSOM = TRUE;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed with %r\n", __FUNCTION__, Status));
    return Status;
  }

  Link        = GetFirstNode (MessageBodyChunk);
  MessageBody = MCTP_INFO_FROM_LINK (Link);

  InitPushPopBuffer (MessageBody->DataBlock, MessageBody->Size, &Chunk);
  BufferUpdate (&Chunk, MessageBody->Length);

  while (BufferLength (&PhysicalMsg) > 0) {
    if (BufferLength (&Chunk) == BufferSize (&Chunk)) {
      if (IsNodeAtEnd (MessageBodyChunk, Link)) {
        break;
      }

      //
      // Get data from linked list
      //
      Link        = GetNextNode (MessageBodyChunk, Link);
      MessageBody = MCTP_INFO_FROM_LINK (Link);

      //
      // Update PushPopBuffer
      //
      InitPushPopBuffer (MessageBody->DataBlock, MessageBody->Size, &Chunk);
      BufferUpdate (&Chunk, MessageBody->Length);

      continue;
    }

    //
    // Get the maximum length of data that is possible to pop
    //
    Length = MIN (BufferSize (&Chunk) - BufferLength (&Chunk), BufferLength (&PhysicalMsg));

    //
    // Add data to Chunk
    //
    Status = BufferPopFrontPush (&PhysicalMsg, Length, &Chunk);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    //
    // Tell caller about the received data
    //
    MessageBody->Length = BufferLength (&Chunk);
  }

  if (BufferLength (&PhysicalMsg) > 0) {
    *Remaining += BufferLength (&PhysicalMsg);
    //
    // Do not return EFI_BUFFER_TOO_SMALLER.
    // If the caller passed in a smaller buffer this was for reasons.
    // Let the caller decide to return EFI_BUFFER_TOO_SMALLER when Remaining > 0.
    //
  }

  return Status;
}

EFI_STATUS
EFIAPI
MctpTransportLibConstructor (
  VOID
  )
{
  return EFI_SUCCESS;
}
