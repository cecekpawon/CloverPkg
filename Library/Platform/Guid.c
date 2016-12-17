/**
 guid.c
 **/

#include <Library/Platform/Platform.h>

EFI_GUID gNotifyMouseActivity                     = {0xF913C2C2, 0x5351, 0x4FDB, {0x93, 0x44, 0x70, 0xFF, 0xED, 0xB8, 0x42, 0x25}};
EFI_GUID gEfiDataHubProtocolGuid                  = {0xAE80D021, 0x618E, 0x11D4, {0xBC, 0xD7, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81}};
EFI_GUID gEfiMiscSubClassGuid                     = {0x772484B2, 0x7482, 0x4b91, {0x9F, 0x9A, 0xAD, 0x43, 0xF8, 0x1C, 0x58, 0x81}};
EFI_GUID gEfiProcessorSubClassGuid                = {0x26FDEB7E, 0xB8AF, 0x4CCF, {0xAA, 0x97, 0x02, 0x63, 0x3C, 0xE4, 0x8C, 0xA7}};
EFI_GUID gEfiMemorySubClassGuid                   = {0x4E8F4EBB, 0x64B9, 0x4e05, {0x9B, 0x18, 0x4C, 0xFE, 0x49, 0x23, 0x50, 0x97}};
EFI_GUID gAppleDeviceControlProtocolGuid          = {0x8ECE08D8, 0xA6D4, 0x430B, {0xA7, 0xB0, 0x2D, 0xF3, 0x18, 0xE7, 0x88, 0x4A}};

//from reza jelveh
EFI_GUID gAppleSystemInfoProducerNameGuid         = {0x64517CC8, 0x6561, 0x4051, {0xB0, 0x3C, 0x59, 0x64, 0xB6, 0x0F, 0x4C, 0x7A}};
EFI_GUID gAppleFsbFrequencyPropertyGuid           = {0xD1A04D55, 0x75B9, 0x41A3, {0x90, 0x36, 0x8F, 0x4A, 0x26, 0x1C, 0xBB, 0xA2}};
EFI_GUID gAppleDevicePathsSupportedGuid           = {0x5BB91CF7, 0xD816, 0x404B, {0x86, 0x72, 0x68, 0xF2, 0x7F, 0x78, 0x31, 0xDC}};



//all these codes are still under the question
EFI_GUID GPT_MSDOS_PARTITION = { 0xEBD0A0A2, 0xB9E5, 0x4433,{ 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 }};  //ExFAT
// HFS+ partition - 48465300-0000-11AA-AA11-00306543ECAC
EFI_GUID GPT_HFSPLUS_PARTITION = { 0x48465300, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }};
EFI_GUID GPT_EMPTY_PARTITION = { 0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
// turbo - Apple Boot Partition - 426F6F74-0000-11AA-AA11-00306543ECAC  //RecoveryHD
// Microsoft Reserved Partition - E3C9E316-0B5C-4DB8-817DF92DF00215AE
//EFI_PART_TYPE_LEGACY_MBR_GUID {0x024DEE41, 0x33E7, 0x11D3, {0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F }};

//EFI_GUID ESPGuid = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };

//TODO - discover the follow guids
//efi/configuration-table/5751DA6E-1376-4E02-BA92-D294FDD30901
//efi/configuration-table/F76761DC-FF89-44E4-9C0C-CD0ADA4EF983
/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/** Returns TRUE is Str is ascii Guid in format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
BOOLEAN
IsValidGuidAsciiString (
  IN CHAR8    *Str
) {
  UINTN   Index;

  if ((Str == NULL) || (AsciiStrLen(Str) != 36)) {
    return FALSE;
  }

  for (Index = 0; Index < 36; Index++, Str++) {
    if ((Index == 8) || (Index == 13) || (Index == 18) || (Index == 23)) {
      if (*Str != '-') {
        return FALSE;
      }
    } else {
      if (!(
            (*Str >= '0' && *Str <= '9') ||
            (*Str >= 'a' && *Str <= 'f') ||
            (*Str >= 'A' && *Str <= 'F')
           )
      ) {
        return FALSE;
      }
    }
  }

  return TRUE;
}

static
EFI_STATUS
StrHToBuf (
  OUT UINT8    *Buf,
  IN  UINTN    BufferLength,
  IN  CHAR16   *Str
) {
  UINTN       Index, StrLength;
  UINT8       Digit, Byte;

  Digit = 0;

  //
  // Two hex char make up one byte
  //
  StrLength = BufferLength * sizeof (CHAR16);

  for(Index = 0; Index < StrLength; Index++, Str++) {

    if ((*Str >= L'a') && (*Str <= L'f')) {
      Digit = (UINT8) (*Str - L'a' + 0x0A);
    } else if ((*Str >= L'A') && (*Str <= L'F')) {
      Digit = (UINT8) (*Str - L'A' + 0x0A);
    } else if ((*Str >= L'0') && (*Str <= L'9')) {
      Digit = (UINT8) (*Str - L'0');
    } else {
      return EFI_INVALID_PARAMETER;
    }

    //
    // For odd characters, write the upper nibble for each buffer byte,
    // and for even characters, the lower nibble.
    //
    if ((Index & 1) == 0) {
      Byte = (UINT8) (Digit << 4);
    } else {
      Byte = Buf[Index / 2];
      Byte &= 0xF0;
      Byte = (UINT8) (Byte | Digit);
    }

    Buf[Index / 2] = Byte;
  }

  return EFI_SUCCESS;
}

/**
 Converts a string to GUID value.
 Guid Format is xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

 @param Str              The registry format GUID string that contains the GUID value.
 @param Guid             A pointer to the converted GUID value.

 @retval EFI_SUCCESS     The GUID string was successfully converted to the GUID value.
 @retval EFI_UNSUPPORTED The input string is not in registry format.
 @return others          Some error occurred when converting part of GUID value.

 **/

EFI_STATUS
StrToGuidLE (
  IN  CHAR16      *Str,
  OUT EFI_GUID    *Guid
) {
  UINT8   GuidLE[16];

  StrHToBuf (&GuidLE[0], 4, Str);
  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrHToBuf (&GuidLE[4], 2, Str);
  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrHToBuf (&GuidLE[6], 2, Str);
  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrHToBuf (&GuidLE[8], 2, Str);
  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrHToBuf (&GuidLE[10], 6, Str);

  CopyMem((UINT8*)Guid, &GuidLE[0], 16);
  return EFI_SUCCESS;
}
