/** @file

A Setup Menu for configuring boot options.

Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BootOptionsLib.h>

#include "SetupMenu.h"
#include "SetupMenuNVDataStruc.h"

/*
 * === Extending the SetupMenu ===
 *
 * 1. add a new option key and its default to Include/Library/BootOptionsLib.h
 *
 *      #define OPT_NEWOPT L"NewOpt" // option key
 *      #define OPT_NEWOPT_DFL TRUE  // option default
 *
 * 2. optional: add the same key and default to coreboot. In case of the
 *    Atlas board, see src/mainboard/prodrive/atlas/boot_options.h
 *
 * 3. extend the NVDataStructure in Library/SetupMenuUiLib/SetupMenuNVDataStruc.h
 *    and increment the MAX_OPTION_KEYS macro in Library/SetupMenuUiLib/Constants.h
 *
 *      typedef struct {
 *
 *        [...]
 *        UINT8 NewOpt; // NV storage for the new option      
 *
 *      } SETUPMENU_CONFIGURATION;
 *
 *      #define MAX_OPTION_KEYS 16 // 15
 *
 * 4. add the key from step 1 to gOptionKeys and its default to gDefaultConfig in
 *    Library/SetupMenuUiLib/SetupMenu.c
 *
 *      CHAR16 *gOptionKeys[] = {
 *
 *        [...]
 *        OPT_NEWOPT
 *      };
 *
 *      const SETUPMENU_CONFIGURATION gDefaultConfig = {
 *
 *        [...]
 *        .NewOpt = OPT_NEWOPT_DFL
 *      };
 *
 * 5. add menu strings (PROMPT and HELP) for the new boot option in
 *    Library/SetupMenuUiLib/SetupMenuStrings.uni
 *
 *      #string STR_NEWOPT_PROMPT           #language en-US  "NewOpt"
 *                                          #language fr-FR  "NewOpt"
 *      #string STR_NEWOPT_HELP             #language en-US  "Configure NewOpt"
 *                                          #language fr-FR  "Configure NewOpt"
 *
 * 6. and if required, add menu strings for the choices to the same file within
 *    the section "Boot Options - Choices"
 *
 *      #string STR_CHOICE_ONE              #language en-US  "Example Choice 1"
 *                                          #language fr-FR  "Example Choice 1"
 *      #string STR_CHOICE_TWO              #language en-US  "Example Choice 2"
 *                                          #language fr-FR  "Example Choice 2"
 * 
 * 7. add a new function "CreateComponentNewOpt" to Library/SetupMenuUiLib/SetupMenu.c
 *    (you can copy an already existent "CreateComponent" function and adjust it)
 *
 *      VOID
 *      CreateComponentNewOpt (
 *        VOID  *StartOpCodeHandle,
 *        UINTN  QuestionId
 *      )
 *      {
 *        // =-=-=-=-= Options / Choices =-=-=-=-=
 *
 *        VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
 *        ASSERT (OptionsOpCodeHandle != NULL);
 *
 *        HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
 *                                    STRING_TOKEN (STR_CHOICE_ONE),
 *                                    0,
 *                                    EFI_IFR_NUMERIC_SIZE_1,
 *                                    1 // value of option one);
 *
 *        HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
 *                                    STRING_TOKEN (STR_CHOICE_TWO),
 *                                    0,
 *                                    EFI_IFR_NUMERIC_SIZE_1,
 *                                    2 // value of option two);
 *
 *        // =-=-=-=-= OneOf Question =-=-=-=-=
 * 
 *        HiiCreateOneOfOpCode (StartOpCodeHandle,
 *                              QuestionId,
 *                              CONFIGURATION_VARSTORE_ID,
 *                              OFFSET_OF (SETUPMENU_CONFIGURATION, NewOpt),
 *                              STRING_TOKEN (STR_NEWOPT_PROMPT),
 *                              STRING_TOKEN (STR_NEWOPT_HELP),
 *                              EFI_IFR_FLAG_CALLBACK,
 *                              EFI_IFR_NUMERIC_SIZE_1,
 *                              OptionsOpCodeHandle,
 *                              NULL);
 *
 *        HiiFreeOpCodeHandle (OptionsOpCodeHandle);
 *      }
 *
 * 8. call "CreateComponentNewOpt" from within "CreateRuntimeComponents"
 *    in Library/SetupMenuUiLib/SetupMenu.c and pass it an unique question ID
 *    by incrementing the ID of the previous option
 *
 *      VOID
 *      CreateRuntimeComponents(
 *        EFI_HII_HANDLE HiiHandle
 *      )
 *      {
 *        [...]
 *
 *        CreateComponentPreviousOpt     (StartOpCodeHandle, 0x800e);
 *        CreateComponentNewOpt          (StartOpCodeHandle, 0x800f);
 *
 *        [...]
 *      }
 *
 * That's it. The SetupMenu should now contain a new entry for "NewOpt". You
 * can now retrieve the user-configured value of "NewOpt" in coreboot via
 *
 *   // src/mainboard/prodrive/atlas/boot_options.h
 *   uint8_t optval = get_uint_option(OPT_NEWOPT, OPT_NEWOPT_DFL);
 *
 * and in EDK2 via
 *
 *   // Include/Library/BootOptionsLib.h
 *   UINT8 OptVal = LoadBootOption (OPT_NEWOPT, OPT_NEWOPT_DFL);
 *
 * Note, that you will need to enable the SMMSTORE as option backend in order to
 * access the boot options from within coreboot.
 *
 **/

CHAR16   mConfigNVName [] = L"SetupMenuNVConfig";
CHAR16   mSetupMenuLock[] = L"SetupMenuLock";
EFI_GUID mSetupMenuGuid   = SETUP_MENU_FORMSET_GUID;

CHAR16 *gOptionKeys[] = { 

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
  OPT_IBECC
};

/* default setupmenu configuration */
const SETUPMENU_CONFIGURATION gDefaultConfig = {

  .Hyperthreading       = OPT_HYPERTHREADING_DFL,
  .TurboMode            = OPT_TURBOMODE_DFL,
  .Cx                   = OPT_CX_DFL,
  .CxLimit              = OPT_CX_LIMIT_DFL,
  .PrimaryDisplay       = OPT_PRIMARY_DISPLAY_DFL,
  .EnergyEfficientTurbo = OPT_EE_TURBO_DFL,
  .LlcDeadline          = OPT_LLC_DEADLINE_DFL,
  .IntelVtx             = OPT_INTEL_VTX_DFL,
  .IntelVtd             = OPT_INTEL_VTD_DFL,
  .SecureBoot           = OPT_SECURE_BOOT_DFL,
  .PxeRetries           = OPT_PXE_RETRIES_DFL,
  .PwrG3                = OPT_PWR_G3_DFL,
  .PcieSsc              = OPT_PCIE_SSC_DFL,
  .PcieSris             = OPT_PCIE_SRIS_DFL,
  .Ibecc                = OPT_IBECC_DFL
};

/* internal NV configuration storage */
SETUPMENU_CONFIGURATION gInternalConfig;

SETUP_MENU_CALLBACK_DATA  gSetupMenuPrivate = {

  SETUP_MENU_CALLBACK_DATA_SIGNATURE,
  NULL,
  NULL,
  {
    SetupMenuExtractConfig,
    SetupMenuRouteConfig,
    SetupMenuCallback
  }
};

HII_VENDOR_DEVICE_PATH  mSetupMenuHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8) (sizeof (VENDOR_DEVICE_PATH)),
        (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    
    { 0x102579a0, 0x3686, 0x466e, { 0xac, 0xd8, 0x80, 0xc0, 0x87, 0x4, 0x4f, 0x4a } }
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8) (END_DEVICE_PATH_LENGTH),
      (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

VOID
CreateComponentHyperthreading (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, Hyperthreading),
                        STRING_TOKEN (STR_HYPERTHREADING_PROMPT),
                        STRING_TOKEN (STR_HYPERTHREADING_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentTurboMode (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, TurboMode),
                        STRING_TOKEN (STR_TURBOMODE_PROMPT),
                        STRING_TOKEN (STR_TURBOMODE_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentCx (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, Cx),
                        STRING_TOKEN (STR_CX_PROMPT),
                        STRING_TOKEN (STR_CX_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentCxLimit (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C0),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              0 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C1),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              0 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C2),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              1 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C3),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              2 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C6),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              3 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C7),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              4 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_CX_LIMIT_C8),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              6 /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, CxLimit),
                        STRING_TOKEN (STR_CX_LIMIT_PROMPT),
                        STRING_TOKEN (STR_CX_LIMIT_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentPrimaryDisplay (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_PRIMARY_IGFX),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              0 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_PRIMARY_PCIE),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              2 /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, PrimaryDisplay),
                        STRING_TOKEN (STR_PRIMARY_DISPLAY_PROMPT),
                        STRING_TOKEN (STR_PRIMARY_DISPLAY_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentEnergyEfficientTurbo (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, EnergyEfficientTurbo),
                        STRING_TOKEN (STR_EE_TURBO_PROMPT),
                        STRING_TOKEN (STR_EE_TURBO_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentLlcDeadline (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, LlcDeadline),
                        STRING_TOKEN (STR_LLC_DEADLINE_PROMPT),
                        STRING_TOKEN (STR_LLC_DEADLINE_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentIntelVtx (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, IntelVtx),
                        STRING_TOKEN (STR_INTEL_VTX_PROMPT),
                        STRING_TOKEN (STR_INTEL_VTX_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentIntelVtd (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, IntelVtd),
                        STRING_TOKEN (STR_INTEL_VTD_PROMPT),
                        STRING_TOKEN (STR_INTEL_VTD_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentSecureBoot (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, SecureBoot),
                        STRING_TOKEN (STR_SECURE_BOOT_PROMPT),
                        STRING_TOKEN (STR_SECURE_BOOT_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentPxeRetries (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, PxeRetries),
                        STRING_TOKEN (STR_PXE_RETRIES_PROMPT),
                        STRING_TOKEN (STR_PXE_RETRIES_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentPwrG3 (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_PWR_G3_S0),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              0 /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_PWR_G3_S5),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              1 /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, PwrG3),
                        STRING_TOKEN (STR_PWR_G3_PROMPT),
                        STRING_TOKEN (STR_PWR_G3_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentPcieSsc (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, PcieSsc),
                        STRING_TOKEN (STR_PCIE_SSC_PROMPT),
                        STRING_TOKEN (STR_PCIE_SSC_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentPcieSris (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, PcieSris),
                        STRING_TOKEN (STR_PCIE_SRIS_PROMPT),
                        STRING_TOKEN (STR_PCIE_SRIS_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateComponentIbecc (
  VOID  *StartOpCodeHandle,
  UINTN  QuestionId
)
{
  /* =-=-=-=-= Options / Choices =-=-=-=-= */

  VOID *OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OptionsOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_ENABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              TRUE /* value */);

  HiiCreateOneOfOptionOpCode (OptionsOpCodeHandle,
                              STRING_TOKEN (STR_CHOICE_DISABLED),
                              0,
                              EFI_IFR_NUMERIC_SIZE_1,
                              FALSE /* value */);

  /* =-=-=-=-= OneOf Question =-=-=-=-= */

  HiiCreateOneOfOpCode (StartOpCodeHandle,
                        QuestionId,
                        CONFIGURATION_VARSTORE_ID,
                        OFFSET_OF (SETUPMENU_CONFIGURATION, Ibecc),
                        STRING_TOKEN (STR_IBECC_PROMPT),
                        STRING_TOKEN (STR_IBECC_HELP),
                        EFI_IFR_FLAG_CALLBACK,
                        EFI_IFR_NUMERIC_SIZE_1,
                        OptionsOpCodeHandle,
                        NULL);

  HiiFreeOpCodeHandle (OptionsOpCodeHandle);
}

VOID
CreateRuntimeComponents(
  EFI_HII_HANDLE HiiHandle
)
{
  EFI_IFR_GUID_LABEL *StartLabel;
  EFI_IFR_GUID_LABEL *EndLabel;
  VOID               *StartOpCodeHandle;
  VOID               *EndOpCodeHandle;

  /* =-=-=-=-= Initialize StartOpCode and EndOpCode Handles =-=-=-=-= */

  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);

  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);
  
  StartLabel = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (StartOpCodeHandle, 
                                                           &gEfiIfrTianoGuid,
                                                           NULL, 
                                                           sizeof (EFI_IFR_GUID_LABEL));
  
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = LABEL_RT_COMP;

  EndLabel = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (EndOpCodeHandle, 
                                                         &gEfiIfrTianoGuid,
                                                         NULL, 
                                                         sizeof (EFI_IFR_GUID_LABEL));
  
  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_RT_COMP_END;

  /* =-=-=-=-= Create Runtime Components =-=-=-=-= */

  CreateComponentHyperthreading       (StartOpCodeHandle, 0x8001);
  CreateComponentTurboMode            (StartOpCodeHandle, 0x8002);
  CreateComponentCx                   (StartOpCodeHandle, 0x8003);
  CreateComponentCxLimit              (StartOpCodeHandle, 0x8004);
  CreateComponentPrimaryDisplay       (StartOpCodeHandle, 0x8005);
  CreateComponentEnergyEfficientTurbo (StartOpCodeHandle, 0x8006);
  CreateComponentLlcDeadline          (StartOpCodeHandle, 0x8007);
  CreateComponentIntelVtx             (StartOpCodeHandle, 0x8008);
  CreateComponentIntelVtd             (StartOpCodeHandle, 0x8009);
  CreateComponentSecureBoot           (StartOpCodeHandle, 0x800a);
  CreateComponentPxeRetries           (StartOpCodeHandle, 0x800b);
  CreateComponentPwrG3                (StartOpCodeHandle, 0x800c);
  CreateComponentPcieSsc              (StartOpCodeHandle, 0x800d);
  CreateComponentPcieSris             (StartOpCodeHandle, 0x800e);
  CreateComponentIbecc                (StartOpCodeHandle, 0x800f);

  /* =-=-=-=-= Submit Components =-=-=-=-= */

  HiiUpdateForm (HiiHandle,
                 &mSetupMenuGuid,
                 SETUP_MENU_FORM_ID,
                 StartOpCodeHandle,
                 EndOpCodeHandle);
  
  /* =-=-=-=-= Cleanup Handles =-=-=-=-= */

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);  
}

BOOLEAN
EfiVarExists (
  CHAR16   *Key,
  EFI_GUID Guid
)
{
  EFI_STATUS Status;
  UINTN      DataSize;
  UINT32     Attributes;
  UINT8      Value;

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  DataSize   = sizeof (UINT8);

  Status = gRT->GetVariable (Key, &Guid, &Attributes, &DataSize, &Value);
  return !EFI_ERROR (Status);
}

EFI_STATUS
LockSetupMenu (
  VOID
)
{
  EFI_STATUS Status;
  UINT8      Lock;

  /* acquire lock */
  Lock = TRUE;

  Status = gRT->SetVariable (mSetupMenuLock, &mSetupMenuGuid,
                             EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                             sizeof (UINT8), &Lock);
               
  return Status; 
}

EFI_STATUS
UnlockSetupMenu (
  VOID
)
{
  EFI_STATUS Status;
  UINT8      Lock;

  /* release lock */
  Lock = FALSE;

  Status = gRT->SetVariable (mSetupMenuLock, &mSetupMenuGuid,
                             EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                             sizeof (UINT8), &Lock);

  return Status; 
}

BOOLEAN
SetupMenuIsLocked (
  VOID
)
{
  EFI_STATUS Status;
  UINTN      DataSize;
  UINT32     Attributes;
  UINT8      Lock;

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  DataSize   = sizeof (UINT8);

  Status = gRT->GetVariable (mSetupMenuLock, &mSetupMenuGuid,
                             &Attributes, &DataSize, &Lock);

  return (EFI_ERROR (Status)) ? FALSE : (BOOLEAN) Lock;
}

EFI_STATUS
PublishConfiguration (
  CONST VOID *Conf
)
{  
  EFI_STATUS Status;
  UINT8      Iterator;

  LockSetupMenu ();

  for (Iterator = 0; Iterator < MAX_OPTION_KEYS; Iterator++) {

    Status = gRT->SetVariable (gOptionKeys[Iterator], &gEficorebootNvDataGuid,
                               EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                               sizeof (UINT8), ((UINT8*) Conf) + Iterator);

    if (EFI_ERROR (Status))
      break;
  }

  UnlockSetupMenu ();  
  return Status;
}

EFI_STATUS
PublishInternalConfiguration(
  VOID
)
{
  EFI_STATUS              Status;
  SETUPMENU_CONFIGURATION Conf;
  UINTN                   DataSize;
  UINT32                  Attributes;

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  DataSize   = sizeof (SETUPMENU_CONFIGURATION);

  Status = gRT->GetVariable (mConfigNVName, &mSetupMenuGuid,
                             &Attributes, &DataSize, &Conf);

  if (EFI_ERROR (Status))
    return Status;

  return PublishConfiguration (&Conf);
}

EFI_STATUS
LoadConfiguration (
  VOID
)
{
  EFI_STATUS              Status;
  SETUPMENU_CONFIGURATION Conf;
  UINTN                   DataSize;
  UINT32                  Attributes;
  UINT8                   Iterator;

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  DataSize   = sizeof (UINT8);

  for (Iterator = 0; Iterator < MAX_OPTION_KEYS; Iterator++) {

    Status = gRT->GetVariable (gOptionKeys[Iterator], &gEficorebootNvDataGuid,
                               &Attributes, &DataSize, ((UINT8*) &Conf) + Iterator);

    if (EFI_ERROR (Status))
      return Status;
  }

  Status = gRT->SetVariable (mConfigNVName, &mSetupMenuGuid,
                             EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                             sizeof (SETUPMENU_CONFIGURATION), &Conf);

  return Status;
}

/**
  Load and Publish SetupMenu Configuration Storage
**/
EFI_STATUS
InitConfigurationStorage(
  VOID
)
{
  EFI_STATUS Status;

  if (SetupMenuIsLocked ()) {

    Status = PublishConfiguration (&gDefaultConfig);
    ASSERT (!EFI_ERROR (Status));
  }

  if (!EfiVarExists (gOptionKeys[0], gEficorebootNvDataGuid)) {
  
    Status = PublishConfiguration (&gDefaultConfig);
    ASSERT (!EFI_ERROR (Status));
  }

  Status = LoadConfiguration ();
  ASSERT (!EFI_ERROR (Status));

  return Status;
}

VOID
CreateSetupMenuForm(
  IN EFI_FORM_ID      NextShowFormId
)
{
  EFI_STATUS Status;

  /* =-=-=-=-= Initialize Varstore and publish current configuration to the FormBrowser -=-=-=-= */
  
  Status = InitConfigurationStorage ();
  ASSERT (!EFI_ERROR (Status));

  SETUPMENU_CONFIGURATION *Conf = AllocateZeroPool (sizeof (SETUPMENU_CONFIGURATION));
  ASSERT (Conf != NULL);

  if (HiiGetBrowserData (&mSetupMenuGuid, mConfigNVName, sizeof (SETUPMENU_CONFIGURATION), (UINT8*) Conf)) {

    CopyMem (Conf, &gInternalConfig, sizeof (SETUPMENU_CONFIGURATION));
    HiiSetBrowserData (&mSetupMenuGuid, mConfigNVName, sizeof (SETUPMENU_CONFIGURATION), (UINT8*) Conf, NULL);
  }

  FreePool (Conf);

  /* =-=-=-=-= Create Runtime Components for the Form =-=-=-=-= */

  CreateRuntimeComponents (gSetupMenuPrivate.HiiHandle);
}

EFI_STATUS
EFIAPI
SetupMenuExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Request,
  OUT EFI_STRING                             *Progress,
  OUT EFI_STRING                             *Results
  )
{
  if (Progress == NULL || Results == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  *Progress = Request;
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
SetupMenuRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Configuration,
  OUT EFI_STRING                             *Progress
  )
{
  if (Configuration == NULL || Progress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Configuration;

  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
SetupMenuCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  EFI_BROWSER_ACTION                     Action,
  IN  EFI_QUESTION_ID                        QuestionId,
  IN  UINT8                                  Type,
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  )
{
  EFI_STATUS Status;

  if (Action == EFI_BROWSER_ACTION_SUBMITTED) {

    Status = PublishInternalConfiguration ();
    ASSERT (!EFI_ERROR (Status));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetupMenuUiLibConstructor (
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
)
{
  EFI_STATUS Status;

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
                  &mSetupMenuGuid,
                  gSetupMenuPrivate.DriverHandle,
                  SetupMenuVfrBin,
                  SetupMenuUiLibStrings,
                  NULL
                  );

  ASSERT (gSetupMenuPrivate.HiiHandle != NULL);

  //
  // Create the SetupMenu Form
  //
  CreateSetupMenuForm (SETUP_MENU_FORM_ID);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetupMenuUiLibDestructor(
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
)
{
  EFI_STATUS                  Status;

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  gSetupMenuPrivate.DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mSetupMenuHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &gSetupMenuPrivate.ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  HiiRemovePackages (gSetupMenuPrivate.HiiHandle);

  return EFI_SUCCESS;
}

