/** @file

  Copyright (c) 2026, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/Smbios.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

RETURN_STATUS
EFIAPI
MpInitLibPlatformLibEntryPoint (
  VOID
  )
{
  RETURN_STATUS             Status;
  EFI_SMBIOS_PROTOCOL       *Smbios;
  EFI_SMBIOS_HANDLE         SmbiosHandle;
  EFI_SMBIOS_TABLE_HEADER   *Record;
  SMBIOS_TABLE_TYPE4        *CpuRecord;
  UINT32                    Count;
  UINT32                    LogicalProcessorCount;

  LogicalProcessorCount = 0;

  //
  // Find the SMBIOS protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiSmbiosProtocolGuid,
                  NULL,
                  (VOID **)&Smbios
                  );

  if (EFI_ERROR (Status)) {
    //
    // Should not happen due to DEPEX.
    //
    return Status;
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;

  //
  // Iterate all SMBIOS records to find the processor information and calculate
  // the logical processor count.
  //
  do {
    Status = Smbios->GetNext (Smbios, &SmbiosHandle, NULL, &Record, NULL);

    if (EFI_ERROR (Status) || SmbiosHandle == SMBIOS_HANDLE_PI_RESERVED) {
      break;
    }

    if (Record->Type != SMBIOS_TYPE_PROCESSOR_INFORMATION) {
      continue;
    }

    CpuRecord = (SMBIOS_TABLE_TYPE4 *)Record;
    if (CpuRecord->Hdr.Length <= 0x26) {
      /* SMBIOS 2.3 doesn't contain CPU cores */
      continue;
    }
    if (CpuRecord->Hdr.Length >= 0x32 && CpuRecord->ThreadEnabled > 0 &&
        CpuRecord->ThreadEnabled != 0xffff) {
      /* SMBIOS 3.6 */
      Count = CpuRecord->ThreadEnabled;
    } else if (CpuRecord->Hdr.Length >= 0x30 && CpuRecord->ThreadCount2 > 0 &&
                CpuRecord->ThreadCount2 != 0xffff) {
      /* SMBIOS 3.0 */
      Count = CpuRecord->ThreadCount2;
    } else if (CpuRecord->Hdr.Length >= 0x25 && CpuRecord->ThreadCount > 0 &&
                CpuRecord->ThreadCount != 0xff) {
      /* SMBIOS 2.5 */
      Count = CpuRecord->ThreadCount;
    } else if (CpuRecord->Hdr.Length >= 0x25 && CpuRecord->EnabledCoreCount > 0 &&
                CpuRecord->EnabledCoreCount != 0xff) {
      /* SMBIOS 2.5 */
      Count = CpuRecord->EnabledCoreCount;
    } else if (CpuRecord->Hdr.Length >= 0x2c && CpuRecord->CoreCount2 > 0 &&
                CpuRecord->CoreCount2 != 0xffff) {
      /* SMBIOS 3.0 */
      Count = CpuRecord->CoreCount2;
    } else {
      Count = 0;
    }
    LogicalProcessorCount += Count;
  } while (1);

  if (LogicalProcessorCount > 0) {
    //
    // Speed up MPinit by directly using the logical processor count from SMBIOS, instead of
    // detecting the logical processor count with timeout.
    //
    Status = PcdSet32S (PcdCpuBootLogicalProcessorNumber, LogicalProcessorCount);
    ASSERT_RETURN_ERROR (Status);

    Status = PcdSet32S (PcdCpuMaxLogicalProcessorNumber, LogicalProcessorCount);
    ASSERT_RETURN_ERROR (Status);
  }

  return EFI_SUCCESS;
}
