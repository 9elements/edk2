/** @file
  Legacy Master Boot Record Format Definition.

Copyright (c) 2022, 9elements GmbH All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MCTP_H_
#define _MCTP_H_

#pragma pack(1)

// Baseline transmission unit
#define MCTP_BTU  64

// MCTP Message Types defined in DSP0239 1.6.0
#define MCTP_MAX_DEFINED_MESSAGE_TYPES  8

#define MCTP_TYPE_CONTROL_MSG   0x00
#define MCTP_TYPE_PLDM_MSG      0x01
#define MCTP_TYPE_NCSI_MSG      0x02
#define MCTP_TYPE_ETHERNET_MSG  0x03
#define MCTP_TYPE_NVME_MSG      0x04
#define MCTP_TYPE_SPDM_MSG      0x05
#define MCTP_TYPE_PCI_VENDOR    0x7E
#define MCTP_TYPE_IANA_VENDOR   0x7F

// MCTP defined Endpoint IDs
#define MCTP_EID_NULL       0x00
#define MCTP_EID_BROADCAST  0xff

// MCTP Control protocol

// MCTP error codes
#define MCTP_SUCCESS               0x00
#define MCTP_ERR                   0x01
#define MCTP_ERR_INVALID_DATA      0x02
#define MCTP_ERR_INVALID_LEN       0x03
#define MCTP_ERR_NOT_READY         0x04
#define MCTP_ERR_UNSUPPORTED       0x05
#define MCTP_ERR_INVALID_MSG_TYPE  0x80

enum MCTP_CMD_CODE {
  MctpCmdReserved = 0,
  MctpCmdSetEndpointID,
  MctpCmdGetEndpointID,
  MctpCmdGetEndpointUUID,
  MctpCmdGetMCTPVersionSupport,
  MctpCmdGetMCTPMessageTypeSupport,
  MctpCmdGetMCTPVendorMessageSupport,
  MctpCmdResolveEndpointID,
  MctpCmdAllocateEndpointID,
  MctpCmdRoutingInformationUpdate,
  MctpCmdGetRoutingTableEntries,
  MctpCmdPrepareForEndpointDiscovery,
  MctpCmdEndpointDiscovery,
  MctpCmdDiscoveryNotify,
  MctpCmdGetNetworkID,
  MctpCmdQueryHop,
  MctpCmdResolveUUID,
};

// Defined in DSP0236 v1.3.1
typedef struct {
  UINT8    ICMsgType;
} MCTP_MSG_TYPE;
#define MCTP_MSG_TYPE_IC    (1 << 7)
#define MCTP_MSG_TYPE_MASK  0x7f

typedef struct {
  UINT8    ICMsgType;
  UINT8    Rq         : 1;
  UINT8    D          : 1;
  UINT8    rsvd       : 1;
  UINT8    InstanceID : 5;
  UINT8    CommandCode;
} MCTP_CONTROL_MSG_HEADER;

typedef struct {
  UINT8    ICMsgType;
  UINT8    Rq         : 1;
  UINT8    D          : 1;
  UINT8    rsvd       : 1;
  UINT8    InstanceID : 5;
  UINT8    CommandCode;
  UINT8    CompletionCode;
} MCTP_CONTROL_MSG_RESP_HEADER;

typedef struct {
  UINT8    RequestData;
  UINT8    EndpointID;
} MCTP_CONTROL_SET_ENDPOINT_REQ_MSG;

typedef struct {
  UINT8    Status;
  UINT8    EIDSetting;
  UINT8    EIDPoolSize;
} MCTP_CONTROL_SET_ENDPOINT_RESP_MSG;

typedef struct {
  UINT8    EndpointID;
  UINT8    EndpointType;
  UINT8    MediumSpecific;
} MCTP_CONTROL_GET_ENDPOINT_RESP_MSG;

typedef struct {
  UINT8    MessageTypeNumber;
} MCTP_CONTROL_GET_MCTP_VERSION_REQ_MSG;

typedef struct {
  UINT8    Major;
  UINT8    Minor;
  UINT8    Update;
  CHAR8    Alpha;
} MCTP_CONTROL_VERSION_ENTRY;

typedef struct {
  UINT8                         EntryCount;
  MCTP_CONTROL_VERSION_ENTRY    Entry[4];
} MCTP_CONTROL_GET_MCTP_VERSION_RESP_MSG;

typedef struct {
  UINT8    MessageTypeCount;
  UINT8    Entry[MCTP_MAX_DEFINED_MESSAGE_TYPES];
} MCTP_CONTROL_GET_MSG_TYPE_RESP_MSG;

typedef struct {
  UINT8    VendorIDSelector;
} MCTP_CONTROL_GET_VENDOR_MSG_TYPE_REQ_MSG;

#define MCTP_VENDOR_ID_FORMAT_PCI   0
#define MCTP_VENDOR_ID_FORMAT_IANA  1

typedef struct {
  UINT16    PCIVendorID;
  UINT16    VendorBitField;
} MCTP_CONTROL_VENDOR_ID_PCI;

typedef struct {
  UINT32    IANAEnterpriseID;
  UINT16    VendorBitField;
} MCTP_CONTROL_VENDOR_ID_IANA;

typedef struct {
  UINT8    VendorIDFormat;
  union {
    MCTP_CONTROL_VENDOR_ID_PCI     Pci;
    MCTP_CONTROL_VENDOR_ID_IANA    Iana;
  };
} MCTP_CONTROL_VENDOR_ID;

typedef struct {
  UINT8                     VendorIDSelector;
  MCTP_CONTROL_VENDOR_ID    VendorID;
} MCTP_CONTROL_GET_VENDOR_MSG_TYPE_RESP_MSG;

typedef union {
  MCTP_CONTROL_SET_ENDPOINT_REQ_MSG           SetEndpointReq;
  MCTP_CONTROL_GET_MCTP_VERSION_REQ_MSG       GetMctpVersionReq;
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_REQ_MSG    GetVendorDefinedMessageTypeReq;
  UINT8                                       Raw[MCTP_BTU];
} MCTP_CONTROL_BODY;

typedef union {
  MCTP_CONTROL_SET_ENDPOINT_RESP_MSG           SetEndpointResp;
  MCTP_CONTROL_GET_ENDPOINT_RESP_MSG           GetEndpointResp;
  MCTP_CONTROL_GET_MCTP_VERSION_RESP_MSG       GetMctpVersionResp;
  MCTP_CONTROL_GET_MSG_TYPE_RESP_MSG           GetMessageTypeResp;
  MCTP_CONTROL_GET_VENDOR_MSG_TYPE_RESP_MSG    GetVendorDefinedMessageTypeResp;
  UINT8                                        Raw[MCTP_BTU];
} MCTP_CONTROL_RESP_BODY;

typedef struct {
  MCTP_CONTROL_MSG_HEADER    Header;
  MCTP_CONTROL_BODY          Body;
} MCTP_CONTROL_MSG;

typedef struct {
  MCTP_CONTROL_MSG_RESP_HEADER    Header;
  MCTP_CONTROL_RESP_BODY          Body;
} MCTP_CONTROL_RESP_MSG;

// Vendor Defined - PCI Message Format
typedef struct {
  UINT8     ICMsgType;
  UINT16    PCIVendorID;
} MCTP_VENDOR_PCI_MSG_HEADER;

typedef struct {
  MCTP_VENDOR_PCI_MSG_HEADER    Header;
  UINT8                         Body[MCTP_BTU - sizeof (MCTP_VENDOR_PCI_MSG_HEADER)];
} MCTP_VENDOR_PCI_MSG;

// Vendor Defined - IANA Message Format
typedef struct {
  UINT8     ICMsgType;
  UINT32    IANAEnterpriseID;
} MCTP_VENDOR_IANA_MSG_HEADER;

typedef struct {
  MCTP_VENDOR_IANA_MSG_HEADER    Header;
  UINT8                          Body[MCTP_BTU - sizeof (MCTP_VENDOR_IANA_MSG_HEADER)];
} MCTP_VENDOR_IANA_MSG;

// Defined in DSP0236 v1.3.1
typedef struct {
  UINT8    Hdr;
  UINT8    DestEID;
  UINT8    SourceEID;
  UINT8    FlagsSeqTag;
} MCTP_TRANSPORT_HEADER;

/* Definitions for FlagsSeqTag field */
#define MCTP_TRANSPORT_HDR_FLAG_SOM   (1 << 7)
#define MCTP_TRANSPORT_HDR_FLAG_EOM   (1 << 6)
#define MCTP_TRANSPORT_HDR_FLAG_TO    (1 << 3)
#define MCTP_TRANSPORT_HDR_TO_SHIFT   (3)
#define MCTP_TRANSPORT_HDR_TO_MASK    (1)
#define MCTP_TRANSPORT_HDR_SEQ_SHIFT  (4)
#define MCTP_TRANSPORT_HDR_SEQ_MASK   (0x3)
#define MCTP_TRANSPORT_HDR_TAG_SHIFT  (0)
#define MCTP_TRANSPORT_HDR_TAG_MASK   (0x7)

typedef union {
  MCTP_MSG_TYPE            Type;
  MCTP_CONTROL_MSG         ControlMsg;
  MCTP_CONTROL_RESP_MSG    ControlResponseMsg;
  MCTP_VENDOR_IANA_MSG     IanaVendorMsg;
  MCTP_VENDOR_PCI_MSG      PCIVendorMsg;
  UINT8                    Raw[MCTP_BTU];
} MCTP_BODY;

typedef struct {
  MCTP_TRANSPORT_HEADER    Header;
  MCTP_BODY                Body;
} MCTP_MSG;

#define MCTP_INFO_SIGNATURE  SIGNATURE_32 ('m', 'c', 't', 'p')
///
/// EFI MCTP Data Descriptor
///
typedef struct {
  UINT32        Signature;
  LIST_ENTRY    Link;
  ///
  /// Length in bytes of the data pointed to by DataBlock.
  /// Length is always smaller or equal to Size.
  ///
  UINT64        Length;
  ///
  /// Size in bytes of the buffer pointed to by DataBlock.
  ///
  UINT64        Size;
  ///
  /// Virtual address of the data block.
  ///
  VOID          *DataBlock;
} EFI_MCTP_DATA_DESCRIPTOR;
#define MCTP_INFO_FROM_LINK(a)  CR (a, EFI_MCTP_DATA_DESCRIPTOR, Link, MCTP_INFO_SIGNATURE)

#pragma pack()

#endif
