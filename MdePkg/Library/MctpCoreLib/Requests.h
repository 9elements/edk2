
#include <Base.h>
#include <Uefi.h>
#include <IndustryStandard/Mctp.h>

EFI_STATUS
MctpResponseConstructHeader(
  IN MCTP_MSG   *Msg,
  IN UINT8      CompletionCode,
  OUT MCTP_MSG  *Response
  );

UINT8
MctpResponseErrorToCompletionCode(
  IN EFI_STATUS Error
  );

EFI_STATUS
MctpValidateSetEndpointID (
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength
  );

EFI_STATUS
MctpResponseSetEndpointID(
  IN     MCTP_CONTROL_MSG  *ControlMsg,
  IN     UINT8             SourceEID,
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  );

EFI_STATUS
MctpResponseGetEndpointID(
  OUT    UINTN        *Length,
  IN OUT MCTP_MSG     *Response
  );

EFI_STATUS
MctpValidateGetMCTPVersionSupport (
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength
  );

EFI_STATUS
MctpResponseGetMCTPVersionSupport(
  IN     MCTP_CONTROL_MSG  *ControlMsg,
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  );

EFI_STATUS
MctpResponseGetMCTPMessageTypeSupport(
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  );

EFI_STATUS
MctpResponseVendorDefinedMessageType(
  IN     MCTP_CONTROL_MSG  *ControlMsg,
  OUT    UINTN             *Length,
  IN OUT MCTP_MSG          *Response
  );

EFI_STATUS
MctpHandleControlMsg(
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength,
  IN UINTN      TimeoutUsec
  );