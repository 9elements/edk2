
#include <Base.h>
#include <Uefi.h>
#include <IndustryStandard/Mctp.h>

EFI_STATUS
MCTPHandleMsg(
  IN MCTP_TRANSPORT_HEADER *Hdr,
  IN UINT32                *BufferLength,
  IN UINT8                 *Buffer
  );