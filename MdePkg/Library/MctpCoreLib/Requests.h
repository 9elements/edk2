
#include <Base.h>
#include <Uefi.h>
#include <IndustryStandard/Mctp.h>

EFI_STATUS
MctpHandleControlMsg(
  IN MCTP_MSG  *Msg,
  IN UINTN      BodyLength,
  IN UINTN      TimeoutUsec
  );