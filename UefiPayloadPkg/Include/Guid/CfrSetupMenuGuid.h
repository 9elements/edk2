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
extern EFI_GUID  gEfiCfrSetupMenuFormGuid;

//
// The following tags are for CFR (Cursed Form Representation) entries.
//
// CFR records form a tree structure. The size of a record includes
// the size of its own fields plus the size of all children records.
// CFR tags can appear multiple times except for `LB_TAG_CFR` which
// is used for the root record.
//
// The following structures have comments that describe the supported
// children records. These comments cannot be replaced with code! The
// structures are variable-length, so the offsets won't be valid most
// of the time. Besides, the implementation uses `sizeof()` to obtain
// the size of the "record header" (the fixed-length members); adding
// the children structures as struct members will increase the length
// returned by `sizeof()`, which complicates things for zero reason.
//
// The optional flags describe the visibilty of the option and the
// effect on the non-volatile variable.
// CFR_OPTFLAG_READONLY:
//   Prevents writes to the variable.
// CFR_OPTFLAG_GRAYOUT:
//   Implies READONLY. The option is visible, but cannot be modified
//   because one of the dependencies are not given. However there's a
//   possibilty to enable the option by changing runtime configuration.
// CFR_OPTFLAG_SUPPRESS:
//   Runtime code sets this flag to indicate that the option has no effect
//   and is never reachable, not even by changing runtime configuration.
//   This option is never shown in the UI.
// CFR_OPTFLAG_VOLATILE:
//   Implies READONLY.
//   The option is not backed by a non-volatile variable. This is useful
//   to display the current state of a specific component, a dependency or
//   a serial number. This information could be passed in a new coreboot
//   table, but it not useful other than to be shown at this spot in the
//   UI.

enum CFR_TAGS {
  CB_CFR_TAG_OPTION_FORM         = 1,
  CB_CFR_TAG_ENUM_VALUE          = 2,
  CB_CFR_TAG_OPTION_ENUM         = 3,
  CB_CFR_TAG_OPTION_NUMBER       = 4,
  CB_CFR_TAG_OPTION_BOOL         = 5,
  CB_CFR_TAG_OPTION_VARCHAR      = 6,
  CB_CFR_TAG_VARCHAR_OPT_NAME    = 7,
  CB_CFR_TAG_VARCHAR_UI_NAME     = 8,
  CB_CFR_TAG_VARCHAR_UI_HELPTEXT = 9,
  CB_CFR_TAG_VARCHAR_DEF_VALUE   = 10,
  CB_CFR_TAG_OPTION_COMMENT      = 11,
};

enum CFR_OPTION_FLAGS {
  CFR_OPTFLAG_READONLY = 1 << 0,
  CFR_OPTFLAG_GRAYOUT  = 1 << 1,
  CFR_OPTFLAG_SUPPRESS = 1 << 2,
  CFR_OPTFLAG_VOLATILE = 1 << 3,
  CFR_OPTFLAG_RUNTIME  = 1 << 4,
};

#pragma pack(1)
//
// The CFR varbinary describes a variable length data
// typically used by strings.
//
typedef struct {
  UINT32    Tag;        // Any CFR_VARBINARY or CFR_VARCHAR
  UINT32    Size;       // Length of the entire structure
  UINT32    DataLength; // Length of data, including NULL terminator for strings
  UINT8     Data[];
} CFR_VARBINARY;

//
// A CFR enum value describes a possible option that is
// used by a CFR_OPTION_NUMERIC option of type CFR_OPTION_ENUM
//
typedef struct {
  UINT32    Tag;
  UINT32    Size;
  UINT32    Value;
  //
  // Child nodes:
  // CFR_VARBINARY UiName
  //
} CFR_ENUM_VALUE;

//
// A CFR numeric describes an integer option.
// Integer options are used by
//  - enum
//  - number
//  - boolean
//
typedef struct {
  UINT32    Tag;                 // One of CFR_OPTION_ENUM, CFR_OPTION_NUMBER, CFR_OPTION_BOOL
  UINT32    Size;                // Size including this header and variable body
  UINT64    ObjectId;            // Uniqueue ID
  UINT64    DependencyId;        // If non 0, check the referenced option of type
                                 // lb_cfr_numeric_option and Grayout if value is 0.
                                 // Ignore if field is 0.
  UINT32    Flags;               // enum CFR_OPTION_FLAGS
  UINT32    DefaultValue;

  //
  // Child nodes:
  // CFR_VARBINARY      OptionName
  // CFR_VARBINARY      UiName
  // CFR_VARBINARY      UiHelpText (Optional)
  // CFR_ENUM_VALUE     EnumValues[] (Optional)
  //
} CFR_OPTION_NUMERIC;

//
// A CFR varchar describes a string option
//
typedef struct {
  UINT32    Tag;                 // CFR_OPTION_VARCHAR
  UINT32    Size;                // Size including this header and variable body
  UINT64    ObjectId;            // Uniqueue ID
  UINT64    DependencyId;        // If non 0, check the referenced option of type
                                 // lb_cfr_numeric_option and Grayout if value is 0.
                                 // Ignore if field is 0.
  UINT32    Flags;               // enum CFR_OPTION_FLAGS

  //
  // Child nodes:
  // CFR_VARBINARY      OptionName
  // CFR_VARBINARY      UiName
  // CFR_VARBINARY      UiHelpText (Optional)
  // CFR_VARBINARY      DefaultValue
  //
} CFR_OPTION_VARCHAR;

//
// A CFR option comment is roughly equivalent to a Kconfig comment.
// Option comments are *NOT* string options (see CFR_OPTION_VARCHAR
// instead) but they're considered an option for simplicity's sake.
//
typedef struct {
  UINT32    Tag;                 // CFR_OPTION_COMMENT
  UINT32    Size;                // Size including this header and variable body
  UINT64    ObjectId;            // Uniqueue ID
  UINT64    DependencyId;        // If non 0, check the referenced option of type
                                 // lb_cfr_numeric_option and Grayout if value is 0.
                                 // Ignore if field is 0.
  UINT32    Flags;               // enum CFR_OPTION_FLAGS
  //
  // Child nodes:
  // CFR_VARBINARY      UiName
  // CFR_VARBINARY      UiHelpText (Optional)
  //
} CFR_OPTION_COMMENT;

//
// CFR forms are considered options as they can be nested inside other forms
//
typedef struct {
  UINT32    Tag;                 // CFR_OPTION_FORM
  UINT32    Size;                // Size including this header and variable body
  UINT64    ObjectId;            // Uniqueue ID
  UINT64    DependencyId;        // If non 0, check the referenced option of type
                                 // lb_cfr_numeric_option and Grayout if value is 0.
                                 // Ignore if field is 0.
  UINT32    Flags;               // enum CFR_OPTION_FLAGS

  //
  // Child nodes:
  // CFR_VARBINARY       UiName
  // CFR_OPTION_COMMENT  Options[] (Optional)
  // CFR_OPTION_VARCHAR  Options[] (Optional)
  // CFR_OPTION_NUMERIC  Options[] (Optional)
  //
} CFR_OPTION_FORM;

//
// The CFR header is the same for defined structs above.
//
typedef struct {
  UINT32    Tag;
  UINT32    Size;
} CFR_HEADER;

#pragma pack(0)

#endif // __CFR_SETUP_MENU_GUID_H__
