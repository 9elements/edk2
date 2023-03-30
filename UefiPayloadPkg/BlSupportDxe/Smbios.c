/** @file
  This driver will install SMBIOS tables provided by bootloader.

  Copyright (c) 2014 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BlSupportDxe.h"
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/Smbios.h>
#include <Guid/MemoryTypeInformation.h>
#include <Guid/SmmStoreInfoGuid.h>
/**

  Acquire the string associated with the Index from smbios structure and return it.
  The caller is responsible for free the string buffer.

  @param    OptionalStrStart  The start position to search the string
  @param    Index             The index of the string to extract
  @param    String            The string that is extracted

  @retval   EFI_SUCCESS       The function returns EFI_SUCCESS always.

**/
EFI_STATUS
GetOptionalStringByIndex (
  IN      CHAR8                   *OptionalStrStart,
  IN      UINT8                   Index,
  OUT     CHAR16                  **String
  )
{
  UINTN          StrSize;

  if (Index == 0) {
    *String = AllocateZeroPool (sizeof (CHAR16));
    return EFI_SUCCESS;
  }

  StrSize = 0;
  do {
    Index--;
    OptionalStrStart += StrSize;
    StrSize           = AsciiStrSize (OptionalStrStart);
  } while (OptionalStrStart[StrSize] != 0 && Index != 0);

  if ((Index != 0) || (StrSize == 1)) {
    //
    // Meet the end of strings set but Index is non-zero, or
    // Find an empty string
    //
    *String = L"";
  } else {
    *String = AllocatePool (StrSize * sizeof (CHAR16));
    AsciiStrToUnicodeStrS (OptionalStrStart, *String, StrSize);
  }

  return EFI_SUCCESS;
}

/**
  Returns the mainboard name.

  @param[out] Name              The mainboard name.
  @param[out] Manufacturer      The mainboard manufacturer name.

  @retval EFI_SUCCESS           The tables could successfully be installed.
  @retval other                 Some error occurs when installing SMBIOS tables.

**/
STATIC
EFI_STATUS
EFIAPI
BlDxeGetMainboardName(
  OUT CHAR16 **Name,
  OUT CHAR16 **Manufacturer
)
{
  EFI_STATUS                    Status;
  EFI_SMBIOS_HANDLE             SmbiosHandle;
  EFI_SMBIOS_PROTOCOL           *SmbiosProto;
  EFI_SMBIOS_TABLE_HEADER       *Record;
  SMBIOS_TABLE_TYPE1            *Type1Record;
  UINT8                         StrIndex;

  //
  // Locate Smbios protocol.
  //
  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&SmbiosProto);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate gEfiSmbiosProtocolGuid\n",
      __FUNCTION__));
    return Status;
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = SmbiosProto->GetNext (SmbiosProto, &SmbiosHandle, NULL, &Record, NULL);
  while (!EFI_ERROR(Status)) {

    if (Record->Type == SMBIOS_TYPE_SYSTEM_INFORMATION) {
      Type1Record = (SMBIOS_TABLE_TYPE1 *) Record;
      StrIndex = Type1Record->ProductName;
      GetOptionalStringByIndex ((CHAR8*)((UINT8*)Type1Record + Type1Record->Hdr.Length), StrIndex, Name);
      StrIndex = Type1Record->Manufacturer;
      GetOptionalStringByIndex ((CHAR8*)((UINT8*)Type1Record + Type1Record->Hdr.Length), StrIndex, Manufacturer);
      return EFI_SUCCESS;
    }
    Status = SmbiosProto->GetNext (SmbiosProto, &SmbiosHandle, NULL, &Record, NULL);
  }

  return EFI_NOT_FOUND;
}

/**
  Main entry for the bootloader SMBIOS support DXE module.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
BlDxeSmbiosEntryPoint (
  IN EFI_HANDLE              ImageHandle,
  IN EFI_SYSTEM_TABLE        *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_HOB_GUID_TYPE          *GuidHob;
  CHAR16                     *MainboardName;
  CHAR16                     *ManufacturerName;

  Status = EFI_SUCCESS;

  //
  // Find the system table information guid hob
  //
  GuidHob = GetFirstGuidHob (&gEfiSmmStoreInfoHobGuid);
  ASSERT (GuidHob != NULL);
  if (GuidHob == NULL) {
    return Status;
  }


  //
  // Install board specific NULL protocols
  //
  Status = BlDxeGetMainboardName(&MainboardName, &ManufacturerName);
  if (!EFI_ERROR(Status)) {
    DEBUG ((DEBUG_INFO, "Running on mainboard %s '%s'\n", ManufacturerName, MainboardName));
    if (StrCmp(ManufacturerName, L"Prodrive") == 0 &&
        StrCmp(MainboardName, L"Hermes CFL") == 0) {
      Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
        &gEfiProdriveHermesBoardNullGuid, NULL, NULL);
      ASSERT_EFI_ERROR (Status);
    }

    FreePool (MainboardName);
    FreePool (ManufacturerName);
  }

  //
  // Install board specific NULL protocols
  //
  Status = BlDxeGetMainboardName(&MainboardName, &ManufacturerName);
  if (!EFI_ERROR(Status)) {
    DEBUG ((DEBUG_INFO, "Running on mainboard %s '%s'\n", ManufacturerName, MainboardName));
    if (StrCmp(ManufacturerName, L"Prodrive") == 0 && 
        StrCmp(MainboardName, L"Hermes CFL") == 0) {
      Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
        &gEfiProdriveHermesBoardNullGuid, NULL, NULL);
      ASSERT_EFI_ERROR (Status);
    }

    FreePool (MainboardName);
    FreePool (ManufacturerName);
  }

  return EFI_SUCCESS;
}

