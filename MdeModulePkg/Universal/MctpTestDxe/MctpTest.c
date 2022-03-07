
#include <Base.h>
#include <Uefi.h>
#include <Library/MctpCoreLib.h>
#include <Library/MctpPCIVendorLib.h>
#include <Library/DebugLib.h>

EFI_STATUS
EFIAPI
MctpTestEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS               Status;
  MCTP_PCI_VENDOR_PROTOCOL Protocol;
  UINTN                    Index;
  UINTN                    Length;
  UINT8                    TxBuffer[128];
  UINT8                    RxBuffer[128];

  for (Index = 0; Index < sizeof(TxBuffer); Index++) {
    TxBuffer[Index] = Index;
  }

  Status = MctpVendorPCIConnect(9, 0xcafe, 0, 0, &Protocol);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  DEBUG ((DEBUG_INFO, "Connected to dest EID 1\n"));

  Status = MctpVendorPCISend(&Protocol, TxBuffer, sizeof(TxBuffer), 0);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  DEBUG ((DEBUG_INFO, "Send %d byte message\n", sizeof(TxBuffer)));

  Length = sizeof(RxBuffer);
  Status = MctpVendorPCIReceiveMsg(&Protocol, RxBuffer, &Length, 0);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  DEBUG ((DEBUG_INFO, "Received %d byte message\n", Length));

  for (Index = 0; Index < sizeof(RxBuffer); Index++) {
    if (TxBuffer[Index] != RxBuffer[Index]) {
      DEBUG ((DEBUG_INFO, "Loop back test failed at %d 0x%x != 0x%x\n",
          Index, TxBuffer[Index], RxBuffer[Index]));
      return EFI_PROTOCOL_ERROR;
    }
  }
  return EFI_SUCCESS;
}