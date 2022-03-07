#include <Base.h>
#include <Uefi.h>

#include "AspeedLPCMctpPhysicalTransportLib.h"

extern UINT8  mKcsStatus;

EFI_STATUS
MctpInitHost (
  OUT   MCTP_BINDING_ASTLPC  *Binding,
  IN    UINTN                Timeout
  )
{
  UINT8                StatusReg;
  EFI_STATUS           Status;
  MCTP_ASTLPC_MAP_HDR  Hdr;
  UINT16               VerMinBe = HostToBigEndian16 (ASTLPC_VER_MIN);
  UINT16               VerCurBe = HostToBigEndian16 (ASTLPC_VER_CUR);
  UINT16               BmcVerMin;
  UINT16               BmcVerCur;
  UINTN                Negotiated;
  UINTN                Loop;

  DEBUG ((DEBUG_INFO, "%a: Called...\n", __FUNCTION__));

  ZeroMem (Binding, sizeof (*Binding));

  Status = MctpKcsRead (MCTP_ASTLPC_KCS_REG_STATUS, &StatusReg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Binding->mKcsStatus = StatusReg;

  //
  // Test if remote MCTP daemon is running
  //
  if (!(StatusReg & KCS_STATUS_BMC_READY)) {
    return EFI_NOT_READY;
  }

  Status = MctpLpcRead (&Hdr, 0, sizeof (Hdr));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: LPC read failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  BmcVerMin = BigEndian16ToHost (Hdr.BmcVerMin);
  BmcVerCur = BigEndian16ToHost (Hdr.BmcVerCur);

  DEBUG ((DEBUG_INFO, "%a: BmcVerMin %04x BmcVerCur %04x\n", __FUNCTION__, BmcVerMin, BmcVerCur));

  /* Calculate the expected value of negotiated_ver */
  Negotiated = MctpNegotiateVersion (
                 BmcVerMin,
                 BmcVerCur,
                 ASTLPC_VER_MIN,
                 ASTLPC_VER_CUR
                 );
  if (!Negotiated) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot negotiate with invalid versions\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  /* Assign protocol ops so we can calculate the packet buffer sizes */
  AstLpcSetNegotiatedProtocolVersion (Binding, Negotiated);

  /* Negotiate packet buffers in v2 style if the BMC supports it */
  if (Negotiated >= 2) {
    DEBUG ((DEBUG_INFO, "%a: Negotiate packet buffers in v2 style ...\n", __FUNCTION__));
    Status = MctpNegotiateLayoutHost (Binding);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Negotiate layout host failed with: %r\n", __FUNCTION__, Status));
    }
  }

  /* Advertise the host's supported protocol versions */
  Status = MctpLpcWrite (
             &VerMinBe,
             OFFSET_OF (MCTP_ASTLPC_MAP_HDR, HostVerMin),
             sizeof (VerMinBe)
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: LPC write failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = MctpLpcWrite (
             &VerCurBe,
             OFFSET_OF (MCTP_ASTLPC_MAP_HDR, HostVerCur),
             sizeof (VerCurBe)
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: LPC write failed with: %r\n", __FUNCTION__, Status));
    return Status;
  }

  /* Send channel init command */
  Status = MctpKcsWrite (MCTP_ASTLPC_KCS_REG_DATA, 0x0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: KCS write failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Send channel init command\n", __FUNCTION__));

  /*
   * Configure the host so `astlpc->proto->version == 0` holds until we
   * receive a subsequent status update from the BMC. Until then,
   * `astlpc->proto->version == 0` indicates that we're yet to complete
   * the channel initialisation handshake.
   *
   * When the BMC provides a status update with KCS_STATUS_CHANNEL_ACTIVE
   * set we will assign the appropriate protocol ops struct in accordance
   * with `negotiated_ver`.
   */
  AstLpcSetNegotiatedProtocolVersion (Binding, ASTLPC_VER_BAD);

  for (Loop = 0; Loop < Timeout; Loop++) {
    Status = MctpChannelPoll (Binding);
    if (!EFI_ERROR (Status)) {
      break;
    }

    MicroSecondDelay (1);
  }

  return Status;
}
