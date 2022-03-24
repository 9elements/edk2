#include "AspeedLPCMctpPhysicalTransportLib.h"

BOOLEAN
MctpBufferValidate (
  IN const MCTP_ASTLPC_BUFFER  *Buffer,
  IN const CHAR8               *Name
  )
{
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();
  if (AstLpcProtocol == NULL) {
    return FALSE;
  }

  /* Check for overflow */
  if (Buffer->Offset + Buffer->Size < Buffer->Offset) {
    DEBUG ((
      DEBUG_ERROR,
      "%s packet buffer parameters overflow: offset: 0x%x"
      ", size: %x",
      Name,
      Buffer->Offset,
      Buffer->Size
      ));
    return FALSE;
  }

  /* Check that the buffers are contained within the allocated space */
  if (Buffer->Offset + Buffer->Size > PcdGet32 (PcdAspeedLPCWindowSize)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s packet buffer parameters exceed %uk window size: offset: 0x%x"
      ", size: %x",
      Name,
      (PcdGet32 (PcdAspeedLPCWindowSize) / 1024),
      Buffer->Offset,
      Buffer->Size
      ));
    return FALSE;
  }

  /* Check that the baseline transmission unit is supported */
  if (Buffer->Size < AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (MCTP_BTU))) {
    DEBUG ((
      DEBUG_ERROR,
      "%s packet buffer too small: Require %x bytes to support the %u byte baseline transmission unit, found %x",
      Name,
      AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (MCTP_BTU)),
      MCTP_BTU,
      Buffer->Size
      ));
    return FALSE;
  }

  /* Check for overlap with the control space */
  if (Buffer->Offset < CONTROL_SIZE) {
    DEBUG ((
      DEBUG_ERROR,
      "%s packet buffer overlaps control region {0x%x"
      ", %x}: Rx {0x%x, %x}",
      Name,
      0U,
      CONTROL_SIZE,
      Buffer->Offset,
      Buffer->Size
      ));
    return FALSE;
  }

  return TRUE;
}
