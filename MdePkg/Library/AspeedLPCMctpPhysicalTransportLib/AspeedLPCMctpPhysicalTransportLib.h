#include <Base.h>
#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>

#include <Library/MctpPhysicalTransportLib.h>

#if defined (__LITTLE_ENDIAN__) \
  || defined (__ARMEL__) \
  || defined (__THUMBEL__) \
  || defined (__AARCH64EL__) \
  || defined (__MIPSEL__) \
  || defined (__MIPSEL) \
  || defined (_MIPSEL) \
  || defined (__BFIN__) \
  || (defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))

#define HostToBigEndian32  SwapBytes32
#define HostToBigEndian16  SwapBytes16
#define BigEndian32ToHost  SwapBytes32
#define BigEndian16ToHost  SwapBytes16
#else
#define HostToBigEndian32(x)  x
#define HostToBigEndian16(x)  x
#define BigEndian32ToHost(x)  x
#define BigEndian16ToHost(x)  x
#endif

#define CONTROL_SIZE  0x100

#define ASTLPC_MCTP_MAGIC  0x4d435450
#define ASTLPC_VER_BAD     0
#define ASTLPC_VER_MIN     1

/* Support testing of new binding protocols */
#ifndef ASTLPC_VER_CUR
#define ASTLPC_VER_CUR  3
#endif

#pragma pack(1)

typedef struct  {
  UINT32    Magic;

  UINT16    BmcVerMin;
  UINT16    BmcVerCur;
  UINT16    HostVerMin;
  UINT16    HostVerCur;
  UINT16    NegotiatedVer;
  UINT16    Pad0;

  struct {
    UINT32    RxOffset;
    UINT32    RxSize;
    UINT32    TxOffset;
    UINT32    TxSize;
  } Layout;
} MCTP_ASTLPC_MAP_HDR;

#pragma pack()

typedef struct  {
  UINT32    Offset;
  UINT32    Size;
} MCTP_ASTLPC_BUFFER;

typedef struct {
  MCTP_ASTLPC_BUFFER    Rx;
  MCTP_ASTLPC_BUFFER    Tx;
} MCTP_ASTLPC_LAYOUT;

typedef struct {
  UINT16    Version;
  UINT32    (*PacketSize)(
    UINT32  Body
    );
  UINT32    (*BodySize)(
    UINT32  Packet
    );
  VOID        (*PktBufProtect)(
    MCTP_PHYSICAL_MSG  *Pkt
    );
  BOOLEAN        (*PktBufValidate)(
    MCTP_PHYSICAL_MSG  *Pkt
    );
} MCTP_ASTLPC_PROTOCOL;

enum mctp_binding_astlpc_kcs_reg {
  MCTP_ASTLPC_KCS_REG_DATA   = 0,
  MCTP_ASTLPC_KCS_REG_STATUS = 1,
};

#define KCS_STATUS_BMC_READY       0x80
#define KCS_STATUS_CHANNEL_ACTIVE  0x40
#define KCS_STATUS_IBF             0x02
#define KCS_STATUS_OBF             0x01

extern MCTP_ASTLPC_LAYOUT  mAstLpcLayout;
extern UINT32              mPktSize;

EFI_STATUS
MctpInitHost (
  VOID
  );

EFI_STATUS
AstLpcSetNegotiatedProtocolVersion (
  IN UINTN  Index
  );

CONST MCTP_ASTLPC_PROTOCOL *
AstLpcGetNegotiatedProtocol (
  VOID
  );

EFI_STATUS
MctpNegotiateLayoutHost (
  VOID
  );

EFI_STATUS
MctpNegotiateLayoutBmc (
  VOID
  );

EFI_STATUS
MctpLayoutRead (
  OUT MCTP_ASTLPC_LAYOUT  *Layout
  );

BOOLEAN
MctpLayoutValidate (
  IN CONST MCTP_ASTLPC_LAYOUT  *Layout
  );

EFI_STATUS
MctpLayoutWrite (
  IN MCTP_ASTLPC_LAYOUT  *Layout
  );

BOOLEAN
MctpBufferValidate (
  IN const MCTP_ASTLPC_BUFFER  *Buffer,
  IN const CHAR8               *Name
  );

BOOLEAN
MctpValidateVersion (
  IN UINT16  BmcVerMin,
  IN UINT16  BmcVerCur,
  IN UINT16  HostVerMin,
  IN UINT16  HostVerCur
  );

UINT16
MctpNegotiateVersion (
  IN UINT16  BmcVerMin,
  IN UINT16  BmcVerCur,
  IN UINT16  HostVerMin,
  IN UINT16  HostVerCur
  );

EFI_STATUS
MctpKcsRead (
  IN  UINT8  Offset,
  OUT UINT8  *Data
  );

EFI_STATUS
MctpKcsWrite (
  IN UINT8  Offset,
  IN UINT8  Data
  );

BOOLEAN
MctpKcsReadReady (
  IN UINT8  Status
  );

BOOLEAN
MctpKcsWriteReady (
  IN UINT8  Status
  );

EFI_STATUS
MctpKcsSend (
  IN UINT8  Data
  );

EFI_STATUS
MctpAstLpcKcsSetStatus (
  UINT8  Status
  );

EFI_STATUS
MctpLpcRead (
  OUT VOID   *Data,
  IN  UINTN  Offset,
  IN  UINTN  Length
  );

EFI_STATUS
MctpLpcWrite (
  IN VOID   *Data,
  IN UINTN  Offset,
  IN UINTN  Length
  );

EFI_STATUS
MctpChannelDataReg (
  UINT8  *DataReg
  );

EFI_STATUS
MctpAstLpcChannelTx (
  IN MCTP_PHYSICAL_MSG  *Pkt,
  IN UINTN              Timeout
  );

EFI_STATUS
MctpAstLpcRxStart (
  IN MCTP_PHYSICAL_MSG  *Pkt,
  IN UINTN              Timeout
  );

/* packet size */
#define MCTP_PACKET_SIZE(unit)  ((unit) + sizeof(MCTP_TRANSPORT_HEADER))
#define MCTP_BODY_SIZE(unit)    ((unit) - sizeof(MCTP_TRANSPORT_HEADER))
