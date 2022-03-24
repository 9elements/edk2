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
  IN MCTP_PHYSICAL_MSG  *Pkt
  )
{
  (VOID)Pkt;
}

STATIC
BOOLEAN
AstLpcPktBufValidatev1 (
  IN MCTP_PHYSICAL_MSG  *Pkt
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
  IN MCTP_PHYSICAL_MSG  *Pkt
  )
{
  UINT32  Code;
  UINT32  CrcOut;

  if (Pkt->Length + sizeof (CrcOut) > Pkt->Mtu) {
    DEBUG ((DEBUG_WARN, "CRC32 doesn't fit into packet\n"));
    return;
  }

  CrcOut = CalculateCrc32 (Pkt->Msg, Pkt->Length);

  Code = HostToBigEndian32 (CrcOut);
  DEBUG ((DEBUG_VERBOSE, "a: 0x%x\n", __FUNCTION__, Code));
  CopyMem (((VOID *)Pkt->Msg) + Pkt->Length, &Code, sizeof (Code));
  Pkt->Length += sizeof (Code);
}

STATIC
BOOLEAN
AstLpcPktBufValidatev3 (
  IN MCTP_PHYSICAL_MSG  *Pkt
  )
{
  UINT32  Code;
  VOID    *Check;
  UINT32  CrcOut;

  if (Pkt->Length < sizeof (Code)) {
    return FALSE;
  }

  CrcOut = CalculateCrc32 (Pkt->Msg, Pkt->Length);

  Code = BigEndian32ToHost (CrcOut);

  DEBUG ((DEBUG_VERBOSE, "a: 0x%x\n", __FUNCTION__, Code));
  Check        = (VOID *)Pkt->Msg + Pkt->Length - sizeof (Code);
  Pkt->Length -= sizeof (Code);
  return Check && !CompareMem (&Code, Check, 4);
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

STATIC CONST MCTP_ASTLPC_PROTOCOL  *mAstLpcProtocol;

EFI_STATUS
AstLpcSetNegotiatedProtocolVersion (
  IN UINTN  Index
  )
{
  ASSERT (Index < ARRAY_SIZE (AstLpcProtocolVersion));
  if (Index >= ARRAY_SIZE (AstLpcProtocolVersion)) {
    return EFI_INVALID_PARAMETER;
  }

  mAstLpcProtocol = &AstLpcProtocolVersion[Index];
  return EFI_SUCCESS;
}

CONST MCTP_ASTLPC_PROTOCOL *
AstLpcGetNegotiatedProtocol (
  VOID
  )
{
  return mAstLpcProtocol;
}
