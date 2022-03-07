
#include <Base.h>
#include <Uefi.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpTransportLib.h>
#include <Library/MctpPhysicalTransportLib.h>
#include <Library/BaseMemoryLib.h>

#include "Core.h"

/**  Fill the MCTP transport header and the control message header.

  @param  [in]   Msg               The received message to answer.
  @param  [in]   CompletionCode    The command completion code to insert into the header.
  @param  [out]  Response          Pointer to MCTP message to work on.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseConstructHeader(
  IN MCTP_MSG   *Msg,
  IN UINT8      CompletionCode,
  OUT MCTP_MSG  *Response
  )
{
  MCTP_CONTROL_MSG_HEADER       *Hdr;
  MCTP_CONTROL_MSG_RESP_HEADER  *ResponseHdr;

  if (Msg == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Hdr = &Msg->Body.ControlMsg.Header;
  ResponseHdr = &Response->Body.ControlResponseMsg.Header;

  CopyMem(ResponseHdr, Hdr, sizeof(MCTP_CONTROL_MSG_HEADER));
  ResponseHdr->Rq = 0;
  ResponseHdr->CompletionCode = CompletionCode;

  return EFI_SUCCESS;
}

/**  Converts the en EFI error to MCTP completition code.

  @param  [in]  CompletionCode   The completion code to decode.

  @retval EFI_SUCCESS            The completion code contains no error.
  @retval EFI_INVALID_PARAMETER  The completion code is unknown.
  @retval others                 See below.
**/
STATIC
UINT8
MctpResponseErrorToCompletionCode(
  IN EFI_STATUS Error
  )
{
  switch (Error) {
    case EFI_SUCCESS:
      return MCTP_SUCCESS;
    case EFI_DEVICE_ERROR:
      return MCTP_ERR;
    case EFI_INVALID_PARAMETER:
    case EFI_COMPROMISED_DATA:
      return MCTP_ERR_INVALID_DATA;
    case EFI_BAD_BUFFER_SIZE:
    case EFI_BUFFER_TOO_SMALL:
      return MCTP_ERR_INVALID_LEN;
    case EFI_NOT_READY:
      return MCTP_ERR_NOT_READY;
    case EFI_UNSUPPORTED:
      return MCTP_ERR_UNSUPPORTED;
    default:
      return MCTP_ERR;
  }
}

/**  Verify that a full SetEndpointReq message had been received.

  @param  [in]  Msg             The received message to verify.
  @param  [in]  BodyLength      The length of the control message body, without the 3 byte header.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_PROTOCOL_ERROR     The command was incomplete.
**/
STATIC
EFI_STATUS
MctpValidateSetEndpointID (
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength
  )
{
  if (BodyLength < sizeof(MCTP_CONTROL_SET_ENDPOINT_REQ_MSG)) {
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}

/**  This command should only be issued by a bus owner to assign
     an EID to an endpoint at a particular physical address.

  @param  [in]  Msg             The received message to answer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseSetEndpointID(
  IN MCTP_MSG *Msg,
  IN UINTN    TimeoutUsec
  )
{
  MCTP_CONTROL_SET_ENDPOINT_REQ_MSG  *Req;
  MCTP_CONTROL_SET_ENDPOINT_RESP_MSG Resp;
  MCTP_MSG                           RespMsg;
  UINTN                              Length;
  EFI_STATUS                         Status;
  UINT8                              CompletionCode;

  Req = &Msg->Body.ControlMsg.Body.SetEndpointReq;

  if (!MctpIsAssignableEndpointID(Req->EndpointID)) {
    Status = EFI_INVALID_PARAMETER;
  } else if (MctpIsBusOwner()) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    switch (Req->RequestData & 0x3) {
      case 0:
      case 1:
        //
        // Only support one BUS for now. Accept all requests.
        //
        MctpSetOwnEndpointID(Req->EndpointID);
        MctpSetBusOwnerEndpointID(Msg->Header.SourceEID);
      case 2:
        if (!MctpSupportsStaticEID()) {
          Status = EFI_INVALID_PARAMETER;
        } else {
          MctpResetEID();
        }
      break;
      case 3:
        Status = EFI_INVALID_PARAMETER;
      break;
    }
  }
  if (!EFI_ERROR(Status)) {
    Resp.Status = 0;
    Resp.EIDSetting = MctpGetOwnEndpointID();
    Resp.EIDPoolSize = 0;
  }

  CompletionCode = MctpResponseErrorToCompletionCode(Status);
  ZeroMem(&RespMsg, sizeof(RespMsg));

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &RespMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  if (CompletionCode == MCTP_SUCCESS) {
    //
    // Install control body
    //
    RespMsg.Body.ControlResponseMsg.Body.SetEndpointResp = Resp;
    Length += sizeof(Resp);
  }

  return MctpTransportSendMessage(&RespMsg.Header,
    NULL,
    0,
    RespMsg.Body.Raw,
    Length,
    TimeoutUsec
  );
}

/**  This command is typically issued only by a bus owner to retrieve the
     EID that was assigned to a particular physical address..

  @param  [in]  Msg             The received message to answer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseGetEndpointID(
  IN MCTP_MSG *Msg,
  IN UINTN    TimeoutUsec
  )
{
  MCTP_CONTROL_GET_ENDPOINT_RESP_MSG Resp;
  MCTP_MSG                           RespMsg;
  UINTN                              Length;
  EFI_STATUS                         Status;
  UINT8                              CompletionCode;

  Resp.EndpointID = MctpGetOwnEndpointID();
  Resp.EndpointType = 0;
  if (MctpIsBusOwner()) {
    Resp.EndpointType |= 0x10;
  }
  if (MctpSupportsStaticEID() && (MctpSGetStaticEID() != MctpGetOwnEndpointID())) {
    Resp.EndpointType |= 0x3;
  } else if (MctpSupportsStaticEID() && (MctpSGetStaticEID() == MctpGetOwnEndpointID())) {
    Resp.EndpointType |= 0x2;
  }

  Resp.MediumSpecific = MctpPhysicalGetEndpointInformation();

  CompletionCode = MCTP_SUCCESS;
  ZeroMem(&RespMsg, sizeof(RespMsg));

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &RespMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  if (CompletionCode == MCTP_SUCCESS) {
    //
    // Install control body
    //
    RespMsg.Body.ControlResponseMsg.Body.GetEndpointResp = Resp;
    Length += sizeof(Resp);
  }

  return MctpTransportSendMessage(&RespMsg.Header,
    NULL,
    0,
    RespMsg.Body.Raw,
    Length,
    TimeoutUsec
  );
}

/**  Verify that a full GetMctpVersionReq message had been received.

  @param  [in]  Msg             The received message to verify.
  @param  [in]  BodyLength      The length of the control message body, without the 3 byte header.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_PROTOCOL_ERROR     The command was incomplete.
**/
STATIC
EFI_STATUS
MctpValidateGetMCTPVersionSupport (
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength
  )
{
  if (BodyLength < sizeof(MCTP_CONTROL_GET_MCTP_VERSION_REQ_MSG)) {
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}

/**  This command can be used to retrieve the MCTP base specification versions
     that the endpoint supports, and also the message type specification versions 
     upported for each message type. 

  @param  [in]  Msg             The received message to answer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseGetMCTPVersionSupport(
  IN MCTP_MSG *Msg,
  IN UINTN    TimeoutUsec
  )
{
  MCTP_CONTROL_GET_MCTP_VERSION_RESP_MSG Resp;
  MCTP_CONTROL_GET_MCTP_VERSION_REQ_MSG  *Req;
  MCTP_MSG                               RespMsg;
  UINTN                                  Length;
  EFI_STATUS                             Status;
  UINT8                                  CompletionCode;

  Req = &Msg->Body.ControlMsg.Body.GetMctpVersionReq;

  switch (Req->MessageTypeNumber) {
    case 0x00:
    case 0xff:
      Resp.EntryCount = 1;
      Resp.Entry[0].Major = 0xF1;
      Resp.Entry[0].Minor = 0xF1;
      Resp.Entry[0].Update = 0xFF;
      Resp.Entry[0].Alpha = 0x00;
      CompletionCode = MCTP_SUCCESS;
      break;
    default:
    CompletionCode = MCTP_ERR_INVALID_MSG_TYPE;
  }

  ZeroMem(&RespMsg, sizeof(RespMsg));

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &RespMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  if (CompletionCode == MCTP_SUCCESS) {
    //
    // Install control body
    //
    RespMsg.Body.ControlResponseMsg.Body.GetMctpVersionResp = Resp;
    Length += sizeof(Resp.EntryCount) + Resp.EntryCount * sizeof(Resp.Entry[0]);
  }

  return MctpTransportSendMessage(&RespMsg.Header,
    NULL,
    0,
    RespMsg.Body.Raw,
    Length,
    TimeoutUsec
  );
}

/**  This command can be used to retrieve the MCTP base specification versions
     that the endpoint supports, and also the message type specification versions 
     upported for each message type. 

  @param  [in]  Msg             The received message to answer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseGetMCTPMessageTypeSupport(
  IN MCTP_MSG *Msg,
  IN UINTN    TimeoutUsec
  )
{
  MCTP_CONTROL_GET_MSG_TYPE_RESP_MSG Resp;
  MCTP_MSG                           RespMsg;
  UINTN                              Length;
  EFI_STATUS                         Status;
  UINT8                              CompletionCode;
  UINTN                              Index;

  for (Index = 0; Index < NumSupportedMessageTypes; Index++) {
    Resp.Entry[Index] = SupportedMessageTypes[Index];
  }
  Resp.MessageTypeCount = NumSupportedMessageTypes;

  CompletionCode = MCTP_SUCCESS;

  ZeroMem(&RespMsg, sizeof(RespMsg));

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &RespMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  if (CompletionCode == MCTP_SUCCESS) {
    //
    // Install control body
    //
    RespMsg.Body.ControlResponseMsg.Body.GetMessageTypeResp = Resp;
    Length += sizeof(Resp.MessageTypeCount) + Resp.MessageTypeCount * sizeof(Resp.Entry[0]);
  }

  return MctpTransportSendMessage(&RespMsg.Header,
    NULL,
    0,
    RespMsg.Body.Raw,
    Length,
    TimeoutUsec
  );
}

/**  Verify that a full GetVendorDefinedMessageTypeReq message had been received.

  @param  [in]  Msg             The received message to verify.
  @param  [in]  BodyLength      The length of the control message body, without the 3 byte header.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_PROTOCOL_ERROR     The command was incomplete.
**/
STATIC
EFI_STATUS
MctpValidateVendorDefinedMessageType (
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength
  )
{
  if (BodyLength < sizeof(MCTP_CONTROL_GET_VENDOR_MSG_TYPE_REQ_MSG)) {
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}

/**  The Get Vendor Defined Message Support operation enables management
     controllers to discover whether the endpoint supports vendor-defined messages.

  @param  [in]  Msg             The received message to answer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseVendorDefinedMessageType(
  IN MCTP_MSG *Msg,
  IN UINTN    TimeoutUsec
  )
{
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_RESP_MSG Resp;
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_REQ_MSG  *Req;
  MCTP_MSG                                  RespMsg;
  UINTN                                     Length;
  EFI_STATUS                                Status;
  UINT8                                     CompletionCode;

  Req = &Msg->Body.ControlMsg.Body.GetVendorDefinedMessageTypeReq;

  if (Req->VendorIDSelector < NumSupportedVendorDefinedMessages) {
    Resp.VendorID = SupportedVendorMessage[Req->VendorIDSelector];
    if ((Req->VendorIDSelector + 1) < NumSupportedVendorDefinedMessages) {
      Resp.VendorIDSelector = Req->VendorIDSelector + 1;
    } else {
      Resp.VendorIDSelector = 0xff;
    }
    CompletionCode = MCTP_SUCCESS;
  } else {
    CompletionCode = MCTP_ERR;
  }

  ZeroMem(&RespMsg, sizeof(RespMsg));

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &RespMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);
  if (CompletionCode == MCTP_SUCCESS) {
    //
    // Install control body
    //
    RespMsg.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp = Resp;
    Length += sizeof(Resp.VendorIDSelector);
  }

  return MctpTransportSendMessage(&RespMsg.Header,
    NULL,
    0,
    RespMsg.Body.Raw,
    Length,
    TimeoutUsec
  );
}

/**  Answeres the request that it's not supported.

  @param  [in]  Msg             The received message to answer.
  @param  [in]  CompletionCode  The completion code to transmit.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
STATIC
EFI_STATUS
MctpResponseUnsupported(
  IN MCTP_MSG *Msg,
  IN UINT8    CompletionCode,
  IN UINTN    TimeoutUsec
  )
{
  MCTP_MSG   RespMsg;
  UINTN      Length;
  EFI_STATUS Status;

  if (Msg == NULL) {
    return EFI_INVALID_PARAMETER;  
  }

  ZeroMem(&RespMsg, sizeof(RespMsg));
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &RespMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }
  return MctpTransportSendMessage(&RespMsg.Header,
    NULL,
    0,
    RespMsg.Body.Raw,
    Length,
    TimeoutUsec
  );
}

/**  Handle a received MCTP control message. Will automatically send an answer
     if the request is well formed, supported and contains valid data.

  @param  [in]  Msg             The received message to answer.
  @param  [in]  BodyLength      The length of the received message, including the MCTP control header.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
MctpHandleControlMsg(
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength,
  IN UINTN      TimeoutUsec
  )
{
  MCTP_CONTROL_MSG_HEADER *Hdr;
  EFI_STATUS              Status;

  if (Msg == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (BodyLength < sizeof(MCTP_CONTROL_MSG_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }

  Hdr = &Msg->Body.ControlMsg.Header;

  if (Hdr->IC == 1 || Hdr->Rq == 0) {
    return EFI_PROTOCOL_ERROR;
  }

  switch (Hdr->CommandCode) {
  case MctpCmdSetEndpointID:
    Status = MctpValidateSetEndpointID(Msg, BodyLength);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    Status = MctpResponseSetEndpointID(Msg, TimeoutUsec);
    break;
  case MctpCmdGetEndpointID:
    Status = MctpResponseGetEndpointID(Msg, TimeoutUsec);
    break;
  case MctpCmdGetMCTPVersionSupport:
    Status = MctpValidateGetMCTPVersionSupport(Msg, BodyLength);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    Status = MctpResponseGetMCTPVersionSupport(Msg, TimeoutUsec);
    break;
  case MctpCmdGetMCTPMessageTypeSupport:
    Status = MctpResponseGetMCTPMessageTypeSupport(Msg, TimeoutUsec);
    break;
  case MctpCmdGetMCTPVendorMessageSupport:
    Status = MctpValidateVendorDefinedMessageType(Msg, BodyLength);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    Status = MctpResponseVendorDefinedMessageType(Msg, TimeoutUsec);
    break;
  case MctpCmdReserved:
  default:
    Status = MctpResponseUnsupported(Msg, MCTP_ERR_UNSUPPORTED, TimeoutUsec);
  }
  return Status;
}
