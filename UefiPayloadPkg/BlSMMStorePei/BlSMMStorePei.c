/** @file  BlSMMStorePei.c

  Copyright (c) 2020, 9elements Agency GmbH<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BlSMMStorePei.h"

/**
  This is the entrypoint of PEIM

  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS if it completed successfully.
**/
EFI_STATUS
EFIAPI
BlSMMSTOREEntryPoint (
  IN       EFI_PEI_FILE_HANDLE     FileHandle,
  IN CONST EFI_PEI_SERVICES        **PeiServices
  )
{
  VOID                                    *GuidHob;
  SMMSTORE_INFO                           SMMStoreInfo;

  if (PcdGetBool (PcdEmuVariableNvModeEnable)) {
    DEBUG ((DEBUG_WARN, "Variable emulation is active! Skipping driver init.\n"));
    return EFI_SUCCESS;
  }

  //
  // Find the SMMSTORE information guid hob
  //
  GuidHob = GetFirstGuidHob (&gEfiSMMSTOREInfoHobGuid);
  if (GuidHob == NULL) {
    DEBUG ((DEBUG_WARN, "SMMSTORE not supported! Skipping driver init.\n"));
    PcdSetBoolS (PcdEmuVariableNvModeEnable, TRUE);
    return EFI_SUCCESS;
  }

  CopyMem(&SMMStoreInfo, GET_GUID_HOB_DATA (GuidHob), GET_GUID_HOB_DATA_SIZE(GuidHob));

  if (!SMMStoreInfo.MmioAddress ||
      !SMMStoreInfo.ComBuffer ||
      !SMMStoreInfo.BlockSize ||
      !SMMStoreInfo.NumBlocks) {
    DEBUG((EFI_D_ERROR, "%a: Invalid data in SMMStore Info hob\n", __FUNCTION__));
    PcdSetBoolS (PcdEmuVariableNvModeEnable, TRUE);
    return EFI_WRITE_PROTECTED;
  }

  // Update PCDs for Variable/RuntimeDxe
  PcdSet32S (PcdFlashNvStorageVariableBase,
      PcdGet32 (PcdFlashNvStorageVariableBase) + SMMStoreInfo.MmioAddress);
  PcdSet32S (PcdFlashNvStorageFtwWorkingBase,
      PcdGet32 (PcdFlashNvStorageFtwWorkingBase) + SMMStoreInfo.MmioAddress);
  PcdSet32S (PcdFlashNvStorageFtwSpareBase,
      PcdGet32 (PcdFlashNvStorageFtwSpareBase) + SMMStoreInfo.MmioAddress);

  return EFI_SUCCESS;
}
