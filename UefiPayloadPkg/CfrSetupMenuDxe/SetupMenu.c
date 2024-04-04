/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SetupMenu.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

/*
 * This function is the entry point for this DXE.
 * It creates at runtime a HII Form and HII String Package from given CFR data.
 */
EFI_STATUS
EFIAPI
CfrSetupMenuEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Get CFR data and parse it to IFR data
  //

  // Get CFR Form HOB
  EFI_HOB_GUID_TYPE *GuidHob = GetFirstGuidHob (&gEfiCfrSetupMenuFormGuid);
  ASSERT(GuidHob != NULL);
  CFR_FORM *CfrForm = GET_GUID_HOB_DATA (GuidHob);
  ASSERT(CfrForm != NULL);

  // Create OpCodeHandle (Buffer) on which all Form data is added to
  VOID *OpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (OpCodeHandle != NULL);

  // Create some strings that we always need in the HII Strings package
  BUFFER StrsBuf = { 0 };
  AddHiiStringBlock(&StrsBuf, EFI_HII_SIBT_STRING_SCSU, "English"); // create PRINTABLE_LANGUAGE_NAME string
  AddHiiStringBlock(&StrsBuf, EFI_HII_SIBT_STRING_SCSU, ""); // create empty string (EMPTY_STRING_ID)

  // Recursively go through the whole CFR Form tree and parse it into IFR opcodes (into OpcodeHandle buffer).
  // It also creates the necessary strings for the string package.
  ProcessFormSet(OpCodeHandle, CfrForm, &StrsBuf);

  // create end string block (terminator for HII string package)
  AddHiiStringBlock(&StrsBuf, EFI_HII_SIBT_END, NULL);

  //
  // Create HII Form Package
  //

  // from BaseTools/Source/Python/AutoGen/StrGather.py
#define EFI_HII_ARRAY_SIZE_LENGTH 4
  ASSERT (EFI_HII_ARRAY_SIZE_LENGTH == sizeof(UINT32)); // must always be the case since we depend on it

  HII_LIB_OPCODE_BUFFER *OpCodeBufferStart = (HII_LIB_OPCODE_BUFFER *)OpCodeHandle;
  // ARRAY LENGTH + PACKAGE HEADER + PACKAGE DATA
  UINT32 ArrayLength = EFI_HII_ARRAY_SIZE_LENGTH + sizeof(EFI_HII_PACKAGE_HEADER) + OpCodeBufferStart->Position;
  UINT8 *IfrPackage = AllocatePool(ArrayLength); // allocate final IFR Package buffer
  
  CopyMem (IfrPackage, &ArrayLength, EFI_HII_ARRAY_SIZE_LENGTH); // first fill in ARRAY LENGTH
  
  // next fill in PACKAGE HEADER
  EFI_HII_PACKAGE_HEADER *PackageHeader = (EFI_HII_PACKAGE_HEADER *)(IfrPackage + EFI_HII_ARRAY_SIZE_LENGTH);
  PackageHeader->Length = ArrayLength - EFI_HII_ARRAY_SIZE_LENGTH;
  PackageHeader->Type = EFI_HII_PACKAGE_FORMS;

  // next fill in PACKAGE DATA (IFR opcodes)
  CopyMem(IfrPackage + EFI_HII_ARRAY_SIZE_LENGTH + sizeof(EFI_HII_PACKAGE_HEADER), OpCodeBufferStart->Buffer, OpCodeBufferStart->Position);
  HiiFreeOpCodeHandle (OpCodeHandle);
  
  //
  // Create HII Strings Package
  //

  // Calculate the length of the HII Strings package and allocate it.
  // -1 because of Language property in EFI_HII_STRING_PACKAGE_HDR
  UINTN StrsPckHdrLen = sizeof(EFI_HII_STRING_PACKAGE_HDR) - 1 + sizeof("en-US");
  UINTN StrsPckLen = EFI_HII_ARRAY_SIZE_LENGTH + StrsPckHdrLen + StrsBuf.Size;
  UINT8 *StringsPackage = AllocatePool(StrsPckLen);
  
  // fill in the Package Length
  *(UINT32 *)StringsPackage = StrsPckLen;

  // fill in the EFI_HII_STRING_PACKAGE_HDR
  EFI_HII_STRING_PACKAGE_HDR *StrsPck = (EFI_HII_STRING_PACKAGE_HDR *)(StringsPackage + EFI_HII_ARRAY_SIZE_LENGTH);
  StrsPck->Header.Type = EFI_HII_PACKAGE_STRINGS;
  StrsPck->Header.Length = StrsPckLen - EFI_HII_ARRAY_SIZE_LENGTH;
  StrsPck->HdrSize = 0x34;
  StrsPck->StringInfoOffset = 0x34;
  ZeroMem(StrsPck->LanguageWindow, sizeof(StrsPck->LanguageWindow));
  StrsPck->LanguageName = 1;
  AsciiStrCpyS(StrsPck->Language, sizeof("en-US"), "en-US");

  // fill in the actual strings for the package
  CopyMem(((UINT8 *)StrsPck) + StrsPckHdrLen, StrsBuf.Data, StrsBuf.Size);
  FreePool(StrsBuf.Data);

  //
  // Install Device Path to driver handle
  //
  mSetupMenuPrivate.DriverHandle = NULL;
  Status                         = gBS->InstallMultipleProtocolInterfaces (
                                          &mSetupMenuPrivate.DriverHandle,
                                          &gEfiDevicePathProtocolGuid,
                                          &mSetupMenuHiiVendorDevicePath,
                                          NULL
                                          );
  ASSERT_EFI_ERROR (Status);

  mSetupMenuPrivate.HiiHandle = HiiAddPackages (
                                  &mSetupMenuHiiGuid,
                                  mSetupMenuPrivate.DriverHandle,
                                  IfrPackage,
                                  StringsPackage,
                                  NULL
                                  );
  
  // HII code does not set variables, if they do not exist yet.
  // Therefore we set all options in the underlying storage beforehand.
  CfrSetDefaultOptions(CfrForm);

  return Status;
}

EFI_STATUS
EFIAPI
CfrSetupMenuUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  //
  // Uninstall Device Path and Config Access protocols
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  mSetupMenuPrivate.DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mSetupMenuHiiVendorDevicePath,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Remove our HII data
  //
  HiiRemovePackages (mSetupMenuPrivate.HiiHandle);

  return Status;
}
