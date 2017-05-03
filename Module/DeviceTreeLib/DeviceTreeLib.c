/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/Platform/DeviceTree.h>

#define RoundLong(x) (((x) + 3UL) & ~(3UL))
#define NextProp(x)  ((DeviceTreeNodeProperty *) (((UINT8 *)x) + sizeof (DeviceTreeNodeProperty) + RoundLong (x->length)))

/* Entry */

INTN          DTInitialized;
RealDTEntry   DTRootNode;

/*
 * Support Routines
*/
RealDTEntry
SkipProperties (
  RealDTEntry   Entry
) {
  DeviceTreeNodeProperty    *Prop;
  UINTN                     k;

  if ((Entry == NULL) || (Entry->nProperties == 0)) {
    return NULL;
  } else {
    Prop = (DeviceTreeNodeProperty *)(Entry + 1);
    for (k = 0; k < Entry->nProperties; k++) {
      Prop = NextProp (Prop);
    }
  }

  return ((RealDTEntry) Prop);
}

RealDTEntry
SkipTree (
  RealDTEntry   Root
) {
  RealDTEntry   Entry;
  UINTN         k;

  Entry = SkipProperties (Root);
  if (Entry == NULL) {
    return NULL;
  }

  for (k = 0; k < Root->nChildren; k++) {
    Entry = SkipTree (Entry);
  }

  return Entry;
}

RealDTEntry
GetFirstChild (
  RealDTEntry   Parent
) {
  return SkipProperties (Parent);
}

RealDTEntry
GetNextChild (
  RealDTEntry   Sibling
) {
  return SkipTree (Sibling);
}

CONST
CHAR8 *
GetNextComponent (
  CONST CHAR8   *Cp,
        CHAR8   *Bp
) {
  while (*Cp != 0) {
    if (*Cp == kDTPathNameSeparator) {
      Cp++;
      break;
    }
    *Bp++ = *Cp++;
  }

  *Bp = 0;

  return Cp;
}

RealDTEntry
FindChild (
  RealDTEntry   Cur,
  CHAR8         *Buf
) {
  RealDTEntry   Child;
  UINT32        Index;
  CHAR8         *Str;
  UINT32        Dummy;

  if (Cur->nChildren == 0) {
    return NULL;
  }

  Index = 1;
  Child = GetFirstChild (Cur);

  while (1) {
    if (DTGetProperty (Child, "name", (VOID **)&Str, &Dummy) != kSuccess) {
      break;
    }

    if (AsciiStrCmp ((CHAR8 *)Str, (CHAR8 *)Buf) == 0) {
      return Child;
    }

    if (Index >= Cur->nChildren) {
      break;
    }

    Child = GetNextChild (Child);
    Index++;
  }

  return NULL;
}

/*
 * External Routines
 */
VOID
EFIAPI
DTInit (
  VOID    *Base
) {
  DTRootNode = (RealDTEntry) Base;
  DTInitialized = (DTRootNode != 0);
}

INTN
EFIAPI
DTEntryIsEqual (
  CONST DTEntry   Ref1,
  CONST DTEntry   Ref2
) {
  /* equality of pointers */
  return (Ref1 == Ref2);
}

// APTIO
CHAR8   *startingP;   // needed for FindEntry
//INTN    FindEntry (CONST CHAR8 *propName, CONST CHAR8 *propValue, DTEntry *EntryH);

INTN
FindEntry (
  CONST CHAR8       *PropName,
  CONST CHAR8       *PropValue,
        DTEntry     *EntryH
) {
  DeviceTreeNode  *NodeP = (DeviceTreeNode *)(VOID *)startingP;
  UINTN           k;

  if (NodeP->nProperties == 0) {
    return (kError);  // End of the list of nodes
  }

  startingP = (CHAR8 *)(NodeP + 1);

  // Search current entry
  for (k = 0; k < NodeP->nProperties; ++k) {
    DeviceTreeNodeProperty    *PropP = (DeviceTreeNodeProperty *)(VOID *)startingP;

    startingP += sizeof (*PropP) + ((PropP->length + 3) & -4);

    if (AsciiStrCmp ((CHAR8 *)PropP->name, (CHAR8 *)PropName) == 0) {
      if (
        (PropValue == NULL) ||
        (AsciiStrCmp ( (CHAR8 *)(PropP + 1), (CHAR8 *)PropValue) == 0)
      ) {
        *EntryH = (DTEntry)NodeP;
        return (kSuccess);
      }
    }
  }

  // Search Child nodes
  for (k = 0; k < NodeP->nChildren; ++k) {
    if (FindEntry (PropName, PropValue, EntryH) == kSuccess) {
      return (kSuccess);
    }
  }

  return (kError);
}

INTN
EFIAPI
DTFindEntry (
  CONST CHAR8     *PropName,
  CONST CHAR8     *PropValue,
        DTEntry   *EntryH
) {
  if (!DTInitialized) {
    return kError;
  }

  startingP = (CHAR8 *)DTRootNode;
  return (FindEntry (PropName, PropValue, EntryH));
}

INTN
EFIAPI
DTLookupEntry (
  CONST DTEntry   SearchPoint,
  CONST CHAR8     *PathName,
        DTEntry   *FoundEntry
) {
        DTEntryNameBuf  Buf;
        RealDTEntry     Cur;
  CONST CHAR8           *Cp;

  if (!DTInitialized) {
    return kError;
  }

  if (SearchPoint == NULL)   {
    Cur = DTRootNode;
  } else {
    Cur = SearchPoint;
  }

  Cp = PathName;
  if (*Cp == kDTPathNameSeparator) {
    Cp++;
    if (*Cp == 0) {
      *FoundEntry = Cur;
      return kSuccess;
    }
  }

  do {
    Cp = GetNextComponent (Cp, Buf);

    /* Check for done */
    if (*Buf == 0) {
      if (*Cp == 0) {
        *FoundEntry = Cur;
        return kSuccess;
      }
      break;
    }

    Cur = FindChild (Cur, Buf);

  } while (Cur != NULL);

  return kError;
}

INTN
EFIAPI
DTCreateEntryIterator (
  CONST DTEntry           StartEntry,
        DTEntryIterator   *Iterator
) {
  RealDTEntryIterator   Iter;

  if (!DTInitialized) {
    return kError;
  }

  Iter = (RealDTEntryIterator) AllocatePool (sizeof (struct OpaqueDTEntryIterator));
  if (StartEntry != NULL) {
    Iter->outerScope = (RealDTEntry)StartEntry;
    Iter->currentScope = (RealDTEntry)StartEntry;
  } else {
    Iter->outerScope = DTRootNode;
    Iter->currentScope = DTRootNode;
  }

  Iter->currentEntry = NULL;
  Iter->savedScope = NULL;
  Iter->currentIndex = 0;

  *Iterator = Iter;

  return kSuccess;
}

INTN
EFIAPI
DTDisposeEntryIterator (
  DTEntryIterator   Iterator
) {
  RealDTEntryIterator   Iter = Iterator;
  DTSavedScopePtr       Scope;

  while ((Scope = Iter->savedScope) != NULL) {
    Iter->savedScope = Scope->nextScope;
    FreePool (Scope);
  }

  FreePool (Iterator);

  return kSuccess;
}

INTN
EFIAPI
DTEnterEntry (
  DTEntryIterator   Iterator,
  DTEntry           ChildEntry
) {
  RealDTEntryIterator   Iter = Iterator;
  DTSavedScopePtr       NewScope;

  if (ChildEntry == NULL) {
    return kError;
  }

  NewScope = (DTSavedScopePtr) AllocatePool (sizeof (struct DTSavedScope));
  NewScope->nextScope = Iter->savedScope;
  NewScope->scope = Iter->currentScope;
  NewScope->entry = Iter->currentEntry;
  NewScope->index = Iter->currentIndex;

  Iter->currentScope = ChildEntry;
  Iter->currentEntry = NULL;
  Iter->savedScope = NewScope;
  Iter->currentIndex = 0;

  return kSuccess;
}

INTN
EFIAPI
DTExitEntry (
  DTEntryIterator   Iterator,
  DTEntry           *CurrentPosition
) {
  RealDTEntryIterator   Iter = Iterator;
  DTSavedScopePtr       NewScope;

  NewScope = Iter->savedScope;
  if (NewScope == NULL) {
    return kError;
  }

  Iter->savedScope = NewScope->nextScope;
  Iter->currentScope = NewScope->scope;
  Iter->currentEntry = NewScope->entry;
  Iter->currentIndex = NewScope->index;
  *CurrentPosition = Iter->currentEntry;

  FreePool (NewScope);

  return kSuccess;
}

INTN
EFIAPI
DTIterateEntries (
  DTEntryIterator   Iterator,
  DTEntry           *NextEntry
) {
  RealDTEntryIterator Iter = Iterator;

  if (Iter->currentIndex >= Iter->currentScope->nChildren) {
    *NextEntry = NULL;
    return kIterationDone;
  } else {
    Iter->currentIndex++;

    Iter->currentEntry = (Iter->currentIndex == 1)
      ? GetFirstChild (Iter->currentScope)
      : GetNextChild (Iter->currentEntry);

    *NextEntry = Iter->currentEntry;

    return kSuccess;
  }
}

INTN
EFIAPI
DTRestartEntryIteration (
  DTEntryIterator   Iterator
) {
  RealDTEntryIterator   Iter = Iterator;

#if 0
  // This commented out code allows a second argument (outer)
  // which (if TRUE) causes restarting at the outer scope
  // rather than the current scope.
  DTSavedScopePtr scope;

  if (outer) {
    while ((scope = Iter->savedScope) != NULL) {
      Iter->savedScope = scope->nextScope;
      FreePool (scope);
    }
    Iter->currentScope = Iter->outerScope;
  }
#endif

  Iter->currentEntry = NULL;
  Iter->currentIndex = 0;

  return kSuccess;
}

INTN
EFIAPI
DTGetProperty (
  CONST DTEntry   Entry,
  CONST CHAR8     *PropertyName,
        VOID      **PropertyValue,
        UINT32    *PropertySize
) {
  DeviceTreeNodeProperty    *Prop;
  UINTN                     k;

  if ((Entry == NULL) || (Entry->nProperties == 0)) {
    return kError;
  } else {
    Prop = (DeviceTreeNodeProperty *)(Entry + 1);
    for (k = 0; k < Entry->nProperties; k++) {
      if (AsciiStrCmp ((CHAR8 *)Prop->name, (CHAR8 *)PropertyName) == 0) {
        *PropertyValue = (VOID *) (((UINT8 *)Prop) + sizeof (DeviceTreeNodeProperty));
        *PropertySize = Prop->length;

        return kSuccess;
      }

      Prop = NextProp (Prop);
    }
  }

  return kError;
}

INTN
EFIAPI
DTCreatePropertyIterator (
  CONST DTEntry             Entry,
        DTPropertyIterator  *Iterator
) {
  RealDTPropertyIterator    Iter;

  Iter = (RealDTPropertyIterator) AllocatePool (sizeof (struct OpaqueDTPropertyIterator));
  Iter->entry = Entry;
  Iter->currentProperty = NULL;
  Iter->currentIndex = 0;

  *Iterator = Iter;

  return kSuccess;
}

// dmazar: version without mem alloc which can be used during or after ExitBootServices.
// caller should not call DTDisposePropertyIterator when using this version.
INTN
EFIAPI
DTCreatePropertyIteratorNoAlloc (
  CONST DTEntry             Entry,
        DTPropertyIterator  Iterator
) {
  RealDTPropertyIterator Iter = Iterator;

  Iter->entry = Entry;
  Iter->currentProperty = NULL;
  Iter->currentIndex = 0;

  return kSuccess;
}

INTN
EFIAPI
DTDisposePropertyIterator (
  DTPropertyIterator    Iterator
) {
  FreePool (Iterator);

  return kSuccess;
}

INTN
EFIAPI
DTIterateProperties (
  DTPropertyIterator    Iterator,
  CHAR8                 **FoundProperty
) {
  RealDTPropertyIterator    Iter = Iterator;

  if (Iter->currentIndex >= Iter->entry->nProperties) {
    *FoundProperty = NULL;
    return kIterationDone;
  } else {
    Iter->currentIndex++;
    if (Iter->currentIndex == 1) {
      Iter->currentProperty = (DeviceTreeNodeProperty *)(Iter->entry + 1);
    } else {
      Iter->currentProperty = NextProp (Iter->currentProperty);
    }

    *FoundProperty = Iter->currentProperty->name;

    return kSuccess;
  }
}

INTN
EFIAPI
DTRestartPropertyIteration (
  DTPropertyIterator    Iterator
) {
  RealDTPropertyIterator    Iter = Iterator;

  Iter->currentProperty = NULL;
  Iter->currentIndex = 0;

  return kSuccess;
}
