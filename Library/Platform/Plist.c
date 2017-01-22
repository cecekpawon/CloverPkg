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

#include <Library/Platform/Platform.h>

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

#define DBG(...) DebugLog (DEBUG_PLIST, __VA_ARGS__)

#define USE_REF 1
#define DUMP_PLIST 1

#define kTagsPerBlock (0x1000)

INT32       ParseTagList (CHAR8 *buffer, TagPtr *tag, INT32 type, INT32 empty);
INT32       ParseNextTag (CHAR8 *buffer, TagPtr *tag);

INT32       dbgCount = 0;
SymbolPtr   gSymbolsHead;
TagPtr      gTagsFree;
CHAR8       *buffer_start = NULL;

sREF        *ref_strings = NULL, *ref_integer = NULL;

// intended to look for two versions of the tag; now just for sizeof
#define MATCHTAG(parsedTag, keyTag) \
    (!AsciiStrnCmp (parsedTag, keyTag, sizeof (keyTag)-1))

#if DEBUG_PLIST
// for debugging parsing failures
INT32   gTagsParsed;
CHAR8   *gLastTag;
#endif

/* Function for basic XML character entities parsing */
typedef struct XMLEntity {
  CONST CHAR8   *name;
        UINTN   nameLen;
        CHAR8   value;
} XMLEntity;

/* This is ugly, but better than specifying the lengths by hand */
#define _e(str,c) {str,sizeof (str)-1,c}
CONST XMLEntity ents[] = {
  _e ("quot;",'"'), _e ("apos;",'\''),
  _e ("lt;",  '<'), _e ("gt;",  '>'),
  _e ("amp;", '&')
};

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

  CopyMem ( subbuff, str, i-1 );
  subbuff[i-1] = '\0';
  MsgLog ("#----- %a: '%a'\n", lbl, subbuff);
  FreePool (subbuff);
}

CHAR8 *
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
  out = AllocateZeroPool (len+1);
  if (!out)
    return 0;
#else // unsafe
  // out is always <= src, let's overwrite src
  out = src;
#endif

  o = out;
  s = src;

  while (s <= src+len) { /* Make sure the terminator is also copied */
     if ( *s == '&' ) {
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

INTN
GetPropInt (
  CHAR8   *Prop
) {
  if ((Prop[1] == 'x') || (Prop[1] == 'X')) {
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
  CHAR8     *Str,
  CHAR8     *Attr
) {
  CHAR8     *vAttr = GetAttr (Str, Attr);

  if (vAttr == NULL) {
    return -1;
  }

  return (INT32)GetPropInt (vAttr);
}

//
//  Reference: Chameleon
//

VOID
SaveRefString (
  CHAR8   *val,
  INT32   id,
  INTN    size
) {
  sREF  *tmp = ref_strings, *new_ref;

  while (tmp) {
    if (tmp->id == id) {
      tmp->string = AllocateCopyPool (AsciiStrSize (val), val);
      tmp->size = size;
      return;
    }

    tmp = tmp->next;
  }

  new_ref = AllocatePool (sizeof (sREF));
  new_ref->string = AllocateCopyPool (AsciiStrSize (val), val);
  new_ref->size = size;
  new_ref->id = id;
  new_ref->next = ref_strings;

  ref_strings = new_ref;
}

VOID
SaveRefInteger (
  CHAR8   *val,
  INTN    decval,
  INT32   id,
  INTN    size
) {
  sREF  *tmp = ref_integer, *new_ref;

  while (tmp) {
    if (tmp->id == id) {
      tmp->string = AllocateCopyPool (AsciiStrSize (val), val);
      tmp->integer = decval;
      tmp->size = size;
      return;
    }

    tmp = tmp->next;
  }

  new_ref = AllocatePool (sizeof (sREF));
  new_ref->string = AllocateCopyPool (AsciiStrSize (val), val);
  new_ref->integer = decval;
  new_ref->size = size;
  new_ref->id = id;
  new_ref->next = ref_integer;

  ref_integer = new_ref;
}
#endif

EFI_STATUS
GetRefString (
  IN TagPtr   tag,
  IN INT32    id,
  OUT CHAR8   **val,
  OUT INTN    *size
) {
  sREF  *tmp = tag->ref_strings;

  if (tag->ref_strings == NULL) {
    goto error;
  }

  while (tmp) {
    if (tmp->id == id) {
      *val = AllocateCopyPool (AsciiStrSize (tmp->string), tmp->string);
      *size = tmp->size;
      return EFI_SUCCESS;
    }

    tmp = tmp->next;
  }

  error:

  return EFI_UNSUPPORTED;
}

EFI_STATUS
GetRefInteger (
   IN TagPtr  tag,
   IN INT32   id,
  OUT CHAR8   **val,
  OUT INTN    *decval,
  OUT INTN    *size
) {
  sREF  *tmp = tag->ref_integer;

  if (tag->ref_integer == NULL) {
    goto error;
  }

  while (tmp) {
    if (tmp->id == id) {
      *val = AllocateCopyPool (AsciiStrSize (tmp->string), tmp->string);
      *decval = tmp->integer;
      *size = tmp->size;
      return EFI_SUCCESS;
    }

    tmp = tmp->next;
  }

  error:

  return EFI_UNSUPPORTED;
}

//
//  Symbol
//

SymbolPtr
FindSymbol (
  CHAR8       *string,
  SymbolPtr   *prevSymbol
) {
  SymbolPtr   symbol = gSymbolsHead, prev = 0;

  while (symbol != 0) {
    if (!AsciiStrCmp (symbol->string, string)) {
      break;
    }

    prev = symbol;
    symbol = symbol->next;
  }

  if ((symbol != 0) && (prevSymbol != 0)) {
    *prevSymbol = prev;
  }

  return symbol;
}

CHAR8 *
NewSymbol (
  CHAR8   *string
) {
  SymbolPtr   symbol;

  // Look for string in the list of symbols.
  symbol = FindSymbol (string, 0);

  // Add the new symbol.
  if (symbol == 0) {
    symbol = AllocateZeroPool (sizeof (Symbol)/* + AsciiStrLen (string)*/);
    if (symbol == 0) return 0;

    // Set the symbol's data.
    symbol->refCount = 0;
    //AsciiStrCpy (symbol->string, string);
    symbol->string = AllocateCopyPool (AsciiStrSize (string), string);

    // Add the symbol to the list.
    symbol->next = gSymbolsHead;
    gSymbolsHead = symbol;
  }

  // Update the refCount and return the string.
  symbol->refCount++;
  return symbol->string;
}

// currently a no-op
VOID
FreeSymbol (
  CHAR8   *string
) {
  SymbolPtr symbol, prev;

  // Look for string in the list of symbols.
  symbol = FindSymbol (string, &prev);
  if (symbol == 0) return;

  // Update the refCount.
  symbol->refCount--;

  if (symbol->refCount != 0) return;

  // Remove the symbol from the list.
  if (prev != 0) prev->next = symbol->next;
  else gSymbolsHead = symbol->next;

  // Free the symbol's memory.
  FreePool (symbol);
}

//
//  Parse Tag - Data
//

INT32
GetNextTag (
  CHAR8   *buffer,
  CHAR8   **tag,
  INT32   *start,
  INT32   *empty
) {
  INT32   cnt, cnt2;

  if (tag == 0) {
    return -1;
  }

  // Find the start of the tag.
  cnt = 0;
  while ((buffer[cnt] != '\0') && (buffer[cnt] != '<')) {
    cnt++;
  }

  if (buffer[cnt] == '\0') {
    return -1;
  }

  // Find the end of the tag.
  cnt2 = cnt + 1;
  while ((buffer[cnt2] != '\0') && (buffer[cnt2] != '>')) {
    cnt2++;
  }

  if (buffer[cnt2] == '\0') {
    return -1;
  }

  if (empty && (cnt2 > 1)) {
    *empty = buffer[cnt2-1] == '/';
  }

  // Fix the tag data.
  *tag = buffer + cnt + 1;
  buffer[cnt2] = '\0';
  if (start) {
    *start = cnt;
  }

  return cnt2 + 1;
}

INT32
FixDataMatchingTag (
  CHAR8   *buffer,
  CHAR8   *tag
) {
  INT32   length, start = 0, stop;
  CHAR8   *endTag;

  while (1) {
    length = GetNextTag (buffer + start, &endTag, &stop, NULL);
    if (length == -1) {
      return -1;
    }

    if ((*endTag == '/') && !AsciiStrCmp (endTag + 1, tag)) {
      break;
    }

    start += length;
  }

  buffer[start + stop] = '\0';

  return start + length;
}

TagPtr
NewTag () {
  INT32     cnt;
  TagPtr    tag;

  if (gTagsFree == 0) {
    tag = (TagPtr)AllocateZeroPool (kTagsPerBlock * sizeof (TagStruct));
    if (tag == 0) {
      return 0;
    }

    // Initalize the new tags.
    for (cnt = 0; cnt < kTagsPerBlock; cnt++) {
      tag[cnt].type = kTagTypeNone;
      tag[cnt].string = 0;
      tag[cnt].integer = 0;
      tag[cnt].data = 0;
      tag[cnt].size = 0;
      tag[cnt].tag = 0;
      tag[cnt].tagNext = tag + cnt + 1;
    }

    tag[kTagsPerBlock - 1].tagNext = 0;

    gTagsFree = tag;
  }

  tag = gTagsFree;
  gTagsFree = tag->tagNext;

  return tag;
}

// currently a no-op
VOID
FreeTag (
  TagPtr  tag
) {
  if (tag == 0) {
    return;
  }

  if (tag->string) {
    FreeSymbol (tag->string);
  }

  if (tag->data) {
    FreePool (tag->data);
  }

  FreeTag (tag->tag);
  FreeTag (tag->tagNext);

  // Clear and free the tag.
  tag->type = kTagTypeNone;
  tag->string = 0;
  tag->integer = 0;
  tag->data = 0;
  tag->size = 0;
  tag->tag = 0;
  tag->tagNext = gTagsFree;

  gTagsFree = tag;
}

INT32
ParseTagKey (
  CHAR8   *buffer,
  TagPtr  *tag
) {
  INT32   length, length2;
  CHAR8   *string;
  TagPtr  tmpTag, subTag = (TagPtr) - 1;  // eliminate possible stale tag

  length = FixDataMatchingTag (buffer, kXMLTagKey);
  if (length == -1) {
    return -1;
  }

  length2 = ParseNextTag (buffer + length, &subTag);
  if (length2 == -1) {
    return -1;
  }

  if (subTag == (TagPtr) - 1) {
    subTag = NULL;
  }

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    FreeTag (subTag);
    return -1;
  }

  string = NewSymbol (buffer);
  if (string == 0) {
    FreeTag (subTag);
    FreeTag (tmpTag);
    return -1;
  }

  tmpTag->type = kTagTypeKey;
  tmpTag->string = string;
  tmpTag->integer = 0;
  tmpTag->data = 0;
  tmpTag->size = 0;
  tmpTag->tag = subTag;
  tmpTag->tagNext = 0;

  *tag = tmpTag;

  return length + length2;
}

INT32
ParseTagString (
  CHAR8     *buffer,
  TagPtr    *tag,
  INT32     attrID,
  INTN      attrSize
) {
  INT32   length;
  CHAR8   *string;
  TagPtr  tmpTag;

  length = FixDataMatchingTag (buffer, kXMLTagString);
  if (length == -1) {
    return -1;
  }

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    return -1;
  }

  string = XMLDecode (buffer);
  string = NewSymbol (string);
  if (string == 0) {
    FreeTag (tmpTag);
    return -1;
  }

  tmpTag->type = kTagTypeString;
  tmpTag->string = string;
  tmpTag->integer = 0;
  tmpTag->data = 0;
  tmpTag->size = attrSize;
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;

  tmpTag->id = attrID;
  tmpTag->ref = -1;

  *tag = tmpTag;

  return length;
}

INT32
ParseTagInteger (
  CHAR8     *buffer,
  TagPtr    *tag,
  INT32     attrID,
  INTN      attrSize
) {
  INT32   length;
  CHAR8   *string;
  TagPtr  tmpTag;

  length = FixDataMatchingTag (buffer, kXMLTagInteger);
  if (length == -1) {
    return -1;
  }

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    return -1;
  }

  string = NewSymbol (buffer);

  if (string == 0) {
    FreeTag (tmpTag);
    return -1;
  }

  tmpTag->type = kTagTypeInteger;
  tmpTag->string = 0;
  //tmpTag->string = string;
  tmpTag->integer = GetPropInt (string);
  tmpTag->data = 0;
  tmpTag->size = attrSize;
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;

  tmpTag->id = attrID;
  tmpTag->ref = -1;

  *tag = tmpTag;

  return length;
}

INT32
ParseTagData (
  CHAR8   *buffer,
  TagPtr  *tag
) {
  INT32   length;
  TagPtr  tmpTag;
  CHAR8   *string;

  length = FixDataMatchingTag (buffer, kXMLTagData);
  if (length == -1) {
    return -1;
  }

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    return -1;
  }

  string = NewSymbol (buffer);
  tmpTag->type = kTagTypeData;
  tmpTag->string = 0;
  tmpTag->integer = 0;
  tmpTag->data = (UINT8 *)Base64Decode (string, &tmpTag->size);
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;

  *tag = tmpTag;

  return length;
}

INT32
ParseTagDate (
  CHAR8   *buffer,
  TagPtr  *tag
) {
  INT32   length;
  TagPtr  tmpTag;

  length = FixDataMatchingTag (buffer, kXMLTagDate);
  if (length == -1) {
    return -1;
  }

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    return -1;
  }

  tmpTag->type = kTagTypeDate;
  tmpTag->integer = 0;
  tmpTag->data = 0;
  tmpTag->size = 0;
  tmpTag->string = 0;
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;

  *tag = tmpTag;

  return length;
}

INT32
ParseTagBoolean (
  CHAR8   *buffer,
  TagPtr  *tag,
  INT32   type
) {
  TagPtr  tmpTag;

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    return -1;
  }

  tmpTag->type = type;
  tmpTag->integer = 0;
  tmpTag->data = 0;
  tmpTag->size = 0;
  tmpTag->string = 0;
  tmpTag->tag = 0;
  tmpTag->tagNext = 0;

  *tag = tmpTag;

  return 0;
}

INT32
ParseNextTag (
  CHAR8   *buffer,
  TagPtr  *tag
) {
  INT32     length, pos, empty = 0;
  UINTN     tLen;
  CHAR8     *tagName;
  //TagPtr  refTag;
  BOOLEAN   isTagFalse, isTagTrue;

  length = GetNextTag (buffer, &tagName, 0, &empty);
  if (length == -1) {
    return -1;
  }

#if DEBUG_PLIST
  gLastTag = tagName;
  gTagsParsed++;
#endif

  pos = length;

  tLen = AsciiStrLen (tagName);

  isTagFalse =  (!AsciiStrCmp (tagName, kXMLTagFalse) || !AsciiStrCmp (tagName, kXMLTagFalse2));
  isTagTrue =   (!AsciiStrCmp (tagName, kXMLTagTrue)  || !AsciiStrCmp (tagName, kXMLTagTrue2));

#ifndef USE_REF
  if (
    tLen && (tagName[tLen - 1] == '/') &&
    !isTagFalse && !isTagTrue
  ) {
    //MsgLog ("tagName: %a\n", tagName);
    return pos;
  }
#endif

  if (MATCHTAG (tagName, kXMLTagPList)) {
    length = 0;  // just a header; nothing to parse
    // return-via-reference tag should be left alone

  } else if (MATCHTAG (tagName, kXMLTagDict)) {
    length = ParseTagList (buffer + pos, tag, kTagTypeDict, empty);

  } else if (!AsciiStrCmp (tagName, kXMLTagKey)) {
    length = ParseTagKey (buffer + pos, tag);
  }

  /* else if (MATCHTAG (tagName, kXMLTagReference) &&
  (refTag = TagFromRef (tagName, sizeof (kXMLTagReference)-1))) {
      *tag = refTag;
      length = 0;

  }*/ else if (MATCHTAG (tagName, kXMLTagString)) {
    INT32   attrID = -1, attrSize = 0;
#if USE_REF
    INT32   attrIDRef = -1;

    if (MATCHTAG (tagName, kXMLTagString " ")) {
      attrSize = GetAttrID (tagName, kXMLTagSIZE);
      if (attrSize == -1) {
        attrSize = 0;
      }

      attrIDRef = GetAttrID (tagName, kXMLTagIDREF);

      if (attrIDRef != -1) {
        TagPtr tmpTag = NewTag ();
        if (tmpTag == 0) {
          return -1;
        }

        tmpTag->type = kTagTypeString;
        tmpTag->string = 0;
        tmpTag->integer = 0;
        tmpTag->data = 0;
        tmpTag->size = attrSize;
        tmpTag->tag = 0;
        tmpTag->tagNext = 0;
        tmpTag->id = -1;
        tmpTag->ref = attrIDRef;

        *tag = tmpTag;

        length = 0;
      } else {
        attrID = GetAttrID (tagName, kXMLTagID);
        length = ParseTagString (buffer + pos, tag, attrID, attrSize);
        if (attrID != -1) {
          SaveRefString (buffer + pos, attrID, attrSize);
        }
      }
    } else {
#endif
      length = ParseTagString (buffer + pos, tag, attrID, attrSize);
#if USE_REF
    }
#endif

  } else if (MATCHTAG (tagName, kXMLTagInteger)) {
    INT32   attrID = -1, attrSize = 0;
#if USE_REF
    INT32   attrIDRef = -1;

    if (MATCHTAG (tagName, kXMLTagInteger " ")) {
      attrSize = GetAttrID (tagName, kXMLTagSIZE);
      if (attrSize == -1) {
        attrSize = 0;
      }

      attrIDRef = GetAttrID (tagName, kXMLTagIDREF);

      if (attrIDRef != -1) {
        TagPtr tmpTag = NewTag ();
        if (tmpTag == 0) {
          return -1;
        }

        tmpTag->type = kTagTypeInteger;
        tmpTag->string = 0;
        tmpTag->integer = 0;
        tmpTag->data = 0;
        tmpTag->size = attrSize;
        tmpTag->tag = 0;
        tmpTag->tagNext = 0;
        tmpTag->id = -1;
        tmpTag->ref = attrIDRef;

        *tag = tmpTag;

        length = 0;
      } else {
        attrID = GetAttrID (tagName, kXMLTagID);
        length = ParseTagInteger (buffer + pos, tag, attrID, attrSize);
        if (attrID != -1) {
          SaveRefInteger ((*tag)->string, (*tag)->integer, attrID, attrSize);
        }
      }
    } else {
#endif
      length = ParseTagInteger (buffer + pos, tag, attrID, attrSize);
#if USE_REF
    }
#endif

  } else if (!AsciiStrCmp (tagName, kXMLTagData)) {
    length = ParseTagData (buffer + pos, tag);

  } else if (!AsciiStrCmp (tagName, kXMLTagDate)) {
    length = ParseTagDate (buffer + pos, tag);

  } else if (isTagFalse/*!AsciiStrCmp (tagName, kXMLTagFalse)*/) {
    length = ParseTagBoolean (buffer + pos, tag, kTagTypeFalse);

  } else if (isTagTrue/*!AsciiStrCmp (tagName, kXMLTagTrue)*/) {
    length = ParseTagBoolean (buffer + pos, tag, kTagTypeTrue);

  } else if (MATCHTAG (tagName, kXMLTagArray)) {
    length = ParseTagList (buffer + pos, tag, kTagTypeArray, empty);

  } else {
    // it wasn't parsed so we consumed no additional characters
    length = 0;

    if (tagName[0] == '/') { // was it an end tag (indicated w/*tag = 0)
      *tag = 0;
    } else {
      //DBG ("ignored plist tag: %s (*tag: %x)\n", tagName, *tag);
      *tag = (TagPtr) - 1;  // we're * not * returning a tag
    }
  }

  if (length == -1) {
    return -1;
  }

  return pos + length;
}

INT32
ParseTagList (
  CHAR8   *buffer,
  TagPtr  *tag,
  INT32   type,
  INT32   empty
) {
  INT32   length, pos = 0;
  TagPtr  tagList = 0, tmpTag = (TagPtr) - 1;

  if (!empty) {
    while (1) {
      tmpTag = (TagPtr) - 1;
      length = ParseNextTag (buffer + pos, &tmpTag);
      if (length == -1) {
        break;
      }

      pos += length;

      // detect end of list
      if (tmpTag == 0) {
        break;
      }

      // if we made a new tag, insert into list
      if (tmpTag != (TagPtr) - 1) {
        tmpTag->tagNext = tagList;
        tagList = tmpTag;
      }
    }

    if (length == -1) {
      FreeTag (tagList);
      return -1;
    }
  }

  tmpTag = NewTag ();
  if (tmpTag == 0) {
    FreeTag (tagList);
    return -1;
  }

  tmpTag->type = type;
  tmpTag->string = 0;
  tmpTag->integer = 0;
  tmpTag->data = 0;
  tmpTag->size = 0;
  tmpTag->tag = tagList;
  tmpTag->tagNext = 0;
  tmpTag->offset = (UINT32)(buffer_start ? buffer - buffer_start : 0);
  tmpTag->taglen = (UINT32)((pos > length) ? pos - length : 0);

  *tag = tmpTag;

  return pos;
}

EFI_STATUS
ParseXML (
  CHAR8   *buffer,
  TagPtr  *dict,
  UINT32  bufSize
) {
  INT32     length, pos, bufferSize = 0;
  TagPtr    moduleDict;
  CHAR8     *fixedBuffer = NULL;

  if (bufSize) {
    bufferSize=bufSize;
  } else {
    bufferSize=(UINT32)AsciiStrLen (buffer);
  }

  if (dict == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  fixedBuffer = AllocateZeroPool (bufferSize+1);
  if (fixedBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (fixedBuffer, buffer, bufferSize);
  buffer_start = fixedBuffer;

#if DEBUG_PLIST
  gTagsParsed = 0;
  gLastTag = NULL;
#endif

  pos = 0;

  ref_strings = NULL;
  ref_integer = NULL;

  while (1) {
    moduleDict = (TagPtr) - 1;  // have to detect changes to by-ref parameter
    length = ParseNextTag (fixedBuffer + pos, &moduleDict);
    if (length == -1) {
      break;
    }

    pos += length;

    if (moduleDict == 0) {
      continue;
    }

    // did we actually create anything?
    if (moduleDict != (TagPtr) - 1) {
      if (
        (moduleDict->type == kTagTypeDict) ||
        (moduleDict->type == kTagTypeArray)
      ) {
        break;
      }

      FreeTag (moduleDict);
    }
  }


  if (length != -1) {
//#if USE_REF
    moduleDict->ref_strings = (ref_strings != NULL)
      ? AllocateCopyPool (sizeof (ref_strings) * sizeof (sREF), ref_strings)
      : NULL;
    moduleDict->ref_integer = (ref_integer != NULL)
      ? AllocateCopyPool (sizeof (ref_integer) * sizeof (sREF), ref_integer)
      : NULL;
//#endif

    *dict = moduleDict;

#if DEBUG_PLIST
  } else {
    DBG ("ParseXML gagged (-1) after '%s' (%d tags); buf+pos: %s\n",
      gLastTag, gTagsParsed, buffer + pos);
#endif
  }

  FreePool (fixedBuffer);

  dbgCount++;

  // return 0 for no error
  return (length != -1) ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
}

//
// Public
//

TagPtr
GetProperty (
  TagPtr  dict,
  CHAR8   *key
) {
  TagPtr  tagList, tag;

  if (dict->type != kTagTypeDict) {
    return 0;
  }

  tag = 0;    // ?
  tagList = dict->tag;

  while (tagList) {
    tag = tagList;
    tagList = tag->tagNext;

    if ((tag->type != kTagTypeKey) || (tag->string == 0)) {
      continue;
    }

    if (!AsciiStrCmp (tag->string, key)) {
      return tag->tag;
    }
  }

  return 0;
}

INTN
GetTagCount (
  TagPtr    dict
) {
  INTN    count = 0;
  TagPtr  tagList, tag;

  if ((dict->type != kTagTypeDict) && (dict->type != kTagTypeArray)) {
    return 0;
  }

  tag = 0;
  tagList = dict->tag;

  while (tagList) {
    tag = tagList;
    tagList = tag->tagNext;

    if (
      (
        (tag->type != kTagTypeKey) &&
        ((tag->string == 0) || (tag->string[0] == 0))
      ) && (dict->type != kTagTypeArray)  // If we are an array, any element is valid
    ) {
      continue;
    }

    if (tag->type == kTagTypeKey) {
      DBG ("Located key %s\n", tag->string);
    }

    count++;
  }

  return count;
}

EFI_STATUS
GetElement (
  TagPtr    dict,
  INTN      id,
  TagPtr    *dict1
) {
  INTN    element = 0;
  TagPtr  child;

  if (!dict || !dict1 || (dict->type != kTagTypeArray)) {
    return EFI_UNSUPPORTED;
  }

  child = dict->tag;

  while (element < id) {
    element++;
    child = child->tagNext;
  }

  *dict1 = child;

  return EFI_SUCCESS;
}

//
//  Debug
//

#if DUMP_PLIST
CHAR8 *
DumpSpaces (
  INT32   depth
) {
  INT32   cnt = 0, total = (depth * 2);
  CHAR8   *spaces = AllocateZeroPool (total);

  while (cnt < total) {
    spaces[cnt] = ' ';
    cnt++;
  }

  spaces[total] = '\0';

  return spaces;
}

VOID
Dump (
  CHAR16  *str,
  INT32   depth
) {
  MsgLog ("%a%s\n", DumpSpaces (depth), str);
}

VOID
DumpTagDict (
  TagPtr  tag,
  INT32   depth
) {
  TagPtr  tagList;

  if (tag->tag == 0) {
    Dump (PoolPrint (L"<%a/>", kXMLTagDict), depth);
  } else {
    Dump (PoolPrint (L"<%a>", kXMLTagDict), depth);

    tagList = tag->tag;
    while (tagList) {
      DumpTag (tagList, depth + 1);
      tagList = tagList->tagNext;
    }

    Dump (PoolPrint (L"</%a>", kXMLTagDict), depth);
  }
}

VOID
DumpTagKey (
  TagPtr  tag,
  INT32   depth
) {
  Dump (PoolPrint (L"<%a>%a</%a>", kXMLTagKey, tag->string, kXMLTagKey), depth);

  DumpTag (tag->tag, depth);
}

VOID
DumpTagString (
  TagPtr  tag,
  INT32   depth
) {
  Dump (PoolPrint (L"<%a>%a</%a>", kXMLTagString, tag->string, kXMLTagString), depth);
}

/* integers used to live as char * s but we need 64 bit ints */
VOID
DumpTagInteger (
  TagPtr  tag,
  INT32   depth
) {
  Dump (PoolPrint (L"<%a>%d</%a>", kXMLTagInteger, tag->integer, kXMLTagInteger), depth);
}

VOID
DumpTagData (
  TagPtr  tag,
  INT32   depth
) {
  Dump (PoolPrint (L"<%a>%x</%a>", kXMLTagData, tag->data, kXMLTagData), depth);
}

VOID
DumpTagDate (
  TagPtr  tag,
  INT32   depth
) {
  Dump (PoolPrint (L"<%a>%x</%a>", kXMLTagDate, tag->string, kXMLTagDate), depth);
}

VOID
DumpTagBoolean (
  TagPtr  tag,
  INT32   depth
) {
  Dump (PoolPrint (L"<%a>", (tag->type == kTagTypeTrue) ? kXMLTagTrue : kXMLTagFalse), depth);
}

VOID
DumpTagArray (
  TagPtr  tag,
  INT32   depth
) {
  TagPtr  tagList;

  if (tag->tag == 0) {
    Dump (PoolPrint (L"<%a/>", kXMLTagArray), depth);
  } else {
    Dump (PoolPrint (L"<%a>", kXMLTagArray), depth);

    tagList = tag->tag;
    while (tagList) {
      DumpTag (tagList, depth + 1);
      tagList = tagList->tagNext;
    }

    Dump (PoolPrint (L"</%a>", kXMLTagArray), depth);
  }
}

VOID
DumpTag (
  TagPtr  tag,
  INT32   depth
) {
  if (tag == 0) {
    return;
  }

  switch (tag->type) {
    case kTagTypeDict :
      DumpTagDict (tag, depth);
      break;

    case kTagTypeKey :
      DumpTagKey (tag, depth);
      break;

    case kTagTypeString :
      DumpTagString (tag, depth);
      break;

    case kTagTypeInteger :
      DumpTagInteger (tag, depth);
      break;

    case kTagTypeData :
      DumpTagData (tag, depth);
      break;

    case kTagTypeDate :
      DumpTagDate (tag, depth);
      break;

    case kTagTypeFalse :
    case kTagTypeTrue :
      DumpTagBoolean (tag, depth);
      break;

    case kTagTypeArray :
      DumpTagArray (tag, depth);
      break;

    default :
      break;
  }
}
#endif
