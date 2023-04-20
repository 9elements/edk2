/** @file
  This file defines the hob structure for bootloader's CFR option menu.

  Copyright (c) 2023, 9elements GmbH. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CFR_SETUP_MENU_GUID_H__
#define __CFR_SETUP_MENU_GUID_H__

///
/// CFR options form GUID
///
extern EFI_GUID gEfiCfrSetupMenuFormGuid;


/*
 * The following tags are for CFR (Cursed Form Representation) entries
 * - Member fields intentionally commented so that bootloader `sizeof()`
     determines size as known
 */
enum cfr_option_flags {
  SETUP_MENU_VAR_FLAG_READONLY	= 1 << 0,
  SETUP_MENU_VAR_FLAG_GRAYOUT	= 1 << 1,
  SETUP_MENU_VAR_FLAG_SUPPRESS	= 1 << 2,
};

/*
 * 1. `length` of the string
 * 2. Aligned to `LB_ENTRY_ALIGN`
 */
#define LB_ENTRY_ALIGN  4
typedef struct {
  UINT32 length;
  UINT8  str[];
} CFR_STRING;

#define CB_TAG_CFR_FORM  0x0101
typedef struct {
  UINT32 tag;
  UINT32 size;
  /* CFR_STRING ui_name; */
} CFR_FORM;

#define CB_TAG_CFR_ENUM_VALUE  0x0103
typedef struct {
  UINT32 tag;
  UINT32 size;
  UINT32 value;
  /* CFR_STRING ui_name; */
} CFR_ENUM_VALUE;

#define CB_TAG_CFR_OPTION_ENUM  0x0104
#define CB_TAG_CFR_OPTION_NUMBER  0x0105
#define CB_TAG_CFR_OPTION_BOOL 0x0106
typedef struct {
  UINT32 tag;
  UINT32 size;
  UINT32 flags;
  UINT32 default_value;
  /* CFR_STRING opt_name; */
  /* CFR_STRING ui_name; */
  /* CFR_ENUM_VALUE  enum_values[]; */
} CFR_NUMERIC_OPTION;

#endif // __CFR_SETUP_MENU_GUID_H__
