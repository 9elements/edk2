#include "AspeedLPCMctpPhysicalTransportLib.h"

BOOLEAN
MctpValidateVersion (
  IN UINT16  BmcVerMin,
  IN UINT16  BmcVerCur,
  IN UINT16  HostVerMin,
  IN UINT16  HostVerCur
  )
{
  if (!(BmcVerMin && BmcVerCur && HostVerMin && HostVerCur)) {
    DEBUG ((
      DEBUG_ERROR,
      "Invalid version present in [%x, %x], [%x, %x]",
      BmcVerMin,
      BmcVerCur,
      HostVerMin,
      HostVerCur
      ));
    return FALSE;
  } else if (BmcVerMin > BmcVerCur) {
    DEBUG ((
      DEBUG_ERROR,
      "Invalid bmc version range [%x, %x]",
      BmcVerMin,
      BmcVerCur
      ));
    return FALSE;
  } else if (HostVerMin > HostVerCur) {
    DEBUG ((
      DEBUG_ERROR,
      "Invalid host version range [%x, %x]",
      HostVerMin,
      HostVerCur
      ));
    return FALSE;
  } else if ((HostVerCur < BmcVerMin) ||
             (HostVerMin > BmcVerCur))
  {
    DEBUG ((
      DEBUG_ERROR,
      "Unable to satisfy version negotiation with ranges [%x, %x] and [%x, %x]",
      BmcVerMin,
      BmcVerCur,
      HostVerMin,
      HostVerCur
      ));
    return FALSE;
  }

  return TRUE;
}

UINT16
MctpNegotiateVersion (
  IN UINT16  BmcVerMin,
  IN UINT16  BmcVerCur,
  IN UINT16  HostVerMin,
  IN UINT16  HostVerCur
  )
{
  if (!MctpValidateVersion (
         BmcVerMin,
         BmcVerCur,
         HostVerMin,
         HostVerCur
         ))
  {
    return ASTLPC_VER_BAD;
  }

  if (BmcVerCur < HostVerCur) {
    return BmcVerCur;
  }

  return HostVerCur;
}
