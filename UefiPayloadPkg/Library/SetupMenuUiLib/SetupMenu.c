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
 * 3. add the key and its default from step 1 to the gDefaults table in
 *    Library/SetupMenuUiLib/SetupMenu.c
 *
 *      BootOptionDefault gDefaults[MAX_OPTION_KEYS] = {
 *
 *        [...]
 *        { .key = OPT_NEWOPT, .dfl = OPT_NEWOPT_DFL }
 *      };
 *
 *    and increment the MAX_OPTION_KEYS macro in Library/SetupMenuUiLib/Constants.h
 *
 *      #define MAX_OPTION_KEYS 16 // 15
 *
 * 4. add menu strings (PROMPT and HELP) for the new boot option in
 *    Library/SetupMenuUiLib/SetupMenuStrings.uni
 *
 *      #string STR_NEWOPT_PROMPT           #language en-US  "NewOpt"
 *                                          #language fr-FR  "NewOpt"
 *      #string STR_NEWOPT_HELP             #language en-US  "Configure NewOpt"
 *                                          #language fr-FR  "Configure NewOpt"
 *
 * 5. and if required, add menu strings for the choices to the same file within
 *    the section "Boot Options - Choices"
 *
 *      #string STR_CHOICE_ONE              #language en-US  "Example Choice 1"
 *                                          #language fr-FR  "Example Choice 1"
 *      #string STR_CHOICE_TWO              #language en-US  "Example Choice 2"
 *                                          #language fr-FR  "Example Choice 2"
 *
 * 6. declare a new efivarstore for the option in Library/SetupMenuUiLib/SetupMenuVfr.Vfr
 *    We are using a separate store for each option since coreboot's get_uint_option routine
 *    is only capable of extracting the first 4 bytes of an efivar.
 *
 *      efivarstore OPTION_STORAGE,                                                  // underlying datastruture, no adjustments
 *        attribute = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,   // no adjustments needed here
 *        name      = NewOpt,                                                        // efivar name (see step 1, L"NewOpt")
 *        guid      = gEficorebootNvDataGuid;                                        // no adjustments needed here
 *
 * 7. finally, create the desired OneOf question for your new option
 *    in Library/SetupMenuUiLib/SetupMenuVfr.Vfr
 *
 *      oneof name = OneOfNewOpt,
 *
 *        varid  = NewOpt.Value,
 *        prompt = STRING_TOKEN(STR_NEWOPT_PROMPT),
 *        help   = STRING_TOKEN(STR_NEWOPT_HELP),
 *
 *        option text = STRING_TOKEN(STR_CHOICE_ONE), value = 0x1, flags = 0;
 *        option text = STRING_TOKEN(STR_CHOICE_TWO), value = 0x2, flags = 0;
 *
 *      endoneof;
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

typedef struct {

  CHAR16 *key;
  UINT8   dfl;
  
} BootOptionDefault;

BootOptionDefault gDefaults[MAX_OPTION_KEYS] = {

  { .key = OPT_HYPERTHREADING,  .dfl = OPT_HYPERTHREADING_DFL  },
  { .key = OPT_TURBOMODE,       .dfl = OPT_TURBOMODE_DFL       },
  { .key = OPT_CX,              .dfl = OPT_CX_DFL              },
  { .key = OPT_CX_LIMIT,        .dfl = OPT_CX_LIMIT_DFL        },
  { .key = OPT_PRIMARY_DISPLAY, .dfl = OPT_PRIMARY_DISPLAY_DFL },
  { .key = OPT_EE_TURBO,        .dfl = OPT_EE_TURBO_DFL        },
  { .key = OPT_LLC_DEADLINE,    .dfl = OPT_LLC_DEADLINE_DFL    },
  { .key = OPT_INTEL_VTX,       .dfl = OPT_INTEL_VTX_DFL       },
  { .key = OPT_INTEL_VTD,       .dfl = OPT_INTEL_VTD_DFL       },
  { .key = OPT_SECURE_BOOT,     .dfl = OPT_SECURE_BOOT_DFL     },
  { .key = OPT_PXE_RETRIES,     .dfl = OPT_PXE_RETRIES_DFL     },
  { .key = OPT_PWR_G3,          .dfl = OPT_PWR_G3_DFL          },
  { .key = OPT_PCIE_SSC,        .dfl = OPT_PCIE_SSC_DFL        },
  { .key = OPT_PCIE_SRIS,       .dfl = OPT_PCIE_SRIS_DFL       },
  { .key = OPT_IBECC,           .dfl = OPT_IBECC_DFL           }
};

/* GUID of this formset */
EFI_GUID mSetupMenuGuid = SETUP_MENU_FORMSET_GUID;

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
PublishDefaultConfiguration (
  VOID
)
{  
  EFI_STATUS Status;
  UINT8      Iterator;

  for (Iterator = 0; Iterator < MAX_OPTION_KEYS; Iterator++) {

    Status = gRT->SetVariable (gDefaults[Iterator].key, &gEficorebootNvDataGuid,
                               EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                               sizeof (UINT8), &(gDefaults[Iterator].dfl));

    if (EFI_ERROR (Status))
      break;
  }

  return Status;
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
  if (Progress == NULL || Results == NULL)
    return EFI_INVALID_PARAMETER;

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
  if (Configuration == NULL || Progress == NULL)
    return EFI_INVALID_PARAMETER;

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

  if (!EfiVarExists (gDefaults[0].key, gEficorebootNvDataGuid)) {
  
    Status = PublishDefaultConfiguration ();
    ASSERT (!EFI_ERROR (Status));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetupMenuUiLibDestructor(
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
)
{
  EFI_STATUS Status;

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
