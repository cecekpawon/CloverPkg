/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  plist.c - plist parsing functions
 *
 *  Copyright (c) 2000-2005 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 *  code split out from drivers.c by Soren Spies, 2005
 */

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/Common/CommonLib.h>
#include <Library/Common/MemLogLib.h>
#include <Library/Common/PlistLib.h>
#include <Library/MemoryAllocationLib.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_PLIST
#define DEBUG_PLIST -1
#endif
#else
#ifdef DEBUG_PLIST
#undef DEBUG_PLIST
#endif
#define DEBUG_PLIST DEBUG_ALL
#endif

#if DEBUG_PLIST > 0
#define DBG(...) MemLog (TRUE, 1, __VA_ARGS__)
#else
#define DBG(...)
#endif

#define USE_REF 1
#define DUMP_PLIST 1

#define kTagsPerBlock (0x1000)

INT32       ParseTagList (CHAR8 *Buffer, TagPtr *Tag, INT32 Type, INT32 Empty);
INT32       ParseNextTag (CHAR8 *Buffer, TagPtr *Tag);

//INT32       dbgCount = 0;
SymbolPtr   gSymbolsHead;
TagPtr      gTagsFree;
CHAR8       *BufferStart = NULL;

sREF        *gRefString = NULL, *gRefInteger = NULL;

// intended to look for two versions of the tag; now just for sizeof
#define MATCHTAG(ParsedTag, KeyTag) (!AsciiStrnCmp (ParsedTag, KeyTag, AsciiStrLen (KeyTag)))

#if DEBUG_PLIST
// for debugging parsing failures
INT32   gTagsParsed;
CHAR8   *gLastTag;
#endif

#if 0
/* Function for basic XML character entities parsing */
typedef struct XMLEntity {
  CONST CHAR8   *name;
        UINTN   nameLen;
        CHAR8   value;
} XMLEntity;

/* This is ugly, but better than specifying the Lengths by hand */
#define _e(str, c) { str, sizeof (str) - 1, c }
CONST XMLEntity ents[] = {
  _e ("quot;",'"'), _e ("aPos;",'\''),
  _e ("lt;",  '<'), _e ("gt;",  '>'),
  _e ("amp;", '&')
};

CHAR8 *
EFIAPI
XMLDecode (
  CHAR8   *src
) {
  UINTN len;
  CONST CHAR8 *s;
  CHAR8 *out, *o;

  if (!src) {
    return 0;
  }

  AsciiTrimSpaces (&src);

  len = AsciiStrLen (src);

#if 0
  out = AllocateZeroPool (len + 1);
  if (!out)
    return 0;
#else // unsafe
  // out is always <= src, let's overwrite src
  out = src;
#endif

  o = out;
  s = src;

  while (s <= (src + len)) { /* Make sure the terminator is also copied */
     if (*s == '&') {
      BOOLEAN   entFound = FALSE;
      UINTN     i, aSize = ARRAY_SIZE (ents);

      s++;
      for ( i = 0; i < aSize; i++) {
        if (AsciiStrnCmp (s, ents[i].name, ents[i].nameLen) == 0) {
          entFound = TRUE;
          break;
        }
      }

      if (entFound) {
        *o++ = ents[i].value;
        s += ents[i].nameLen;
        continue;
      }
    }

    *o++ = *s++;
  }

  return out;
}

VOID
DbgOnce (
  CHAR8   *lbl,
  CHAR8   *str
) {
  INT32   i = 10;
  CHAR8   *subbuff = AllocateZeroPool (i);

  if (dbgCount) {
    return;
  }

  CopyMem ( subbuff, str, i - 1 );
  subbuff[i - 1] = '\0';
  DBG ("#----- %a: '%a'\n", lbl, subbuff);
  FreePool (subbuff);
}
#endif

INTN
GetPropInt (
  CHAR8   *Prop
) {
  if ((Prop[0] == '0') && (TO_AUPPER (Prop[1]) == 'X')) {
    return (INTN)AsciiStrHexToUintn (Prop);
  }

  if (Prop[0] == '-') {
    return -(INTN)AsciiStrDecimalToUintn (Prop + 1);
  }

  return (INTN)AsciiStrDecimalToUintn (Prop);
}

//
//  Attributes
//

#if USE_REF
CHAR8 *
GetAttr (
  CHAR8   *Haystack,
  CHAR8   *Needle
) {
  CHAR8   *Str = AsciiStrStr (Haystack, Needle);

  if (Str != NULL) {
    INT32   i = 0;
    CHAR8   *Prop;

    Str += AsciiStrLen (Needle) + 1;
    Prop = AllocateZeroPool (AsciiStrSize (Str));

    while (Str[i] != '"') {
      Prop[i] = Str[i];
      i++;
    }

    return Prop;
  }

  return NULL;
}

INT32
GetAttrID (
  CHAR8   *Str,
  CHAR8   *Attr
) {
  CHAR8   *AttrVal = GetAttr (Str, Attr);

  if (AttrVal == NULL) {
    return -1;
  }

  return (INT32)GetPropInt (AttrVal);
}

//
//  Reference: Chameleon
//

VOID
SaveRefString (
  CHAR8   *Val,
  INT32   Id,
  INTN    Size
) {
  sREF  *Tmp = gRefString, *NewRef;

  while (Tmp) {
    if (Tmp->id == Id) {
      Tmp->string = AllocateCopyPool (AsciiStrSize (Val), Val);
      Tmp->size = Size;
      return;
    }

    Tmp = Tmp->next;
  }

  NewRef = AllocatePool (sizeof (sREF));
  NewRef->string = AllocateCopyPool (AsciiStrSize (Val), Val);
  NewRef->size = Size;
  NewRef->id = Id;
  NewRef->next = gRefString;

  gRefString = NewRef;
}

VOID
SaveRefInteger (
  CHAR8   *Val,
  INTN    DecVal,
  INT32   Id,
  INTN    Size
) {
  sREF  *Tmp = gRefInteger, *NewRef;

  while (Tmp) {
    if (Tmp->id == Id) {
      Tmp->string = AllocateCopyPool (AsciiStrSize (Val), Val);
      Tmp->integer = DecVal;
      Tmp->size = Size;
      return;
    }

    Tmp = Tmp->next;
  }

  NewRef = AllocatePool (sizeof (sREF));
  NewRef->string = AllocateCopyPool (AsciiStrSize (Val), Val);
  NewRef->integer = DecVal;
  NewRef->size = Size;
  NewRef->id = Id;
  NewRef->next = gRefInteger;

  gRefInteger = NewRef;
}
#endif

EFI_STATUS
EFIAPI
GetRefString (
  IN  TagPtr  Tag,
  IN  INT32   Id,
  OUT CHAR8   **Val,
  OUT INTN    *Size
) {
  sREF  *Tmp = Tag->ref_strings;

  while (Tmp) {
    if (Tmp->id == Id) {
      *Val = AllocateCopyPool (AsciiStrSize (Tmp->string), Tmp->string);
      *Size = Tmp->size;
      return EFI_SUCCESS;
    }

    Tmp = Tmp->next;
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
GetRefInteger (
   IN TagPtr  Tag,
   IN INT32   Id,
  OUT CHAR8   **Val,
  OUT INTN    *DecVal,
  OUT INTN    *Size
) {
  sREF  *Tmp = Tag->ref_integer;

  while (Tmp) {
    if (Tmp->id == Id) {
      *Val = AllocateCopyPool (AsciiStrSize (Tmp->string), Tmp->string);
      *DecVal = Tmp->integer;
      *Size = Tmp->size;
      return EFI_SUCCESS;
    }

    Tmp = Tmp->next;
  }

  return EFI_UNSUPPORTED;
}

//
//  Symbol
//

SymbolPtr
FindSymbol (
  CHAR8       *String,
  SymbolPtr   *PrevSymbol
) {
  SymbolPtr   Symbol = gSymbolsHead, Prev = 0;

  while (Symbol != 0) {
    if (!AsciiStrCmp (Symbol->string, String)) {
      break;
    }

    Prev = Symbol;
    Symbol = Symbol->next;
  }

  if ((Symbol != 0) && (PrevSymbol != 0)) {
    *PrevSymbol = Prev;
  }

  return Symbol;
}

CHAR8 *
NewSymbol (
  CHAR8   *String
) {
  SymbolPtr   Symbol;

  // Look for string in the list of Symbols.
  Symbol = FindSymbol (String, 0);

  // Add the new Symbol.
  if (Symbol == 0) {
    Symbol = AllocateZeroPool (sizeof (Symbol));
    if (Symbol == 0) {
      return 0;
    }

    AsciiTrimSpaces (&String);

    // Set the Symbol's data.
    Symbol->refCount = 0;
    Symbol->string = AllocateCopyPool (AsciiStrSize (String), String);

    // Add the Symbol to the list.
    Symbol->next = gSymbolsHead;
    gSymbolsHead = Symbol;
  }

  // Update the refCount and return the string.
  Symbol->refCount++;
  return Symbol->string;
}

// currently a no-op
VOID
FreeSymbol (
  CHAR8   *String
) {
  SymbolPtr   Symbol, Prev;

  // Look for string in the list of Symbols.
  Symbol = FindSymbol (String, &Prev);
  if (Symbol == 0) {
    return;
  }

  // Update the refCount.
  Symbol->refCount--;

  if (Symbol->refCount != 0) {
    return;
  }

  // Remove the Symbol from the list.
  if (Prev != 0) {
    Prev->next = Symbol->next;
  } else {
    gSymbolsHead = Symbol->next;
  }

  // Free the Symbol's memory.
  FreePool (Symbol);
}

//
//  Parse Tag - Data
//

INT32
GetNextTag (
  CHAR8   *Buffer,
  CHAR8   **Tag,
  INT32   *Start,
  INT32   *Empty
) {
  INT32   Cnt, Cnt2;

  if (Tag == 0) {
    return -1;
  }

  // Find the start of the tag.
  Cnt = 0;
  while ((Buffer[Cnt] != '\0') && (Buffer[Cnt] != '<')) {
    Cnt++;
  }

  if (Buffer[Cnt] == '\0') {
    return -1;
  }

  // Find the end of the tag.
  Cnt2 = Cnt + 1;
  while ((Buffer[Cnt2] != '\0') && (Buffer[Cnt2] != '>')) {
    Cnt2++;
  }

  if (Buffer[Cnt2] == '\0') {
    return -1;
  }

  if (Empty && (Cnt2 > 1)) {
    *Empty = Buffer[Cnt2 - 1] == '/';
  }

  // Fix the tag data.
  *Tag = Buffer + Cnt + 1;
  Buffer[Cnt2] = '\0';
  if (Start) {
    *Start = Cnt;
  }

  return Cnt2 + 1;
}

INT32
FixDataMatchingTag (
  CHAR8   *Buffer,
  CHAR8   *Tag
) {
  INT32   Length, Start = 0, Stop;
  CHAR8   *EndTag;

  while (1) {
    Length = GetNextTag (Buffer + Start, &EndTag, &Stop, NULL);
    if (Length == -1) {
      return -1;
    }

    if ((*EndTag == '/') && !AsciiStrCmp (EndTag + 1, Tag)) {
      break;
    }

    Start += Length;
  }

  Buffer[Start + Stop] = '\0';

  return Start + Length;
}

TagPtr
NewTag () {
  INT32     Cnt;
  TagPtr    Tag;

  if (gTagsFree == 0) {
    Tag = (TagPtr)AllocateZeroPool (kTagsPerBlock * sizeof (TagStruct));
    if (Tag == 0) {
      return 0;
    }

    // Initalize the new tags.
    for (Cnt = 0; Cnt < kTagsPerBlock; Cnt++) {
      Tag[Cnt].type = kTagTypeNone;
      Tag[Cnt].string = 0;
      Tag[Cnt].integer = 0;
      Tag[Cnt].data = 0;
      Tag[Cnt].size = 0;
      Tag[Cnt].tag = 0;
      Tag[Cnt].tagNext = Tag + Cnt + 1;
    }

    Tag[kTagsPerBlock - 1].tagNext = 0;

    gTagsFree = Tag;
  }

  Tag = gTagsFree;
  gTagsFree = Tag->tagNext;

  return Tag;
}

// currently a no-op
VOID
EFIAPI
FreeTag (
  TagPtr  Tag
) {
  if (Tag == 0) {
    return;
  }

  if (Tag->string) {
    FreeSymbol (Tag->string);
  }

  if (Tag->data) {
    FreePool (Tag->data);
  }

  FreeTag (Tag->tag);
  FreeTag (Tag->tagNext);

  // Clear and free the tag.
  Tag->type = kTagTypeNone;
  Tag->string = 0;
  Tag->integer = 0;
  Tag->data = 0;
  Tag->size = 0;
  Tag->tag = 0;
  Tag->tagNext = gTagsFree;

  gTagsFree = Tag;
}

INT32
ParseTagKey (
  CHAR8   *Buffer,
  TagPtr  *Tag
) {
  INT32   Length, Length2;
  CHAR8   *String;
  TagPtr  TmpTag, SubTag = (TagPtr) - 1;  // eliminate possible stale tag

  Length = FixDataMatchingTag (Buffer, kXMLTagKey);
  if (Length == -1) {
    return -1;
  }

  Length2 = ParseNextTag (Buffer + Length, &SubTag);
  if (Length2 == -1) {
    return -1;
  }

  if (SubTag == (TagPtr) - 1) {
    SubTag = NULL;
  }

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    FreeTag (SubTag);
    return -1;
  }

  String = NewSymbol (Buffer);
  if (String == 0) {
    FreeTag (SubTag);
    FreeTag (TmpTag);
    return -1;
  }

  AsciiTrimSpaces (&String);

  if (!AsciiStrLen (String) || (String[0] == '#')) {
    FreeTag (SubTag);
    SubTag = NULL;
  }

  TmpTag->type = kTagTypeKey;
  TmpTag->string = String;
  TmpTag->integer = 0;
  TmpTag->data = 0;
  TmpTag->size = 0;
  TmpTag->tag = SubTag;
  TmpTag->tagNext = 0;

  *Tag = TmpTag;

  return Length + Length2;
}

INT32
ParseTagString (
  CHAR8     *Buffer,
  TagPtr    *Tag,
  INT32     AttrID,
  INTN      AttrSize
) {
  INT32   Length;
  CHAR8   *String;
  TagPtr  TmpTag;

  Length = FixDataMatchingTag (Buffer, kXMLTagString);
  if (Length == -1) {
    return -1;
  }

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    return -1;
  }

  //String = NewSymbol (XMLDecode (Buffer));
  String = NewSymbol (Buffer);
  if (String == 0) {
    FreeTag (TmpTag);
    return -1;
  }

  AsciiTrimSpaces (&String);

  TmpTag->type = AsciiStrLen (String) ? kTagTypeString : kTagTypeNone;
  TmpTag->string = String;
  TmpTag->integer = 0;
  TmpTag->data = 0;
  TmpTag->size = AttrSize;
  TmpTag->tag = 0;
  TmpTag->tagNext = 0;

  TmpTag->id = AttrID;
  TmpTag->ref = -1;

  *Tag = TmpTag;

  return Length;
}

INT32
ParseTagInteger (
  CHAR8     *Buffer,
  TagPtr    *Tag,
  INT32     AttrID,
  INTN      AttrSize
) {
  INT32   Length;
  CHAR8   *String;
  TagPtr  TmpTag;

  Length = FixDataMatchingTag (Buffer, kXMLTagInteger);
  if (Length == -1) {
    return -1;
  }

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    return -1;
  }

  String = NewSymbol (Buffer);
  if (String == 0) {
    FreeTag (TmpTag);
    return -1;
  }

  AsciiTrimSpaces (&String);

  TmpTag->type = kTagTypeInteger;
  //TmpTag->string = 0;
  TmpTag->string = AsciiStrLen (String) ? String : 0;
  TmpTag->integer = GetPropInt (String);
  TmpTag->data = 0;
  TmpTag->size = AttrSize;
  TmpTag->tag = 0;
  TmpTag->tagNext = 0;

  TmpTag->id = AttrID;
  TmpTag->ref = -1;

  *Tag = TmpTag;

  return Length;
}

INT32
ParseTagData (
  CHAR8   *Buffer,
  TagPtr  *Tag
) {
  INT32   Length;
  TagPtr  TmpTag;
  CHAR8   *String;

  Length = FixDataMatchingTag (Buffer, kXMLTagData);
  if (Length == -1) {
    return -1;
  }

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    return -1;
  }

  String = NewSymbol (Buffer);
  if (String == 0) {
    FreeTag (TmpTag);
    return -1;
  }

  AsciiTrimSpaces (&String);

  TmpTag->type = AsciiStrLen (String) ? kTagTypeData : kTagTypeNone;
  TmpTag->string = AsciiStrLen (String) ? String : 0;
  TmpTag->integer = 0;
  TmpTag->data = Base64Decode (String, &TmpTag->size);
  TmpTag->tag = 0;
  TmpTag->tagNext = 0;

  *Tag = TmpTag;

  return Length;
}

#if 0
INT32
ParseTagDate (
  CHAR8   *Buffer,
  TagPtr  *Tag
) {
  INT32   Length;
  TagPtr  TmpTag;

  Length = FixDataMatchingTag (Buffer, kXMLTagDate);
  if (Length == -1) {
    return -1;
  }

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    return -1;
  }

  TmpTag->type = kTagTypeDate;
  TmpTag->integer = 0;
  TmpTag->data = 0;
  TmpTag->size = 0;
  TmpTag->string = 0;
  TmpTag->tag = 0;
  TmpTag->tagNext = 0;

  *Tag = TmpTag;

  return Length;
}
#endif

INT32
ParseTagBoolean (
  CHAR8   *Buffer,
  TagPtr  *Tag,
  INT32   Type
) {
  TagPtr  TmpTag;

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    return -1;
  }

  TmpTag->type = Type;
  TmpTag->integer = 0;
  TmpTag->data = 0;
  TmpTag->size = 0;
  TmpTag->string = 0;
  TmpTag->tag = 0;
  TmpTag->tagNext = 0;

  *Tag = TmpTag;

  return 0;
}

INT32
ParseNextTag (
  CHAR8   *Buffer,
  TagPtr  *Tag
) {
  INT32     Length, Pos, Empty = 0;
  UINTN     Len;
  CHAR8     *TagName;
  //TagPtr  refTag;

  Length = GetNextTag (Buffer, &TagName, 0, &Empty);
  if (Length == -1) {
    return -1;
  }

#if DEBUG_PLIST
  gLastTag = TagName;
  gTagsParsed++;
#endif

  Pos = Length;

  Len = AsciiStrLen (TagName);

  if (MATCHTAG (TagName, kXMLTagFalse)) {
    Length = ParseTagBoolean (Buffer + Pos, Tag, kTagTypeFalse);
    goto Finish;

  } else if (MATCHTAG (TagName, kXMLTagTrue)) {
    Length = ParseTagBoolean (Buffer + Pos, Tag, kTagTypeTrue);
    goto Finish;
  }

#ifndef USE_REF
  if (Len && (TagName[Len - 1] == '/')) {
    //DBG ("TagName: %a\n", TagName);
    //*Tag = 0;
    return Pos;
  }
#endif

  if (MATCHTAG (TagName, kXMLTagPList)) {
    Length = 0;  // just a header; nothing to parse
    // return-via-reference tag should be left alone

  } else if (MATCHTAG (TagName, kXMLTagDict)) {
    Length = ParseTagList (Buffer + Pos, Tag, kTagTypeDict, Empty);

  } else if (!AsciiStrCmp (TagName, kXMLTagKey)) {
    Length = ParseTagKey (Buffer + Pos, Tag);
  }

  /* else if (MATCHTAG (TagName, kXMLTagReference) &&
  (refTag = TagFromRef (TagName, sizeof (kXMLTagReference) - 1))) {
      *Tag = refTag;
      Length = 0;

  }*/ else if (MATCHTAG (TagName, kXMLTagString)) {
    INT32   AttrID = -1, AttrSize = 0;
#if USE_REF
    INT32   AttrIDRef = -1;

    if (MATCHTAG (TagName, kXMLTagString " ")) {
      AttrSize = GetAttrID (TagName, kXMLTagSIZE);
      if (AttrSize == -1) {
        AttrSize = 0;
      }

      AttrIDRef = GetAttrID (TagName, kXMLTagIDREF);

      if (AttrIDRef != -1) {
        TagPtr TmpTag = NewTag ();
        if (TmpTag == 0) {
          return -1;
        }

        TmpTag->type = kTagTypeString;
        TmpTag->string = 0;
        TmpTag->integer = 0;
        TmpTag->data = 0;
        TmpTag->size = AttrSize;
        TmpTag->tag = 0;
        TmpTag->tagNext = 0;
        TmpTag->id = -1;
        TmpTag->ref = AttrIDRef;

        *Tag = TmpTag;

        Length = 0;
      } else {
        AttrID = GetAttrID (TagName, kXMLTagID);
        Length = ParseTagString (Buffer + Pos, Tag, AttrID, AttrSize);
        if (AttrID != -1) {
          SaveRefString (Buffer + Pos, AttrID, AttrSize);
        }
      }
    } else {
#endif
      Length = ParseTagString (Buffer + Pos, Tag, AttrID, AttrSize);
#if USE_REF
    }
#endif

  } else if (MATCHTAG (TagName, kXMLTagInteger)) {
    INT32   AttrID = -1, AttrSize = 0;
#if USE_REF
    INT32   AttrIDRef = -1;

    if (MATCHTAG (TagName, kXMLTagInteger " ")) {
      AttrSize = GetAttrID (TagName, kXMLTagSIZE);
      if (AttrSize == -1) {
        AttrSize = 0;
      }

      AttrIDRef = GetAttrID (TagName, kXMLTagIDREF);

      if (AttrIDRef != -1) {
        TagPtr TmpTag = NewTag ();
        if (TmpTag == 0) {
          return -1;
        }

        TmpTag->type = kTagTypeInteger;
        TmpTag->string = 0;
        TmpTag->integer = 0;
        TmpTag->data = 0;
        TmpTag->size = AttrSize;
        TmpTag->tag = 0;
        TmpTag->tagNext = 0;
        TmpTag->id = -1;
        TmpTag->ref = AttrIDRef;

        *Tag = TmpTag;

        Length = 0;
      } else {
        AttrID = GetAttrID (TagName, kXMLTagID);
        Length = ParseTagInteger (Buffer + Pos, Tag, AttrID, AttrSize);
        if (AttrID != -1) {
          SaveRefInteger ((*Tag)->string, (*Tag)->integer, AttrID, AttrSize);
        }
      }
    } else {
#endif
      Length = ParseTagInteger (Buffer + Pos, Tag, AttrID, AttrSize);
#if USE_REF
    }
#endif

  } else if (MATCHTAG (TagName, kXMLTagData)) {
    Length = ParseTagData (Buffer + Pos, Tag);

/*
  } else if (!AsciiStrCmp (TagName, kXMLTagDate)) {
    Length = ParseTagDate (Buffer + Pos, Tag);

*/
  } else if (MATCHTAG (TagName, kXMLTagArray)) {
    Length = ParseTagList (Buffer + Pos, Tag, kTagTypeArray, Empty);

  } else {
    // it wasn't parsed so we consumed no additional characters
    Length = 0;

    if (TagName[0] == '/') { // was it an end tag (indicated w/*Tag = 0)
      *Tag = 0;
    } else {
      //DBG ("ignored plist tag: %s (*Tag: %x)\n", TagName, *Tag);
      *Tag = (TagPtr) - 1;  // we're * not * returning a tag
    }
  }

  Finish:

  if (Length == -1) {
    return -1;
  }

  return Pos + Length;
}

INT32
ParseTagList (
  CHAR8   *Buffer,
  TagPtr  *Tag,
  INT32   Type,
  INT32   Empty
) {
  INT32   Length = -1, Pos = 0, Size = 0;
  TagPtr  TagList = 0, TmpTag = (TagPtr) - 1;

  if (!Empty) {
    while (1) {
      TmpTag = (TagPtr) - 1;
      Length = ParseNextTag (Buffer + Pos, &TmpTag);
      if (Length == -1) {
        break;
      }

      Pos += Length;

      // detect end of list
      if (TmpTag == 0) {
        break;
      }

      // if we made a new tag, insert into list
      if (TmpTag != (TagPtr) - 1) {
        TmpTag->tagNext = TagList;
        TagList = TmpTag;
        Size++;
      }
    }

    if (Length == -1) {
      FreeTag (TagList);
      return -1;
    }
  }

  TmpTag = NewTag ();
  if (TmpTag == 0) {
    FreeTag (TagList);
    return -1;
  }

  TmpTag->type = Type;
  TmpTag->string = 0;
  TmpTag->integer = 0;
  TmpTag->data = 0;
  TmpTag->size = Size;
  TmpTag->tag = TagList;
  TmpTag->tagNext = 0;
  TmpTag->offset = (UINT32)(BufferStart ? Buffer - BufferStart : 0);
  TmpTag->taglen = (UINT32)((Pos > Length) ? Pos - Length : 0);

  *Tag = TmpTag;

  return Pos;
}

EFI_STATUS
EFIAPI
ParseXML (
  CHAR8   *Buffer,
  UINT32  BufSize,
  TagPtr  *Dict
) {
  INT32     Length, Pos,
            FixedBufferSize = BufSize ? BufSize : (UINT32)AsciiStrLen (Buffer);
  TagPtr    ModuleDict;
  CHAR8     *FixedBuffer = NULL;

  if (Dict == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FixedBuffer = AllocateZeroPool (FixedBufferSize + 1);
  if (FixedBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (FixedBuffer, Buffer, FixedBufferSize);
  BufferStart = FixedBuffer;

#if DEBUG_PLIST
  gTagsParsed = 0;
  gLastTag = NULL;
#endif

  Pos = 0;

  gRefString = NULL;
  gRefInteger = NULL;

  while (1) {
    ModuleDict = (TagPtr) - 1;  // have to detect changes to by-ref parameter
    Length = ParseNextTag (FixedBuffer + Pos, &ModuleDict);
    if (Length == -1) {
      break;
    }

    Pos += Length;

    if (ModuleDict == 0) {
      continue;
    }

    // did we actually create anything?
    if (ModuleDict != (TagPtr) - 1) {
      if (
        (ModuleDict->type == kTagTypeDict) ||
        (ModuleDict->type == kTagTypeArray)
      ) {
        break;
      }

      FreeTag (ModuleDict);
    }
  }

  if (Length != -1) {
//#if USE_REF
    ModuleDict->ref_strings = (gRefString != NULL)
      ? AllocateCopyPool (sizeof (gRefString) * sizeof (sREF), gRefString)
      : NULL;
    ModuleDict->ref_integer = (gRefInteger != NULL)
      ? AllocateCopyPool (sizeof (gRefInteger) * sizeof (sREF), gRefInteger)
      : NULL;
//#endif

    *Dict = ModuleDict;

#if DEBUG_PLIST
  } else {
    //DBG ("ParseXML gagged (-1) after '%s' (%d tags); buf + Pos: %s\n",
    //  gLastTag, gTagsParsed, Buffer + Pos);
#endif
  }

  FreePool (FixedBuffer);

  //dbgCount++;

  // return 0 for no error
  return (Length != -1) ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
}

//
// Public
//

TagPtr
EFIAPI
GetProperty (
  TagPtr  Dict,
  CHAR8   *Key
) {
  TagPtr  TagList, Tag;

  if (Dict->type != kTagTypeDict) {
    return 0;
  }

  Tag = 0;    // ?
  TagList = Dict->tag;

  while (TagList) {
    Tag = TagList;
    TagList = Tag->tagNext;

    if ((Tag->type != kTagTypeKey) || (Tag->string == 0)) {
      continue;
    }

    if (!AsciiStrCmp (Tag->string, Key)) {
      return Tag->tag;
    }
  }

  return 0;
}

INTN
EFIAPI
GetTagCount (
  TagPtr    Dict
) {
  return Dict->size;
}

EFI_STATUS
EFIAPI
GetElement (
  TagPtr    Dict,
  INTN      Id,
  INTN      Count,
  TagPtr    *Dict1
) {
  TagPtr  Child;

  if (!Dict || !Dict1 || (Dict->type != kTagTypeArray)) {
    return EFI_UNSUPPORTED;
  }

  Child = Dict->tag;

  if (Count > Id) {
    while (Child && (--Count > Id)) {
      Child = Child->tagNext;
    }
  } else {
    INTN    element = 0;

    while (Child && (element++ < Id)) {
      Child = Child->tagNext;
    }
  }

  *Dict1 = Child;

  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
GetPropertyBool (
  TagPtr    Prop,
  BOOLEAN   Default
) {
  return (
    (Prop == NULL)
      ? Default
      : (Prop->type == kTagTypeTrue)
  );
}

INTN
EFIAPI
GetPropertyInteger (
  TagPtr  Prop,
  INTN    Default
) {
  if (Prop != NULL) {
    if (Prop->type == kTagTypeInteger) {
      return Prop->integer; //(INTN)Prop->string;
    } else if (Prop->type == kTagTypeString) {
      if ((Prop->string[0] == '0') && (TO_AUPPER (Prop->string[1]) == 'X')) {
        return (INTN)AsciiStrHexToUintn (Prop->string);
      }

      if (Prop->string[0] == '-') {
        return -(INTN)AsciiStrDecimalToUintn (Prop->string + 1);
      }

      return (INTN)AsciiStrDecimalToUintn (Prop->string);
    }
  }

  return Default;
}

CHAR8 *
EFIAPI
GetPropertyString (
  TagPtr  Prop,
  CHAR8   *Default
) {
  if (
    (Prop != NULL) &&
    (Prop->type == kTagTypeString) &&
    AsciiStrLen (Prop->string)
  ) {
    return Prop->string;
  }

  return Default;
}

//
// returns binary setting in a new allocated buffer and data length in dataLen.
// data can be specified in <data></data> base64 encoded
// or in <string></string> hex encoded
//

VOID *
EFIAPI
GetDataSetting (
  IN   TagPtr   Dict,
  IN   CHAR8    *PropName,
  OUT  UINTN    *DataLen
) {
  TagPtr    Prop;
  UINT8     *Data = NULL;
  UINTN     Len;

  Prop = GetProperty (Dict, PropName);
  if (Prop != NULL) {
    if ((Prop->type == kTagTypeData) && Prop->data && Prop->size) {
      // data property
      Data = AllocateCopyPool (Prop->size, Prop->data);

      if (Data != NULL) {
        *DataLen = Prop->size;
      }

      /*
        DBG ("Data: %p, Len: %d = ", Data, Prop->size);
        for (i = 0; i < Prop->size; i++) {
          DBG ("%02x ", Data[i]);
        }
        DBG ("\n");
      */
    } else {
      // assume data in hex encoded string property
      Data = StringDataToHex (Prop->string, &Len);
      *DataLen = Len;

      /*
        DBG ("Data (str): %p, Len: %d = ", data, len);
        for (i = 0; i < Len; i++) {
          DBG ("%02x ", data[i]);
        }
        DBG ("\n");
      */
    }
  }

  return Data;
}

//
//  Debug
//

#if DUMP_PLIST
CHAR16  *StrRet = NULL;
UINTN   StrRetLen = 0;

CHAR8 *
DumpSpaces (
  INT32   Depth
) {
  INT32   Cnt = 0, Total = (Depth * 2);
  CHAR8   *Spaces = AllocateZeroPool (Total);

  SetMem (&Spaces[Cnt], Total - Cnt, 0x20);

  Spaces[Total] = '\0';

  return Spaces;
}

VOID
Dump (
  CHAR16  *String,
  INT32   Depth
) {
  CHAR16  *StrTmp = PoolPrint (L"%a%s\n", DumpSpaces (Depth), String);

  StrnCatGrow (&StrRet, &StrRetLen, StrTmp, StrSize (StrTmp));
  FreePool (StrTmp);
}

VOID
EFIAPI
DumpBody (
  CHAR16  **Str,
  INT32   Depth
) {
  CHAR16  *NewStr = *Str;
  UINTN   i, Len = StrLen (NewStr);

  for (i = 0; i < Len; i++) {
    if (NewStr[i] == '>') {
      NewStr += i + 1;
      while ((*NewStr != '<') && (*(NewStr++) != '\0'));
      break;
    }
  }

  for (i = Len; i >= 0; i--) {
    if (NewStr[i] == '<') {
      while (i-- && (NewStr[i] != '\n'));
      NewStr[i + 1] = '\0';
      break;
    }
  }

  *Str = PoolPrint (L"%a%s", DumpSpaces (Depth), NewStr);
}

VOID
DumpTagDict (
  TagPtr  Tag,
  INT32   Depth
) {
  TagPtr  TagList;

  if (Tag->tag == 0) {
    Dump (PoolPrint (L"<%a/>", kXMLTagDict), Depth);
  } else {
    Dump (PoolPrint (L"<%a>", kXMLTagDict), Depth);

    TagList = Tag->tag;
    while (TagList) {
      DumpTag (TagList, Depth + 1);
      TagList = TagList->tagNext;
    }

    Dump (PoolPrint (L"</%a>", kXMLTagDict), Depth);
  }
}

VOID
DumpTagKey (
  TagPtr  Tag,
  INT32   Depth
) {
  Dump (PoolPrint (L"<%a>%a</%a>", kXMLTagKey, Tag->string, kXMLTagKey), Depth);

  DumpTag (Tag->tag, Depth);
}

VOID
DumpTagString (
  TagPtr  Tag,
  INT32   Depth
) {
  Dump (
    Tag->string
      ? PoolPrint (L"<%a>%a</%a>", kXMLTagString, Tag->string, kXMLTagString)
      : PoolPrint (L"<%a/>", kXMLTagString),
    Depth
  );
}

/* integers used to live as char * s but we need 64 bit ints */
VOID
DumpTagInteger (
  TagPtr  Tag,
  INT32   Depth
) {
  Dump (PoolPrint (L"<%a>%a</%a>", kXMLTagInteger, Tag->string, kXMLTagInteger), Depth);
}

VOID
DumpTagData (
  TagPtr  Tag,
  INT32   Depth
) {
  Dump (
    Tag->string
      ? PoolPrint (L"<%a>%a</%a>", kXMLTagData, Tag->string, kXMLTagData)
      : PoolPrint (L"<%a/>", kXMLTagData),
    Depth
  );
}

#if 0
VOID
DumpTagDate (
  TagPtr  Tag,
  INT32   Depth
) {
  Dump (PoolPrint (L"<%a>%x</%a>", kXMLTagDate, Tag->string, kXMLTagDate), Depth);
}
#endif

VOID
DumpTagBoolean (
  TagPtr  Tag,
  INT32   Depth
) {
  Dump (PoolPrint (L"<%a/>", (Tag->type == kTagTypeTrue) ? kXMLTagTrue : kXMLTagFalse), Depth);
}

VOID
DumpTagArray (
  TagPtr  Tag,
  INT32   Depth
) {
  TagPtr  TagList;

  if (Tag->tag == 0) {
    Dump (PoolPrint (L"<%a/>", kXMLTagArray), Depth);
  } else {
    Dump (PoolPrint (L"<%a>", kXMLTagArray), Depth);

    TagList = Tag->tag;
    while (TagList) {
      DumpTag (TagList, Depth + 1);
      TagList = TagList->tagNext;
    }

    Dump (PoolPrint (L"</%a>", kXMLTagArray), Depth);
  }
}

CHAR16 *
EFIAPI
DumpTag (
  TagPtr  Tag,
  INT32   Depth
) {
  if (Tag == 0) {
    goto Finish;
  }

  switch (Tag->type) {
    case kTagTypeDict :
      DumpTagDict (Tag, Depth);
      break;

    case kTagTypeKey :
      DumpTagKey (Tag, Depth);
      break;

    case kTagTypeString :
      DumpTagString (Tag, Depth);
      break;

    case kTagTypeInteger :
      DumpTagInteger (Tag, Depth);
      break;

    case kTagTypeData :
      DumpTagData (Tag, Depth);
      break;

    //case kTagTypeDate :
    //  DumpTagDate (Tag, Depth);
    //  break;

    case kTagTypeFalse :
    case kTagTypeTrue :
      DumpTagBoolean (Tag, Depth);
      break;

    case kTagTypeArray :
      DumpTagArray (Tag, Depth);
      break;

    default :
      break;
  }

  Finish:

  return StrRet;
}
#endif
