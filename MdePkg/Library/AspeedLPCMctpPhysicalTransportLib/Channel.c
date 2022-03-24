#include <Base.h>
#include <Uefi.h>

#include "AspeedLPCMctpPhysicalTransportLib.h"

UINT8    mKcsStatus;
BOOLEAN  mReadyToTx;

VOID
MctpAstLpcInitChannel (
  )
{
  UINT16               Negotiated;
  UINT16               NegotiatedBe;
  MCTP_ASTLPC_MAP_HDR  Hdr;
  UINT8                StatusReg;
  EFI_STATUS           Status;

  Status = MctpLpcRead (&Hdr, 0, sizeof (Hdr));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read from LPC: %r\n", __FUNCTION__, Status));
    return;
  }

  /* Version negotiation */
  Negotiated =
    MctpNegotiateVersion (
      ASTLPC_VER_MIN,
      ASTLPC_VER_CUR,
      BigEndian16ToHost (Hdr.HostVerMin),
      BigEndian16ToHost (Hdr.HostVerCur)
      );

  /* MTU negotiation requires knowing which protocol we'll use */
  AstLpcSetNegotiatedProtocolVersion (Negotiated);

  /* Host Rx MTU negotiation: Failure terminates channel init */
  Status = MctpNegotiateLayoutBmc ();
  if (EFI_ERROR (Status)) {
    Negotiated = ASTLPC_VER_BAD;
  }

  /* Populate the negotiated version */
  NegotiatedBe = HostToBigEndian16 (Negotiated);
  Status       = MctpLpcWrite (
                   &NegotiatedBe,
                   OFFSET_OF (MCTP_ASTLPC_MAP_HDR, NegotiatedVer),
                   sizeof (NegotiatedBe)
                   );

  /* Finalise the configuration */
  StatusReg = KCS_STATUS_BMC_READY | KCS_STATUS_OBF;
  if (Negotiated > 0) {
    DEBUG ((DEBUG_INFO, "Negotiated binding version 0x%x\n", Negotiated));
    StatusReg |= KCS_STATUS_CHANNEL_ACTIVE;
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to initialise channel\n"));
  }

  MctpAstLpcKcsSetStatus (StatusReg);

  mReadyToTx = !!(Status & KCS_STATUS_CHANNEL_ACTIVE);
}

EFI_STATUS
MctpAstLpcRxStart (
  IN MCTP_PHYSICAL_MSG  *Pkt,
  IN UINTN              Timeout
  )
{
  UINT32                      Body, Packet;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;
  EFI_STATUS                  Status;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();
  if (AstLpcProtocol == NULL) {
    return EFI_NOT_READY;
  }

  Status = MctpLpcRead (
             &Body,
             mAstLpcLayout.Rx.Offset,
             sizeof (Body)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Body = BigEndian32ToHost (Body);

  if (Body > AstLpcProtocol->BodySize (mAstLpcLayout.Rx.Size)) {
    DEBUG ((DEBUG_WARN, "invalid RX len 0x%x\n", Body));
    return EFI_BUFFER_TOO_SMALL;
  }

  ASSERT (mPktSize >= 0);
  if (Body > mPktSize) {
    DEBUG ((DEBUG_WARN, "invalid RX len 0x%x", Body));
    return EFI_BUFFER_TOO_SMALL;
  }

  /* Eliminate the medium-specific header that we just read */
  Packet = AstLpcProtocol->PacketSize (Body) - 4;
  if (Packet > Pkt->Mtu) {
    DEBUG ((DEBUG_WARN, "Not enough space in packet buffer: 0x%x > 0x%x\n", Packet, Pkt->Mtu));
    return EFI_BUFFER_TOO_SMALL;
  }

  /*
   * Read payload and medium-specific trailer from immediately after the
   * medium-specific header.
   */
  Status = MctpLpcRead (
             Pkt->Msg,
             mAstLpcLayout.Rx.Offset + 4,
             Packet
             );

  /* Inform the other side of the MCTP interface that we have read
   * the packet off the bus before handling the contents of the packet.
   */
  MctpKcsSend (0x2);

  /*
   * v3 will validate the CRC32 in the medium-specific trailer and adjust
   * the packet size accordingly. On older protocols validation is a no-op
   * that always returns true.
   */
  if (!EFI_ERROR (Status) && AstLpcProtocol->PktBufValidate (Pkt)) {
    Status      = EFI_SUCCESS;
    Pkt->Length = Packet;
  } else {
    /* TODO: Drop any associated assembly */
    Status = EFI_CRC_ERROR;
    DEBUG ((DEBUG_VERBOSE, "Dropped corrupt packet\n"));
  }

  return Status;
}

VOID
MctpAstLpcTxComplete (
  VOID
  )
{
  mReadyToTx = TRUE;
}

EFI_STATUS
MctpAstLpcFinaliseChannel (
  VOID
  )
{
  MCTP_ASTLPC_LAYOUT          Layout;
  UINT16                      Negotiated;
  EFI_STATUS                  Status;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  Status = MctpLpcRead (
             &Negotiated,
             OFFSET_OF (
               MCTP_ASTLPC_MAP_HDR,
               NegotiatedVer
               ),
             sizeof (Negotiated)
             );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Negotiated = BigEndian16ToHost (Negotiated);
  DEBUG ((DEBUG_INFO, "Version negotiation got: %u", Negotiated));

  if ((Negotiated == ASTLPC_VER_BAD) || (Negotiated < ASTLPC_VER_MIN) ||
      (Negotiated > ASTLPC_VER_CUR))
  {
    DEBUG ((DEBUG_ERROR, "Failed to negotiate version, got: %u", Negotiated));
    return EFI_UNSUPPORTED;
  }

  AstLpcSetNegotiatedProtocolVersion (Negotiated);

  Status = MctpLayoutRead (&Layout);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!MctpLayoutValidate (&Layout)) {
    DEBUG ((DEBUG_ERROR, "BMC proposed invalid buffer parameters"));
    return EFI_UNSUPPORTED;
  }

  mAstLpcLayout = Layout;

  if (Negotiated >= 2) {
    AstLpcProtocol = AstLpcGetNegotiatedProtocol ();
    if (AstLpcProtocol == NULL) {
      return EFI_NOT_READY;
    }

    mPktSize = AstLpcProtocol->BodySize (mAstLpcLayout.Tx.Size);
  }

  return 0;
}

EFI_STATUS
MctpAstLpcUpdateChannel (
  UINT8  StatusReg
  )
{
  UINT8                       Updated;
  EFI_STATUS                  Status;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();

  Status = EFI_SUCCESS;

  Updated = mKcsStatus ^ StatusReg;

  DEBUG ((DEBUG_VERBOSE, "%a: Status: 0x%x, Update: 0x%x\n", __FUNCTION__, StatusReg, Updated));

  if (Updated & KCS_STATUS_BMC_READY) {
    if (StatusReg & KCS_STATUS_BMC_READY) {
      mKcsStatus = StatusReg;
      // return astlpc->binding.start (&astlpc->binding);
      return EFI_SUCCESS;
    } else {
      mReadyToTx = FALSE;
    }
  }

  if ((AstLpcProtocol->Version == 0) ||
      Updated & KCS_STATUS_CHANNEL_ACTIVE)
  {
    Status     = MctpAstLpcFinaliseChannel ();
    mReadyToTx = (Status & KCS_STATUS_CHANNEL_ACTIVE) && !EFI_ERROR (Status);
  }

  mKcsStatus = StatusReg;

  return Status;
}

EFI_STATUS
MctpAstLpcChannelTx (
  IN MCTP_PHYSICAL_MSG  *Pkt,
  IN UINTN              Timeout
  )
{
  UINT32                      Len;
  UINT32                      LenBe;
  MCTP_TRANSPORT_HEADER       *Hdr;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();

  if (mReadyToTx == FALSE) {
    return EFI_NOT_READY;
  }

  Hdr = &Pkt->Msg->Header;
  Len = Pkt->Length;

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Transmitting 0x%x byte packet (%x,%x,%x)\n",
    __FUNCTION__,
    Len,
    Hdr->SourceEID,
    Hdr->DestEID,
    Hdr->MsgTag
    ));

  if (Len > AstLpcProtocol->BodySize (mAstLpcLayout.Tx.Size)) {
    DEBUG ((
      DEBUG_WARN,
      "%a: Invalid Tx len %x : %x\n",
      __FUNCTION__,
      Len,
      AstLpcProtocol->BodySize (mAstLpcLayout.Tx.Size)
      ));

    return EFI_BAD_BUFFER_SIZE;
  }

  LenBe = HostToBigEndian32 (Len);
  MctpLpcWrite (
    &LenBe,
    mAstLpcLayout.Tx.Offset,
    sizeof (LenBe)
    );

  AstLpcProtocol->PktBufProtect (Pkt);
  Len = Pkt->Length;

  MctpLpcWrite (
    Pkt->Msg,
    mAstLpcLayout.Tx.Offset + 4,
    Len
    );

  mReadyToTx = FALSE;

  MctpKcsSend (0x1);

  return 0;
}

EFI_STATUS
MctpChannelDataReg (
  UINT8  *DataReg
  )
{
  UINT8       StatusReg;
  EFI_STATUS  Status;

  if (DataReg == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_STATUS, &StatusReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Status: 0x%x\n", __FUNCTION__, Status));

  if (!MctpKcsReadReady (Status)) {
    return Status;
  }

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_DATA, DataReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  switch (*DataReg) {
    case 0x0:
      MctpAstLpcInitChannel ();
      break;
    case 0x1:
      // Handled by caller
      break;
    case 0x2:
      // Handled by caller
      break;
    case 0xff:
      Status = MctpAstLpcFinaliseChannel ();
      if (EFI_ERROR (Status)) {
        return Status;
      }

      break;
    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown message 0x%x\n", __FUNCTION__, *DataReg));
  }

  /* Handle silent loss of bmc-ready */
  if (!(StatusReg & KCS_STATUS_BMC_READY && (*DataReg == 0xff))) {
    return MctpAstLpcUpdateChannel (StatusReg);
  }

  return EFI_SUCCESS;
}

#if 0
EFI_STATUS
MctpPoll (
  VOID
  )
{
  UINT8                       StatusReg;
  UINT8                       DataReg;
  EFI_STATUS                  Status;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_STATUS, &StatusReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Status: 0x%x\n", __FUNCTION__, Status));

  if (!MctpKcsReadReady (Status)) {
    return EFI_NOT_READY;
  }

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_DATA, &DataReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Data: 0x%x\n", __FUNCTION__, DataReg));

  if ((AstLpcProtocol == NULL) || (AstLpcProtocol->Version == 0) && !((DataReg == 0x0) || (DataReg == 0xff))) {
    DEBUG ((
      DEBUG_WARN,
      "%a: Invalid message for binding state: 0x%x\n",
      __FUNCTION__,
      DataReg
      ));
    return EFI_PROTOCOL_ERROR;
  }

  switch (DataReg) {
    case 0x0:
      MctpAstLpcInitChannel ();
      break;
    case 0x1:
      MctpAstLpcRxStart ();
      break;
    case 0x2:
      MctpAstLpcTxComplete ();
      break;
    case 0xff:
      Status = MctpAstLpcFinaliseChannel ();
      if (EFI_ERROR (Status)) {
        return Status;
      }

      break;
    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown message 0x%x\n", __FUNCTION__, DataReg));
  }

  /* Handle silent loss of bmc-ready */
  if (!(StatusReg & KCS_STATUS_BMC_READY && (DataReg == 0xff))) {
    return MctpAstLpcUpdateChannel (StatusReg);
  }

  return EFI_SUCCESS;
}

#endif
