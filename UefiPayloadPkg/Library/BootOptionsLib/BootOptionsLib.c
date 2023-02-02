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

#define PROFILE_IDX(p) ((p) - 'A')

static const UINT8 Fallbacks[PROFILE_END_ENUM][OPT_END_ENUM] = {

	[PROFILE_DEFAULT] = {

		[OPT_HYPERTHREADING ] = TRUE,
		[OPT_TURBOMODE      ] = TRUE,
		[OPT_CX             ] = TRUE,
		[OPT_CX_LIMIT       ] = CX_LIMIT_C8,
		[OPT_PRIMARY_DISPLAY] = DISPLAY_IGFX,
		[OPT_EE_TURBO       ] = FALSE,
		[OPT_LLC_DEADLINE   ] = FALSE,
		[OPT_INTEL_VTX      ] = FALSE,
		[OPT_INTEL_VTD      ] = FALSE,
		[OPT_SECURE_BOOT    ] = TRUE,
		[OPT_PXE_RETRIES    ] = FALSE,
		[OPT_PWR_G3         ] = PWR_G3_S5,
		[OPT_PCIE_SSC       ] = PCIE_SSC_AUTO,
		[OPT_PCIE_SRIS      ] = FALSE,
		[OPT_IBECC          ] = FALSE
	},

	[PROFILE_REALTIME] = {

		[OPT_HYPERTHREADING ] = FALSE,
		[OPT_TURBOMODE      ] = TRUE,
		[OPT_CX             ] = TRUE,
		[OPT_CX_LIMIT       ] = CX_LIMIT_C8,
		[OPT_PRIMARY_DISPLAY] = DISPLAY_IGFX,
		[OPT_EE_TURBO       ] = FALSE,
		[OPT_LLC_DEADLINE   ] = FALSE,
		[OPT_INTEL_VTX      ] = FALSE,
		[OPT_INTEL_VTD      ] = FALSE,
		[OPT_SECURE_BOOT    ] = TRUE,
		[OPT_PXE_RETRIES    ] = FALSE,
		[OPT_PWR_G3         ] = PWR_G3_S5,
		[OPT_PCIE_SSC       ] = PCIE_SSC_AUTO,
		[OPT_PCIE_SRIS      ] = FALSE,
		[OPT_IBECC          ] = FALSE
	}
};

static CHAR16* OptionNameLookupTable[OPT_END_ENUM] = {

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
  UINT8 Profile;
  UINT8 Idx;

  /*
   * todo: extract profile from coreboot
   * table and replace following line
   * and add sanity check for Profile
   */
  Profile = 'A';
  Idx     = PROFILE_IDX (Profile);

  return Fallbacks[Idx][BootOpt]; 
}

CHAR16*
EFIAPI
GetBootOptionName (
  IN OPT BootOpt
)
{
  return OptionNameLookupTable[BootOpt];
}
