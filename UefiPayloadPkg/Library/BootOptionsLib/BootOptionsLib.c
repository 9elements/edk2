/** @file

  Implementation of a BootOption Loader.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BootOptionsLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/GlobalVariable.h>
#include <Guid/AuthenticatedVariableFormat.h>

/* --- SMMSTORE GUID ---

static EFI_GUID gEficorebootNvDataGuid = {
	0xceae4c1d, 0x335b, 0x4685, { 0xa4, 0xa0, 0xfc, 0x4a, 0x94, 0xee, 0xa0, 0x85 } };

*/

UINT8
EFIAPI
LoadBootOption (
  IN CHAR16* Option,
  IN UINT8   Default
  )
{
  EFI_STATUS Status;
  UINT8      CustomSettings;
  UINTN      DataSize;
  UINT32     Attributes;

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  DataSize   = sizeof (CustomSettings);

  Status = gRT->GetVariable (Option, &gEficorebootNvDataGuid,
                             &Attributes, &DataSize, &CustomSettings);

  return (EFI_ERROR (Status)) ? Default : CustomSettings;
}
