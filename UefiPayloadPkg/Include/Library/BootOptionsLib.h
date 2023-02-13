/** @file  BootOptionsLib.h

  Copyright (c) 2020, 9elements Agency GmbH<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __BOOT_OPTIONS_LIB_H__
#define __BOOT_OPTIONS_LIB_H__

#include <Base.h>
#include <Uefi/UefiBaseType.h>

/* --- Boot Option Enumeration --- */

typedef enum OPT {

	OPT_HYPERTHREADING,
	OPT_TURBOMODE,
	OPT_CX,
	OPT_CX_LIMIT,
	OPT_PRIMARY_DISPLAY,
	OPT_EE_TURBO,
	OPT_LLC_DEADLINE,
	OPT_INTEL_VTX,
	OPT_INTEL_VTD,
	OPT_SECURE_BOOT,
	OPT_PXE_RETRIES,
	OPT_PWR_G3,
	OPT_PCIE_SSC,
	OPT_PCIE_SRIS,
	OPT_IBECC,

	OPT_END_ENUM

} OPT;

/**
  Loads custom boot options from SMMSTORE. If no custom setting
  exists, a fallback/default value from an option table is returned.

  @param[in]  BootOpt   The desired option. Use the OPT enumeration.
  @retval     The current setting of the desired option. Either a custom value or a fallback.
**/
UINT8
EFIAPI
LoadBootOption (
  IN OPT BootOpt
  );

/**
  Loads the default boot option from an option table based on the Board's
  profile configuration.

  @param[in]  BootOpt   The desired option. Use the OPT enumeration.
  @retval     The default value of BootOpt.
**/
UINT8
EFIAPI
LoadDefaultBootOption (
  IN OPT BootOpt
);

CHAR16*
EFIAPI
GetBootOptionName (
  IN OPT BootOpt
);

#endif /* __BOOT_OPTIONS_LIB_H__ */
