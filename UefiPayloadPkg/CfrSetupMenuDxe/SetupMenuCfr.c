/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.
  This file parses CFR to produce HII IFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SetupMenu.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CfrHelpersLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/VariablePolicyHelperLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/CfrSetupMenuGuid.h>
#include <Guid/VariableFormat.h>

/**
  Produce unconditional HII `*_IF` for CFR flags.

  Caller to close each `*_IF` with `HiiCreateEndOpCode()`.

**/
STATIC
VOID
EFIAPI
CfrProduceHiiForFlags (
  IN VOID    *StartOpCodeHandle,
  IN UINT16  QuestionId,
  IN UINT8   OpCode
  )
{
  EFI_IFR_OP_HEADER  IfOpHeader;
  UINT8              *TempHiiBuffer;
  EFI_IFR_EQ_ID_VAL  ConditionEqIdVal;
  EFI_IFR_TRUE       ConditionTrue;

  IfOpHeader.OpCode = OpCode;
  IfOpHeader.Length = sizeof (EFI_IFR_OP_HEADER);
  // `if` statements are new scopes
  IfOpHeader.Scope = 1;

  TempHiiBuffer = HiiCreateRawOpCodes (
                    StartOpCodeHandle,
                    (UINT8 *)&IfOpHeader,
                    sizeof (EFI_IFR_OP_HEADER)
                    );
  ASSERT (TempHiiBuffer != NULL);

  if (QuestionId != 0) {
    ConditionEqIdVal.Header.OpCode = EFI_IFR_EQ_ID_VAL_OP;
    ConditionEqIdVal.Header.Length = sizeof (ConditionEqIdVal);
    // Same scope as above statement
    ConditionEqIdVal.Header.Scope = 0;
    ConditionEqIdVal.QuestionId   = QuestionId;
    ConditionEqIdVal.Value        = 0;

    TempHiiBuffer = HiiCreateRawOpCodes (
                      StartOpCodeHandle,
                      (UINT8 *)&ConditionEqIdVal,
                      sizeof (ConditionEqIdVal)
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else {
    ConditionTrue.Header.OpCode = EFI_IFR_TRUE_OP;
    ConditionTrue.Header.Length = sizeof (ConditionTrue);
    // Same scope as above statement
    ConditionTrue.Header.Scope = 0;
    TempHiiBuffer              = HiiCreateRawOpCodes (
                                   StartOpCodeHandle,
                                   (UINT8 *)&ConditionTrue,
                                   sizeof (ConditionTrue)
                                   );
    ASSERT (TempHiiBuffer != NULL);
  }
}

/**
  Produce variable and VARSTORE for CFR option name.

**/
STATIC
EFI_STATUS
EFIAPI
CfrProduceStorageForOption (
  IN CHAR16  *VariableName,
  IN VOID    *CfrOptionDefaultValue,
  IN UINTN   CfrOptionLength,
  IN UINT8   OptionFlags,
  IN VOID    *StartOpCodeHandle,
  IN UINTN   QuestionIdVarStoreId
  )
{
  UINT32            VariableAttributes;
  UINTN             DataSize;
  EFI_STATUS        Status;
  UINTN             OptionNameLength;
  UINTN             VarStoreStructSize;
  EFI_IFR_VARSTORE  *VarStore;
  UINT8             *TempHiiBuffer;
  CHAR8             *AsciiVariableName;

  if ((VariableName == NULL) || (CfrOptionDefaultValue == NULL) ||
      (StartOpCodeHandle == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  OptionNameLength  = StrLen (VariableName) + 1;
  AsciiVariableName = AllocatePool (OptionNameLength * sizeof (CHAR8));
  if (AsciiVariableName == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeStrToAsciiStrS (VariableName, AsciiVariableName, OptionNameLength);

  //
  // Variables can be runtime accessible later, if desired
  //
  VariableAttributes = EFI_VARIABLE_BOOTSERVICE_ACCESS;
  if (!(OptionFlags & CFR_OPTFLAG_VOLATILE)) {
    VariableAttributes |= EFI_VARIABLE_NON_VOLATILE;
  }

  if (Option->Flags & CFR_OPTFLAG_RUNTIME) {
    VariableAttributes |= EFI_VARIABLE_RUNTIME_ACCESS;
  }

  DataSize = 0;
  Status   = gRT->GetVariable (
                    VariableName,
                    &gEficorebootNvDataGuid,
                    NULL,
                    &DataSize,
                    NULL
                    );
  if (Status == EFI_NOT_FOUND) {
    DataSize = CfrOptionLength;
    Status   = gRT->SetVariable (
                      VariableName,
                      &gEficorebootNvDataGuid,
                      VariableAttributes,
                      DataSize,
                      CfrOptionDefaultValue
                      );
    ASSERT_EFI_ERROR (Status);
  }

  if (OptionFlags & CFR_OPTFLAG_READONLY && (mVariablePolicy != NULL)) {
    Status = RegisterBasicVariablePolicy (
               mVariablePolicy,
               &gEficorebootNvDataGuid,
               VariableName,
               VARIABLE_POLICY_NO_MIN_SIZE,
               VARIABLE_POLICY_NO_MAX_SIZE,
               VARIABLE_POLICY_NO_MUST_ATTR,
               VARIABLE_POLICY_NO_CANT_ATTR,
               VARIABLE_POLICY_TYPE_LOCK_NOW
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "CFR: Failed to lock variable \"%s\"!\n", VariableName));
    }
  }

  //
  // Build a `varstore` and copy it as raw HII opcodes. Then free this
  //
  // Struct contains space for terminator only, allocate with name too
  VarStoreStructSize = sizeof (EFI_IFR_VARSTORE) + OptionNameLength;
  ASSERT (VarStoreStructSize <= 0x7F);
  if (VarStoreStructSize > 0x7F) {
    DEBUG ((DEBUG_ERROR, "CFR: Option name length 0x%x is too long!\n", OptionNameLength));
    FreePool (AsciiVariableName);
    return EFI_OUT_OF_RESOURCES;
  }

  VarStore = AllocateZeroPool (VarStoreStructSize);
  ASSERT (VarStore != NULL);
  if (VarStore == NULL) {
    DEBUG ((DEBUG_ERROR, "CFR: Failed to allocate memory for varstore!\n"));
    FreePool (AsciiVariableName);
    return EFI_OUT_OF_RESOURCES;
  }

  VarStore->Header.OpCode = EFI_IFR_VARSTORE_OP;
  VarStore->Header.Length = VarStoreStructSize;

  // Direct mapping
  VarStore->VarStoreId = QuestionIdVarStoreId;
  VarStore->Size       = CfrOptionLength;

  CopyMem (&VarStore->Guid, &gEficorebootNvDataGuid, sizeof (EFI_GUID));
  CopyMem (VarStore->Name, AsciiVariableName, OptionNameLength);

  TempHiiBuffer = HiiCreateRawOpCodes (
                    StartOpCodeHandle,
                    (UINT8 *)VarStore,
                    VarStoreStructSize
                    );
  ASSERT (TempHiiBuffer != NULL);
  FreePool (VarStore);
  FreePool (AsciiVariableName);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
CfrFormCallback  (
  IN CONST CFR_HEADER  *Root,
  IN CONST CFR_HEADER  *Child,
  IN OUT   VOID        *Private
  );

/**
  Process one CFR form - its UI name - and create HII component.
  Therefore, *do not* advanced index by the size field.

  It's currently too difficult to produce form HII IFR, because these
  seem unable to be nested, so generating the VfrBin at runtime would be required.
  However, maybe we'll look into that, or HII "scopes" later.

**/
STATIC
EFI_STATUS
EFIAPI
CfrProcessFormOption (
  IN     CFR_OPTION_FORM  *Option,
  IN     VOID             *StartOpCodeHandle,
  IN     UINT16           FormId
  )
{
  UINTN          DependencyQuestionId;
  UINT8          *TempHiiBuffer;
  EFI_STATUS     Status;
  EFI_STRING_ID  HiiDisplayStringId;
  EFI_STRING_ID  HiiHelpTextId;
  CHAR16         *DisplayName;

  if (Option->Flags & CFR_OPTFLAG_SUPPRESS) {
    return EFI_SUCCESS;
  }

  DependencyQuestionId = CFR_COMPONENT_START + Option->DependencyId;

  Status = CfrExtractString ((UINT8 *)Option, CB_CFR_TAG_VARCHAR_UI_NAME, &DisplayName);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "CFR: Item (Tag 0x%x, Size 0x%x) has no DisplayName: %r\n",
      Option->Tag,
      Option->Size,
      Status
      ));
    return Status;
  }

  Status = SetupMenuExtractHiiString ((UINT8 *)Option, CB_CFR_TAG_VARCHAR_UI_NAME, &HiiDisplayStringId);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    FreePool (DisplayName);
    DEBUG ((
      DEBUG_ERROR,
      "CFR: Item (Tag 0x%x, Size 0x%x) has no HiiDisplayStringId: %r\n",
      Option->Tag,
      Option->Size,
      Status
      ));
    return Status;
  }

  Status = SetupMenuExtractHiiString ((UINT8 *)Option, CB_CFR_TAG_VARCHAR_UI_HELPTEXT, &HiiHelpTextId);
  if (EFI_ERROR (Status)) {
    Status        = EFI_SUCCESS;
    HiiHelpTextId = STRING_TOKEN (STR_EMPTY_STRING);
  }

  DEBUG ((
    DEBUG_INFO,
    "CFR: Process form[%d] \"%s\" of size 0x%x\n",
    Option->ObjectId,
    DisplayName,
    Option->Size
    ));

  if (Option->Flags & CFR_OPTFLAG_SUPPRESS) {
    CfrProduceHiiForFlags (StartOpCodeHandle, 0, EFI_IFR_SUPPRESS_IF_OP);
  } else if ((DependencyQuestionId != 0) || Option->Flags & CFR_OPTFLAG_GRAYOUT) {
    CfrProduceHiiForFlags (StartOpCodeHandle, DependencyQuestionId, EFI_IFR_GRAY_OUT_IF_OP);
  }

  if (FormId != 0) {
    HiiCreateGotoOpCode (
      StartOpCodeHandle,
      FormId,
      HiiDisplayStringId,
      HiiHelpTextId,
      EFI_IFR_FLAG_CALLBACK,
      FormId
      );
  } else {
    TempHiiBuffer = HiiCreateSubTitleOpCode (
                      StartOpCodeHandle,
                      HiiDisplayStringId,
                      STRING_TOKEN (STR_EMPTY_STRING),
                      0,
                      0
                      );
    ASSERT (TempHiiBuffer != NULL);

    Status = CfrWalkTree ((UINT8 *)Option, CfrFormCallback, StartOpCodeHandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "CFR: Error walking form: %r\n",
        Status
        ));
    }
  }

  if ((DependencyQuestionId != 0) || (Option->Flags & (CFR_OPTFLAG_GRAYOUT | CFR_OPTFLAG_SUPPRESS))) {
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }

  FreePool (DisplayName);

  return Status;
}

/**
  Iterate over CB_CFR_TAG_ENUM_VALUE fields and invoked
  HiiCreateOneOfOptionOpCode for each.

**/
STATIC
EFI_STATUS
EFIAPI
CfrEnumCallback  (
  IN CONST CFR_HEADER  *Root,
  IN CONST CFR_HEADER  *Child,
  IN OUT   VOID        *Private
  )
{
  CFR_ENUM_VALUE  *CfrEnumValue;
  EFI_STATUS      Status;
  EFI_STRING_ID   HiiEnumStringsId;
  UINT8           *TempHiiBuffer;
  VOID            *OptionOpCodeHandle;

  if ((Root == NULL) || (Child == NULL) || (Private == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Child->Tag != CB_CFR_TAG_ENUM_VALUE) {
    return EFI_SUCCESS;
  }

  CfrEnumValue       = (CFR_ENUM_VALUE *)Child;
  OptionOpCodeHandle = Private;

  Status = SetupMenuExtractHiiString (
             (UINT8 *)Child,
             CB_CFR_TAG_VARCHAR_UI_NAME,
             &HiiEnumStringsId
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  TempHiiBuffer = HiiCreateOneOfOptionOpCode (
                    OptionOpCodeHandle,
                    HiiEnumStringsId,
                    0,
                    EFI_IFR_TYPE_NUM_SIZE_32,
                    CfrEnumValue->Value
                    );
  ASSERT (TempHiiBuffer != NULL);
  return EFI_SUCCESS;
}

/**
  Process one CFR numeric option and create HII component.

**/
STATIC
EFI_STATUS
EFIAPI
CfrProcessNumericOption (
  IN     CFR_OPTION_NUMERIC  *Option,
  IN     VOID                *StartOpCodeHandle,
  IN     CHAR16              *DisplayName,
  IN     EFI_STRING_ID       HiiDisplayStringId,
  IN     EFI_STRING_ID       HiiHelpTextId,
  IN     CHAR16              *OptionName
  )
{
  UINTN       QuestionIdVarStoreId;
  UINTN       DependencyQuestionId;
  UINT8       QuestionFlags;
  VOID        *DefaultOpCodeHandle;
  UINT8       *TempHiiBuffer;
  VOID        *OptionOpCodeHandle;
  EFI_STATUS  Status;

  DEBUG ((
    DEBUG_INFO,
    "CFR: Process option[%llx] \"%s\" of size 0x%x\n",
    Option->ObjectId,
    OptionName,
    Option->Size
    ));

  if (Option->Flags & CFR_OPTFLAG_SUPPRESS) {
    return EFI_SUCCESS;
  }

  //
  // Processing start
  //
  QuestionIdVarStoreId = CFR_COMPONENT_START + Option->ObjectId;
  DependencyQuestionId = CFR_COMPONENT_START + Option->DependencyId;
  Status               = CfrProduceStorageForOption (
                           OptionName,
                           &Option->DefaultValue,
                           sizeof (Option->DefaultValue),
                           Option->Flags,
                           StartOpCodeHandle,
                           QuestionIdVarStoreId
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  QuestionFlags = EFI_IFR_FLAG_RESET_REQUIRED;
  if (Option->Flags & CFR_OPTFLAG_READONLY) {
    QuestionFlags |= EFI_IFR_FLAG_READ_ONLY;
  }

  if (Option->Flags & CFR_OPTFLAG_SUPPRESS) {
    CfrProduceHiiForFlags (StartOpCodeHandle, 0, EFI_IFR_SUPPRESS_IF_OP);
  } else if ((DependencyQuestionId != 0) || Option->Flags & CFR_OPTFLAG_GRAYOUT) {
    CfrProduceHiiForFlags (StartOpCodeHandle, DependencyQuestionId);
  }

  DefaultOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (DefaultOpCodeHandle != NULL);

  TempHiiBuffer = HiiCreateDefaultOpCode (
                    DefaultOpCodeHandle,
                    EFI_HII_DEFAULT_CLASS_STANDARD,
                    EFI_IFR_TYPE_NUM_SIZE_32,
                    Option->DefaultValue
                    );
  ASSERT (TempHiiBuffer != NULL);

  //
  // Create HII opcodes, processing complete.
  //
  OptionOpCodeHandle = NULL;
  if (Option->Tag == CB_CFR_TAG_OPTION_ENUM) {
    OptionOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (OptionOpCodeHandle != NULL);

    Status = CfrWalkTree ((UINT8 *)Option, CfrEnumCallback, OptionOpCodeHandle);
    if (EFI_ERROR (Status)) {
      HiiFreeOpCodeHandle (OptionOpCodeHandle);
      return Status;
    }

    TempHiiBuffer = HiiCreateOneOfOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_4,
                      OptionOpCodeHandle,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->Tag == CB_CFR_TAG_OPTION_NUMBER) {
    TempHiiBuffer = HiiCreateNumericOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_4 | EFI_IFR_DISPLAY_UINT_DEC,
                      0x00000000,
                      0xFFFFFFFF,
                      0,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->Tag == CB_CFR_TAG_OPTION_BOOL) {
    // TODO: Or use ONE_OF instead?
    TempHiiBuffer = HiiCreateCheckBoxOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      0,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  }

  if ((DependencyQuestionId != 0) || (Option->Flags & (CFR_OPTFLAG_GRAYOUT | EFI_IFR_SUPPRESS_IF_OP))) {
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }

  if (OptionOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (OptionOpCodeHandle);
  }

  HiiFreeOpCodeHandle (DefaultOpCodeHandle);

  return EFI_SUCCESS;
}

/**
  Process one CFR character option and create HII component.

**/
STATIC
EFI_STATUS
EFIAPI
CfrProcessCharacterOption (
  IN     CFR_OPTION_VARCHAR  *Option,
  IN     VOID                *StartOpCodeHandle,
  IN     CHAR16              *DisplayName,
  IN     EFI_STRING_ID       HiiDisplayStringId,
  IN     EFI_STRING_ID       HiiHelpTextId,
  IN     CHAR16              *OptionName,
  IN     CHAR16              *DefaultStringValue
  )
{
  UINTN          QuestionIdVarStoreId;
  UINTN          DependencyQuestionId;
  CHAR16         *HiiDefaultValue;
  EFI_STRING_ID  HiiDefaultValueId;
  UINTN          HiiDefaultValueLengthChars;
  UINT8          QuestionFlags;
  VOID           *DefaultOpCodeHandle;
  UINT8          *TempHiiBuffer;

  //
  // Extract variable-length fields that follow the header
  //
  ASSERT (sizeof (CFR_OPTION_VARCHAR) == sizeof (CFR_OPTION_COMMENT));

  if (Option->Flags & CFR_OPTFLAG_SUPPRESS) {
    return EFI_SUCCESS;
  }

  DEBUG ((
    DEBUG_INFO,
    "CFR: Process option[%llx] \"%s\" of size 0x%x\n",
    Option->ObjectId,
    (OptionName != NULL) ? OptionName : DisplayName,
    Option->Size
    ));

  QuestionIdVarStoreId = CFR_COMPONENT_START + Option->ObjectId;
  DependencyQuestionId = CFR_COMPONENT_START + Option->DependencyId;

  //
  // Processing start
  //
  if (Option->Tag == CB_CFR_TAG_OPTION_VARCHAR) {
    if (DefaultStringValue == NULL) {
      return EFI_NOT_FOUND;
    }

    if (StrLen (DefaultStringValue) > 0xFF) {
      DEBUG ((DEBUG_ERROR, "CFR: Default value length 0x%x is too long!\n", StrLen (DefaultStringValue)));
      return EFI_BUFFER_TOO_SMALL;
    }

    if (StrLen (DefaultStringValue) > 0) {
      HiiDefaultValueId = HiiSetString (
                            mSetupMenuPrivate.HiiHandle,
                            0,
                            DefaultStringValue,
                            NULL
                            );
      ASSERT (HiiDefaultValueId != 0);
      HiiDefaultValue = DefaultStringValue;
    } else {
      HiiDefaultValue   = HiiGetString (mSetupMenuPrivate.HiiHandle, STRING_TOKEN (STR_INVALID_STRING), NULL);
      HiiDefaultValueId = STRING_TOKEN (STR_INVALID_STRING);
    }

    HiiDefaultValueLengthChars = StrLen (HiiDefaultValue) + 1;

    CfrProduceStorageForOption (
      OptionName,
      HiiDefaultValue,
      HiiDefaultValueLengthChars * sizeof (CHAR16),
      Option->Flags,
      StartOpCodeHandle,
      QuestionIdVarStoreId
      );

    if (HiiDefaultValue != NULL) {
      FreePool (HiiDefaultValue);
    }
  }

  if (Option->Flags & CFR_OPTFLAG_SUPPRESS) {
    CfrProduceHiiForFlags (StartOpCodeHandle, 0, EFI_IFR_SUPPRESS_IF_OP);
  } else if ((DependencyQuestionId != 0) || Option->Flags & CFR_OPTFLAG_GRAYOUT) {
    CfrProduceHiiForFlags (StartOpCodeHandle, DependencyQuestionId);
  }

  //
  // Create HII opcodes, processing complete.
  //
  if (Option->Tag == CB_CFR_TAG_OPTION_VARCHAR) {
    QuestionFlags = EFI_IFR_FLAG_RESET_REQUIRED;
    if (Option->Flags & CFR_OPTFLAG_READONLY) {
      QuestionFlags |= EFI_IFR_FLAG_READ_ONLY;
    }

    DefaultOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (DefaultOpCodeHandle != NULL);

    TempHiiBuffer = HiiCreateDefaultOpCode (
                      DefaultOpCodeHandle,
                      EFI_HII_DEFAULT_CLASS_STANDARD,
                      EFI_IFR_TYPE_NUM_SIZE_16,
                      HiiDefaultValueId
                      );
    ASSERT (TempHiiBuffer != NULL);

    // TODO: User can adjust length of string?
    TempHiiBuffer = HiiCreateStringOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      HiiHelpTextId,
                      QuestionFlags,
                      0,
                      HiiDefaultValueLengthChars - 1,
                      HiiDefaultValueLengthChars - 1,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);

    HiiFreeOpCodeHandle (DefaultOpCodeHandle);
  } else if (Option->Tag == CB_CFR_TAG_OPTION_COMMENT) {
    TempHiiBuffer = HiiCreateTextOpCode (
                      StartOpCodeHandle,
                      HiiDisplayStringId,
                      HiiHelpTextId,
                      STRING_TOKEN (STR_EMPTY_STRING)
                      );
    ASSERT (TempHiiBuffer != NULL);
  }

  if ((DependencyQuestionId != 0) || (Option->Flags & (CFR_OPTFLAG_GRAYOUT | EFI_IFR_SUPPRESS_IF_OP))) {
    TempHiiBuffer = HiiCreateEndOpCode (StartOpCodeHandle);
    ASSERT (TempHiiBuffer != NULL);
  }

  return EFI_SUCCESS;
}

/**
  Iterate over CFR_OPTION_FORM fields and generate
  Hii data for each child.

**/
STATIC
EFI_STATUS
EFIAPI
CfrFormCallback  (
  IN CONST CFR_HEADER  *Root,
  IN CONST CFR_HEADER  *Child,
  IN OUT   VOID        *Private
  )
{
  VOID           *StartOpCodeHandle;
  EFI_STATUS     Status;
  EFI_STRING_ID  HiiDisplayStringId;
  EFI_STRING_ID  HiiHelpTextId;
  CHAR16         *OptionName;
  CHAR16         *DisplayName;
  CHAR16         *DefaultStringValue;

  if ((Root == NULL) || (Child == NULL) || (Private == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  StartOpCodeHandle = Private;

  if (Child->Tag == CB_CFR_TAG_VARCHAR_UI_NAME) {
    return EFI_SUCCESS;
  } else if (Child->Tag == CB_CFR_TAG_OPTION_FORM) {
    DEBUG ((DEBUG_INFO, "CFR: Nested form, will produce subtitle\n"));
    return CfrProcessFormOption (
             (CFR_OPTION_FORM *)Child,
             StartOpCodeHandle,
             0
             );
  }

  // Extract common items
  Status = CfrExtractString ((UINT8 *)Child, CB_CFR_TAG_VARCHAR_UI_NAME, &DisplayName);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "CFR: Item (Tag 0x%x, Size 0x%x) at offset 0x%llx has no DisplayName: %r\n",
      Child->Tag,
      Child->Size,
      (UINTN)((UINT8 *)Child - (UINT8 *)Root),
      Status
      ));
    return EFI_SUCCESS;
  }

  Status = SetupMenuExtractHiiString ((UINT8 *)Child, CB_CFR_TAG_VARCHAR_UI_NAME, &HiiDisplayStringId);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    FreePool (DisplayName);
    DEBUG ((
      DEBUG_ERROR,
      "CFR: Item (Tag 0x%x, Size 0x%x) at offset 0x%llx has no HiiDisplayStringId: %r\n",
      Child->Tag,
      Child->Size,
      (UINTN)((UINT8 *)Child - (UINT8 *)Root),
      Status
      ));
    return EFI_SUCCESS;
  }

  Status = SetupMenuExtractHiiString ((UINT8 *)Child, CB_CFR_TAG_VARCHAR_UI_HELPTEXT, &HiiHelpTextId);
  if (EFI_ERROR (Status)) {
    HiiHelpTextId = STRING_TOKEN (STR_EMPTY_STRING);
  }

  Status = CfrExtractString ((UINT8 *)Child, CB_CFR_TAG_VARCHAR_OPT_NAME, &OptionName);
  if (EFI_ERROR (Status)) {
    // Comments do not have options assigned
    ASSERT (Child->Tag == CB_CFR_TAG_OPTION_COMMENT);
    OptionName = NULL;
  }

  Status = CfrExtractString ((UINT8 *)Child, CB_CFR_TAG_VARCHAR_DEF_VALUE, &DefaultStringValue);
  if (EFI_ERROR (Status)) {
    DefaultStringValue = NULL;
  }

  switch (Child->Tag) {
    case CB_CFR_TAG_OPTION_ENUM:
    case CB_CFR_TAG_OPTION_NUMBER:
    case CB_CFR_TAG_OPTION_BOOL:
      Status = CfrProcessNumericOption (
                 (CFR_OPTION_NUMERIC *)Child,
                 StartOpCodeHandle,
                 DisplayName,
                 HiiDisplayStringId,
                 HiiHelpTextId,
                 OptionName
                 );
      break;
    case CB_CFR_TAG_OPTION_VARCHAR:
    case CB_CFR_TAG_OPTION_COMMENT:
      Status = CfrProcessCharacterOption (
                 (CFR_OPTION_VARCHAR *)Child,
                 StartOpCodeHandle,
                 DisplayName,
                 HiiDisplayStringId,
                 HiiHelpTextId,
                 OptionName,
                 DefaultStringValue
                 );
      break;
    default:
      DEBUG ((
        DEBUG_ERROR,
        "CFR: Offset 0x%llx - Unexpected entry 0x%x (size 0x%x)!\n",
        (UINTN)((UINT8 *)Child - (UINT8 *)Root),
        Child->Tag,
        Child->Size
        ));
      break;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "CFR: Item \"%s\" (Tag 0x%x, Size 0x%x) at offset 0x%llx failed with: %r\n",
      (OptionName != NULL) ? OptionName : DisplayName,
      Child->Tag,
      Child->Size,
      (UINTN)((UINT8 *)Child - (UINT8 *)Root),
      Status
      ));
  }

  FreePool (DisplayName);
  if (OptionName != NULL) {
    FreePool (OptionName);
  }

  if (DefaultStringValue != NULL) {
    FreePool (DefaultStringValue);
  }

  return EFI_SUCCESS;
}

/**
  Create runtime components by iterating CFR forms.

**/
VOID
EFIAPI
CfrCreateRuntimeComponents (
  IN  UINT16  FormId
  )
{
  VOID                *StartOpCodeHandle;
  VOID                *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL  *StartLabel;
  EFI_IFR_GUID_LABEL  *EndLabel;
  EFI_HOB_GUID_TYPE   *GuidHob;
  CFR_OPTION_FORM     *CfrFormHob;
  EFI_STATUS          Status;
  UINTN               Index;

  //
  // Allocate GUIDed markers at runtime component offset in IFR
  //
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);

  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);

  StartLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                       StartOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  ASSERT (StartLabel != NULL);

  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = FormId;

  EndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                     EndOpCodeHandle,
                                     &gEfiIfrTianoGuid,
                                     NULL,
                                     sizeof (EFI_IFR_GUID_LABEL)
                                     );
  ASSERT (EndLabel != NULL);

  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_END;

  //
  // For each HOB, create forms
  //
  GuidHob = GetFirstGuidHob (&gEfiCfrSetupMenuFormGuid);
  Index   = SETUP_SUBMENU01_FORM_ID;
  while (GuidHob != NULL) {
    CfrFormHob = GET_GUID_HOB_DATA (GuidHob);
    if (FormId == FORM_MAIN_ID) {
      if (Index > SETUP_SUBMENU15_FORM_ID) {
        break;
      }

      Status = CfrProcessFormOption (
                 CfrFormHob,
                 StartOpCodeHandle,
                 Index
                 );
      ASSERT_EFI_ERROR (Status);
    } else if (FormId == Index) {
      Status = CfrProcessFormOption (
                 CfrFormHob,
                 StartOpCodeHandle,
                 0
                 );
      ASSERT_EFI_ERROR (Status);

      HiiCreateGotoOpCode (
        StartOpCodeHandle,
        FORM_MAIN_ID,
        STRING_TOKEN (STR_FORM_GOTO_MAIN),
        STRING_TOKEN (STR_FORM_GOTO_MAIN),
        0,
        FORM_MAIN_ID
        );
      break;
    }

    GuidHob = GetNextGuidHob (&gEfiCfrSetupMenuFormGuid, GET_NEXT_HOB (GuidHob));
    Index++;
  }

  //
  // Submit updates
  //
  Status = HiiUpdateForm (
             mSetupMenuPrivate.HiiHandle,
             &mSetupMenuFormsetGuid,
             FormId,
             StartOpCodeHandle,
             EndOpCodeHandle
             );

  ASSERT_EFI_ERROR (Status);
  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);
}
