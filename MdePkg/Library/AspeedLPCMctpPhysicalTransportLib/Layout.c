#include "AspeedLPCMctpPhysicalTransportLib.h"

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
  IN OUT   MCTP_BINDING_ASTLPC  *Binding,
  IN CONST MCTP_ASTLPC_LAYOUT   *Layout
  )
{
  const MCTP_ASTLPC_BUFFER  *Rx = &Layout->Rx;
  const MCTP_ASTLPC_BUFFER  *Tx = &Layout->Tx;
  BOOLEAN                   RxValid, TxValid;

  RxValid = MctpBufferValidate (Binding, Rx, "Rx");
  TxValid = MctpBufferValidate (Binding, Tx, "Tx");

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
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  EFI_STATUS                  Status;
  MCTP_ASTLPC_LAYOUT          Layout;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = Binding->mAstLpcProtocol;
  if (AstLpcProtocol == NULL) {
    return EFI_NOT_READY;
  }

  Status = MctpLayoutRead (&Layout);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!MctpLayoutValidate (Binding, &Layout)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: BMC provided invalid buffer layout: Rx {0x%x, %x}, Tx {0x%x, %x}\n",
      __FUNCTION__,
      Layout.Rx.Offset,
      Layout.Rx.Size,
      Layout.Tx.Offset,
      Layout.Tx.Size
      ));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "%a: Desire an MTU of %u bytes\n", __FUNCTION__, PcdGet32 (PcdMctpMtu)));

  Layout.Rx.Size = AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (PcdGet32 (PcdMctpMtu)));

  if (!MctpLayoutValidate (Binding, &Layout)) {
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

  DEBUG ((DEBUG_INFO, "%a: Requesting MTU of %u bytes\n", __FUNCTION__, PcdGet32 (PcdMctpMtu)));

  return MctpLayoutWrite (&Layout);
}

STATIC
UINT32
MctpAstLpcCalculateMtu (
  IN OUT   MCTP_BINDING_ASTLPC   *Binding,
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
  Low   = MIN (Binding->mAstLpcLayout.Rx.Offset, Binding->mAstLpcLayout.Tx.Offset);
  High  = MAX (Binding->mAstLpcLayout.Rx.Offset, Binding->mAstLpcLayout.Tx.Offset);
  Limit = High - Low;

  /* Determine the largest MTU the BMC _wants_ to support */
  if (Binding->mRequestedMtu) {
    Rmtu = Binding->mRequestedMtu;

    Rpkt  = AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (Rmtu));
    Limit = MIN (Limit, Rpkt);
  }

  /* Determine the accepted MTU, applied both directions by convention */
  Rpkt = MIN (Limit, Binding->mAstLpcLayout.Tx.Size);
  return MCTP_BODY_SIZE (AstLpcProtocol->BodySize (Rpkt));
}

EFI_STATUS
MctpNegotiateLayoutBmc (
  IN OUT   MCTP_BINDING_ASTLPC  *Binding
  )
{
  EFI_STATUS                  Status;
  MCTP_ASTLPC_LAYOUT          Proposed;
  MCTP_ASTLPC_LAYOUT          Pending;
  UINT32                      Mtu;
  UINT32                      Size;
  CONST MCTP_ASTLPC_PROTOCOL  *AstLpcProtocol;

  AstLpcProtocol = Binding->mAstLpcProtocol;

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
  if (!MctpLayoutValidate (Binding, &Proposed)) {
    return EFI_UNSUPPORTED;
  }

  /* Negotiate the MTU */
  Mtu  = MctpAstLpcCalculateMtu (Binding, &Proposed, AstLpcProtocol);
  Size = AstLpcProtocol->PacketSize (MCTP_PACKET_SIZE (Mtu));

  /*
   * Use symmetric MTUs by convention and to pass constraints in rx/tx
   * functions
   */
  Pending         = Binding->mAstLpcLayout;
  Pending.Tx.Size = Size;
  Pending.Rx.Size = Size;

  if (MctpLayoutValidate (Binding, &Pending)) {
    /* We found a sensible Rx MTU, so honour it */
    Binding->mAstLpcLayout = Pending;

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
    Binding->mPktSize = MCTP_PACKET_SIZE (Mtu);
  }

  return 0;
}
