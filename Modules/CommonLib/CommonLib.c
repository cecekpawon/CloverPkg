/*
 * refit/scan/common.c
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//#include <Library/Platform/Platform.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

#include <Library/Common/MemLogLib.h>
#include <Library/Common/CommonLib.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_COMMON
#define DEBUG_COMMON -1
#endif
#else
#ifdef DEBUG_COMMON
#undef DEBUG_COMMON
#endif
#define DEBUG_COMMON DEBUG_ALL
#endif

//#define DBG(...) DebugLog (DEBUG_COMMON, __VA_ARGS__)

#if DEBUG_COMMON > 0
#define DBG(...) MemLog(TRUE, 1, __VA_ARGS__)
#else
#define DBG(...)
#endif

CONST CHAR16 *OsxPathLCaches[] = {
   L"\\System\\Library\\Caches\\com.apple.kext.caches\\Startup\\kernelcache",
   L"\\System\\Library\\Caches\\com.apple.kext.caches\\Startup\\Extensions.mkext",
   L"\\System\\Library\\Extensions.mkext",
   L"\\com.apple.recovery.boot\\kernelcache",
   L"\\com.apple.recovery.boot\\Extensions.mkext",
   L"\\.IABootFiles\\kernelcache"
};

CONST   UINTN OsxPathLCachesCount = ARRAY_SIZE (OsxPathLCaches);
CHAR8   *OsVerUndetected = "10.10.10";  //longer string

//--> Base64

typedef enum {
  StepA, StepB, StepC, StepD
} Base64DecodeStep;

typedef struct {
  Base64DecodeStep   Step;
  CHAR8              PlainChar;
} Base64DecodeState;

INT32
Base64DecodeValue (
  CHAR8   Value
) {
  STATIC CONST INT8   Decoding[] = {
    62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1, -1,  0,  1,  2,  3,  4,  5, 6,
     7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
  };

  INT32   NewValue = (UINT8)Value;

  NewValue -= 43;
  if (NewValue < 0 || ((UINT32)NewValue >= sizeof (Decoding))) {
    return -1;
  }

  return Decoding[NewValue];
}

VOID
Base64InitDecodeState (
  Base64DecodeState    *State
) {
  State->Step = StepA;
  State->PlainChar = 0;
}

INT32
Base64DecodeBlock (
  CONST CHAR8               *Code,
  CONST INT32               Length,
        CHAR8               *PlainText,
        Base64DecodeState   *State
) {
  CONST CHAR8  *CodeChar = Code;
  CHAR8        *PlainChar = PlainText;
  INT32        Fragment;

  *PlainChar = State->PlainChar;

  switch (State->Step) {
    while (1) {
      case StepA:
        do {
          if (CodeChar == (Code + Length)) {
            State->Step = StepA;
            State->PlainChar = *PlainChar;
            return (INT32)(PlainChar - PlainText);
          }

          Fragment = Base64DecodeValue (*CodeChar++);
        } while (Fragment < 0);

        *PlainChar = (CHAR8)((Fragment & 0x03f) << 2);

      case StepB:
        do {
          if (CodeChar == (Code + Length)) {
            State->Step = StepB;
            State->PlainChar = *PlainChar;
            return (INT32)(PlainChar - PlainText);
          }

          Fragment = Base64DecodeValue (*CodeChar++);
        } while (Fragment < 0);

        *PlainChar++ |= (CHAR8)((Fragment & 0x030) >> 4);
        *PlainChar    = (CHAR8)((Fragment & 0x00f) << 4);

      case StepC:
        do {
          if (CodeChar == (Code + Length)) {
            State->Step = StepC;
            State->PlainChar = *PlainChar;
            return (INT32)(PlainChar - PlainText);
          }

          Fragment = Base64DecodeValue (*CodeChar++);
        } while (Fragment < 0);

        *PlainChar++ |= (CHAR8)((Fragment & 0x03c) >> 2);
        *PlainChar    = (CHAR8)((Fragment & 0x003) << 6);

      case StepD:
        do {
          if (CodeChar == (Code + Length))
          {
            State->Step = StepD;
            State->PlainChar = *PlainChar;
            return (INT32)(PlainChar - PlainText);
          }

          Fragment = Base64DecodeValue (*CodeChar++);
        } while (Fragment < 0);

        *PlainChar++ |= (CHAR8)((Fragment & 0x03f));
    }
  }

  /* control should not reach here */
  return (INT32)(PlainChar - PlainText);
}

/** UEFI interface to base54 decode.
 * Decodes EncodedData into a new allocated buffer and returns it. Caller is responsible to FreePool () it.
 * If DecodedSize != NULL, then size od decoded data is put there.
 */
UINT8 *
Base64Decode (
  IN  CHAR8     *EncodedData,
  OUT UINTN     *DecodedSize
) {
  UINTN                 EncodedSize;
  INT32                 DecodedSizeInternal;
  UINT8                 *DecodedData;
  Base64DecodeState     State;

  if (EncodedData == NULL) {
    return NULL;
  }

  EncodedSize = AsciiStrLen (EncodedData);

  if (EncodedSize == 0) {
    return NULL;
  }

  // to simplify, we'll allocate the same size, although smaller size is needed
  DecodedData = AllocateZeroPool (EncodedSize);

  Base64InitDecodeState (&State);
  DecodedSizeInternal = Base64DecodeBlock (EncodedData, (CONST INT32)EncodedSize, (CHAR8 *)DecodedData, &State);

  if (DecodedSize != NULL) {
    *DecodedSize = (UINTN)DecodedSizeInternal;
  }

  return DecodedData;
}

//<-- Base64

/**
  Duplicate a string.

  @param Src             The source.

  @return A new string which is duplicated copy of the source.
  @retval NULL If there is not enough memory.

**/
CHAR16 *
EFIAPI
EfiStrDuplicate (
  IN CHAR16   *Src
) {
  CHAR16  *Dest;
  UINTN   Size;

  Size  = StrSize (Src); //at least 2bytes
  Dest  = AllocatePool (Size);
  //ASSERT (Dest != NULL);
  if (Dest != NULL) {
    CopyMem (Dest, Src, Size);
  }

  return Dest;
}

INTN
EFIAPI
StrniCmp (
  IN CHAR16   *Str1,
  IN CHAR16   *Str2,
  IN UINTN    Count
) {
  CHAR16  Ch1, Ch2;

  if (Count == 0) {
    return 0;
  }

  if (Str1 == NULL) {
    if (Str2 == NULL) {
      return 0;
    } else {
      return -1;
    }
  } else  if (Str2 == NULL) {
    return 1;
  }

  do {
    Ch1 = TO_LOWER (*Str1);
    Ch2 = TO_LOWER (*Str2);

    Str1++;
    Str2++;

    if (Ch1 != Ch2) {
      return (Ch1 - Ch2);
    }

    if (Ch1 == 0) {
      return 0;
    }
  } while (--Count > 0);

  return 0;
}

CHAR16 *
EFIAPI
StriStr (
  IN CHAR16   *Str,
  IN CHAR16   *SearchFor
) {
  CHAR16  *End;
  UINTN   Length, SearchLength;

  if ((Str == NULL) || (SearchFor == NULL)) {
    return NULL;
  }

  Length = StrLen (Str);
  if (Length == 0) {
    return NULL;
  }

  SearchLength = StrLen (SearchFor);

  if (SearchLength > Length) {
    return NULL;
  }

  End = Str + (Length - SearchLength) + 1;

  while (Str < End) {
    if (StrniCmp (Str, SearchFor, SearchLength) == 0) {
      return Str;
    }
    ++Str;
  }

  return NULL;
}

CHAR16 *
EFIAPI
StrToLower (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate (Str);
  INTN      i;

  for (i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_LOWER (Tmp[i]);
  }

  return Tmp;
}

CHAR16 *
EFIAPI
StrToUpper (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate (Str);
  INTN      i;

  for (i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_UPPER (Tmp[i]);
  }

  return Tmp;
}

CHAR16 *
EFIAPI
StrToTitle (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate (Str);
  INTN      i;
  BOOLEAN   First = TRUE;

  for (i = 0; Tmp[i]; i++) {
    if (First) {
      Tmp[i] = TO_UPPER (Tmp[i]);
      First = FALSE;
    } else {
      if (Tmp[i] != 0x20) {
        Tmp[i] = TO_LOWER (Tmp[i]);
      } else {
        First = TRUE;
      }
    }
  }

  return Tmp;
}

//Compare strings case insensitive
INTN
EFIAPI
StriCmp (
  IN  CONST CHAR16   *FirstS,
  IN  CONST CHAR16   *SecondS
) {
  if (
    (FirstS == NULL) || (SecondS == NULL) ||
    (StrLen (FirstS) != StrLen (SecondS))
  ) {
    return 1;
  }

  while (*FirstS != L'\0') {
    if ( (((*FirstS >= 'a') && (*FirstS <= 'z')) ? (*FirstS - ('a' - 'A')) : *FirstS ) !=
      (((*SecondS >= 'a') && (*SecondS <= 'z')) ? (*SecondS - ('a' - 'A')) : *SecondS ) ) break;
    FirstS++;
    SecondS++;
  }

  return *FirstS - *SecondS;
}

/**
  Adjusts the size of a previously allocated buffer.

  @param OldPool         - A pointer to the buffer whose size is being adjusted.
  @param OldSize         - The size of the current buffer.
  @param NewSize         - The size of the new buffer.

  @return   The newly allocated buffer.
  @retval   NULL  Allocation failed.

**/
VOID *
EFIAPI
EfiReallocatePool (
  IN VOID     *OldPool,
  IN UINTN    OldSize,
  IN UINTN    NewSize
) {
  VOID  *NewPool = NULL;

  if (NewSize != 0) {
    NewPool = AllocateZeroPool (NewSize);
  }

  if (OldPool != NULL) {
    if (NewPool != NULL) {
      CopyMem (NewPool, OldPool, OldSize < NewSize ? OldSize : NewSize);
    }

    FreePool (OldPool);
  }

  return NewPool;
}

UINT64
EFIAPI
AsciiStrVersionToUint64 (
  CONST   CHAR8 *Version,
  UINT8   MaxDigitByPart,
  UINT8   MaxParts
) {
  UINT64    result = 0;
  UINT16    part_value = 0, part_mult  = 1, max_part_value;

  if (!Version) {
    Version = OsVerUndetected; //pointer to non-NULL string
  }

  while (MaxDigitByPart--) {
    part_mult = part_mult * 10;
  }

  max_part_value = part_mult - 1;

  while (*Version && (MaxParts > 0)) {  //Slice - NULL pointer dereferencing
    if (*Version >= '0' && *Version <= '9') {
      part_value = part_value * 10 + (*Version - '0');

      if (part_value > max_part_value) {
        part_value = max_part_value;
      }
    }
    else if (*Version == '.') {
      result = MultU64x64 (result, part_mult) + part_value;
      part_value = 0;
      MaxParts--;
    }

    Version++;
  }

  while (MaxParts--) {
    result = MultU64x64 (result, part_mult) + part_value;
    part_value = 0; // part_value is only used at first pass
  }

  return result;
}

CHAR8 *
EFIAPI
AsciiStrToLower (
  IN CHAR8   *Str
) {
  CHAR8   *Tmp = AllocateCopyPool (AsciiStrSize (Str), Str);
  INTN      i;

  for (i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_ALOWER (Tmp[i]);
  }

  return Tmp;
}

CHAR8 *
EFIAPI
AsciiStriStr (
  IN CHAR8    *String,
  IN CHAR8    *SearchString
) {
  return AsciiStrStr (AsciiStrToLower (String), AsciiStrToLower (SearchString));
}

/*
  Taken from Shell
  Trim leading trailing spaces
*/
EFI_STATUS
EFIAPI
AsciiTrimSpaces (
  IN CHAR8 **String
) {
  if (!String || !(*String)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Remove any spaces and tabs at the beginning of the (*String).
  //
  while (((*String)[0] == ' ') || ((*String)[0] == '\t')) {
    CopyMem ((*String), (*String) + 1, AsciiStrSize ((*String)) - sizeof ((*String)[0]));
  }

  //
  // Remove any spaces and tabs at the end of the (*String).
  //
  while (
    (AsciiStrLen (*String) > 0) &&
    (
      ((*String)[AsciiStrLen ((*String)) - 1] == ' ') ||
      ((*String)[AsciiStrLen ((*String)) - 1] == '\t')
    )
  ) {
    (*String)[AsciiStrLen ((*String)) - 1] = '\0';
  }

  return (EFI_SUCCESS);
}

// If Null-terminated strings are case insensitive equal or its sSize symbols are equal then TRUE
BOOLEAN
EFIAPI
AsciiStriNCmp (
  IN  CONST CHAR8   *FirstS,
  IN  CONST CHAR8   *SecondS,
  IN  CONST UINTN    sSize
) {
  INTN    i = sSize;

  while ( i && (*FirstS != '\0') ) {
    if ( (((*FirstS >= 'a') && (*FirstS <= 'z')) ? (*FirstS - ('a' - 'A')) : *FirstS ) !=
      (((*SecondS >= 'a') && (*SecondS <= 'z')) ? (*SecondS - ('a' - 'A')) : *SecondS ) ) return FALSE;
    FirstS++;
    SecondS++;
    i--;
  }

  return TRUE;
}

// This function determines ascii string length ending by space.
// search restricted to MaxLen, for example
// AsciiTrimStrLen ("ABC    ", 20) == 3
// if MaxLen=0 then as usual strlen but bugless
UINTN
EFIAPI
AsciiTrimStrLen (
  CHAR8   *String,
  UINTN   MaxLen
) {
  UINTN   Len = 0;
  CHAR8   *BA;

  if (MaxLen > 0) {
    for (Len=0; Len < MaxLen; Len++) {
      if (String[Len] == 0) {
        break;
      }
    }

    BA = &String[Len - 1];

    while ((Len != 0) && ((*BA == ' ') || (*BA == 0))) {
      BA--;
      Len--;
    }
  } else {
    BA = String;
    while (*BA) {
      BA++;
      Len++;
    }
  }

  return Len;
}

// Case insensitive search of WhatString in WhereString
BOOLEAN
EFIAPI
AsciiStrStriN (
  IN  CONST CHAR8   *WhatString,
  IN  CONST UINTN   sWhatSize,
  IN  CONST CHAR8   *WhereString,
  IN  CONST UINTN   sWhereSize
) {
  INTN      i = sWhereSize;
  BOOLEAN   Found = FALSE;

  if (sWhatSize > sWhereSize) return FALSE;
  for (; i && !Found; i--) {
    Found = AsciiStriNCmp (WhatString, WhereString, sWhatSize);
    WhereString++;
  }

  return Found;
}

UINT8
EFIAPI
HexStrToUint8 (
  CHAR8   *buf
) {
  INT8  i = 0;

  if (IS_DIGIT (buf[0])) {
    i = buf[0]-'0';
  } else if (IS_HEX (buf[0])) {
    i = (buf[0] | 0x20) - 'a' + 10;
  }

  if (AsciiStrLen (buf) == 1) {
    return i;
  }

  i <<= 4;
  if (IS_DIGIT (buf[1])) {
    i += buf[1]-'0';
  } else if (IS_HEX (buf[1])) {
    i += (buf[1] | 0x20) - 'a' + 10;
  }

  return i;
}

BOOLEAN
IsHexDigit (
  CHAR8   c
) {
  return (IS_DIGIT (c) || (IS_HEX (c))) ? TRUE : FALSE;
}

//out value is a number of byte. If len is even then out = len/2

UINT32
EFIAPI
Hex2Bin (
  IN  CHAR8   *hex,
  OUT UINT8   *bin,
  IN  UINT32  len
) {  //assume len = number of UINT8 values
  CHAR8   *p, buf[3];
  UINT32  i, outlen = 0;

  if ((hex == NULL) || (bin == NULL) || (len <= 0 )|| (AsciiStrLen (hex) < len * 2)) {
    //DBG ("[ERROR] bin2hex input error\n"); //this is not error, this is empty value
    return FALSE;
  }

  buf[2] = '\0';
  p = (CHAR8 *)hex;

  for (i = 0; i < len; i++) {
    while ((*p == 0x20) || (*p == ',')) {
      p++; //skip spaces and commas
    }

    if (*p == 0) {
      break;
    }

    if (!IsHexDigit (p[0]) || !IsHexDigit (p[1])) {
      //MsgLog ("[ERROR] bin2hex '%a' syntax error\n", hex);
      return 0;
    }

    buf[0] = *p++;
    buf[1] = *p++;
    bin[i] = HexStrToUint8 (buf);
    outlen++;
  }

  //bin[outlen] = 0;

  return outlen;
}

CHAR8 *
EFIAPI
Bytes2HexStr (
  UINT8   *Bytes,
  UINTN   Len
) {
  INT32   i,  y;
  CHAR8   *Nib = "0123456789ABCDEF",
          *Res = AllocatePool ((Len * 2) + 1);

  for (i = 0, y = 0; i < Len; i++) {
    Res[y++] = Nib[(Bytes[i] >> 4) & 0x0F];
    Res[y++] = Nib[Bytes[i] & 0x0F];
  }

  Res[y] = '\0';

  return Res;
}

/** Returns pointer to last Char in String or NULL. */
CHAR16 *
EFIAPI
GetStrLastChar (
  IN CHAR16   *String
) {
  CHAR16    *Pos;

  if ((String == NULL) || (*String == L'\0')) {
    return NULL;
  }

  // go to end
  Pos = String;

  while (*Pos != L'\0') {
    Pos++;
  }

  Pos--;

  return Pos;
}

/** Returns pointer to last occurence of Char in String or NULL. */
CHAR16 *
EFIAPI
GetStrLastCharOccurence (
  IN CHAR16   *String,
  IN CHAR16   Char
) {
  CHAR16    *Pos;

  if ((String == NULL) || (*String == L'\0')) {
    return NULL;
  }

  // go to end
  Pos = String;
  while (*Pos != L'\0') {
    Pos++;
  }

  // search for Char
  while ((*Pos != Char) && (Pos != String)) {
    Pos--;
  }

  return (*Pos == Char) ? Pos : NULL;
}

/** Returns TRUE if String1 starts with String2, FALSE otherwise.
Compares just first 8 bits of chars (valid for ASCII), case insensitive.. */
BOOLEAN
EFIAPI
StriStartsWith (
  IN CHAR16   *String1,
  IN CHAR16   *String2
) {
  CHAR16    Chr1, Chr2;
  BOOLEAN   Result;

  //DBG ("StriStarts ('%s', '%s') ", String1, String2);

  if ((String1 == NULL) || (String2 == NULL)) {
    return FALSE;
  }

  if ((*String1 == L'\0') && (*String2 == L'\0')) {
    return TRUE;
  }

  if ((*String1 == L'\0') || (*String2 == L'\0')) {
    return FALSE;
  }

  Chr1 = TO_UPPER (*String1);
  Chr2 = TO_UPPER (*String2);

  while ((Chr1 != L'\0') && (Chr2 != L'\0') && (Chr1 == Chr2)) {
    String1++;
    String2++;
    Chr1 = TO_UPPER (*String1);
    Chr2 = TO_UPPER (*String2);
  }

  Result = ((Chr1 == L'\0') && (Chr2 == L'\0')) ||
           ((Chr1 != L'\0') && (Chr2 == L'\0'));

  //DBG ("=%s \n", Result ? L"TRUE" : L"FALSE");

  return Result;
}

INTN
EFIAPI
CountOccurrences (
  CHAR8   *s,
  CHAR8   c
) {
  return (*s == '\0')
    ? 0
    : CountOccurrences (s + 1, c) + (*s == c);
}
