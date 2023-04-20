/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SetupMenu.h"
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/VariableFormat.h>

/**
  This function installs the HII form.

**/
EFI_STATUS
EFIAPI
CfrSetupMenuUiLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Install Device Path and Config Access protocols to driver handle
  //
  gSetupMenuPrivate.DriverHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &gSetupMenuPrivate.DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mSetupMenuHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &gSetupMenuPrivate.ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Publish our HII data.
  //
  gSetupMenuPrivate.HiiHandle = HiiAddPackages (
                                  &mSetupMenuFormsetGuid,
                                  gSetupMenuPrivate.DriverHandle,
                                  SetupMenuVfrBin,
                                  CfrSetupMenuUiLibStrings,
                                  NULL
                                  );
  ASSERT (gSetupMenuPrivate.HiiHandle != NULL);

  //
  // Insert runtime components from bootloader's CFR table.
  //
  CfrCreateRuntimeComponents ();

  return Status;
}

/**
  This function uninstalls the HII form.

**/
EFI_STATUS
EFIAPI
CfrSetupMenuUiLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Uninstall Device Path and Config Access protocols
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  gSetupMenuPrivate.DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mSetupMenuHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &gSetupMenuPrivate.ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Remove our HII data
  //
  HiiRemovePackages (gSetupMenuPrivate.HiiHandle);

  return Status;
}
