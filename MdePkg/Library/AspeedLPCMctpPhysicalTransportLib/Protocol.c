#include "AspeedLPCMctpPhysicalTransportLib.h"

STATIC
UINT32
AstLpcPacketSizev1 (
  IN UINT32  Body
  )
{
  ASSERT ((Body + 4) > Body);

  return Body + 4;
}

STATIC
UINT32
AstLpcBodySizev1 (
  IN UINT32  Packet
  )
{
  ASSERT ((Packet - 4) < Packet);

  return Packet - 4;
}

STATIC
VOID
AstLpcPktBufProtectv1 (
  IN PUSH_POP_BUFFER  *Pkt
  )
{
  (VOID)Pkt;
}

STATIC
BOOLEAN
AstLpcPktBufValidatev1 (
  IN PUSH_POP_BUFFER  *Pkt
  )
{
  (VOID)Pkt;
  return TRUE;
}

STATIC
UINT32
AstLpcPacketSizev3 (
  IN UINT32  Body
  )
{
  ASSERT ((Body + 4 + 4) > Body);

  return Body + 4 + 4;
}

STATIC
UINT32
AstLpcBodySizev3 (
  IN UINT32  Packet
  )
{
  ASSERT ((Packet - 4 - 4) < Packet);

  return Packet - 4 - 4;
}

STATIC
VOID
AstLpcPktBufProtectv3 (
  IN PUSH_POP_BUFFER  *Pkt
  )
{
  UINT32  Code;
  UINT32  CrcOut;

  if (BufferLength (Pkt) + sizeof (CrcOut) > BufferSize (Pkt)) {
    DEBUG ((DEBUG_WARN, "CRC32 doesn't fit into packet\n"));
    return;
  }

  CrcOut = Crc32 (BufferData (Pkt), BufferLength (Pkt));

  Code = HostToBigEndian32 (CrcOut);
  DEBUG ((DEBUG_VERBOSE, "%a: 0x%x\n", __FUNCTION__, Code));
  BufferPush (Pkt, &Code, sizeof (Code));
}

STATIC
BOOLEAN
AstLpcPktBufValidatev3 (
  IN PUSH_POP_BUFFER  *Pkt
  )
{
  UINT32      Code;
  UINT32      Check;
  UINT32      CrcOut;
  EFI_STATUS  Status;

  if (BufferLength (Pkt) < sizeof (Code)) {
    return FALSE;
  }

  CrcOut = Crc32 (BufferData (Pkt), BufferLength (Pkt) - sizeof (Code));

  Code = BigEndian32ToHost (CrcOut);

  Status = BufferPop (Pkt, &Check, sizeof (Check));
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Calced CRC 0x%x, CRC in message 0x%x\n", __FUNCTION__, Code, Check));

  return Code == Check;
}

STATIC CONST MCTP_ASTLPC_PROTOCOL  AstLpcProtocolVersion[] = {
  [0] = {
    .Version        = 0,
    .PacketSize     = NULL,
    .BodySize       = NULL,
    .PktBufProtect  = NULL,
    .PktBufValidate = NULL,
  },
  [1] = {
    .Version        = 1,
    .PacketSize     = AstLpcPacketSizev1,
    .BodySize       = AstLpcBodySizev1,
    .PktBufProtect  = AstLpcPktBufProtectv1,
    .PktBufValidate = AstLpcPktBufValidatev1,
  },
  [2] = {
    .Version        = 2,
    .PacketSize     = AstLpcPacketSizev1,
    .BodySize       = AstLpcBodySizev1,
    .PktBufProtect  = AstLpcPktBufProtectv1,
    .PktBufValidate = AstLpcPktBufValidatev1,
  },
  [3] = {
    .Version        = 3,
    .PacketSize     = AstLpcPacketSizev3,
    .BodySize       = AstLpcBodySizev3,
    .PktBufProtect  = AstLpcPktBufProtectv3,
    .PktBufValidate = AstLpcPktBufValidatev3,
  },
};

EFI_STATUS
AstLpcSetNegotiatedProtocolVersion (
  IN OUT    MCTP_BINDING_ASTLPC  *Binding,
  IN UINTN                       Index
  )
{
  ASSERT (Index < ARRAY_SIZE (AstLpcProtocolVersion));
  if ((Index >= ARRAY_SIZE (AstLpcProtocolVersion)) || (Binding == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: Version = %d\n", __FUNCTION__, Index));

  Binding->mAstLpcProtocol = &AstLpcProtocolVersion[Index];
  return EFI_SUCCESS;
}
