
#include <Base.h>
#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpTransportLib.h>
#include <Library/DebugLib.h>

STATIC UINT8 InstanceID;

/**  Fill the MCTP transport header and the control message header.

  @param  [in]      DestEID      The endpoint ID to send the message to.
  @param  [in]      SourceEID    The source endpoint ID.
  @param  [in]      D            Datagramm bit.
  @param  [in]      CommandCode  CommandCode identifying the msg.
  @param  [in out]  Msg          Pointer to MCTP message to work on.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
EFIAPI
MctpControlConstructHeader(
  IN UINT8         DestEID,
  IN UINT8         SourceEID,
  IN UINT8         D,
  IN UINT8         CommandCode,
  IN OUT MCTP_MSG  *Msg
  )
{
  MCTP_CONTROL_MSG_HEADER *Hdr;

  if (Msg == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Hdr = &Msg->Body.ControlMsg.Header;

  ZeroMem(Hdr, sizeof(MCTP_TRANSPORT_HEADER));
  Hdr->IC = 0;
  Hdr->MsgType = MCTP_TYPE_CONTROL_MSG;
  Hdr->Rq = 1;
  Hdr->InstanceID = InstanceID;
  Hdr->D = D;
  Hdr->CommandCode = CommandCode;

  InstanceID = (InstanceID + 1) % 32;

  return EFI_SUCCESS;
}

/**  Converts the MCTP completition code to an EFI error.

  @param  [in]  CompletionCode   The completion code to decode.

  @retval EFI_SUCCESS            The completion code contains no error.
  @retval EFI_INVALID_PARAMETER  The completion code is unknown.
  @retval others                 See below.
**/
STATIC
EFI_STATUS
EFIAPI
MctpControlCompletionCodeToError(
  IN UINT8 CompletionCode
  )
{
  switch (CompletionCode) {
    case MCTP_SUCCESS:
      return EFI_SUCCESS;
    case MCTP_ERR:
      return EFI_DEVICE_ERROR;
    case MCTP_ERR_INVALID_DATA:
      return EFI_COMPROMISED_DATA;
    case MCTP_ERR_INVALID_LEN:
      return EFI_BAD_BUFFER_SIZE;
    case MCTP_ERR_NOT_READY:
      return EFI_NOT_READY;
    case MCTP_ERR_UNSUPPORTED:
      return EFI_UNSUPPORTED;
    default:
      return EFI_INVALID_PARAMETER;
  }
}

/**  Waits for a response on the given command in Msg. Discard all other message.

  @param  [in]      Msg             The transmitted control/request message.
  @param  [in]      TimeoutUsec     Timeout in microseconds to wait for a response.
  @param  [in out]  Body            The buffert to write the received packet into.
  @param  [out]     BodyLength      The length of the buffer where to receive the packet.

  @retval EFI_SUCCESS            The completion code contains no error.
  @retval EFI_TIMEOUT            The command timed out.
  @retval EFI_INVALID_PARAMETER  The paramter is invalid.
**/
STATIC
EFI_STATUS
EFIAPI
MctpControlAwaitResponse(
  IN     MCTP_TRANSPORT_HEADER *SendHeader,
  IN     UINTN                 TimeoutUsec,
  OUT    MCTP_BODY             *Body,
  IN OUT UINTN                 *BodyLength
  )
{
  EFI_STATUS Status;
  MCTP_MSG   *Response;
  UINTN      Length;
  UINTN      Offset;

  if (SendHeader == NULL || Body == NULL || BodyLength == 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (*BodyLength < sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
    return EFI_INVALID_PARAMETER;
  }

  do {
    Offset = 0;
    do {
      Length = 0;
      Status = MctpCoreReceiveMessage(&Response,
          &Length,
          TimeoutUsec);
      if (EFI_ERROR(Status)) {
        return Status;
      }

      //
      // Only check direct response message
      //
      if (Response->Header.DestEID != SendHeader->SourceEID ||
          Response->Header.SourceEID != SendHeader->DestEID ||
          Response->Header.TO != 0 ||
          Response->Header.MsgTag != SendHeader->MsgTag) {
            continue;
      }
      if (Response->Header.SOM == 1) {
        //
        // Need a type header
        //
        if (Length < sizeof(MCTP_MSG_TYPE)) {
          continue;
        }

        //
        // Drop non control packets
        //
        if (Response->Body.Type.MsgType != MCTP_TYPE_CONTROL_MSG) {
          continue;
        }

        //
        // Drop malformed control packets
        //
        if (Response->Body.Type.IC != 0) {
          continue;
        }
      }

      if ((Length + Offset) > *BodyLength) {
        Length = *BodyLength - Offset;
      }

      if (Offset < *BodyLength) {
        CopyMem(&Body->Raw[Offset], Response->Body.Raw, Length);
        Offset += Length;
      }

    } while (Response->Header.EOM == 0);

    Length = Offset;

    //
    // Need control header and completion code
    //
    if (Length < sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
      continue;
    }

    //
    // Received a valid control response, check if it's valid and answering the request
    //
    if ((Body->ControlResponseMsg.Header.Rq != 0) ||
        (Body->ControlResponseMsg.Header.D != 0)) {
      continue;
    }

    // FIXME: check Instance ID

    //
    // Found Response. Now decode the completion code. On error return now.
    //
    Status = MctpControlCompletionCodeToError(Body->ControlResponseMsg.Header.CompletionCode);
    if (EFI_ERROR (Status)) {
      *BodyLength = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
    } else {
      *BodyLength = Length;
    }

    return Status;
  } while (TimeoutUsec--); // FIXME: Timeout

  return EFI_TIMEOUT;
}

/**  This command should only be issued by a bus owner to assign
     an EID to an endpoint at a particular physical address.

  @param  [in]  EndpointID      The new Endpoint ID to use by DestinationEID.
  @param  [in]  DestinationEID  The Destination EID, typically set to null destination value.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpControlSetEndpointID(
  IN UINT8 EndpointID,
  IN UINT8 DestinationEID,
  IN UINTN TimeoutUsec
  )
{
  EFI_STATUS                         Status;
  MCTP_MSG                           Msg;
  MCTP_CONTROL_SET_ENDPOINT_REQ_MSG  Req;
  MCTP_CONTROL_SET_ENDPOINT_RESP_MSG Resp;
  UINTN                              Length;

  if (!MctpIsAssignableEndpointID(EndpointID) ||
      !MctpIsValidEndpointID(DestinationEID)) {
    return EFI_INVALID_PARAMETER;
  }

  Req.EndpointID = EndpointID;
  Req.RequestData = 0;

  ZeroMem(&Msg, sizeof(Msg));

  //
  // Install transport and control header
  //
  Status = MctpControlConstructHeader(
      DestinationEID,
      MctpGetOwnEndpointID(),
      0,
      MctpCmdSetEndpointID,
      &Msg);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // Install control body
  //
  Msg.Body.ControlMsg.Body.SetEndpointReq = Req;

  Status = MctpTransportSendMessage(&Msg.Header,
    NULL,
    0,
    Msg.Body.Raw,
    sizeof(MCTP_CONTROL_MSG_HEADER) + sizeof(Req),
    TimeoutUsec
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Length = sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  Status = MctpControlAwaitResponse(
      &Msg.Header,
      TimeoutUsec,
      &Msg.Body,
      &Length
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  if (Length < sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }
  Resp = Msg.Body.ControlResponseMsg.Body.SetEndpointResp;
  //
  // Check if EID has been accepted
  //
  if ((Resp.Status & 0x30) == 0x10)
    return EFI_ACCESS_DENIED;
  if (Resp.EIDSetting != EndpointID)
    return EFI_ACCESS_DENIED;

  return EFI_SUCCESS;
}

/**  Retreive the EID for an enpoint. This command is typically issued only by
     a bus owner to retrieve the EID that was assigned to a particular physical address.

  @param  [in]  DestinationEID  The Destination EID, typically set to null destination value.
  @param  [out] EndpointID      The retrieved EID. 
  @param  [out] EndpointType    Additional information about the EID and the endpoint.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpControlGetEndpointID(
  IN  UINT8  DestinationEID,
  OUT UINT8  *EndpointID,
  OUT UINT8  *EndpointType,
  IN  UINTN  TimeoutUsec
  )
{
  EFI_STATUS                         Status;
  MCTP_MSG                           Msg;
  MCTP_CONTROL_GET_ENDPOINT_RESP_MSG Resp;
  UINTN                              Length;

  if (EndpointID == NULL || EndpointType == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpIsValidEndpointID(DestinationEID)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem(&Msg, sizeof(Msg));

  //
  // Install transport and control header
  //
  Status = MctpControlConstructHeader(
      DestinationEID,
      MctpGetOwnEndpointID(),
      0,
      MctpCmdGetEndpointID,
      &Msg);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = MctpTransportSendMessage(&Msg.Header,
    NULL,
    0,
    Msg.Body.Raw,
    sizeof(MCTP_CONTROL_MSG_HEADER),
    TimeoutUsec
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Length = sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  Status = MctpControlAwaitResponse(
      &Msg.Header,
      TimeoutUsec,
      &Msg.Body,
      &Length
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  if (Length < sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }
  Resp = Msg.Body.ControlResponseMsg.Body.GetEndpointResp;
  *EndpointID = Resp.EndpointID;
  *EndpointType = Resp.EndpointType;

  return EFI_SUCCESS;
}

/**  This command can be used to retrieve the MCTP base specification versions that
     the endpoint supports, and also the message type specification versions supported
     for each message type. 

  @param  [in]  DestinationEID  The Destination EID.
  @param  [out] Entry           The version entries to fill.
  @param  [out] EntryCount      On entry the number of available version entries.
                                On exit the number of filled version entries.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpControlGetMCTPBaseVersionSupport(
  IN     UINT8                      DestinationEID,
  OUT    MCTP_CONTROL_VERSION_ENTRY *Entry,
  IN OUT UINTN                      *EntryCount,
  IN     UINTN                      TimeoutUsec
  )
{
  EFI_STATUS                             Status;
  MCTP_MSG                               Msg;
  MCTP_CONTROL_GET_MCTP_VERSION_RESP_MSG Resp;
  MCTP_CONTROL_GET_MCTP_VERSION_REQ_MSG  Req;
  UINTN                                  Index;
  UINTN                                  Length;

  if (Entry == NULL || EntryCount == NULL || *EntryCount == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpIsValidEndpointID(DestinationEID)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem(&Msg, sizeof(Msg));

  //
  // Install transport and control header
  //
  Status = MctpControlConstructHeader(
      DestinationEID,
      MctpGetOwnEndpointID(),
      0,
      MctpCmdGetMCTPVersionSupport,
      &Msg);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  //
  // Install control body
  //
  Msg.Body.ControlMsg.Body.GetMctpVersionReq = Req;

  Status = MctpTransportSendMessage(&Msg.Header,
    NULL,
    0,
    Msg.Body.Raw,
    sizeof(MCTP_CONTROL_MSG_HEADER) + sizeof(Req),
    TimeoutUsec
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Length = sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  Status = MctpControlAwaitResponse(
      &Msg.Header,
      TimeoutUsec,
      &Msg.Body,
      &Length
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  if (Length < sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }
  Resp = Msg.Body.ControlResponseMsg.Body.GetMctpVersionResp;
  if (Resp.EntryCount == 0) {
    return EFI_NO_RESPONSE;
  }
  for (Index = 0; Index < Resp.EntryCount && Index < *EntryCount; Index++) {
    Entry[Index] = Resp.Entry[Index];
  }
  *EntryCount = Index + 1;

  return EFI_SUCCESS;
}

/**  This command enables management controllers to discover the MCTP
     control protocol capabilities supported by other MCTP endpoints,
     and get a list of the MCTP message types that are supported by the endpoint. 

  @param  [in]  DestinationEID  The Destination EID.
  @param  [out] FirstEntry      The first entry to return.
  @param  [out] Entry           The message type entries to fill.
  @param  [out] EntryCount      On entry the number of available message type entries.
                                On exit the number of filled message type entries.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpControlGetMCTPMessageTypeSupport(
  IN     UINT8  DestinationEID,
  IN     UINTN  FirstEntry,
  OUT    UINT8  *Entry,
  IN OUT UINTN  *EntryCount,
  IN     UINTN  TimeoutUsec
  )
{
  EFI_STATUS                         Status;
  MCTP_MSG                           Msg;
  MCTP_CONTROL_GET_MSG_TYPE_RESP_MSG Resp;
  UINTN                              Index;
  UINTN                              Length;

  if (Entry == NULL || EntryCount == NULL || *EntryCount == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpIsValidEndpointID(DestinationEID)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem(&Msg, sizeof(Msg));

  //
  // Install transport and control header
  //
  Status = MctpControlConstructHeader(
      DestinationEID,
      MctpGetOwnEndpointID(),
      0,
      MctpCmdGetMCTPMessageTypeSupport,
      &Msg);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to construct a header: %r\n", Status));
    return Status;
  }

  Status = MctpTransportSendMessage(&Msg.Header,
    NULL,
    0,
    Msg.Body.Raw,
    sizeof(MCTP_CONTROL_MSG_HEADER),
    TimeoutUsec
  );
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to send a message: %r\n", Status));
    return Status;
  }

  Length = sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  Status = MctpControlAwaitResponse(
      &Msg.Header,
      TimeoutUsec,
      &Msg.Body,
      &Length
  );
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get a response: %r\n", Status));
    return Status;
  }
  if (Length < sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }

  Resp = Msg.Body.ControlResponseMsg.Body.GetMessageTypeResp;
  if (Resp.MessageTypeCount < FirstEntry) {
    return EFI_NO_RESPONSE;
  }
  for (Index = FirstEntry; Index < Resp.MessageTypeCount && Index < *EntryCount; Index++) {
    Entry[Index] = Resp.Entry[Index];
  }
  *EntryCount = Index + 1;

  return EFI_SUCCESS;
}

/**  This command enables management controllers to discover the MCTP
     control protocol capabilities supported by other MCTP endpoints,
     and get a list of the MCTP message types that are supported by the endpoint. 

  @param  [in]  DestinationEID  The Destination EID.
  @param  [in]  IDSelector      Indicies start at 0 and monotonically increase by 1.
  @param  [out] Entry           The vendor defined message type entry.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
EFIAPI
MctpControlGetMCTPVendorMessageSupport(
  IN  UINT8                   DestinationEID,
  IN  UINT8                   IDSelector,
  OUT MCTP_CONTROL_VENDOR_ID  *Entry,
  IN  UINTN                   TimeoutUsec
  )
{
  EFI_STATUS                                Status;
  MCTP_MSG                                  Msg;
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_RESP_MSG Resp;
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_REQ_MSG  Req;
  UINTN                                     Length;

  if (Entry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!MctpIsValidEndpointID(DestinationEID)) {
    return EFI_INVALID_PARAMETER;
  }

  Req.VendorIDSelector = IDSelector;

  ZeroMem(&Msg, sizeof(Msg));

  //
  // Install transport and control header
  //
  Status = MctpControlConstructHeader(
      DestinationEID,
      MctpGetOwnEndpointID(),
      0,
      MctpCmdGetMCTPVendorMessageSupport,
      &Msg);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  //
  // Install control body
  //
  Msg.Body.ControlMsg.Body.GetVendorDefinedMessageTypeReq = Req;

  Status = MctpTransportSendMessage(&Msg.Header,
    NULL,
    0,
    Msg.Body.Raw,
    sizeof(MCTP_CONTROL_MSG_HEADER) + sizeof(Req),
    TimeoutUsec
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Length = sizeof(Resp) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  Status = MctpControlAwaitResponse(
      &Msg.Header,
      TimeoutUsec,
      &Msg.Body,
      &Length
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (Length < sizeof(UINT8) * 2 + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }

  Resp = Msg.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp;
  if ((Resp.VendorID.VendorIDFormat == MCTP_VENDOR_ID_FORMAT_PCI) &&
     (Length < (sizeof(UINT8) * 2 + sizeof(MCTP_CONTROL_VENDOR_ID_PCI) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)))) {
    return EFI_PROTOCOL_ERROR;
  }
  if ((Resp.VendorID.VendorIDFormat == MCTP_VENDOR_ID_FORMAT_IANA) &&
     (Length < (sizeof(UINT8) * 2 + sizeof(MCTP_CONTROL_VENDOR_ID_IANA) + sizeof(MCTP_CONTROL_MSG_RESP_HEADER)))) {
    return EFI_PROTOCOL_ERROR;
  }
  *Entry = Resp.VendorID;

  return EFI_SUCCESS;
}
