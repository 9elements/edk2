/** @file  BootOptionsLib.h

  Copyright (c) 2020, 9elements Agency GmbH<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __BOOT_OPTIONS_LIB_H__
#define __BOOT_OPTIONS_LIB_H__

#include <Base.h>
#include <Uefi/UefiBaseType.h>

/* --- C-State Limits --- */

#define CX_LIMIT_C0 0
#define CX_LIMIT_C1 0
#define CX_LIMIT_C2 1
#define CX_LIMIT_C3 2
#define CX_LIMIT_C6 3
#define CX_LIMIT_C7 4
#define CX_LIMIT_C8 6

/* --- Primary Display/Video --- */

#define DISPLAY_IGFX 0
#define DISPLAY_PCIE 2

/* --- Powerstate after G3 --- */

#define PWR_G3_S0 0
#define PWR_G3_S5 1

/* --- PCIe SSC Values --- */

#define PCIE_SSC_0_0_P 0x00
#define PCIE_SSC_0_1_P 0x06
#define PCIE_SSC_0_2_P 0x0d
#define PCIE_SSC_0_3_P 0x14
#define PCIE_SSC_0_4_P 0x1a
#define PCIE_SSC_0_5_P 0x21
#define PCIE_SSC_0_6_P 0x28
#define PCIE_SSC_0_7_P 0x2e
#define PCIE_SSC_0_8_P 0x35
#define PCIE_SSC_0_9_P 0x3c
#define PCIE_SSC_1_0_P 0x42
#define PCIE_SSC_1_1_P 0x49
#define PCIE_SSC_1_2_P 0x50
#define PCIE_SSC_1_3_P 0x56
#define PCIE_SSC_1_4_P 0x5d
#define PCIE_SSC_1_5_P 0x64
#define PCIE_SSC_1_6_P 0x6a
#define PCIE_SSC_1_7_P 0x71
#define PCIE_SSC_1_8_P 0x78
#define PCIE_SSC_1_9_P 0x7e
#define PCIE_SSC_2_0_P 0x85
#define PCIE_SSC_AUTO  0xff

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

/* --- Boot Option Profiles --- */

typedef enum PROFILE {

	PROFILE_DEFAULT,
	PROFILE_REALTIME,

	PROFILE_END_ENUM
	
} PROFILE;

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
