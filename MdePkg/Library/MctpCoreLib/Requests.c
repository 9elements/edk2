
#include <Base.h>
#include <Uefi.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpTransportLib.h>
#include <Library/MctpPhysicalTransportLib.h>
#include <Library/BaseMemoryLib.h>

#include "Core.h"
#include "Requests.h"

/**  Fill the MCTP transport header and the control message header.

  @param  [in]   Msg               The received message to answer.
  @param  [in]   CompletionCode    The command completion code to insert into the header.
  @param  [out]  Response          Pointer to MCTP message to work on.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
MctpResponseConstructHeader(
  IN MCTP_MSG   *Msg,
  IN UINT8      CompletionCode,
  OUT MCTP_MSG  *Response
  )
{
  MCTP_CONTROL_MSG_HEADER       *ControlHdr;
  MCTP_CONTROL_MSG_RESP_HEADER  *ResponseHdr;

  if (Msg == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Response->Header.DestEID = Msg->Header.SourceEID;
  Response->Header.SourceEID = MctpGetOwnEndpointID();
  Response->Header.TO = 0;
  Response->Header.MsgTag = Msg->Header.MsgTag;

  ControlHdr = &Msg->Body.ControlMsg.Header;
  ResponseHdr = &Response->Body.ControlResponseMsg.Header;

  ResponseHdr->IC = 0;
  ResponseHdr->MsgType = MCTP_TYPE_CONTROL_MSG;
  ResponseHdr->Rq = 0;
  ResponseHdr->D = 0;
  ResponseHdr->rsvd = 0;
  ResponseHdr->InstanceID = ControlHdr->InstanceID;
  ResponseHdr->CommandCode = ControlHdr->CommandCode;
  ResponseHdr->CompletionCode = CompletionCode;

  return EFI_SUCCESS;
}

/**  Converts the en EFI error to MCTP completition code.

  @param  [in]  CompletionCode   The completion code to decode.
**/
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
    case EFI_INVALID_LANGUAGE:
      return MCTP_ERR_INVALID_MSG_TYPE;
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

  @param  [in]  ControlMsg      The received control message to answer.
  @param  [in]  SourceEID       The endpoint EID of the requester.
  @param  [in]  Length          The message body length to transmit. Doesn't include the MCTP response header.
  @param  [in]  Response        The message to transmit as answer.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
MctpResponseSetEndpointID(
  IN     MCTP_CONTROL_MSG  *ControlMsg,
  IN     UINT8             SourceEID,
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  )
{
  MCTP_CONTROL_SET_ENDPOINT_REQ_MSG  *Req;
  MCTP_CONTROL_SET_ENDPOINT_RESP_MSG Resp;
  EFI_STATUS                         Status;

  if (ControlMsg == NULL || Length == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Req = &ControlMsg->Body.SetEndpointReq;

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
        MctpSetBusOwnerEndpointID(SourceEID);
        Status = EFI_SUCCESS;
        break;
      case 2:
        if (!MctpSupportsStaticEID()) {
          Status = EFI_INVALID_PARAMETER;
        } else {
          MctpResetEID();
          Status = EFI_SUCCESS;
        }
      break;
      case 3:
        Status = EFI_INVALID_PARAMETER;
      break;
    }
  }
  if (!EFI_ERROR(Status)) {
    ZeroMem(&Resp, sizeof(Resp));

    Resp.Status = 0;
    Resp.EIDSetting = MctpGetOwnEndpointID();
    Resp.EIDPoolSize = 0;

    //
    // Install control body
    //
    Response->Body.ControlResponseMsg.Body.SetEndpointResp = Resp;
    *Length += sizeof(Resp);
  }

  return Status;
}

/**  This command is typically issued only by a bus owner to retrieve the
     EID that was assigned to a particular physical address..

  @param  [in]  Length          The message body length to transmit. Doesn't include the MCTP response header.
  @param  [in]  Response        The message to transmit as answer.

  @retval EFI_SUCCESS            The command is executed successfully.
**/
EFI_STATUS
MctpResponseGetEndpointID(
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  )
{
  MCTP_CONTROL_GET_ENDPOINT_RESP_MSG Resp;

  if (Length == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem(&Resp, sizeof(Resp));

  Resp.EndpointID = MctpGetOwnEndpointID();
  Resp.EndpointType = 0;
  if (MctpIsBusOwner()) {
    Resp.EndpointType |= 0x10;
  }
  if (MctpSupportsStaticEID() && (MctpGetStaticEID() != MctpGetOwnEndpointID())) {
    Resp.EndpointType |= 0x3;
  } else if (MctpSupportsStaticEID() && (MctpGetStaticEID() == MctpGetOwnEndpointID())) {
    Resp.EndpointType |= 0x2;
  }

  Resp.MediumSpecific = MctpPhysicalGetEndpointInformation();

  //
  // Install control body
  //
  Response->Body.ControlResponseMsg.Body.GetEndpointResp = Resp;
  *Length += sizeof(Resp);

  return EFI_SUCCESS;
}

/**  Verify that a full GetMctpVersionReq message had been received.

  @param  [in]  Msg             The received message to verify.
  @param  [in]  BodyLength      The length of the control message body, without the 3 byte header.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_PROTOCOL_ERROR     The command was incomplete.
**/
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

  @param  [in]  ControlMsg      The received control message to answer.
  @param  [in]  Length          The message body length to transmit. Doesn't include the MCTP response header.
  @param  [in]  Response        The message to transmit as answer.

  @retval EFI_SUCCESS           The command is executed successfully.
**/
EFI_STATUS
MctpResponseGetMCTPVersionSupport(
  IN     MCTP_CONTROL_MSG  *ControlMsg,
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  )
{
  MCTP_CONTROL_GET_MCTP_VERSION_RESP_MSG Resp;
  MCTP_CONTROL_GET_MCTP_VERSION_REQ_MSG  *Req;
  EFI_STATUS                             Status;

  if (ControlMsg == NULL || Length == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Req = &ControlMsg->Body.GetMctpVersionReq;

  ZeroMem(&Resp, sizeof(Resp));

  switch (Req->MessageTypeNumber) {
    case 0x00:
    case 0xff:
      Resp.EntryCount = 1;
      Resp.Entry[0].Major = 0xF1;
      Resp.Entry[0].Minor = 0xF1;
      Resp.Entry[0].Update = 0xFF;
      Resp.Entry[0].Alpha = 0x00;
      Status = EFI_SUCCESS;
      break;
    default:
      Status = EFI_INVALID_LANGUAGE;
  }

  if (!EFI_ERROR(Status)) {
    //
    // Install control body
    //
    Response->Body.ControlResponseMsg.Body.GetMctpVersionResp = Resp;
    *Length = sizeof(Resp.EntryCount) + Resp.EntryCount * sizeof(Resp.Entry[0]);
  }

  return Status;
}

/**  This command can be used to retrieve the MCTP base specification message types
     that the endpoint supports.

  @param  [in]  Length          The message body length to transmit. Doesn't include the MCTP response header.
  @param  [in]  Response        The message to transmit as answer.

  @retval EFI_SUCCESS           The command is executed successfully.
**/
EFI_STATUS
MctpResponseGetMCTPMessageTypeSupport(
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  )
{
  MCTP_CONTROL_GET_MSG_TYPE_RESP_MSG Resp;
  EFI_STATUS                         Status;
  UINTN                              Index;

  if (Length == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem(&Resp, sizeof(Resp));

  for (Index = 0; Index < NumSupportedMessageTypes; Index++) {
    Resp.Entry[Index] = SupportedMessageTypes[Index];
  }
  Resp.MessageTypeCount = NumSupportedMessageTypes;

  Status = EFI_SUCCESS;

  if (!EFI_ERROR(Status)) {
    //
    // Install control body
    //
    Response->Body.ControlResponseMsg.Body.GetMessageTypeResp = Resp;
    *Length += sizeof(Resp.MessageTypeCount) + Resp.MessageTypeCount * sizeof(Resp.Entry[0]);
  }

  return Status;
}

/**  Verify that a full GetVendorDefinedMessageTypeReq message had been received.

  @param  [in]  Msg             The received message to verify.
  @param  [in]  BodyLength      The length of the control message body, without the 3 byte header.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_PROTOCOL_ERROR     The command was incomplete.
**/
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

  @param  [in]  ControlMsg      The received control message to answer.
  @param  [in]  Length          The message body length to transmit. Doesn't include the MCTP response header.
  @param  [in]  Response        The message to transmit as answer.


  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The command has some invalid parameters.
**/
EFI_STATUS
MctpResponseVendorDefinedMessageType(
  IN     MCTP_CONTROL_MSG  *ControlMsg,
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  )
{
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_RESP_MSG Resp;
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_REQ_MSG  *Req;
  EFI_STATUS                                Status;

  if (ControlMsg == NULL || Length == NULL || Response == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Req = &ControlMsg->Body.GetVendorDefinedMessageTypeReq;

  ZeroMem(&Resp, sizeof(Resp));

  if (Req->VendorIDSelector < NumSupportedVendorDefinedMessages) {
    Resp.VendorID = SupportedVendorMessage[Req->VendorIDSelector];
    if ((Req->VendorIDSelector + 1) < NumSupportedVendorDefinedMessages) {
      Resp.VendorIDSelector = Req->VendorIDSelector + 1;
    } else {
      Resp.VendorIDSelector = 0xff;
    }
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
  }

  if (!EFI_ERROR(Status)) {
    //
    // Install control body
    //
    Response->Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp = Resp;
    *Length += sizeof(Resp.VendorIDSelector) + sizeof(Resp.VendorID.VendorIDFormat);

    if (Resp.VendorID.VendorIDFormat == MCTP_VENDOR_ID_FORMAT_PCI) {
      *Length += sizeof(MCTP_CONTROL_VENDOR_ID_PCI);
    } else {
      *Length += sizeof(MCTP_CONTROL_VENDOR_ID_IANA);
    }
  }

  return Status;
}

STATIC
EFI_STATUS
MctpIsValidControlRequest(
  IN MCTP_CONTROL_MSG_HEADER  *Hdr
  )
{
  if (Hdr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Hdr->MsgType != MCTP_TYPE_CONTROL_MSG) {
    return EFI_UNSUPPORTED;
  }

  if (Hdr->IC == 1) {
    return EFI_COMPROMISED_DATA;
  }

  if (Hdr->Rq == 1) {
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
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
  MCTP_MSG                ResponseMsg;
  UINT8                   CompletionCode;
  UINTN                   Length;

  if (Msg == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (BodyLength < sizeof(MCTP_CONTROL_MSG_HEADER)) {
    return EFI_PROTOCOL_ERROR;
  }

  Hdr = &Msg->Body.ControlMsg.Header;
  Status = MctpIsValidControlRequest(Hdr);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  ZeroMem(&ResponseMsg, sizeof(ResponseMsg));
  Length = sizeof(MCTP_CONTROL_MSG_RESP_HEADER);

  switch (Hdr->CommandCode) {
  case MctpCmdSetEndpointID:
    Status = MctpValidateSetEndpointID(Msg, BodyLength);
    if (EFI_ERROR(Status)) {
      break;
    }
    Status = MctpResponseSetEndpointID(&Msg->Body.ControlMsg, Msg->Header.SourceEID, &Length, &ResponseMsg);
    break;
  case MctpCmdGetEndpointID:
    Status = MctpResponseGetEndpointID(&Length, &ResponseMsg);
    break;
  case MctpCmdGetMCTPVersionSupport:
    Status = MctpValidateGetMCTPVersionSupport(Msg, BodyLength);
    if (EFI_ERROR(Status)) {
      break;
    }
    Status = MctpResponseGetMCTPVersionSupport(&Msg->Body.ControlMsg, &Length, &ResponseMsg);
    break;
  case MctpCmdGetMCTPMessageTypeSupport:
    Status = MctpResponseGetMCTPMessageTypeSupport(&Length, &ResponseMsg);
    break;
  case MctpCmdGetMCTPVendorMessageSupport:
    Status = MctpValidateVendorDefinedMessageType(Msg, BodyLength);
    if (EFI_ERROR(Status)) {
      break;
    }
    Status = MctpResponseVendorDefinedMessageType(&Msg->Body.ControlMsg, &Length, &ResponseMsg);
    break;
  case MctpCmdReserved:
  default:
    Status = EFI_UNSUPPORTED;
  }

  CompletionCode = MctpResponseErrorToCompletionCode(Status);

  //
  // Install transport and control header
  //
  Status = MctpResponseConstructHeader(
      Msg,
      CompletionCode,
      &ResponseMsg
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  return MctpTransportSendMessage(&ResponseMsg.Header,
    NULL,
    0,
    ResponseMsg.Body.Raw,
    Length,
    TimeoutUsec
  );

  return Status;
}
