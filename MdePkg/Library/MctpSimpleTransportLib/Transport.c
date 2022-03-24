#include <Base.h>
#include <Uefi.h>
#include <Library/MctpTransportLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Library/MctpPhysicalTransportLib.h>
#include <Library/DebugLib.h>

STATIC UINTN     MTU = MCTP_BTU;
STATIC MCTP_MSG  RxBuffer;
STATIC MCTP_MSG  *RxMsg;

STATIC UINT8  ExpectSourceEID;
STATIC UINT8  ExpectDestinationEID;

/** Requests a new Maximum Transmission Unit. The default ist 64.

  @param  [in]  Size    Size of the payload to be sent
  @param  [in]  Buffer  A buffer of at least size Size + sizeof(MCTP_TRANSPORT_HEADER)
                        to store the message into.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpSetNewMTU (
  IN UINTN  Size,
  IN UINT8  *Buffer
  )
{
  if (Size < MCTP_BTU) {
    return EFI_INVALID_PARAMETER;
  }

  MTU   = Size;
  RxMsg = (MCTP_MSG *)Buffer;

  return EFI_SUCCESS;
}

/** Returns the current Maximum Transmission Unit.

  @retval The current MTU. By default the BTU is used.

**/
UINTN
EFIAPI
MctpGetMTU (
  VOID
  )
{
  return MTU;
}

/** Return how many MCTP Message need to be send.

  @param  [in]  Size  Size of the payload to be sent

  @retval Number of MCTP Messages
**/
STATIC
UINTN
CalculateAmountMessages (
  IN UINTN  Size
  )
{
  if (Size == 0) {
    return 0;
  }

  return ((Size + MTU - 1) / MTU);
}

/** Constructs a fragment message.

  @param  [in]  Hdr                      The payload header prefilled by the caller.
  @param  [in]  MessageBodyHeader        Optional Message Body header. Will prepended
                                         to the data in MessageBody.
  @param  [in]  MessageBodyHeaderLength  The length of the optional message body header.
  @param  [in]  MessageBody              The Message body to transmit.
  @param  [in]  MessageBodySize          The size of the message body.
  @param  [in]  Index                    Identifies the nth packet to transmit.
  @param  [in]  MaxMessages              The total number of packets to transmit.
  @param  [out] MctpMsg                  The final assembled message.

**/
STATIC
VOID
FillMessage (
  IN  MCTP_TRANSPORT_HEADER  *Hdr,
  IN  UINT8                  *MessageBodyHeader,
  IN  UINTN                  MessageBodyHeaderLength,
  IN  UINT8                  *MessageBody,
  IN  UINTN                  MessageBodyLength,
  IN  UINTN                  Index,
  IN  UINTN                  MaxMessages,
  OUT MCTP_MSG               *MctpMsg,
  OUT UINTN                  *Length

  )
{
  UINTN  OffsetDest;
  UINTN  OffsetSource;

  CopyMem (&MctpMsg->Header, Hdr, sizeof (MCTP_TRANSPORT_HEADER));

  // Fill transport header
  if (Index == 0) {
    MctpMsg->Header.SOM = 1;
  } else {
    MctpMsg->Header.SOM = 0;
  }

  if (Index == (MaxMessages - 1)) {
    MctpMsg->Header.EOM = 1;
  } else {
    MctpMsg->Header.EOM = 0;
  }

  MctpMsg->Header.PktSeq = Index % 4;

  OffsetSource = MTU * Index;
  OffsetDest   = OffsetSource;
  *Length      = MTU;

  if (MessageBodyHeaderLength > 0) {
    if (Index == 0) {
      CopyMem (&MctpMsg->Body.Raw, MessageBodyHeader, MessageBodyHeaderLength);
      OffsetDest += MessageBodyHeaderLength;
      *Length    -= MessageBodyHeaderLength;
    } else {
      OffsetSource -= MessageBodyHeaderLength;
    }
  }

  if (OffsetDest + MTU > (MessageBodyLength + MessageBodyHeaderLength)) {
    *Length = (MessageBodyLength + MessageBodyHeaderLength) - OffsetDest;
  }

  CopyMem (&MctpMsg->Body.Raw[OffsetDest], &MessageBody[OffsetSource], *Length);
}

/** Fragments the message, fills in missing bits into the header and transmits
    the message over the underlaying physical MCTP layer.

  @param  [in]  Hdr                      Prefilled MCTP header. Some fields are updated before tx.
  @param  [in]  MessageBodyHeader        Optional Message Body header. Will prepended
                                         to the data in MessageBody.
  @param  [in]  MessageBodyHeaderLength  The length of the optional message body header.
  @param  [in]  MessageBody              The Message body to transmit.
  @param  [in]  MessageBodyLength        The size of the message body.
  @param  [in]  TimeoutUsec              The time in microseconds for this function to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
  @retval EFI_TIMEOUT            The command timed out.
**/
EFI_STATUS
MctpTransportSendMessage (
  IN MCTP_TRANSPORT_HEADER  *Hdr,
  IN UINT8                  *MessageBodyHeader,
  IN UINTN                  MessageBodyHeaderLength,
  IN UINT8                  *MessageBody,
  IN UINTN                  MessageBodyLength,
  IN UINTN                  TimeoutUsec
  )
{
  EFI_STATUS         Status;
  UINTN              Index;
  UINTN              AmountOfMsgs;
  MCTP_PHYSICAL_MSG  PhysicalMsg;
  MCTP_MSG           Msg;

  // Sanity checks
  if ((Hdr == NULL) || (MessageBody == NULL) || (MessageBodyLength == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = MctpTransportWaitForReadyToSend (TimeoutUsec);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Transport not ready to accept new message: %r\n", Status));
    return Status;
  }

  // Calculate amount of messages
  AmountOfMsgs = CalculateAmountMessages (MessageBodyHeaderLength + MessageBodyLength);

  // Construct and send messages
  for (Index = 0; Index < AmountOfMsgs; Index++) {
    PhysicalMsg.Msg    = &Msg;
    PhysicalMsg.Length = 0;

    FillMessage (
      Hdr,
      MessageBodyHeader,
      MessageBodyHeaderLength,
      MessageBody,
      MessageBodyLength,
      Index,
      AmountOfMsgs,
      PhysicalMsg.Msg,
      &PhysicalMsg.Length
      );

    // Send Message
    Status = MctpPhysicalSend (&PhysicalMsg, TimeoutUsec); // TODO: Do we need to wait on an ack here?
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to send the message: %r\n", Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

// check the transport layer for pending rx
BOOLEAN
EFIAPI
MctpTransportHasMessage (
  VOID
  )
{
  return MctpPhysicalHasMessage ();
}

EFI_STATUS
EFIAPI
MctpTransportWaitForMessage (
  IN     UINTN  TimeoutUsec
  )
{
  UINTN  Timeout = 0;

  while (!MctpPhysicalHasMessage ()) {
    MicroSecondDelay (100);
    Timeout += 100;
    if (Timeout >= TimeoutUsec ) {
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

// Ready to accept a new message?
BOOLEAN
EFIAPI
MctpTransportReadyToSend (
  VOID
  )
{
  return MctpPhysicalReadyToSend ();
}

EFI_STATUS
EFIAPI
MctpTransportWaitForReadyToSend (
  IN     UINTN  TimeoutUsec
  )
{
  UINTN  Timeout = 0;

  while (!MctpPhysicalReadyToSend ()) {
    MicroSecondDelay (100);
    Timeout += 100;
    if (Timeout >= TimeoutUsec ) {
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/** Simple receiver function. Checks multi packet MCTP messages, which must be transmitted from
    the same source to the same destination in one series. Interleaving multiple multi packet MCTP
    messages isn't supported and will be reported as error.
    The multi packet stream might be interleaved by single packet MCTP messages.

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
  OUT  MCTP_MSG  **Msg,
  OUT  UINTN     *MsgLength,
  IN   UINTN     TimeoutUsec
  )
{
  EFI_STATUS         Status;
  UINTN              ExpectSOM;
  UINTN              Seq;
  MCTP_PHYSICAL_MSG  PhysicalMsg;

  // Sanity checks
  if ((Msg == NULL) || (MsgLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = MctpTransportWaitForMessage (TimeoutUsec);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PhysicalMsg.Mtu    = MTU;
  PhysicalMsg.Length = 0;
  PhysicalMsg.Msg    = RxMsg;

  // Underlaying layer will know the exact packet length
  Status = MctpPhysicalReceive (&PhysicalMsg, TimeoutUsec);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (PhysicalMsg.Length < sizeof (MCTP_TRANSPORT_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }

  if ((RxMsg->Header.SOM == 1) && (RxMsg->Header.EOM == 1)) {
    //
    // Single fragment packet
    //
    Status = EFI_SUCCESS;
  } else {
    //
    // Multi fragment packet
    //
    if ((RxMsg->Header.SOM == 0) && (ExpectSOM == 1)) {
      // Not a start packet
      Status = EFI_NOT_STARTED;
    } else if (RxMsg->Header.SOM == 1) {
      if (ExpectSOM == 0) {
        // repeated start
        Status = EFI_ALREADY_STARTED;
      }

      ExpectSOM            = 0;
      ExpectSourceEID      = RxMsg->Header.SourceEID;
      ExpectDestinationEID = RxMsg->Header.DestEID;
      Seq                  = RxMsg->Header.PktSeq;
    } else {
      Seq = (Seq + 1) % 4;
      if (Seq != RxMsg->Header.PktSeq) {
        Status = EFI_PROTOCOL_ERROR;
      } else if (ExpectSourceEID != RxMsg->Header.SourceEID) {
        Status = EFI_PROTOCOL_ERROR;
      } else if (ExpectDestinationEID != RxMsg->Header.DestEID) {
        Status = EFI_PROTOCOL_ERROR;
      }
    }
  }

  if (RxMsg->Header.EOM == 1) {
    ExpectSOM = 1;
  }

  return Status;
}

EFI_STATUS
EFIAPI
MctpTransportLibConstructor (
  VOID
  )
{
  RxMsg = &RxBuffer;

  return EFI_SUCCESS;
}
