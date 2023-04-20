/** @file

  Implementation for a generic i801 SMBus driver. Why we still call this a
  SMBus driver, I don't know.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/GlobalVariable.h>
#include <Guid/AuthenticatedVariableFormat.h>

#include "SMBusConfigLoader.h"

EFI_STATUS
EFIAPI
InstallSMBusConfigLoader (
  IN EFI_HANDLE                        ImageHandle,
  IN EFI_SYSTEM_TABLE                  *SystemTable
  )
{
  UINTN      DataSize;
  EFI_STATUS Status;
  UINT32     SecureBootOption;
  UINT8      SetupMode = 1;

  DEBUG ((DEBUG_INFO, "SMBusConfigLoader: InstallSMBusConfigLoader\n"));

  /* --- Enable/Disable SecureBoot --- */

  // Load SecureBoot settings
  DataSize = sizeof (UINT32);
  Status = gRT->GetVariable (
                  L"edk2_secure_boot",
                  &gEficorebootNvDataGuid,
                  NULL,
                  &DataSize,
                  &SecureBootOption
                  );
  if (Status == EFI_NOT_FOUND) {
    SecureBootOption = 1;
  }

  // Set L"SecureBootEnable". Only affects SecureBootSetupDxe.
  Status = gRT->SetVariable (EFI_SECURE_BOOT_ENABLE_NAME, &gEfiSecureBootEnableDisableGuid,
                             EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                             sizeof SecureBootOption, &SecureBootOption);

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "SMBusConfigLoader: Failed to set SecureBootEnable: %x\n", Status));
  }

  // Set L"SecureBoot". Only affects code outside of SecureBootSetupDxe.
  Status = gRT->SetVariable (EFI_SECURE_BOOT_MODE_NAME, &gEfiGlobalVariableGuid,
                             EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                             sizeof SecureBootOption, &SecureBootOption);

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "SMBusConfigLoader: Failed to set SecureBoot: %x\n", Status));
  }

  // EFI_SETUP_MODE_NAME must be valid if EFI_SECURE_BOOT_MODE_NAME exists
  // Force setup mode to enroll default keys
  Status = gRT->SetVariable (EFI_SETUP_MODE_NAME, &gEfiGlobalVariableGuid,
                             EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                             sizeof SetupMode, &SetupMode);

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "SMBusConfigLoader: Failed to set SetupMode: %x\n", Status));
  }

  return EFI_SUCCESS;
}
