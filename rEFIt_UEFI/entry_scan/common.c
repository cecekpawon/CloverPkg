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

#include "Platform.h"

#ifndef DEBUG_ALL
#define DEBUG_COMMON_MENU 1
#else
#define DEBUG_COMMON_MENU DEBUG_ALL
#endif

#if DEBUG_COMMON_MENU == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_COMMON_MENU, __VA_ARGS__)
#endif

CONST CHAR16 *OsxPathLCaches[] = {
   L"\\System\\Library\\Caches\\com.apple.kext.caches\\Startup\\kernelcache",
   L"\\System\\Library\\Caches\\com.apple.kext.caches\\Startup\\Extensions.mkext",
   L"\\System\\Library\\Extensions.mkext",
   L"\\com.apple.recovery.boot\\kernelcache",
   L"\\com.apple.recovery.boot\\Extensions.mkext",
   L"\\.IABootFiles\\kernelcache"
};

CONST UINTN OsxPathLCachesCount = ARRAY_SIZE(OsxPathLCaches);
CHAR8 *OsVerUndetected = "10.10.10";  //longer string

extern BOOLEAN CopyKernelAndKextPatches(IN OUT KERNEL_AND_KEXT_PATCHES *Dst, IN KERNEL_AND_KEXT_PATCHES *Src);

//--> Base64

typedef enum {
  step_a, step_b, step_c, step_d
} base64_decodestep;

typedef struct {
  base64_decodestep   step;
  char                plainchar;
} base64_decodestate;

int base64_decode_value(char value_in) {
  static const signed char decoding[] = {
    62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,
    7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,
    29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
  };

  int value_in_i = (unsigned char) value_in;
  value_in_i -= 43;
  if (value_in_i < 0 || (unsigned int) value_in_i >= sizeof decoding) return -1;
  return decoding[value_in_i];
}

void
base64_init_decodestate (
  base64_decodestate    *state_in
) {
  state_in->step = step_a;
  state_in->plainchar = 0;
}

int
base64_decode_block (
  const char                *code_in,
  const int                 length_in,
        char                *plaintext_out,
        base64_decodestate  *state_in
) {
  const char  *codechar = code_in;
  char        *plainchar = plaintext_out;
  int         fragment;

  *plainchar = state_in->plainchar;

  switch (state_in->step) {
    while (1) {
      case step_a:
        do {
          if (codechar == code_in+length_in) {
            state_in->step = step_a;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }
          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar    = (char) ((fragment & 0x03f) << 2);

      case step_b:
        do {
          if (codechar == code_in+length_in) {
            state_in->step = step_b;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }

          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar++ |= (char) ((fragment & 0x030) >> 4);
        *plainchar    = (char) ((fragment & 0x00f) << 4);

      case step_c:
        do {
          if (codechar == code_in+length_in) {
            state_in->step = step_c;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }
          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar++ |= (char) ((fragment & 0x03c) >> 2);
        *plainchar    = (char) ((fragment & 0x003) << 6);

      case step_d:
        do {
          if (codechar == code_in+length_in)
          {
            state_in->step = step_d;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }
          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar++   |= (char) ((fragment & 0x03f));
    }
  }

  /* control should not reach here */
  return (int)(plainchar - plaintext_out);
}

/** UEFI interface to base54 decode.
 * Decodes EncodedData into a new allocated buffer and returns it. Caller is responsible to FreePool() it.
 * If DecodedSize != NULL, then size od decoded data is put there.
 */
UINT8
*Base64Decode (
  IN  CHAR8     *EncodedData,
  OUT UINTN     *DecodedSize
) {
  UINTN                 EncodedSize, DecodedSizeInternal;
  UINT8                 *DecodedData;
  base64_decodestate    state_in;

  if (EncodedData == NULL) {
    return NULL;
  }

  EncodedSize = AsciiStrLen(EncodedData);

  if (EncodedSize == 0) {
    return NULL;
  }

  // to simplify, we'll allocate the same size, although smaller size is needed
  DecodedData = AllocateZeroPool(EncodedSize);

  base64_init_decodestate(&state_in);
  DecodedSizeInternal = base64_decode_block(EncodedData, (const int)EncodedSize, (char*) DecodedData, &state_in);

  if (DecodedSize != NULL) {
    *DecodedSize = DecodedSizeInternal;
  }

  return DecodedData;
}

//<-- Base64

LOADER_ENTRY
*DuplicateLoaderEntry (
  IN LOADER_ENTRY   *Entry
) {
  if(Entry == NULL) {
    return NULL;
  }

  LOADER_ENTRY *DuplicateEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));

  if (DuplicateEntry) {
    DuplicateEntry->me.Tag                = Entry->me.Tag;
    //DuplicateEntry->me.AtClick          = ActionEnter;
    DuplicateEntry->Volume                = Entry->Volume;
    DuplicateEntry->DevicePathString      = EfiStrDuplicate(Entry->DevicePathString);
    DuplicateEntry->LoadOptions           = EfiStrDuplicate(Entry->LoadOptions);
    DuplicateEntry->LoaderPath            = EfiStrDuplicate(Entry->LoaderPath);
    DuplicateEntry->VolName               = EfiStrDuplicate(Entry->VolName);
    DuplicateEntry->DevicePath            = Entry->DevicePath;
    DuplicateEntry->Flags                 = Entry->Flags;
    DuplicateEntry->LoaderType            = Entry->LoaderType;
    DuplicateEntry->OSVersion             = Entry->OSVersion;
    DuplicateEntry->BuildVersion          = Entry->BuildVersion;
    DuplicateEntry->KernelAndKextPatches  = Entry->KernelAndKextPatches;
  }

  return DuplicateEntry;
}

CHAR16
*ToggleLoadOptions (
      UINT32    State,
  IN  CHAR16    *LoadOptions,
  IN  CHAR16    *LoadOption
) {
  return State ? AddLoadOption(LoadOptions, LoadOption) : RemoveLoadOption(LoadOptions, LoadOption);
}

CHAR16
*AddLoadOption (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  // If either option strings are null nothing to do
  if (LoadOptions == NULL) {
    if (LoadOption == NULL) {
      return NULL;
    }

    // Duplicate original options as nothing to add
    return EfiStrDuplicate(LoadOption);
  }

  // If there is no option or it is already present duplicate original
  else if ((LoadOption == NULL) || BootArgsExists(LoadOptions, LoadOption)) {
    return EfiStrDuplicate(LoadOptions);
  }

  // Otherwise add option
  return PoolPrint(L"%s %s", LoadOptions, LoadOption);
}

CHAR16
*RemoveLoadOption (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  CHAR16    *Placement, *NewLoadOptions;
  UINTN     Length, Offset, OptionLength;

  //DBG("LoadOptions: '%s', remove LoadOption: '%s'\n", LoadOptions, LoadOption);
  // If there are no options then nothing to do
  if (LoadOptions == NULL) {
    return NULL;
  }

  // If there is no option to remove then duplicate original
  if (LoadOption == NULL) {
    return EfiStrDuplicate(LoadOptions);
  }

  // If not present duplicate original
  Placement = StriStr(LoadOptions, LoadOption);
  if (Placement == NULL) {
    return EfiStrDuplicate(LoadOptions);
  }

  // Get placement of option in original options
  Offset = (Placement - LoadOptions);
  Length = StrLen(LoadOptions);
  OptionLength = StrLen(LoadOption);

  // If this is just part of some larger option (contains non-space at the beginning or end)
  if (
    ((Offset > 0) && (LoadOptions[Offset - 1] != L' ')) ||
    (((Offset + OptionLength) < Length) && (LoadOptions[Offset + OptionLength] != L' '))
  ) {
    return EfiStrDuplicate(LoadOptions);
  }

  // Consume preceeding spaces
  while ((Offset > 0) && (LoadOptions[Offset - 1] == L' ')) {
    OptionLength++;
    Offset--;
  }

  // Consume following spaces
  while (LoadOptions[Offset + OptionLength] == L' ') {
    OptionLength++;
  }

  // If it's the whole string return NULL
  if (OptionLength == Length) {
    return NULL;
  }

  if (Offset == 0) {
    // Simple case - we just need substring after OptionLength position
    NewLoadOptions = EfiStrDuplicate(LoadOptions + OptionLength);
  } else {
    // The rest of LoadOptions is Length - OptionLength, but we may need additional space and ending 0
    NewLoadOptions = AllocateZeroPool((Length - OptionLength + 2) * sizeof(CHAR16));
    // Copy preceeding substring
    CopyMem(NewLoadOptions, LoadOptions, Offset * sizeof(CHAR16));

    if ((Offset + OptionLength) < Length) {
      // Copy following substring, but include one space also
      OptionLength--;

      CopyMem (
        NewLoadOptions + Offset,
        LoadOptions + Offset + OptionLength,
        (Length - OptionLength - Offset) * sizeof(CHAR16)
      );
    }
  }

  return NewLoadOptions;
}

INTN
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
    Ch1 = TO_LOWER(*Str1);
    Ch2 = TO_LOWER(*Str2);

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

CHAR16
*StriStr (
  IN CHAR16   *Str,
  IN CHAR16   *SearchFor
) {
  CHAR16  *End;
  UINTN   Length, SearchLength;

  if ((Str == NULL) || (SearchFor == NULL)) {
    return NULL;
  }

  Length = StrLen(Str);
  if (Length == 0) {
    return NULL;
  }

  SearchLength = StrLen(SearchFor);

  if (SearchLength > Length) {
    return NULL;
  }

  End = Str + (Length - SearchLength) + 1;

  while (Str < End) {
    if (StrniCmp(Str, SearchFor, SearchLength) == 0) {
      return Str;
    }
    ++Str;
  }

  return NULL;
}

CHAR16
*StrToLower (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate(Str);
  INTN      i;

  for(i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_LOWER(Tmp[i]);
  }

  return Tmp;
}

CHAR16
*StrToUpper (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate(Str);
  INTN      i;

  for(i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_UPPER(Tmp[i]);
  }

  return Tmp;
}

CHAR16
*StrToTitle (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate(Str);
  INTN      i;
  BOOLEAN   First = TRUE;

  for(i = 0; Tmp[i]; i++) {
    if (First) {
      Tmp[i] = TO_UPPER(Tmp[i]);
      First = FALSE;
    } else {
      if (Tmp[i] != 0x20) {
        Tmp[i] = TO_LOWER(Tmp[i]);
      } else {
        First = TRUE;
      }
    }
  }

  return Tmp;
}

UINT64
AsciiStrVersionToUint64 (
  const CHAR8   *Version,
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
      result = MultU64x64(result, part_mult) + part_value;
      part_value = 0;
      MaxParts--;
    }

    Version++;
  }

  while (MaxParts--) {
    result = MultU64x64(result, part_mult) + part_value;
    part_value = 0; // part_value is only used at first pass
  }

  return result;
}

CHAR8
*AsciiStrToLower (
  IN CHAR8   *Str
) {
  CHAR8   *Tmp = AllocateCopyPool(AsciiStrSize(Str), Str);
  INTN      i;

  for(i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_ALOWER(Tmp[i]);
  }

  return Tmp;
}

CHAR8
*AsciiStriStr (
  IN CHAR8    *String,
  IN CHAR8    *SearchString
) {
  return AsciiStrStr(AsciiStrToLower(String), AsciiStrToLower(SearchString));
}


/*
  Taken from Shell
  Trim leading trailing spaces
*/
EFI_STATUS
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
    CopyMem((*String), (*String)+1, AsciiStrSize((*String)) - sizeof((*String)[0]));
  }

  //
  // Remove any spaces and tabs at the end of the (*String).
  //
  while (
    (AsciiStrLen (*String) > 0) &&
    (
      ((*String)[AsciiStrLen((*String))-1] == ' ') ||
      ((*String)[AsciiStrLen((*String))-1] == '\t')
    )
  ) {
    (*String)[AsciiStrLen((*String))-1] = '\0';
  }

  return (EFI_SUCCESS);
}

UINT8
hexstrtouint8 (
  CHAR8   *buf
) {
  INT8  i = 0;

  if (IS_DIGIT(buf[0])) {
    i = buf[0]-'0';
  } else if (IS_HEX(buf[0])) {
    i = (buf[0] | 0x20) - 'a' + 10;
  }

  if (AsciiStrLen(buf) == 1) {
    return i;
  }

  i <<= 4;
  if (IS_DIGIT(buf[1])) {
    i += buf[1]-'0';
  } else if (IS_HEX(buf[1])) {
    i += (buf[1] | 0x20) - 'a' + 10;
  }

  return i;
}

BOOLEAN
IsHexDigit (
  CHAR8   c
) {
  return (IS_DIGIT(c) || (IS_HEX(c))) ? TRUE : FALSE;
}

//out value is a number of byte. If len is even then out = len/2

UINT32
hex2bin (
  IN  CHAR8   *hex,
  OUT UINT8   *bin,
      UINT32  len
) {  //assume len = number of UINT8 values
  CHAR8   *p, buf[3];
  UINT32  i, outlen = 0;

  if (hex == NULL || bin == NULL || len <= 0 || AsciiStrLen(hex) < len * 2) {
    //DBG("[ERROR] bin2hex input error\n"); //this is not error, this is empty value
    return FALSE;
  }

  buf[2] = '\0';
  p = (CHAR8 *) hex;

  for (i = 0; i < len; i++) {
    while ((*p == 0x20) || (*p == ',')) {
      p++; //skip spaces and commas
    }

    if (*p == 0) {
      break;
    }

    if (!IsHexDigit(p[0]) || !IsHexDigit(p[1])) {
      MsgLog("[ERROR] bin2hex '%a' syntax error\n", hex);
      return 0;
    }

    buf[0] = *p++;
    buf[1] = *p++;
    bin[i] = hexstrtouint8(buf);
    outlen++;
  }

  //bin[outlen] = 0;

  return outlen;
}

CHAR8
*Bytes2HexStr (
  UINT8   *data,
  UINTN   len
) {
  UINTN   i, j, b = 0;
  CHAR8   *result = AllocateZeroPool((len*2)+1);

  for (i = j = 0; i < len; i++) {
    b = data[i] >> 4;
    result[j++] = (CHAR8) (87 + b + (((b - 10) >> 31) & -39));
    b = data[i] & 0xf;
    result[j++] = (CHAR8) (87 + b + (((b - 10) >> 31) & -39));
  }

  result[j] = '\0';

  return result;
}
