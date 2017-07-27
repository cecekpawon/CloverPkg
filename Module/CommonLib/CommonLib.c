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
 *
 *
 * c source to a base64 encoding/decoding algorithm implementation
 *
 * This is part of the libb64 project, and has been placed in the public domain.
 * For details, see http://sourceforge.net/projects/libb64
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
#define DBG(...) MemLog (TRUE, 1, __VA_ARGS__)
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
} Base64Step;

typedef struct {
  Base64Step  Step;
  CHAR8       PlainChar;
  //INT32        StepCount;
} Base64State;

//CONST INT32 B64_CHARS_PER_LINE = 72;

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
  Base64State    *State
) {
  State->Step = StepA;
  State->PlainChar = 0;
}

INT32
Base64DecodeBlock (
  CONST CHAR8         *Code,
  CONST INT32         Length,
        CHAR8         *PlainText,
        Base64State   *State
) {
  CONST CHAR8   *CodeChar = Code;
        CHAR8   *PlainChar = PlainText;
        INT32   Fragment;

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
        *PlainChar = (CHAR8)((Fragment & 0x00f) << 4);

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
        *PlainChar = (CHAR8)((Fragment & 0x003) << 6);

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

UINT8 *
Base64Decode (
  IN  CHAR8   *Data,
  OUT UINTN   *Size
) {
  UINTN         Len;
  INT32         DecodedSizeInternal;
  UINT8         *Res;
  CHAR8         *DecodedData;
  Base64State   State;

  if (Data == NULL) {
    return NULL;
  }

  Len = AsciiStrLen (Data);

  if (Len == 0) {
    return NULL;
  }

  Res = AllocateZeroPool (Len);
  DecodedData = (CHAR8 *)Res;

  Base64InitDecodeState (&State);
  DecodedSizeInternal = Base64DecodeBlock (Data, (CONST INT32)Len, (CHAR8 *)DecodedData, &State);
  DecodedData += DecodedSizeInternal;
  *DecodedData = 0;

  if (Size != NULL) {
    *Size = (UINTN)DecodedSizeInternal;
  }

  return Res;
}

VOID
Base64InitEncodeState (
  Base64State   *State
) {
  State->Step = StepA;
  State->PlainChar = 0;
  //State->StepCount = 0;
}

CHAR8
Base64EncodeValue (
  CHAR8   Value
) {
  STATIC CONST CHAR8    *Encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  if (Value > 63) {
    return '=';
  }

  return Encoding[(INT32)Value];
}

INT32
Base64EncodeBlock (
  CONST CHAR8         *PlainText,
        INT32         Length,
        CHAR8         *Code,
        Base64State   *State
) {
  CONST CHAR8 *PlainChar = PlainText;
  CONST CHAR8 *CONST PlainTextEnd = PlainText + Length;
        CHAR8 *CodeChar = Code, Result, Fragment;

  Result = State->PlainChar;

  switch (State->Step) {
    while (1) {
      case StepA:
        if (PlainChar == PlainTextEnd) {
          State->PlainChar = Result;
          State->Step = StepA;
          return (INT32)(CodeChar - Code);
        }

        Fragment = *PlainChar++;
        Result = (CHAR8)((Fragment & 0x0fc) >> 2);
        *CodeChar++ = Base64EncodeValue (Result);
        Result = (CHAR8)((Fragment & 0x003) << 4);

      case StepB:
        if (PlainChar == PlainTextEnd) {
          State->PlainChar = Result;
          State->Step = StepB;
          return (INT32)(CodeChar - Code);
        }

        Fragment = *PlainChar++;
        Result |= (CHAR8)((Fragment & 0x0f0) >> 4);
        *CodeChar++ = Base64EncodeValue (Result);
        Result = (CHAR8)((Fragment & 0x00f) << 2);

      case StepC:
        if (PlainChar == PlainTextEnd) {
          State->PlainChar = Result;
          State->Step = StepC;
          return (INT32)(CodeChar - Code);
        }

        Fragment = *PlainChar++;
        Result |= (CHAR8)((Fragment & 0x0c0) >> 6);
        *CodeChar++ = Base64EncodeValue (Result);
        Result = (CHAR8)((Fragment & 0x03f) >> 0);
        *CodeChar++ = Base64EncodeValue (Result);

        /*
        ++(State->StepCount);
        if (State->StepCount == (B64_CHARS_PER_LINE / 4)) {
          *CodeChar++ = '\n';
          State->StepCount = 0;
        }
        */

      //case StepD:
      default:
        continue;
    }
  }

  /* control should not reach here */
  return (INT32)(CodeChar - Code);
}

INT32
Base64EncodeBlockEnd (
  CHAR8         *Code,
  Base64State   *State
) {
  CHAR8   *CodeChar = Code;

  switch (State->Step) {
    case StepB:
      *CodeChar++ = Base64EncodeValue (State->PlainChar);
      *CodeChar++ = '=';
      *CodeChar++ = '=';
      break;

    case StepC:
      *CodeChar++ = Base64EncodeValue (State->PlainChar);
      *CodeChar++ = '=';
      break;

    //case StepA:
    //case StepD:
    default:
      break;
  }

  //*CodeChar++ = '\n';

  return (INT32)(CodeChar - Code);
}

UINT8 *
Base64Encode (
  IN  CHAR8   *Data,
  OUT UINTN   *Size
) {
  UINTN         Len;
  INT32         EncodedSizeInternal;
  UINT8         *Res;
  Base64State   State;
  CHAR8         *EncodedData;

  if (Data == NULL) {
    return NULL;
  }

  Len = AsciiStrLen (Data);

  if (Len == 0) {
    return NULL;
  }

  Res = AllocateZeroPool (AVALUE_MAX_SIZE);
  EncodedData = (CHAR8 *)Res;

  Base64InitEncodeState (&State);
  EncodedSizeInternal = Base64EncodeBlock (Data, (CONST INT32)Len, (CHAR8 *)EncodedData, &State);
  EncodedData += EncodedSizeInternal;
  EncodedSizeInternal += Base64EncodeBlockEnd ((CHAR8 *)EncodedData, &State);
  EncodedData += EncodedSizeInternal;
  *EncodedData = 0;

  if (Size != NULL) {
    *Size = (UINTN)EncodedSizeInternal;
  }

  return Res;
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
  Dest  = AllocateZeroPool (Size);
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
  Safely append with automatic string resizing given length of Destination and
  desired length of copy from Source.

  append the first D characters of Source to the end of Destination, where D is
  the lesser of Count and the StrLen() of Source. If appending those D characters
  will fit within Destination (whose Size is given as CurrentSize) and
  still leave room for a NULL terminator, then those characters are appended,
  starting at the original terminating NULL of Destination, and a new terminating
  NULL is appended.

  If appending D characters onto Destination will result in a overflow of the size
  given in CurrentSize the string will be grown such that the copy can be performed
  and CurrentSize will be updated to the new size.

  If Source is NULL, there is nothing to append, just return the current buffer in
  Destination.

  if Destination is NULL, then ASSERT()
  if Destination's current length (including NULL terminator) is already more then
  CurrentSize, then ASSERT()

  @param[in, out] Destination   The String to append onto
  @param[in, out] CurrentSize   on call the number of bytes in Destination.  On
                                return possibly the new size (still in bytes).  if NULL
                                then allocate whatever is needed.
  @param[in]      Source        The String to append from
  @param[in]      Count         Maximum number of characters to append.  if 0 then
                                all are appended.

  @return Destination           return the resultant string.
**/
CHAR16 *
EFIAPI
StrnCatGrow (
  IN OUT  CHAR16    **Destination,
  IN OUT  UINTN     *CurrentSize,
  IN      CHAR16    *Source,
  IN      UINTN     Count
) {
  UINTN   DestinationStartSize, NewSize;

  //
  // ASSERTs
  //
  //ASSERT (Destination != NULL);
  if (Destination == NULL) {
    return (Source == NULL) ? Source : NULL;
  }

  //
  // If there's nothing to do then just return Destination
  //
  if (Source == NULL) {
    return (*Destination);
  }

  //
  // allow for un-initialized pointers, based on size being 0
  //
  if ((CurrentSize != NULL) && (*CurrentSize == 0)) {
    *Destination = NULL;
  }

  //
  // allow for NULL pointers address as Destination
  //
  if (*Destination != NULL) {
    //ASSERT (CurrentSize != 0);
    if (CurrentSize == 0) {
      return Source;
    }
    DestinationStartSize = StrSize (*Destination);
    //ASSERT (DestinationStartSize <= *CurrentSize);
    if (DestinationStartSize > *CurrentSize) {
      return Source;
    }
  } else {
    DestinationStartSize = 0;
    //ASSERT (*CurrentSize == 0);
  }

  //
  // Append all of Source?
  //
  if (Count == 0) {
    Count = StrLen (Source);
  }

  //
  // Test and grow if required
  //
  if (CurrentSize != NULL) {
    NewSize = *CurrentSize;
    if (NewSize < DestinationStartSize + (Count * sizeof (CHAR16))) {
      while (NewSize < (DestinationStartSize + (Count * sizeof (CHAR16)))) {
        NewSize += 2 * Count * sizeof (CHAR16);
      }

      *Destination = ReallocatePool (*CurrentSize, NewSize, *Destination);
      *CurrentSize = NewSize;
    }
  } else {
    NewSize = (Count + 1) * sizeof (CHAR16);
    *Destination = AllocateZeroPool (NewSize);
  }

  //
  // Now use standard StrnCat on a big enough buffer
  //
  if (*Destination == NULL) {
    return (NULL);
  }

  StrnCatS (*Destination, NewSize / sizeof (CHAR16), Source, Count);

  return *Destination;
}

VOID
EFIAPI
RemoveMultiSpaces (
  IN OUT CHAR16   *Str
) {
  CHAR16  *Dst = Str;

  for (; *Str; ++Str) {
    *Dst++ = *Str;

    if (*Str == 0x20) {
      do ++Str;
      while (*Str == 0x20);
      --Str;
    }
  }

  *Dst = 0;
}

VOID
EFIAPI
StrTrim (
  IN OUT CHAR16   *Str
) {
  CHAR16  *Pointer1, *Pointer2;

  if (*Str == 0) {
    return;
  }

  //
  // Trim off the leading and trailing characters c
  //
  for (Pointer1 = Str; (*Pointer1 != 0) && (*Pointer1 == 0x20); Pointer1++) {
    ;
  }

  Pointer2 = Str;
  if (Pointer2 == Pointer1) {
    while (*Pointer1 != 0) {
      Pointer2++;
      Pointer1++;
    }
  } else {
    while (*Pointer1 != 0) {
      *Pointer2 = *Pointer1;
      Pointer1++;
      Pointer2++;
    }

    *Pointer2 = 0;
  }

  for (Pointer1 = Str + StrLen(Str) - 1; Pointer1 >= Str && *Pointer1 == 0x20; Pointer1--) {
    ;
  }

  if  (Pointer1 !=  Str + StrLen(Str) - 1) {
    *(Pointer1 + 1) = 0;
  }
}

VOID
EFIAPI
StrCleanSpaces (
  IN OUT CHAR16   **Str
) {
  StrTrim (*Str);
  RemoveMultiSpaces (*Str);
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
  CONST   CHAR8   *Version,
          UINT8   MaxDigitByPart,
          UINT8   MaxParts
) {
  UINT64    Res = 0;
  UINT16    PartValue = 0, PartMult  = 1, MaxPartValue;

  if (!Version) {
    //Version = DARWIN_OS_VER_DEFAULT; //pointer to non-NULL string
    return Res;
  }

  while (MaxDigitByPart--) {
    PartMult = PartMult * 10;
  }

  MaxPartValue = PartMult - 1;

  while (*Version && (MaxParts > 0)) {  //Slice - NULL pointer dereferencing
    if (*Version >= '0' && *Version <= '9') {
      PartValue = PartValue * 10 + (*Version - '0');

      if (PartValue > MaxPartValue) {
        PartValue = MaxPartValue;
      }
    }

    else if (*Version == '.') {
      Res = MultU64x64 (Res, PartMult) + PartValue;
      PartValue = 0;
      MaxParts--;
    }

    Version++;
  }

  while (MaxParts--) {
    Res = MultU64x64 (Res, PartMult) + PartValue;
    PartValue = 0; // PartValue is only used at first pass
  }

  return Res;
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
AsciiStrToUpper (
  IN CHAR8   *Str
) {
  CHAR8   *Tmp = AllocateCopyPool (AsciiStrSize (Str), Str);
  INTN      i;

  for (i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_AUPPER (Tmp[i]);
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
  IN CHAR8  **String
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

/**
  Safely append with automatic string resizing given length of Destination and
  desired length of copy from Source.

  append the first D characters of Source to the end of Destination, where D is
  the lesser of Count and the StrLen() of Source. If appending those D characters
  will fit within Destination (whose Size is given as CurrentSize) and
  still leave room for a NULL terminator, then those characters are appended,
  starting at the original terminating NULL of Destination, and a new terminating
  NULL is appended.

  If appending D characters onto Destination will result in a overflow of the size
  given in CurrentSize the string will be grown such that the copy can be performed
  and CurrentSize will be updated to the new size.

  If Source is NULL, there is nothing to append, just return the current buffer in
  Destination.

  if Destination is NULL, then ASSERT()
  if Destination's current length (including NULL terminator) is already more then
  CurrentSize, then ASSERT()

  @param[in, out] Destination   The String to append onto
  @param[in, out] CurrentSize   on call the number of bytes in Destination.  On
                                return possibly the new size (still in bytes).  if NULL
                                then allocate whatever is needed.
  @param[in]      Source        The String to append from
  @param[in]      Count         Maximum number of characters to append.  if 0 then
                                all are appended.

  @return Destination           return the resultant string.
**/
CHAR8 *
EFIAPI
AsciiStrnCatGrow (
  IN OUT  CHAR8   **Destination,
  IN OUT  UINTN   *CurrentSize,
  IN      CHAR8   *Source,
  IN      UINTN   Count
) {
  UINTN DestinationStartSize, NewSize;

  //
  // ASSERTs
  //
  //ASSERT (Destination != NULL);
  if (Destination == NULL) {
    return (Source != NULL) ? Source : NULL;
  }

  //
  // If there's nothing to do then just return Destination
  //
  if (Source == NULL) {
    return (*Destination);
  }

  //
  // allow for un-initialized pointers, based on size being 0
  //
  if ((CurrentSize != NULL) && (*CurrentSize == 0)) {
    *Destination = NULL;
  }

  //
  // allow for NULL pointers address as Destination
  //
  if (*Destination != NULL) {
    //ASSERT (CurrentSize != 0);
    if (CurrentSize == 0) {
      return Source;
    }
    DestinationStartSize = AsciiStrSize (*Destination);
    //ASSERT (DestinationStartSize <= *CurrentSize);
    if (DestinationStartSize > *CurrentSize) {
      return Source;
    }
  } else {
    DestinationStartSize = 0;
    //ASSERT (*CurrentSize == 0);
  }

  //
  // Append all of Source?
  //
  if (Count == 0) {
    Count = AsciiStrLen (Source);
  }

  //
  // Test and grow if required
  //
  if (CurrentSize != NULL) {
    NewSize = *CurrentSize;
    if (NewSize < DestinationStartSize + (Count * sizeof (CHAR8))) {
      while (NewSize < (DestinationStartSize + (Count * sizeof (CHAR8)))) {
        NewSize += 2 * Count * sizeof (CHAR8);
      }
      *Destination = ReallocatePool (*CurrentSize, NewSize, *Destination);
      *CurrentSize = NewSize;
    }
  } else {
    NewSize = (Count + 1) * sizeof (CHAR8);
    *Destination = AllocateZeroPool (NewSize);
  }

  //
  // Now use standard StrnCat on a big enough buffer
  //
  if (*Destination == NULL) {
    return (NULL);
  }

  AsciiStrnCatS (*Destination, NewSize / sizeof (CHAR8), Source, Count);

  return *Destination;
}

BOOLEAN
IsHexDigit (
  CHAR8   C
) {
  return (IS_DIGIT (C) || (IS_HEX (C))) ? TRUE : FALSE;
}

UINT8
EFIAPI
HexStrToUint8 (
  CHAR8   *Buf
) {
  INT8  i = 0;

  if (IS_DIGIT (Buf[0])) {
    i = Buf[0] - '0';
  } else if (IS_HEX (Buf[0])) {
    i = (Buf[0] | 0x20) - 'a' + 10;
  }

  if (AsciiStrLen (Buf) == 1) {
    return i;
  }

  i <<= 4;
  if (IS_DIGIT (Buf[1])) {
    i += Buf[1] - '0';
  } else if (IS_HEX (Buf[1])) {
    i += (Buf[1] | 0x20) - 'a' + 10;
  }

  return i;
}

//out value is a number of byte. If len is even then out = len/2

UINT32
EFIAPI
Hex2Bin (
  IN  CHAR8   *Hex,
  OUT UINT8   *Bin,
  IN  UINT32  Len
) {  //assume len = number of UINT8 values
  CHAR8   *p, Buf[3];
  UINT32  i, Outlen = 0;

  if ((Hex == NULL) || (Bin == NULL) || (Len <= 0 )|| (AsciiStrLen (Hex) < Len * 2)) {
    //DBG ("[ERROR] bin2hex input error\n"); //this is not error, this is empty value
    return 0;
  }

  Buf[2] = '\0';
  p = (CHAR8 *)Hex;

  if ((p[0] == '0') && (TO_UPPER (p[1]) == 'X')) {
    p += 2;
  }

  for (i = 0; i < Len; i++) {
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

    Buf[0] = *p++;
    Buf[1] = *p++;
    Bin[i] = HexStrToUint8 (Buf);
    Outlen++;
  }

  //Bin[Outlen] = 0;

  return Outlen;
}

VOID *
EFIAPI
StringDataToHex (
  IN   CHAR8    *Val,
  OUT  UINTN    *DataLen
) {
  UINT8     *Data = NULL;
  UINT32    Len;

  Len = (UINT32)AsciiStrLen (Val) >> 1; // number of hex digits
  Data = AllocateZeroPool (Len); // 2 chars per byte, one more byte for odd number
  Len  = Hex2Bin (Val, Data, Len);
  *DataLen = Len;

  if (!Len) {
    FreePool (Data);
    return (UINT8 *)Val;
  }

  return Data;
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
  CHAR8   *Str,
  CHAR8   Char
) {
  return (*Str == '\0')
    ? 0
    : CountOccurrences (Str + 1, Char) + (*Str == Char);
}

CHAR8 *
EFIAPI
FindCharDelimited (
  IN CHAR8    *InString,
  IN CHAR8    InChar,
  IN UINTN    Index
) {
  CHAR8    *FoundString = NULL;

  if (InString != NULL) {
    UINTN    StartPos = 0, CurPos = 0, InLength = AsciiStrLen (InString);

    // After while() loop, StartPos marks start of item #Index
    while ((Index > 0) && (CurPos < InLength)) {
      if (InString[CurPos] == InChar) {
        Index--;
        StartPos = CurPos + 1;
      }

      CurPos++;
    }

    // After while() loop, CurPos is one past the end of the element
    while (CurPos < InLength) {
      if (InString[CurPos] == InChar) {
        break;
      }

      CurPos++;
    }

    if (Index == 0) {
      CurPos -= StartPos;
      FoundString = AllocateCopyPool (CurPos, &InString[StartPos]);
      FoundString[CurPos] = 0;
    }
  }

  return FoundString;
}

SVersion *
EFIAPI
StrToVersion (
  CHAR8   *Str
) {
  SVersion  *SVer = AllocateZeroPool (sizeof (SVersion));

  UINTN   i = 0;
  CHAR8   *StrFound = NULL;

  if (Str != NULL) {
    while (TRUE) {
      StrFound = FindCharDelimited (Str, '.', i);

      if (!StrFound || !AsciiStrLen (StrFound)) {
        break;
      }

      switch (i++) {
        case 0:
          SVer->VersionMajor = (UINT8)AsciiStrDecimalToUintn (StrFound);
          break;
        case 1:
          SVer->VersionMinor = (UINT8)AsciiStrDecimalToUintn (StrFound);
          break;
        case 2:
          SVer->Revision = (UINT8)AsciiStrDecimalToUintn (StrFound);
          break;
      }
    }
  }

  if (StrFound != NULL) {
    FreePool (StrFound);
  }

  return SVer;
}

UINT8 *
EFIAPI
StrToMacAddress (
  IN CHAR8   *Str
) {
  UINTN   i = 0, y = 0, Count = AsciiStrLen (Str);
  UINT8   *Ret = AllocateZeroPool (6 * sizeof (UINT8));
  CHAR8   *StrFound = NULL;

  if (!Str || (Count != 17)) {
    return NULL;
  }

  while (y < 6) {
    StrFound = FindCharDelimited (Str, ':', i++);

    if (!StrFound || !AsciiStrLen (StrFound)) {
      break;
    }

    Ret[y++] = (UINT8)AsciiStrHexToUintn (StrFound);
  }

  return (y == 6) ? Ret : NULL;
}
