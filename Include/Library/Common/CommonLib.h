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

#include <Library/UefiLib.h>

#define PoolPrint(...)  CatSPrint (NULL, __VA_ARGS__)

#define CONSTRAIN_MIN(Variable, MinValue) if (Variable < MinValue) Variable = MinValue
#define CONSTRAIN_MAX(Variable, MaxValue) if (Variable > MaxValue) Variable = MaxValue

#define TO_LOWER(ch)            (((ch >= L'A') && (ch <= L'Z')) ? ((ch - L'A') + L'a') : ch)
#define TO_UPPER(ch)            (((ch >= L'a') && (ch <= L'z')) ? ((ch - L'a') + L'A') : ch)
#define TO_ALOWER(ch)           (((ch >= 'A') && (ch <= 'Z')) ? ((ch - 'A') + 'a') : ch)
#define TO_AUPPER(ch)           (((ch >= 'a') && (ch <= 'z')) ? ((ch - 'a') + 'A') : ch)

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
#define IS_ALFA(x)              (((x >= 'a') && (x <= 'z')) || ((x >= 'A') && (x <= 'Z')))
#define IS_ASCII(x)             ((x >= 0x20) && ( x<= 0x7F))
#define IS_PUNCT(x)             ((x == '.') || (x == '-'))

// Allow for 255 unicode characters + 2 byte unicode null terminator.
#define SVALUE_MAX_SIZE         (512)
#define AVALUE_MAX_SIZE         (256)

typedef struct {
  UINT8   VersionMajor;
  UINT8   VersionMinor;
  UINT8   Revision;
} SVersion;

EFI_STATUS
EFIAPI
AsciiTrimSpaces (
  IN CHAR8   **String
);

UINT64
EFIAPI
AsciiStrVersionToUint64 (
  CONST CHAR8   *Version,
        UINT8   MaxDigitByPart,
        UINT8   MaxParts
);

UINT32
EFIAPI
GetCrc32 (
  UINT8   *Buffer,
  UINTN   Size
);

UINT32
EFIAPI
Hex2Bin (
  IN  CHAR8   *Hex,
  OUT UINT8   *Bin,
  IN  UINT32  Len
);

VOID *
EFIAPI
StringDataToHex (
  IN   CHAR8    *Val,
  OUT  UINTN    *DataLen
);

BOOLEAN
EFIAPI
IsHexDigit (
  CHAR8   C
);

UINT8
EFIAPI
HexStrToUint8 (
  CHAR8   *Buf
); //one or two hex letters to one byte

CHAR8 *
EFIAPI
Bytes2HexStr (
  UINT8   *Bytes,
  UINTN   Len
);

CHAR16 *
EFIAPI
GetStrLastCharOccurence (
  IN CHAR16   *String,
  IN CHAR16   Char
);

CHAR16 *
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

CHAR16 *
EFIAPI
EfiStrDuplicate (
  IN CHAR16   *Src
);

INTN
EFIAPI
StriCmp (
  IN CONST CHAR16   *FirstString,
  IN CONST CHAR16   *SecondString
);

CHAR16 *
EFIAPI
StrnCatGrow (
  IN OUT  CHAR16    **Destination,
  IN OUT  UINTN     *CurrentSize,
  IN      CHAR16    *Source,
  IN      UINTN     Count
);

VOID
EFIAPI
RemoveMultiSpaces (
  IN OUT CHAR16   *Str
);

VOID
EFIAPI
StrCleanSpaces (
  IN OUT CHAR16   **Str
);

//INTN EFIAPI AsciiStriCmp (
//  IN CONST CHAR8    *FirstString,
//  IN CONST CHAR8    *SecondString
//);

BOOLEAN
EFIAPI
AsciiStriNCmp (
  IN CONST CHAR8    *FirstString,
  IN CONST CHAR8    *SecondString,
  IN CONST UINTN    sSize
);

BOOLEAN
EFIAPI
AsciiStrStriN (
  IN CONST CHAR8    *WhatString,
  IN CONST UINTN    sWhatSize,
  IN CONST CHAR8    *WhereString,
  IN CONST UINTN    sWhereSize
);

UINTN
EFIAPI
AsciiTrimStrLen (
  CHAR8   *String,
  UINTN   MaxLen
);

CHAR8 *
EFIAPI
AsciiStrnCatGrow (
  IN OUT  CHAR8   **Destination,
  IN OUT  UINTN   *CurrentSize,
  IN      CHAR8   *Source,
  IN      UINTN   Count
);

//UINTN
//EfiDevicePathInstanceCount (
//  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
//);

VOID *
EFIAPI
EfiReallocatePool (
  IN VOID     *OldPool,
  IN UINTN    OldSize,
  IN UINTN    NewSize
);

INTN
EFIAPI
StrniCmp (
  IN CHAR16   *Str1,
  IN CHAR16   *Str2,
  IN UINTN    Count
);

CHAR16 *
EFIAPI
StriStr (
  IN CHAR16   *Str,
  IN CHAR16   *SearchFor
);

CHAR16 *
EFIAPI
StrToLower (
  IN CHAR16   *Str
);

CHAR16 *
EFIAPI
StrToUpper (
  IN CHAR16   *Str
);

CHAR16 *
EFIAPI
StrToTitle (
  IN CHAR16   *Str
);

CHAR8 *
EFIAPI
AsciiStrToLower (
  IN CHAR8   *Str
);

CHAR8 *
EFIAPI
AsciiStrToUpper (
  IN CHAR8   *Str
);

CHAR8 *
EFIAPI
AsciiStriStr (
  IN CHAR8  *String,
  IN CHAR8  *SearchString
);

INTN
EFIAPI
CountOccurrences (
  CHAR8   *Str,
  CHAR8   Char
);

CHAR8 *
EFIAPI
FindCharDelimited (
  IN CHAR8    *InString,
  IN CHAR8    InChar,
  IN UINTN    Index
);

SVersion *
EFIAPI
StrToVersion (
  CHAR8   *Str
);

UINT8 *
EFIAPI
StrToMacAddress (
  IN CHAR8   *Str
);

UINT8 *
Base64Decode (
  IN  CHAR8   *Data,
  OUT UINTN   *Size
);

UINT8 *
Base64Encode (
  IN  CHAR8   *Data,
  OUT UINTN   *Size
);

#endif  // _GENERIC_ICH_H_
