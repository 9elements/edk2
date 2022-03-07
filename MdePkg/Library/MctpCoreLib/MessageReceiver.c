
#include <Base.h>
#include <Uefi.h>
#include "MCTPCommonLib.h"


EFI_STATUS
MctpHandleMsg(
  IN MCTP_TRANSPORT_HEADER *Hdr,
  IN UINT32                *BufferLength,
  IN UINT8                 *Buffer
  )
{
  MCTP_MSG_TYPE           Type;
  MCTP_CONTROL_MSG_HEADER CtrlHdr;
  UINT32                  VendorID;

  if (BufferLength == NULL || Buffer == NULL || Hdr == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (*BufferLength == 0) {
    return EFI_PROTOCOL_ERROR;
  }

  CopyMem(&Type, Buffer, sizeof(MCTP_MSG_TYPE));
  switch (Type.MsgType) {
    case MCTP_CONTROL_MSG:
      DEBUG(( DEBUG_INFO, "MCTP: Received control packet from 0x%x->0x%x\n", Hdr->SourceEID, Hdr->DestEID ));
      if (Type.IC == 1 || BufferLength < sizeof(MCTP_CONTROL_MSG_HEADER)) {
        return EFI_PROTOCOL_ERROR;
      }
      CopyMem(&CtrlHdr, Buffer, sizeof(MCTP_CONTROL_MSG_HEADER));
      return MCTPHandleControlMsg(&CtrlHdr, BufferLength - sizeof(MCTP_CONTROL_MSG_HEADER),
          Buffer + sizeof(MCTP_CONTROL_MSG_HEADER));
    break;
    case MCTP_PCI_VENDOR:
      DEBUG(( DEBUG_INFO, "MCTP: Received PCI vendor packet from 0x%x->0x%x\n", Hdr->SourceEID, Hdr->DestEID ));
      if (BufferLength < sizeof(UINT16)) {
        return EFI_PROTOCOL_ERROR;
      }
      CopyMem(&VendorID, Buffer, sizeof(UINT16));
      return MCTPHandleIANAVendorMsg(VendorID, BufferLength - sizeof(UINT16),
          Buffer + sizeof(UINT16));
    break;
    default:
      DEBUG(( DEBUG_INFO, "MCTP: Received unsupported packet type 0x%x, 0x%x->0x%x\n", Type,
          Hdr->SourceEID, Hdr->DestEID ));
    break;
  }
  
  return EFI_NOT_SUPPORTED;
}