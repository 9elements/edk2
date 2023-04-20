/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.
  This file parses CFR to produce HII IFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SetupMenu.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/CfrSetupMenuGuid.h>
#include <Guid/VariableFormat.h>

/**
  CFR_STRING structs are aligned on 4-byte boundaries.
  Implement this by incrementing past the end of the struct.

**/
STATIC
CFR_STRING *
EFIAPI
ExtractSingleCfrString (
  IN     UINT8  *Buffer,
  IN OUT UINTN  *ProcessedLength
  )
{
  CFR_STRING  *CfrString;

  CfrString = (CFR_STRING *)(Buffer + *ProcessedLength);
  *ProcessedLength = ALIGN_VALUE (
                       *ProcessedLength + sizeof (CFR_STRING)
                       + CfrString->length,
                       LB_ENTRY_ALIGN
                       );

  return CfrString;
}

/**
  CFR_STRING is used as option name and UI name.
  Convert these to formats used for EDK2 HII.

  Caller is responsible for freeing the UnicodeString pool.

**/
STATIC
VOID
EFIAPI
ConvertCfrString (
  IN     CFR_STRING     *CfrString,
  IN OUT CHAR16         **UnicodeString,
  IN OUT EFI_STRING_ID  *HiiStringId      OPTIONAL
  )
{
  EFI_STATUS  Status;

  ASSERT ((CfrString != NULL) && (UnicodeString != NULL));

  *UnicodeString = AllocatePool (CfrString->length * sizeof (CHAR16));
  Status = AsciiStrToUnicodeStrS (
             (CHAR8 *)CfrString->str,
             *UnicodeString,
             CfrString->length
             );
  ASSERT_EFI_ERROR (Status);

  if (HiiStringId != NULL) {
    *HiiStringId = HiiSetString (
                     gSetupMenuPrivate.HiiHandle,
                     0,
                     *UnicodeString,
                     NULL
                     );
    ASSERT (*HiiStringId != 0);
  }
}

/**
  Process one CFR option and create HII component.

**/
VOID
EFIAPI
ProcessOption (
  IN     CFR_NUMERIC_OPTION  *Option,
  IN     VOID                *StartOpCodeHandle,
  IN     UINTN               QuestionIdVarStoreId,
  IN OUT UINTN               *ProcessedLength
  )
{
  UINTN             OptionProcessedLength;
  CFR_STRING        *CfrOptionName;
  CFR_STRING        *CfrDisplayName;
  CHAR16            *VariableCfrName;
  UINTN             DataSize;
  EFI_STATUS        Status;
  UINTN             VarStoreStructSize;
  EFI_IFR_VARSTORE  *VarStore;
  UINT8             *TempHiiBuffer;
  UINT8             QuestionFlags;
  VOID              *DefaultOpCodeHandle;
  CHAR16            *HiiDisplayString;
  EFI_STRING_ID     HiiDisplayStringId;
  VOID              *OptionOpCodeHandle;
  CFR_ENUM_VALUE    *CfrEnumValues;
  CFR_STRING        *CfrEnumUiString;
  CHAR16            *HiiEnumStrings;
  EFI_STRING_ID     HiiEnumStringsId;

  //
  // Extract variable-length fields that follow the header
  //
  OptionProcessedLength = sizeof (CFR_NUMERIC_OPTION);

  CfrOptionName = ExtractSingleCfrString ((UINT8 *)Option, &OptionProcessedLength);
  CfrDisplayName = ExtractSingleCfrString ((UINT8 *)Option, &OptionProcessedLength);

  DEBUG ((DEBUG_INFO, "CFR: Process option \"%a\" of size 0x%x\n", CfrOptionName->str, Option->size));

  //
  // Initialise defaults for efivarstore
  //
  ConvertCfrString (CfrOptionName, &VariableCfrName, NULL);

  DataSize = 0;
  Status = gRT->GetVariable (
                  VariableCfrName,
                  &gEficorebootNvDataGuid,
                  NULL,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_NOT_FOUND) {
    DataSize = sizeof (Option->default_value);
    Status = gRT->SetVariable (
                    VariableCfrName,
                    &gEficorebootNvDataGuid,
                    VARIABLE_ATTRIBUTE_NV_BS,
                    DataSize,
                    &Option->default_value
                    );
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Build a `varstore` and copy it as raw HII opcodes. Then free this
  //
  /* Struct contains space for terminator only, allocate with name too */
  VarStoreStructSize = sizeof (EFI_IFR_VARSTORE)
                         + AsciiStrLen ((CHAR8 *)CfrOptionName->str);
  ASSERT (VarStoreStructSize <= 0x7F);

  VarStore = AllocateZeroPool (VarStoreStructSize);
  ASSERT (VarStore != NULL);

  VarStore->Header.OpCode = EFI_IFR_VARSTORE_OP;
  VarStore->Header.Length = VarStoreStructSize;

  /* Direct mapping */
  VarStore->VarStoreId = QuestionIdVarStoreId;
  VarStore->Size = sizeof (UINT32);

  CopyMem (&VarStore->Guid, &gEficorebootNvDataGuid, sizeof (EFI_GUID));
  CopyMem (
    VarStore->Name,
    CfrOptionName->str,
    AsciiStrLen ((CHAR8 *)CfrOptionName->str) + 1
    );

  TempHiiBuffer = HiiCreateRawOpCodes (
                    StartOpCodeHandle,
                    (UINT8 *)VarStore,
                    VarStoreStructSize
                    );
  ASSERT (TempHiiBuffer != NULL);
  FreePool (VarStore);

  //
  // Parse flags
  // - TODO: Produce grayout and suppress
  //
  QuestionFlags = EFI_IFR_FLAG_RESET_REQUIRED;
  if (Option->flags & SETUP_MENU_VAR_FLAG_READONLY) {
    QuestionFlags |= EFI_IFR_FLAG_READ_ONLY;
  }

  DefaultOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (DefaultOpCodeHandle != NULL);

  TempHiiBuffer = HiiCreateDefaultOpCode (
                    DefaultOpCodeHandle,
                    EFI_HII_DEFAULT_CLASS_STANDARD,
                    EFI_IFR_TYPE_NUM_SIZE_32,
                    Option->default_value
                    );
  ASSERT (TempHiiBuffer != NULL);

  ConvertCfrString (CfrDisplayName, &HiiDisplayString, &HiiDisplayStringId);

  //
  // Create HII opcodes, processing complete.
  //
  OptionOpCodeHandle = NULL;
  if (Option->tag == CB_TAG_CFR_OPTION_ENUM) {
    OptionOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (OptionOpCodeHandle != NULL);

    while (OptionProcessedLength < Option->size) {
      CfrEnumValues = (CFR_ENUM_VALUE *)((UINT8 *)Option + OptionProcessedLength);
      ASSERT (CfrEnumValues->tag == CB_TAG_CFR_ENUM_VALUE);

      CfrEnumUiString = (CFR_STRING *)((UINT8 *)CfrEnumValues + sizeof (CFR_ENUM_VALUE));
      ConvertCfrString (CfrEnumUiString, &HiiEnumStrings, &HiiEnumStringsId);

      TempHiiBuffer = HiiCreateOneOfOptionOpCode (
                        OptionOpCodeHandle,
                        HiiEnumStringsId,
                        0,
                        EFI_IFR_NUMERIC_SIZE_4,
                        CfrEnumValues->value
                        );
      ASSERT (TempHiiBuffer != NULL);

      FreePool (HiiEnumStrings);
      OptionProcessedLength += CfrEnumValues->size;
    }

    TempHiiBuffer = HiiCreateOneOfOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      STRING_TOKEN (STR_EMPTY_STRING),
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_4,
                      OptionOpCodeHandle,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->tag == CB_TAG_CFR_OPTION_NUMBER) {
    TempHiiBuffer = HiiCreateNumericOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      STRING_TOKEN (STR_EMPTY_STRING),
                      QuestionFlags,
                      EFI_IFR_NUMERIC_SIZE_4,
                      0x00000000,
                      0xFFFFFFFF,
                      1,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  } else if (Option->tag == CB_TAG_CFR_OPTION_BOOL) {
    TempHiiBuffer = HiiCreateCheckBoxOpCode (
                      StartOpCodeHandle,
                      QuestionIdVarStoreId,
                      QuestionIdVarStoreId,
                      0x0,
                      HiiDisplayStringId,
                      STRING_TOKEN (STR_EMPTY_STRING),
                      QuestionFlags,
                      0,
                      DefaultOpCodeHandle
                      );
    ASSERT (TempHiiBuffer != NULL);
  }

  if (OptionOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (OptionOpCodeHandle);
  }
  HiiFreeOpCodeHandle (DefaultOpCodeHandle);
  FreePool (VariableCfrName);
  FreePool (HiiDisplayString);

  *ProcessedLength += Option->size;
}

/**
  Create runtime components by iterating CFR forms.

**/
VOID
EFIAPI
CfrCreateRuntimeComponents (
  VOID
  )
{
  VOID                *StartOpCodeHandle;
  VOID                *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL  *StartLabel;
  EFI_IFR_GUID_LABEL  *EndLabel;
  UINTN               QuestionId;
  EFI_HOB_GUID_TYPE   *GuidHob;
  CFR_FORM            *CfrFormHob;
  UINTN               ProcessedLength;
  CFR_STRING          *CfrFormName;
  CFR_FORM            *CfrFormData;
  EFI_STATUS          Status;

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
  StartLabel->Number       = LABEL_RT_COMP_START;

  EndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                       EndOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  ASSERT (EndLabel != NULL);

  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_RT_COMP_END;

  //
  // For each HOB, create forms
  //
  QuestionId = CFR_COMPONENT_START;

  GuidHob = GetFirstGuidHob (&gEfiCfrSetupMenuFormGuid);
  while (GuidHob != NULL) {
    CfrFormHob = GET_GUID_HOB_DATA (GuidHob);

    ProcessedLength = sizeof (CFR_FORM);
    CfrFormName = ExtractSingleCfrString ((UINT8 *)CfrFormHob, &ProcessedLength);

    /* TODO: Produce subtitle - or form, perhaps difficult */
    DEBUG ((DEBUG_INFO, "CFR: Process form \"%a\"\n", CfrFormName->str));

    //
    // Process form tree
    //
    while (ProcessedLength < CfrFormHob->size) {
      CfrFormData = (CFR_FORM *)((UINT8 *)CfrFormHob + ProcessedLength);

      switch (CfrFormData->tag) {
        case CB_TAG_CFR_OPTION_ENUM:
        case CB_TAG_CFR_OPTION_NUMBER:
        case CB_TAG_CFR_OPTION_BOOL:
          ProcessOption (
            (CFR_NUMERIC_OPTION *)CfrFormData,
            StartOpCodeHandle,
            QuestionId++,
            &ProcessedLength
            );
          break;
        default:
          DEBUG ((
            DEBUG_ERROR,
            "CFR: Offset 0x%x - Unexpected entry 0x%x (size 0x%x)!\n",
            ProcessedLength,
            CfrFormData->tag,
            CfrFormData->size
            ));
          ASSERT (FALSE);
          // Hopeful attempt at preventing infinite loop
          ProcessedLength += CfrFormData->size;
          break;
      }
    }

    GuidHob = GetNextGuidHob (&gEfiCfrSetupMenuFormGuid, GET_NEXT_HOB (GuidHob));
  }

  //
  // Submit updates
  //
  Status = HiiUpdateForm (
             gSetupMenuPrivate.HiiHandle,
             &mSetupMenuFormsetGuid,
             SETUP_MENU_FORM_ID,
             StartOpCodeHandle,
             EndOpCodeHandle
             );
  ASSERT_EFI_ERROR (Status);

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);
}
