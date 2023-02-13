/** @file

  Implementation of a BootOption Loader.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BootOptionsLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/GlobalVariable.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/BootOptionsGuid.h>

/* Keep synced with coreboot */
static CHAR16* OptionNameLookupTable[] = {

	[OPT_HYPERTHREADING ] = L"Hyperthreading",
	[OPT_TURBOMODE      ] = L"TurboMode",
	[OPT_CX             ] = L"Cx",
	[OPT_CX_LIMIT       ] = L"CxLimit",
	[OPT_PRIMARY_DISPLAY] = L"PrimaryDisplay",
	[OPT_EE_TURBO       ] = L"EnergyEfficientTurbo",
	[OPT_LLC_DEADLINE   ] = L"LLCDeadline",
	[OPT_INTEL_VTX      ] = L"VTX",
	[OPT_INTEL_VTD      ] = L"VTD",
	[OPT_SECURE_BOOT    ] = L"SecBoot",
	[OPT_PXE_RETRIES    ] = L"PXERetries",
	[OPT_PWR_G3         ] = L"PowerstateG3",
	[OPT_PCIE_SSC       ] = L"PCIeSSC",
	[OPT_PCIE_SRIS      ] = L"PCIeSRIS",
	[OPT_IBECC          ] = L"IBECC"
};

/* --- SMMSTORE GUID ---

static EFI_GUID gEficorebootNvDataGuid = {
	0xceae4c1d, 0x335b, 0x4685, { 0xa4, 0xa0, 0xfc, 0x4a, 0x94, 0xee, 0xa0, 0x85 } };

*/

UINT8
EFIAPI
LoadBootOption (
  IN OPT BootOpt
  )
{
  EFI_STATUS Status;
  UINT8      CustomSettings;
  UINTN      DataSize;
  UINT32     Attributes;

  /* todo: sanity checks for incoming parameters like BootOpt and Profile */

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  DataSize   = sizeof (CustomSettings);

  Status = gRT->GetVariable (OptionNameLookupTable[BootOpt], &gEficorebootNvDataGuid,
                             &Attributes, &DataSize, &CustomSettings);

  return (EFI_ERROR (Status)) ? LoadDefaultBootOption (BootOpt) : CustomSettings;
}

UINT8
EFIAPI
LoadDefaultBootOption (
  IN OPT BootOpt
)
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  BOOT_OPTIONS       *BootOptions;
  UINTN              Index;

  GuidHob = GetFirstGuidHob (&gEfiBootOptionsTableGuid);
  ASSERT (GuidHob != NULL);

  BootOptions = GET_GUID_HOB_DATA (GuidHob);
  // TODO: If out-of-order options are unlikely, optimise by dereferencing index
  for (Index = 0; Index < BootOptions->Count; Index++) {
    // Found OPT
    if (BootOptions->OptionDefaults[Index].Option == BootOpt) {
      return BootOptions->OptionDefaults[Index].DefaultValue;
    }
  }

  // Should not be reached
  DEBUG ((DEBUG_INFO, "Error! Invalid BootOpt %d!\n", BootOpt));
  ASSERT (FALSE);
  return 0xFF;
}

CHAR16*
EFIAPI
GetBootOptionName (
  IN OPT BootOpt
)
{
  return OptionNameLookupTable[BootOpt];
}
