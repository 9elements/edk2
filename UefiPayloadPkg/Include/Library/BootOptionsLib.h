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

/* --- Boot options and defaults --- */

#define OPT_HYPERTHREADING L"Hyperthreading"
#define OPT_HYPERTHREADING_DFL TRUE 

#define OPT_TURBOMODE L"TurboMode"
#define OPT_TURBOMODE_DFL TRUE 

#define OPT_CX L"Cx"
#define OPT_CX_DFL TRUE

#define OPT_CX_LIMIT L"CxLimit"
#define OPT_CX_LIMIT_DFL CX_LIMIT_C8

#define OPT_PRIMARY_DISPLAY L"PrimaryDisplay"
#define OPT_PRIMARY_DISPLAY_DFL DISPLAY_IGFX

#define OPT_EE_TURBO L"EnergyEfficientTurbo"
#define OPT_EE_TURBO_DFL FALSE

#define OPT_LLC_DEADLINE L"LLCDeadline"
#define OPT_LLC_DEADLINE_DFL FALSE 

#define OPT_INTEL_VTX L"VTX"
#define OPT_INTEL_VTX_DFL FALSE

#define OPT_INTEL_VTD L"VTD"
#define OPT_INTEL_VTD_DFL FALSE

#define OPT_SECURE_BOOT L"SecBoot"
#define OPT_SECURE_BOOT_DFL TRUE

#define OPT_PXE_RETRIES L"PXERetries"
#define OPT_PXE_RETRIES_DFL FALSE

#define OPT_PWR_G3 L"PowerstateG3"
#define OPT_PWR_G3_DFL PWR_G3_S5

#define OPT_PCIE_SSC L"PCIeSSC"
#define OPT_PCIE_SSC_DFL PCIE_SSC_AUTO

#define OPT_PCIE_SRIS L"PCIeSRIS"
#define OPT_PCIE_SRIS_DFL FALSE

#define OPT_IBECC L"IBECC"
#define OPT_IBECC_DFL FALSE

/**
  Loads custom boot options from SMMSTORE. If no custom setting
  exists, a fallback/default value is returned.

  @param[in]  Option   The desired option. Use OPT_* macros.
  @param[in]  Default  A fallback value which is returned if no custom setting is found. Use OPT_*_DFL macros.

  @retval The current setting of the desired option. Either a custom value or 'Default'.
**/
UINT8
EFIAPI
LoadBootOption (
  IN CHAR16* Option,
  IN UINT8   Default
  );

#endif /* __BOOT_OPTIONS_LIB_H__ */
