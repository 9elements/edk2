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
extern EFI_GUID  gEfiCfrSetupMenuStringsGuid;

//
// The following tags are for CFR (Cursed Form Representation) entries.
//
// CFR records form a tree structure. The size of a record includes
// the size of its own fields plus the size of all children records.
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
// CFR_OPTFLAG_VOLATILE:
//   Implies READONLY.
//   The option is not backed by a non-volatile variable. This is useful
//   to display the current state of a specific component, a dependency or
//   a serial number. This information could be passed in a new coreboot
//   table, but it not useful other than to be shown at this spot in the
//   UI.

typedef enum {
  CFR_TAG_FORM,
  CFR_TAG_ENUM_VALUE,
  CFR_TAG_OPTION_ENUM,
  CFR_TAG_OPTION_NUM_32,
  CFR_TAG_OPTION_NUM_64,
  CFR_TAG_OPTION_BOOL,
  CFR_TAG_OPTION_STRING,
  CFR_TAG_OPT_NAME,
  CFR_TAG_PROMPT,
  CFR_TAG_HELP,
  CFR_TAG_DEFAULT,
  CFR_TAG_COMMENT,
  CFR_TAG_PROMPT_VALUE,
  CFR_TAG_DEFAULT_VALUE,

  CFR_TAG_DEFAULT_VISIBLE,
  CFR_TAG_PROMPT_VISIBLE,
} CFR_TAG;

typedef enum {
  CFR_EXPR_TYPE_CONST_BOOL,
  CFR_EXPR_TYPE_CONST_NUM,
  CFR_EXPR_TYPE_CONST_STRING,
  CFR_EXPR_TYPE_SYMBOL,
  CFR_EXPR_TYPE_OPERATOR,
} CFR_EXPR_TYPE;

typedef enum {
  CFR_OPERATOR_NOT,
  CFR_OPERATOR_OR,
  CFR_OPERATOR_AND,
  CFR_OPERATOR_EQUAL
} CFR_OPERATOR;

typedef enum {           
  CFR_OPTFLAG_VOLATILE = 1 << 0,
  CFR_OPTFLAG_RUNTIME  = 1 << 1,
} CFR_OPTION_FLAGS;

#pragma pack(1)

typedef struct {
  UINT32 Tag;
  UINT32 Size;
} CFR_HEADER;

typedef struct {
  UINT32 Tag;   // CFR_TAG_DEFAULT
  UINT32 Size;  // size of struct (including optional structs below)
  UINT64 Value;
  /*
   * CFR_EXPRESSION  default_value_visible (Optional)
   */
} CFR_DEFAULT;

typedef UINT64 CFR_STRING;

typedef struct {
  UINT32 Tag;   // CFR_TAG_PROMPT
  UINT32 Size;  // size of struct (including optional structs below)
  CFR_STRING Prompt;
  /*
   * CFR_EXPRESSION  prompt_visible (Optional)
   */
} CFR_PROMPT;

typedef struct {
  UINT32 Type; // enum cfr_expr_type
  UINT64 Value;
} CFR_EXPR;

typedef struct {
  UINT32 Tag;  // CFR_TAG_VISIBLE
  UINT32 Size; // Length of the entire structure
  CFR_EXPR Exprs[]; // Expression stack
} CFR_EXPRESSION;

typedef struct {
  UINT32 Tag;   // CFR_TAG_ENUM_VALUE
  UINT32 Size;
  UINT64 Value;
  CFR_STRING Prompt;
} CFR_ENUM_VALUE;

typedef struct {
  UINT32 Tag;       // CFR_TAG_OPTION_*
  UINT32 Size;      // size of struct (including optional structs below)
  UINT64 ObjectId;  // Uniqueue ID
  UINT32 Flags;     // enum cfr_option_flags
  CFR_STRING Name;
  CFR_STRING Help;
  /*
   * CFR_PROMPT   prompt (Optional)
   * CFR_DEFAULT  default (Optional)
   *
   * if CFR_TAG_OPTION_ENUM:
   * struct lb_cfr_enum_value  enum_values[]
   */
} CFR_OPTION;

typedef struct {
  UINT32 Tag;   // CFR_TAG_COMMENT
  UINT32 Size;  // size of struct (including optional structs below)
  /*
   * CFR_PROMPT  prompt
   */
} CFR_COMMENT;

typedef struct {
  UINT32 Tag;   // CFR_TAG_FORM
  UINT32 Size;  // size of struct (including optional structs below)
  /*
   * CFR_PROMPT  prompt
   * CFR_OPTION,CFR_COMMENT,CFR_FORM  stuff[]
   */
} CFR_FORM;

typedef struct {
  UINT32 Size;
  CHAR8 Strings[];
} CFR_STRINGS;

#pragma pack(0)

#endif // __CFR_SETUP_MENU_GUID_H__
