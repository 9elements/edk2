#include "AspeedLPCMctpPhysicalTransportLib.h"

MCTP_ASTLPC_LAYOUT  mAstLpcLayout;
UINT32              mRequestedMtu;
UINT32              mPktSize;

EFI_STATUS
MctpLayoutWrite (
  IN MCTP_ASTLPC_LAYOUT  *Layout
  )
{
  UINT32  RxSizeBe;

  /*
   * As of v2 we only need to write rx_size - the offsets are controlled
   * by the BMC, as is the BMC's rx_size (host tx_size).
   */
  RxSizeBe = HostToBigEndian32 (Layout->Rx.Size);
  return MctpLpcWrite (
           &RxSizeBe,
           OFFSET_OF (MCTP_ASTLPC_MAP_HDR, Layout.RxSize),
           sizeof (RxSizeBe)
           );
}

BOOLEAN
MctpLayoutValidate (
  IN CONST MCTP_ASTLPC_LAYOUT  *Layout
  )
{
  const MCTP_ASTLPC_BUFFER  *Rx = &Layout->Rx;
  const MCTP_ASTLPC_BUFFER  *Tx = &Layout->Tx;
  BOOLEAN                   RxValid, TxValid;

  RxValid = MctpBufferValidate (Rx, "Rx");
  TxValid = MctpBufferValidate (Tx, "Tx");

  if (!(RxValid && TxValid)) {
    return FALSE;
  }

  /* Check that the buffers are disjoint */
  if (((Rx->Offset <= Tx->Offset) && (Rx->Offset + Rx->Size > Tx->Offset)) ||
      ((Tx->Offset <= Rx->Offset) && (Tx->Offset + Tx->Size > Rx->Offset)))
  {
    DEBUG ((
      DEBUG_ERROR,
      "Rx and Tx packet buffers overlap: Rx {0x%x"
      ", %x}, Tx {0x%x, %x}",
      Rx->Offset,
      Rx->Size,
      Tx->Offset,
      Tx->Size
      ));
    return FALSE;
  }

  return TRUE;
}

EFI_STATUS
MctpLayoutRead (
  OUT MCTP_ASTLPC_LAYOUT  *Layout
  )
{
  MCTP_ASTLPC_MAP_HDR  Hdr;
  EFI_STATUS           Status;

  Status = MctpLpcRead (&Hdr, 0, sizeof (Hdr));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read from LPC: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Layout->Rx.Offset = BigEndian32ToHost (Hdr.Layout.RxOffset);
  Layout->Rx.Size   = BigEndian32ToHost (Hdr.Layout.RxSize);
  Layout->Tx.Offset = BigEndian32ToHost (Hdr.Layout.TxOffset);
  Layout->Tx.Size   = BigEndian32ToHost (Hdr.Layout.TxSize);

  return EFI_SUCCESS;
}

EFI_STATUS
MctpNegotiateLayoutHost (
  VOID
  )
{
  EFI_STATUS                  Status;
  MCTP_ASTLPC_LAYOUT          Layout;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();
  if (AstLpcProtocol == NULL) {
    return EFI_NOT_READY;
  }

  Status = MctpLayoutRead (&Layout);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!MctpLayoutValidate (&Layout)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: BMC provided invalid buffer layout: Rx {0x%x, %x}, Tx {0x%x, %x}",
      __FUNCTION__,
      Layout.Rx.Offset,
      Layout.Rx.Size,
      Layout.Tx.Offset,
      Layout.Tx.Size
      ));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "Desire an MTU of %u bytes", PcdGet32 (PcdMctpMtu)));

  Layout.Rx.Size = AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (PcdGet32 (PcdMctpMtu)));

  if (!MctpLayoutValidate (&Layout)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Generated invalid buffer layout with size: Rx {0x%x, %x}, Tx {0x%x, %x}",
      __FUNCTION__,
      Layout.Rx.Offset,
      Layout.Rx.Size,
      Layout.Tx.Offset,
      Layout.Tx.Size
      ));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "Requesting MTU of %u bytes", PcdGet32 (PcdMctpMtu)));

  return MctpLayoutWrite (&Layout);
}

STATIC
UINT32
MctpAstLpcCalculateMtu (
  IN MCTP_ASTLPC_LAYOUT          *Layout,
  IN CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol
  )
{
  UINT32  Low;
  UINT32  High;
  UINT32  Limit;
  UINT32  Rpkt;
  UINT32  Rmtu;

  /* Derive the largest MTU the BMC _can_ support */
  Low   = MIN (mAstLpcLayout.Rx.Offset, mAstLpcLayout.Tx.Offset);
  High  = MAX (mAstLpcLayout.Rx.Offset, mAstLpcLayout.Tx.Offset);
  Limit = High - Low;

  /* Determine the largest MTU the BMC _wants_ to support */
  if (mRequestedMtu) {
    Rmtu = mRequestedMtu;

    Rpkt  = AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (Rmtu));
    Limit = MIN (Limit, Rpkt);
  }

  /* Determine the accepted MTU, applied both directions by convention */
  Rpkt = MIN (Limit, mAstLpcLayout.Tx.Size);
  return MCTP_BODY_SIZE (AstLpcProtocol->BodySize (Rpkt));
}

EFI_STATUS
MctpNegotiateLayoutBmc (
  VOID
  )
{
  EFI_STATUS                  Status;
  MCTP_ASTLPC_LAYOUT          Proposed;
  MCTP_ASTLPC_LAYOUT          Pending;
  UINT32                      Mtu;
  UINT32                      Size;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = AstLpcGetNegotiatedProtocol ();

  /* Do we have a valid protocol version? */
  if ((AstLpcProtocol == NULL) || (AstLpcProtocol->Version == 0)) {
    return EFI_NOT_READY;
  }

  /* Extract the host's proposed layout */
  Status = MctpLayoutRead (&Proposed);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Do we have a reasonable layout? */
  if (!MctpLayoutValidate (&Proposed)) {
    return EFI_UNSUPPORTED;
  }

  /* Negotiate the MTU */
  Mtu  = MctpAstLpcCalculateMtu (&Proposed, AstLpcProtocol);
  Size = AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (Mtu));

  /*
   * Use symmetric MTUs by convention and to pass constraints in rx/tx
   * functions
   */
  Pending         = mAstLpcLayout;
  Pending.Tx.Size = Size;
  Pending.Rx.Size = Size;

  if (MctpLayoutValidate (&Pending)) {
    /* We found a sensible Rx MTU, so honour it */
    mAstLpcLayout = Pending;

    /* Enforce the negotiated MTU */
    Status = MctpLayoutWrite (&Pending);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    DEBUG ((DEBUG_INFO, "Negotiated an MTU of %x bytes\n", Mtu));
  } else {
    DEBUG ((DEBUG_WARN, "MTU negotiation failed\n"));
    return EFI_PROTOCOL_ERROR;
  }

  if (AstLpcProtocol->Version >= 2) {
    mPktSize = MCTP_PACKET_SIZE (Mtu);
  }

  return 0;
}
