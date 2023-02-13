/** @file
  This file defines the hob structure for bootloader's boot options

  Copyright (c) 2020, 9elements GmbH. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOT_OPTIONS_GUID_H__
#define __BOOT_OPTIONS_GUID_H__

///
/// Boot Options table GUID
///
extern EFI_GUID gEfiBootOptionsTableGuid;

#pragma pack(1)

struct BoardOptionDefaults {
  UINT8 Option;
  UINT8 DefaultValue;
  UINT8 Reserved[2];
};

typedef struct {
  UINT32 Count;
  struct BoardOptionDefaults OptionDefaults[];
} BOOT_OPTIONS;

#pragma pack()

#endif // __BOOT_OPTIONS_GUID_H__
