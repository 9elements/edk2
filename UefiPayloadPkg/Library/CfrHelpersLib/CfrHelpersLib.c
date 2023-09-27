/** @file
  Contains helper functions for working with CFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>
#include <Pi/PiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/CfrHelpersLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Guid/CfrSetupMenuGuid.h>

//
// CFR callback data structure
//
typedef struct {
  UINT32        Tag;
  CONST VOID    *Record;
  BOOLEAN       Found;
} CFR_LOCATE;

/**
  CfrGetHeaderLength returns the length of the CFR header that
  corresponds to the given tag.

**/
UINTN
EFIAPI
CfrGetHeaderLength (
  IN CONST UINT32  Tag
  )
{
  switch (Tag) {
    case CB_CFR_TAG_OPTION_FORM:
      return sizeof (CFR_OPTION_FORM);
    case CB_CFR_TAG_OPTION_ENUM:
    case CB_CFR_TAG_OPTION_NUMBER:
    case CB_CFR_TAG_OPTION_BOOL:
      return sizeof (CFR_OPTION_NUMERIC);
    case CB_CFR_TAG_OPTION_VARCHAR:
      return sizeof (CFR_OPTION_VARCHAR);
    case CB_CFR_TAG_OPTION_COMMENT:
      return sizeof (CFR_OPTION_COMMENT);
    case CB_CFR_TAG_ENUM_VALUE:
      return sizeof (CFR_ENUM_VALUE);
    default:
      return 0;
  }
}

/**
  Walk a CFR tree and invoke the callback for each found child.

  @param  [in]      Root      The Node currently being processed.
  @param  [in]      CallBack  The callback to invoke for each found child.
  @param  [in, out] Private   Private data structure passed to CallBack.

  @retval EFI_SUCCESS            The default value is found.
  @retval EFI_INVALID_PARAMETER  The function parameters are invalid.
  @retval EFI_VOLUME_CORRUPTED   The CFR list is corrupted.
  @retval EFI_UNSUPPORTED        The CFR record isn't supported.

**/
EFI_STATUS
EFIAPI
CfrWalkTree (
  IN CONST UINT8                    *Root,
  IN       EDKII_CFR_TREE_CALLBACK  CallBack,
  IN OUT   VOID                     *Private
  )
{
  UINTN       ProcessedLength;
  EFI_STATUS  Status;
  CFR_HEADER  *Header;
  CFR_HEADER  *Iterator;

  if ((Root == NULL) || (CallBack == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Header = (CFR_HEADER *)Root;

  ProcessedLength = CfrGetHeaderLength (Header->Tag);
  if (ProcessedLength == 0) {
    return EFI_UNSUPPORTED;
  }

  if (ProcessedLength > Header->Size) {
    return EFI_VOLUME_CORRUPTED;
  }

  if (ProcessedLength == Header->Size) {
    return EFI_SUCCESS;
  }

  while (ProcessedLength < Header->Size) {
    Iterator = (CFR_HEADER *)(Root + ProcessedLength);
    if (Iterator->Size <= sizeof (CFR_HEADER)) {
      return EFI_VOLUME_CORRUPTED;
    }

    Status = CallBack ((CONST CFR_HEADER *)Root, Iterator, Private);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    ProcessedLength += Iterator->Size;
  }

  return EFI_SUCCESS;
}

/**
  Callback when walking the CFR tree. Locates a child with the specified
  tag and updates CFR_LOCATE fields passed in using the Private param.

  @param  [in]      Root      The Node currently being processed.
  @param  [in]      CallBack  The callback to invoke for each found child.
  @param  [in, out] Private   Pointer to CFR_LOCATE.

  @retval EFI_SUCCESS            The default value is found.
  @retval EFI_INVALID_PARAMETER  The function parameters are invalid.
  @retval EFI_VOLUME_CORRUPTED   The CFR list is corrupted.
  @retval EFI_UNSUPPORTED        The CFR record isn't supported.

**/
STATIC
EFI_STATUS
EFIAPI
CfrTreeCallbackFindTag (
  IN CONST CFR_HEADER  *Root,
  IN CONST CFR_HEADER  *Child,
  IN OUT   VOID        *Private
  )
{
  CFR_LOCATE  *Locate = (CFR_LOCATE *)Private;

  if ((Root == NULL) || (Child == NULL) || (Private == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Child->Tag == Locate->Tag) {
    Locate->Found  = TRUE;
    Locate->Record = Child;
  }

  return EFI_SUCCESS;
}

/**
  Walk a CFR tree and locate a child with given tag.

  @param  [in]      Root      The Node currently being processed.
  @param  [in]      Tag       The tag of the child node to locate.
  @param  [out]     Out       The pointer to the found child node.

  @retval EFI_SUCCESS            The default value is found.
  @retval EFI_NOT_FOUND          The node with given tag was not found.
  @retval EFI_INVALID_PARAMETER  The function parameters are invalid.
  @retval EFI_VOLUME_CORRUPTED   The CFR list is corrupted.
  @retval EFI_UNSUPPORTED        A CFR record isn't supported.

**/
EFI_STATUS
EFIAPI
CfrFindTag (
  IN CONST  UINT8   *Root,
  IN CONST  UINT32  Tag,
  OUT CONST VOID    **Out
  )
{
  CFR_LOCATE  Locate;
  EFI_STATUS  Status;

  if (Root == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Locate.Record = NULL;
  Locate.Found  = FALSE;
  Locate.Tag    = Tag;

  Status = CfrWalkTree (Root, CfrTreeCallbackFindTag, &Locate);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Locate.Found == FALSE) {
    return EFI_NOT_FOUND;
  }

  if (Out != NULL) {
    *Out = Locate.Record;
  }

  return EFI_SUCCESS;
}

/**
  Walk a CFR tree and return an unicode string found in the node
  with the specified tag.

  @param  [in]      Root          The Node currently being processed.
  @param  [in]      Tag           The tag of the child node to locate.
  @param  [out]     UnicodeString The pointer to set. The caller must
                                  free this memory when no longer needed.

  @retval EFI_SUCCESS            The default value is found.
  @retval EFI_NOT_FOUND          The node with given tag was not found.
  @retval EFI_INVALID_PARAMETER  The function parameters are invalid.
  @retval EFI_VOLUME_CORRUPTED   The CFR list is corrupted.
  @retval EFI_UNSUPPORTED        A CFR record isn't supported.

**/
EFI_STATUS
EFIAPI
CfrExtractString (
  IN CONST UINT8   *Root,
  IN CONST UINT32  Tag,
  OUT      CHAR16  **UnicodeString
  )
{
  EFI_STATUS           Status;
  CONST CFR_VARBINARY  *CfrString;

  if ((Root == NULL) || (UnicodeString == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = CfrFindTag (Root, Tag, (CONST VOID **)&CfrString);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (CfrString->DataLength > 0);

  *UnicodeString = AllocatePool (CfrString->DataLength * sizeof (CHAR16));
  ASSERT (*UnicodeString != NULL);
  Status = AsciiStrToUnicodeStrS (
             (CHAR8 *)CfrString->Data,
             *UnicodeString,
             CfrString->DataLength
             );

  return EFI_SUCCESS;
}
