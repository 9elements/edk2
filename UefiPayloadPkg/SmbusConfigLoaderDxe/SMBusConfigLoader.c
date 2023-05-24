/** @file

  Implementation for a generic i801 SMBus driver. Why we still call this a
  SMBus driver, I don't know.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CfrHelpersLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/GlobalVariable.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/ImageAuthentication.h>

#include "SMBusConfigLoader.h"

#define ATLAS_PROF_UNPROGRAMMED          0  /* EEPROM not initialised */
#define ATLAS_PROF_REALTIME_PERFORMANCE  2

EFI_STATUS
EFIAPI
InstallSMBusConfigLoader (
  IN EFI_HANDLE                        ImageHandle,
  IN EFI_SYSTEM_TABLE                  *SystemTable
  )
{
  UINTN      DataSize;
  UINT8      SetupMode;
  EFI_STATUS Status;
  UINT32     *AtlasProfile;
  UINT8      SecureBootOption;

  DEBUG ((DEBUG_INFO, "SMBusConfigLoader: InstallSMBusConfigLoader\n"));

  /* --- Enable/Disable SecureBoot --- */

  //
  // With gEfiAuthenticatedVariableGuid variable store, AuthVariableLib configures "SetupMode"
  // and user *may* configure secure boot as required
  //
  DataSize = sizeof (SetupMode);
  Status = gRT->GetVariable (
                  EFI_SETUP_MODE_NAME,
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &DataSize,
                  &SetupMode
                  );
  if (!EFI_ERROR (Status) && (SetupMode == USER_MODE)) {
    DEBUG ((DEBUG_INFO, "SMBusConfigLoader: Secure boot already provisioned, exiting.\n"));
    return EFI_SUCCESS;
  }

  //
  // Load SecureBoot settings
  //
  Status = CfrOptionGetDefaultValue (NULL, "profile", (VOID **)&AtlasProfile, NULL);
  if (!EFI_ERROR (Status) && (*AtlasProfile == ATLAS_PROF_REALTIME_PERFORMANCE)) {
    SecureBootOption = 0;
  } else {
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

  return EFI_SUCCESS;
}
