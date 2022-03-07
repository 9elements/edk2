#include <Base.h>
#include <Uefi.h>

#include "AspeedLPCMctpPhysicalTransportLib.h"

VOID
MctpAstLpcInitChannel (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  UINT16               Negotiated;
  UINT16               NegotiatedBe;
  MCTP_ASTLPC_MAP_HDR  Hdr;
  UINT8                StatusReg;
  EFI_STATUS           Status;

  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));

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
  AstLpcSetNegotiatedProtocolVersion (Binding, Negotiated);

  /* Host Rx MTU negotiation: Failure terminates channel init */
  Status = MctpNegotiateLayoutBmc (Binding);
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

  Binding->mReadyToTx = !!(Status & KCS_STATUS_CHANNEL_ACTIVE);
}

EFI_STATUS
MctpAstLpcRxStart (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding,
  IN PUSH_POP_BUFFER            *Pkt,
  IN UINTN                      Timeout
  )
{
  UINT32                      Body, PacketLen;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;
  EFI_STATUS                  Status;

  AstLpcProtocol = Binding->mAstLpcProtocol;
  if (AstLpcProtocol == NULL) {
    return EFI_NOT_READY;
  }

  Status = MctpLpcRead (
             &Body,
             Binding->mAstLpcLayout.Rx.Offset,
             sizeof (Body)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Body = BigEndian32ToHost (Body);

  if (Body > AstLpcProtocol->BodySize (Binding->mAstLpcLayout.Rx.Size)) {
    DEBUG ((DEBUG_WARN, "invalid RX len 0x%x\n", Body));
    return EFI_BUFFER_TOO_SMALL;
  }

  ASSERT (Binding->mPktSize >= 0);
  if (Body > Binding->mPktSize) {
    DEBUG ((DEBUG_WARN, "invalid RX len 0x%x", Body));
    return EFI_BUFFER_TOO_SMALL;
  }

  /* Eliminate the medium-specific header that we just read */
  PacketLen = AstLpcProtocol->PacketSize (Body) - 4;
  if (PacketLen > BufferSize (Pkt)) {
    DEBUG ((DEBUG_WARN, "Not enough space in packet buffer: 0x%x > 0x%x\n", PacketLen, BufferSize (Pkt)));
    return EFI_BUFFER_TOO_SMALL;
  }

  BufferClear (Pkt);

  /*
   * Read payload and medium-specific trailer from immediately after the
   * medium-specific header.
   */
  Status = MctpLpcRead (
             BufferData (Pkt),
             Binding->mAstLpcLayout.Rx.Offset + 4,
             PacketLen
             );
  BufferUpdate (Pkt, PacketLen);

  /* Inform the other side of the MCTP interface that we have read
   * the packet off the bus before handling the contents of the packet.
   */
  Status = MctpKcsSend (0x2, 10000);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "%a: Failed to send KCS status.\n",
      __FUNCTION__
      ));
  }

  Binding->mReadyToRx = FALSE;

  /*
   * v3 will validate the CRC32 in the medium-specific trailer and adjust
   * the packet size accordingly. On older protocols validation is a no-op
   * that always returns true.
   */
  if (!EFI_ERROR (Status) && AstLpcProtocol->PktBufValidate (Pkt)) {
    Status = EFI_SUCCESS;
  } else {
    /* TODO: Drop any associated assembly */
    Status = EFI_CRC_ERROR;
    BufferClear (Pkt);
    DEBUG ((DEBUG_VERBOSE, "Dropped corrupt packet\n"));
  }

  return Status;
}

EFI_STATUS
MctpAstLpcFinaliseChannel (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  MCTP_ASTLPC_LAYOUT          Layout;
  UINT16                      Negotiated;
  EFI_STATUS                  Status;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  DEBUG ((DEBUG_VERBOSE, "%a called\n", __FUNCTION__));

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

  if ((Negotiated == ASTLPC_VER_BAD) || (Negotiated < ASTLPC_VER_MIN) ||
      (Negotiated > ASTLPC_VER_CUR))
  {
    DEBUG ((DEBUG_ERROR, "Failed to negotiate version, got: %u\n", Negotiated));
    return EFI_UNSUPPORTED;
  }

  AstLpcSetNegotiatedProtocolVersion (Binding, Negotiated);

  Status = MctpLayoutRead (&Layout);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!MctpLayoutValidate (Binding, &Layout)) {
    DEBUG ((DEBUG_ERROR, "BMC proposed invalid buffer parameters"));
    return EFI_UNSUPPORTED;
  }

  Binding->mAstLpcLayout = Layout;

  if (Negotiated >= 2) {
    AstLpcProtocol = Binding->mAstLpcProtocol;
    if (AstLpcProtocol == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to negotiate protocol\n", __FUNCTION__));
      return EFI_NOT_READY;
    }

    Binding->mPktSize = AstLpcProtocol->BodySize (Binding->mAstLpcLayout.Tx.Size);
    DEBUG ((DEBUG_INFO, "%a: mPktSize = %d\n", __FUNCTION__, Binding->mPktSize));
  }

  return EFI_SUCCESS;
}

UINTN
MctpAstLpcChannelMTU (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  if (Binding->mPktSize > sizeof (MCTP_TRANSPORT_HEADER)) {
    return Binding->mPktSize - sizeof (MCTP_TRANSPORT_HEADER);
  }

  return MCTP_BTU;
}

EFI_STATUS
MctpAstLpcUpdateChannel (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding,
  UINT8                         StatusReg
  )
{
  UINT8                       Updated;
  EFI_STATUS                  Status;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = Binding->mAstLpcProtocol;

  Status = EFI_SUCCESS;

  Updated = Binding->mKcsStatus ^ StatusReg;

  if (Updated & KCS_STATUS_BMC_READY) {
    if (StatusReg & KCS_STATUS_BMC_READY) {
      Binding->mKcsStatus = StatusReg;
      // return astlpc->binding.start (&astlpc->binding);
      return EFI_SUCCESS;
    } else {
      Binding->mReadyToTx = FALSE;
    }
  }

  if ((AstLpcProtocol->Version == 0) ||
      Updated & KCS_STATUS_CHANNEL_ACTIVE)
  {
    Status = MctpAstLpcFinaliseChannel (Binding);

    Binding->mReadyToTx = (StatusReg & KCS_STATUS_CHANNEL_ACTIVE) && !EFI_ERROR (Status);
  }

  AstLpcProtocol = Binding->mAstLpcProtocol;

  Binding->mKcsStatus = StatusReg;

  return Status;
}

EFI_STATUS
MctpAstLpcChannelTx (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding,
  IN PUSH_POP_BUFFER            *Pkt,
  IN UINTN                      Timeout
  )
{
  UINT32                      Len;
  UINT32                      LenBe;
  MCTP_TRANSPORT_HEADER       *Hdr;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;
  EFI_STATUS                  Status;

  AstLpcProtocol = Binding->mAstLpcProtocol;

  if (Binding->mReadyToTx == FALSE) {
    return EFI_NOT_READY;
  }

  Hdr = BufferData (Pkt);
  Len = BufferLength (Pkt);

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Transmitting 0x%x byte packet (Src %x, Dest %x, FlagsSeqTag %x)\n",
    __FUNCTION__,
    Len,
    Hdr->SourceEID,
    Hdr->DestEID,
    Hdr->FlagsSeqTag
    ));

  if (Len > AstLpcProtocol->BodySize (Binding->mAstLpcLayout.Tx.Size)) {
    DEBUG ((
      DEBUG_WARN,
      "%a: Invalid Tx len %x : %x\n",
      __FUNCTION__,
      Len,
      AstLpcProtocol->BodySize (Binding->mAstLpcLayout.Tx.Size)
      ));

    return EFI_BAD_BUFFER_SIZE;
  }

  LenBe = HostToBigEndian32 (Len);
  MctpLpcWrite (
    &LenBe,
    Binding->mAstLpcLayout.Tx.Offset,
    sizeof (LenBe)
    );

  AstLpcProtocol->PktBufProtect (Pkt);

  MctpLpcWrite (
    BufferData (Pkt),
    Binding->mAstLpcLayout.Tx.Offset + 4,
    BufferLength (Pkt)
    );

  Binding->mReadyToTx = FALSE;

  Status = MctpKcsSend (0x1, 10000);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "%a: Failed to send KCS status.\n",
      __FUNCTION__
      ));
  }

  return EFI_SUCCESS;
}

BOOLEAN
MctpChannelReadyToTx (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  if (Binding->mReadyToTx) {
    DEBUG ((DEBUG_VERBOSE, "%a: mReadyToTx = 0x%x\n", __FUNCTION__, Binding->mReadyToTx));
  }

  return Binding->mReadyToTx;
}

BOOLEAN
MctpChannelReadyToRx (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  if (Binding->mReadyToRx) {
    DEBUG ((DEBUG_VERBOSE, "%a: mReadyToRx = 0x%x\n", __FUNCTION__, Binding->mReadyToRx));
  }

  return Binding->mReadyToRx;
}

EFI_STATUS
MctpChannelPoll (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  UINT8       StatusReg;
  UINT8       DataReg;
  EFI_STATUS  Status;

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_STATUS, &StatusReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  //
  // Test if remote MCTP daemon is running
  //
  if (!(StatusReg & KCS_STATUS_BMC_READY)) {
    return EFI_NOT_READY;
  }

  if (!MctpKcsReadReady (StatusReg)) {
    return Status;
  }

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_DATA, &DataReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: DataReg: 0x%x\n", __FUNCTION__, DataReg));

  switch (DataReg) {
    case 0x0:
      MctpAstLpcInitChannel (Binding);
      break;
    case 0x1:
      Binding->mReadyToRx = TRUE;
      break;
    case 0x2:
      Binding->mReadyToTx = TRUE;
      break;
    case 0xff:
      Status = MctpAstLpcUpdateChannel (Binding, StatusReg);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to update channel: %r\n", __FUNCTION__, Status));
        return Status;
      }

      break;
    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown message 0x%x\n", __FUNCTION__, DataReg));
  }

  /* Handle silent loss of bmc-ready */
  if (!((StatusReg & KCS_STATUS_BMC_READY) && (DataReg == 0xff))) {
    return MctpAstLpcUpdateChannel (Binding, StatusReg);
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

  AstLpcProtocol = Binding->mAstLpcProtocol;

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
      MctpAstLpcInitChannel (Binding);
      break;
    case 0x1:
      MctpAstLpcRxStart ();
      break;
    case 0x2:
      MctpAstLpcTxComplete ();
      break;
    case 0xff:
      Status = MctpAstLpcFinaliseChannel (Binding);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      break;
    default:
      DEBUG ((DEBUG_WARN, "%a: Unknown message 0x%x\n", __FUNCTION__, DataReg));
  }

  /* Handle silent loss of bmc-ready */
  if (!(StatusReg & KCS_STATUS_BMC_READY && (DataReg == 0xff))) {
    return MctpAstLpcUpdateChannel (Binding, StatusReg);
  }

  return EFI_SUCCESS;
}

#endif
