/** @file
  Contains helper functions for working with CFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _CFR_HELPERS_LIB_H_
#define _CFR_HELPERS_LIB_H_

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>
#include <Pi/PiMultiPhase.h>
#include <Guid/CfrSetupMenuGuid.h>

/**
  Callback function prototype used when iterating through the tree.

  @param  [in]      Root      The Node currently being processed.
  @param  [in]      Child     A child node that has been found.
  @param  [in, out] Private   Private data structure passed to CfrWalkTree.

  @retval EFI_SUCCESS         Continue iterating over the node.

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_CFR_TREE_CALLBACK)(
  IN CONST CFR_HEADER        *Root,
  IN CONST CFR_HEADER        *Child,
  IN OUT   VOID              *Private
  );

/**
  CfrGetHeaderLength returns the length of the CFR header that
  corresponds to the given tag.

**/
UINTN
EFIAPI
CfrGetHeaderLength (
  IN CONST UINT32   Tag
  );

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
  IN CONST UINT8                   *Root,
  IN       EDKII_CFR_TREE_CALLBACK CallBack,
  IN OUT   VOID                    *Private
  );

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
  IN CONST  UINT8                   *Root,
  IN CONST  UINT32                  Tag,
  OUT CONST VOID                    **Out
  );

CONST CHAR8*
EFIAPI
CfrExtractString (
  IN  CONST UINT32  StringOffset
  );

#endif
