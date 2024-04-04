/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.
  This file implements the HII Config Access protocol.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SetupMenu.h"
#include <Library/BaseLib.h>
#include <Library/CfrHelpersLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/VariableFormat.h>

SETUP_MENU_DATA  mSetupMenuPrivate = {
  SETUP_MENU_CALLBACK_DATA_SIGNATURE,
  NULL,
  NULL,
};

#define SETUP_MENU_HII_GUID \
    { \
      0x93E6FCD9, 0x8E17, 0x43DF, { 0xB7, 0xF0, 0x91, 0x3E, 0x58, 0xB1, 0xA7, 0x89 } \
    }

EFI_GUID mSetupMenuHiiGuid = SETUP_MENU_HII_GUID;

//TODO do we actually need this?
HII_VENDOR_DEVICE_PATH  mSetupMenuHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    SETUP_MENU_HII_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};
