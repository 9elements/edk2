/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SETUP_MENU_H_
#define _SETUP_MENU_H_

#include <Library/HobLib.h>
#include <PiDxe.h>
#include <Protocol/DevicePath.h>
#include <Protocol/VariablePolicy.h>
#include <Guid/CfrSetupMenuGuid.h>
#include <Guid/MdeModuleHii.h>

#define SETUP_MENU_CALLBACK_DATA_SIGNATURE  SIGNATURE_32 ('D', 'M', 'C', 'B')

typedef struct {
  UINTN Signature;
  EFI_HII_HANDLE HiiHandle;
  EFI_HANDLE     DriverHandle;
} SETUP_MENU_DATA;

typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

extern EFI_GUID        mSetupMenuHiiGuid;
extern SETUP_MENU_DATA mSetupMenuPrivate;

extern HII_VENDOR_DEVICE_PATH           mSetupMenuHiiVendorDevicePath;
extern EDKII_VARIABLE_POLICY_PROTOCOL  *mVariablePolicy;

typedef struct {
  CHAR8        *Data;
  UINTN         Size;
  EFI_STRING_ID CountStrs;
} BUFFER;

//TODO originally defined in MdeModulePkg/Library/UefiHiiLib/HiiLib.c but sadly not exposed in an includable header
typedef struct {
  UINT8    *Buffer;
  UINTN    BufferSize;
  UINTN    Position;
} HII_LIB_OPCODE_BUFFER;

EFI_STRING_ID
AddHiiStringBlock (
  OUT       BUFFER *StrsBuf,
  IN        UINT8   BlockType,
  IN  CONST CHAR8  *Str
  );

VOID
ProcessFormSet (
  IN VOID *StartOpCodeHandle,
  IN CFR_FORM *Form,
  IN BUFFER *StrsBuf
  );

VOID
CfrSetDefaultOptions (
    IN CFR_FORM *CfrForm
  );

#endif
