/** @file
  Generic definitions for registers in the Intel Ich devices.

  These definitions should work for any version of Ich.

  Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _COMMON_LIB_H_
#define _COMMON_LIB_H_

#define TO_LOWER(ch) (((ch >= L'A') && (ch <= L'Z')) ? ((ch - L'A') + L'a') : ch)
#define TO_UPPER(ch) (((ch >= L'a') && (ch <= L'z')) ? ((ch - L'a') + L'A') : ch)
#define TO_ALOWER(ch) (((ch >= 'A') && (ch <= 'Z')) ? ((ch - 'A') + 'a') : ch)
#define TO_AUPPER(ch) (((ch >= 'a') && (ch <= 'z')) ? ((ch - 'a') + 'A') : ch)

//Unicode
#define IS_COMMA(a)             ((a) == L',')
#define IS_HYPHEN(a)            ((a) == L'-')
#define IS_DOT(a)               ((a) == L'.')
#define IS_LEFT_PARENTH(a)      ((a) == L'(')
#define IS_RIGHT_PARENTH(a)     ((a) == L')')
#define IS_SLASH(a)             ((a) == L'/')
#define IS_NULL(a)              ((a) == L'\0')
//Ascii
#define IS_DIGIT(a)             (((a) >= '0') && ((a) <= '9'))
#define IS_HEX(a)               ((((a) >= 'a') && ((a) <= 'f')) || (((a) >= 'A') && ((a) <= 'F')))
#define IS_UPPER(a)             (((a) >= 'A') && ((a) <= 'Z'))
#define IS_ALFA(x)              (((x >= 'a') && (x <='z')) || ((x >= 'A') && (x <='Z')))
#define IS_ASCII(x)             ((x>=0x20) && (x<=0x7F))
#define IS_PUNCT(x)             ((x == '.') || (x == '-'))


EFI_STATUS  AsciiTrimSpaces(IN CHAR8 **String);
UINT64      AsciiStrVersionToUint64(const CHAR8 *Version, UINT8 MaxDigitByPart, UINT8 MaxParts);
UINT32      hex2bin(IN CHAR8 *hex, OUT UINT8 *bin, UINT32 len);
UINT8       hexstrtouint8 (CHAR8* buf); //one or two hex letters to one byte
CHAR8       *Bytes2HexStr(UINT8 *data, UINTN len);

CHAR16*
EFIAPI
GetStrLastCharOccurence (
  IN CHAR16   *String,
  IN CHAR16   Char
);

CHAR16*
EFIAPI
GetStrLastChar (
  IN CHAR16   *String
);

BOOLEAN
EFIAPI
StriStartsWith (
  IN CHAR16   *String1,
  IN CHAR16   *String2
);

//
// entry_scan
//

CHAR16
*EfiStrDuplicate (
  IN CHAR16   *Src
);

INTN StriCmp (
  IN CONST CHAR16   *FirstString,
  IN CONST CHAR16   *SecondString
);

//INTN EFIAPI AsciiStriCmp (
//  IN CONST CHAR8    *FirstString,
//  IN CONST CHAR8    *SecondString
//);

BOOLEAN AsciiStriNCmp (
  IN CONST CHAR8    *FirstString,
  IN CONST CHAR8    *SecondString,
  IN CONST UINTN     sSize
);

BOOLEAN AsciiStrStriN (
  IN CONST CHAR8    *WhatString,
  IN CONST UINTN     sWhatSize,
  IN CONST CHAR8    *WhereString,
  IN CONST UINTN     sWhereSize
);

//UINTN
//EfiDevicePathInstanceCount (
//  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
//);

VOID *
EfiReallocatePool (
  IN VOID     *OldPool,
  IN UINTN    OldSize,
  IN UINTN    NewSize
);

INTN
StrniCmp (
  IN CHAR16 *Str1,
  IN CHAR16 *Str2,
  IN UINTN  Count
);

CHAR16
*StriStr (
  IN CHAR16 *Str,
  IN CHAR16 *SearchFor
);

CHAR16
*StrToLower (
  IN CHAR16   *Str
);

CHAR16
*StrToUpper (
  IN CHAR16   *Str
);

CHAR16
*StrToTitle (
  IN CHAR16   *Str
);

CHAR8
*AsciiStrToLower (
  IN CHAR8   *Str
);

CHAR8
*AsciiStriStr (
  IN CHAR8  *String,
  IN CHAR8  *SearchString
);

CHAR16*
EFIAPI
GuidStr (
  IN EFI_GUID   *Guid
);

#endif  // _GENERIC_ICH_H_
