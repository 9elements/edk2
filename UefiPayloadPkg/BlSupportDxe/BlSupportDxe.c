/** @file
  This driver will report some MMIO/IO resources to dxe core, extract smbios and acpi
  tables from bootloader.

  Copyright (c) 2014 - 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "BlSupportDxe.h"
#include <Guid/TpmInstance.h>


/**
  Main entry for the bootloader support DXE module.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
BlDxeEntryPoint (
  IN EFI_HANDLE              ImageHandle,
  IN EFI_SYSTEM_TABLE        *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_HOB_GUID_TYPE          *GuidHob;
  SYSTEM_TABLE_INFO          *SystemTableInfo;
  EFI_PEI_GRAPHICS_INFO_HOB  *GfxInfo;
  ACPI_BOARD_INFO            *AcpiBoardInfo;
  UINTN                      Size;

  Status = EFI_SUCCESS;

  //
  // Find the system table information guid hob
  //
  GuidHob = GetFirstGuidHob (&gUefiSystemTableInfoGuid);
  ASSERT (GuidHob != NULL);
  SystemTableInfo = (SYSTEM_TABLE_INFO *)GET_GUID_HOB_DATA (GuidHob);

  //
  // Install Acpi Table
  //
  if (SystemTableInfo->AcpiTableBase != 0 && SystemTableInfo->AcpiTableSize != 0) {
    DEBUG ((DEBUG_INFO, "Install Acpi Table at 0x%lx, length 0x%x\n", SystemTableInfo->AcpiTableBase, SystemTableInfo->AcpiTableSize));
    Status = gBS->InstallConfigurationTable (&gEfiAcpiTableGuid, (VOID *)(UINTN)SystemTableInfo->AcpiTableBase);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Find the frame buffer information and update PCDs
  //
  GuidHob = GetFirstGuidHob (&gEfiGraphicsInfoHobGuid);
  if (GuidHob != NULL) {
    GfxInfo = (EFI_PEI_GRAPHICS_INFO_HOB *)GET_GUID_HOB_DATA (GuidHob);
    Status = PcdSet32S (PcdVideoHorizontalResolution, GfxInfo->GraphicsMode.HorizontalResolution);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet32S (PcdVideoVerticalResolution, GfxInfo->GraphicsMode.VerticalResolution);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet32S (PcdSetupVideoHorizontalResolution, GfxInfo->GraphicsMode.HorizontalResolution);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet32S (PcdSetupVideoVerticalResolution, GfxInfo->GraphicsMode.VerticalResolution);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Set PcdPciExpressBaseAddress and PcdPciExpressBaseSize by HOB info
  //
  GuidHob = GetFirstGuidHob (&gUefiAcpiBoardInfoGuid);
  if (GuidHob != NULL) {
    AcpiBoardInfo = (ACPI_BOARD_INFO *)GET_GUID_HOB_DATA (GuidHob);
    Status = PcdSet64S (PcdPciExpressBaseAddress, AcpiBoardInfo->PcieBaseAddress);
    ASSERT_EFI_ERROR (Status);
    Status = PcdSet64S (PcdPciExpressBaseSize, AcpiBoardInfo->PcieBaseSize);
    ASSERT_EFI_ERROR (Status);

    if (AcpiBoardInfo->TPM20Present)
    {
      Size = sizeof (gEfiTpmDeviceInstanceTpm20DtpmGuid);
      Status = PcdSetPtrS (
               PcdTpmInstanceGuid,
               &Size,
               &gEfiTpmDeviceInstanceTpm20DtpmGuid
               );
      ASSERT_EFI_ERROR (Status);
    }
    else if (AcpiBoardInfo->TPM12Present)
    {
      Size = sizeof (gEfiTpmDeviceInstanceTpm12Guid);
      Status = PcdSetPtrS (
                 PcdTpmInstanceGuid,
                 &Size,
                 &gEfiTpmDeviceInstanceTpm12Guid
                 );
      ASSERT_EFI_ERROR (Status);
    }
  }

  return EFI_SUCCESS;
}

