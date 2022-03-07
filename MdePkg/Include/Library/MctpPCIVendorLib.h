
/** @file
  Provide services to MCTP Vendor PCI message channel.

Copyright (c) 2022 9elements GmbH All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MCTP_PCI_VENDOR_LIB_H__
#define __MCTP_PCI_VENDOR_LIB_H__

#include <IndustryStandard/Mctp.h>

typedef struct MCTP_PCI_VENDOR_PROTOCOL {
  UINT8    DestinationEID;
  UINT16   VendorID;
  BOOLEAN  Connected;
} MCTP_PCI_VENDOR_PROTOCOL;

/**  Receive a vendor PCI packet targeting this endpoint. Broadcast and Null EID
     packets are being ignored. The provided buffer must be at least the size of the
     data to be received. Vendor PCI packet with a different Vendor ID will be ignored.

  @param  [in]  This            The connection to operate on.
  @param  [in]  Buffer          The buffer where to place the data.
  @param  [in]  BufferLength    The length of the buffer. Reads in a single packet not bigger
                                than the specified size.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/

EFI_STATUS
MctpVendorPCIReceiveMsg(
  IN MCTP_PCI_VENDOR_PROTOCOL *This,
  OUT UINT8                  *Buffer,
  IN OUT UINTN               *BufferLength,
  IN UINTN                    TimeoutUsec
  );

/**  Send a vendor PCI paket using the specified connection.

  @param  [in]  This            The connection to operate on.
  @param  [in]  Buffer          The buffer where to read the data.
  @param  [in]  BufferLength    The length of the buffer.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval EFI_BAD_BUFFER_SIZE   The received message could not fit into the provided buffer.
  @retval other                 The message type is not supported or another error happened.
**/

EFI_STATUS
MctpVendorPCISend(
  IN MCTP_PCI_VENDOR_PROTOCOL *This,
  IN UINT8                    *Buffer,
  IN UINTN                    BufferLength,
  IN UINTN                    TimeoutUsec
  );

/**  Establish a connection to the remote machine.

  @param  [in]  DestinationEID  The Destination EID.
  @param  [in]  VendorID        The PCI VendorID to use for custom messages.
  @param  [in]  BitField        The vendor defined bitfield as returned by MCTP control messages.
  @param  [in]  TimeoutUsec     The timeout in microseconds for this command to take.

  @retval EFI_SUCCESS           The message type is supported.
  @retval other                 The message type is not supported or another error happened.
*/
EFI_STATUS
MctpVendorPCIConnect(
  IN  UINT8  DestinationEID,
  IN  UINT16 VendorID,
  IN  UINT16 BitField,
  IN  UINTN  TimeoutUsec,
  OUT MCTP_PCI_VENDOR_PROTOCOL *Protocol
  );

#endif