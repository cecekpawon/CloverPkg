/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _PEXPERT_DEVICE_TREE_H_
#define _PEXPERT_DEVICE_TREE_H_

/*
-------------------------------------------------------------------------------
 Foundation Types
-------------------------------------------------------------------------------
*/
enum {
  kDTPathNameSeparator  = '/'       /* 0x2F */
};

/* Property Name Definitions (Property Names are C-Strings)*/
enum {
  kDTMaxPropertyNameLength=31 /* Max length of Property Name (terminator not included) */
};

typedef char DTPropertyNameBuf[32];

/* Entry Name Definitions (Entry Names are C-Strings)*/
enum {
  kDTMaxEntryNameLength   = 31  /* Max length of a C-String Entry Name (terminator not included) */
};

/* length of DTEntryNameBuf = kDTMaxEntryNameLength +1 */
typedef char DTEntryNameBuf[32];

/* Entry */
typedef struct OpaqueDTEntry *DTEntry;

/* Entry Iterator */
typedef struct OpaqueDTEntryIterator *DTEntryIterator;

/* Property Iterator */
typedef struct OpaqueDTPropertyIterator *DTPropertyIterator;

/* status values */
enum {
  kError = -1,
  kIterationDone = 0,
  kSuccess = 1
};

/*
Structures for a Flattened Device Tree
*/

//These definitions show the primitivity of C-language where there is no possibility to
//explain the structure of DT

#define kPropNameLength 32

typedef struct DeviceTreeNodeProperty {
  char      name[kPropNameLength];  // NUL terminated property name
  UINT32    length;   // Length (bytes) of following prop value
  //unsigned long value[1]; // Variable length value of property
  // Padded to a multiple of a longword?
} DeviceTreeNodeProperty;

typedef struct OpaqueDTEntry {
  UINT32    nProperties;  // Number of props[] elements (0 => end)
  UINT32    nChildren;  // Number of children[] elements
  //DeviceTreeNodeProperty  props[];// array size == nProperties
  //DeviceTreeNode  children[]; // array size == nChildren
} DeviceTreeNode;

typedef DeviceTreeNode *RealDTEntry;

typedef struct DTSavedScope {
  struct  DTSavedScope  *nextScope;
          RealDTEntry   scope;
          RealDTEntry   entry;
          UINT32        index;
} *DTSavedScopePtr;

/* Entry Iterator */
typedef struct OpaqueDTEntryIterator {
  RealDTEntry       outerScope;
  RealDTEntry       currentScope;
  RealDTEntry       currentEntry;
  DTSavedScopePtr   savedScope;
  UINT32            currentIndex;
} *RealDTEntryIterator;

/* Property Iterator */
typedef struct OpaqueDTPropertyIterator {
  RealDTEntry             entry;
  DeviceTreeNodeProperty  *currentProperty;
  UINT32                  currentIndex;
} *RealDTPropertyIterator;

typedef struct DTMemMapEntry {
  UINT32  Address;
  UINT32  Length;
} DTMemMapEntry;

typedef struct DeviceTreeBuffer {
  UINT32  paddr;
  UINT32  length;
} DeviceTreeBuffer;

typedef struct BooterKextFileInfo {
  UINT32  infoDictPhysAddr;
  UINT32  infoDictLength;
  UINT32  executablePhysAddr;
  UINT32  executableLength;
  UINT32  bundlePathPhysAddr;
  UINT32  bundlePathLength;
} BooterKextFileInfo;

/*
-------------------------------------------------------------------------------
 Device Tree Calls
-------------------------------------------------------------------------------
*/

/* Used to initalize the device tree functions. */
/* base is the base address of the flatened device tree */
VOID
EFIAPI
DTInit (
  VOID    *Base
);

/*
-------------------------------------------------------------------------------
 Entry Handling
-------------------------------------------------------------------------------
*/
/* Compare two Entry's for equality. */
INTN
EFIAPI
DTEntryIsEqual (
  CONST DTEntry   Ref1,
  CONST DTEntry   Ref2
);

/*
-------------------------------------------------------------------------------
 LookUp Entry by Name
-------------------------------------------------------------------------------
*/
/*
 DTFindEntry:
 Find the device tree entry that contains propName=propValue.
 It currently  searches the entire
 tree.  This function should eventually go in DeviceTree.c.
 Returns:    kSuccess = entry was found.  Entry is in entryH.
             kError   = entry was not found
*/
INTN
EFIAPI
DTFindEntry (
  CONST CHAR8     *PropName,
  CONST CHAR8     *PropValue,
        DTEntry   *EntryH
);

/*
 Lookup Entry
 Locates an entry given a specified subroot (searchPoint) and path name.  If the
 searchPoint pointer is NULL, the path name is assumed to be an absolute path
 name rooted to the root of the device tree.
*/
INTN
EFIAPI
DTLookupEntry (
  CONST DTEntry   SearchPoint,
  CONST CHAR8     *PathName,
        DTEntry   *FoundEntry
);

/*
-------------------------------------------------------------------------------
 Entry Iteration
-------------------------------------------------------------------------------
*/
/*
 An Entry Iterator maintains three variables that are of interest to clients.
 First is an "OutermostScope" which defines the outer boundry of the iteration.
 This is defined by the starting entry and includes that entry plus all of it's
 embedded entries. Second is a "currentScope" which is the entry the iterator is
 currently in. And third is a "currentPosition" which is the last entry returned
 during an iteration.

 Create Entry Iterator
 Create the iterator structure. The outermostScope and currentScope of the iterator
 are set to "startEntry".  If "startEntry" = NULL, the outermostScope and
 currentScope are set to the root entry.  The currentPosition for the iterator is
 set to "nil".
*/
INTN
EFIAPI
DTCreateEntryIterator (
  CONST DTEntry           StartEntry,
        DTEntryIterator   *Iterator
);

/* Dispose Entry Iterator */
INTN
EFIAPI
DTDisposeEntryIterator (
  DTEntryIterator   Iterator
);

/*
 Enter Child Entry
 Move an Entry Iterator into the scope of a specified child entry.  The
 currentScope of the iterator is set to the entry specified in "childEntry".  If
 "childEntry" is nil, the currentScope is set to the entry specified by the
 currentPosition of the iterator.
*/
INTN
EFIAPI
DTEnterEntry (
  DTEntryIterator   Iterator,
  DTEntry           ChildEntry
);

/*
 Exit to Parent Entry
 Move an Entry Iterator out of the current entry back into the scope of it's parent
 entry. The currentPosition of the iterator is reset to the current entry (the
 previous currentScope), so the next iteration call will continue where it left off.
 This position is returned in parameter "currentPosition".
*/
INTN
EFIAPI
DTExitEntry (
  DTEntryIterator   Iterator,
  DTEntry           *CurrentPosition
);

/*
 Iterate Entries
 Iterate and return entries contained within the entry defined by the current
 scope of the iterator.  Entries are returned one at a time. When
INTN== kIterationDone, all entries have been exhausted, and the
 value of nextEntry will be Nil.
*/
INTN
EFIAPI
DTIterateEntries (
  DTEntryIterator   Iterator,
  DTEntry           *NextEntry
);

/*
 Restart Entry Iteration
 Restart an iteration within the current scope.  The iterator is reset such that
 iteration of the contents of the currentScope entry can be restarted. The
 outermostScope and currentScope of the iterator are unchanged. The currentPosition
 for the iterator is set to "nil".
*/
INTN
EFIAPI
DTRestartEntryIteration (
  DTEntryIterator   Iterator
);

/*
-------------------------------------------------------------------------------
 Get Property Values
-------------------------------------------------------------------------------
*/
/*
 Get the value of the specified property for the specified entry.

 Get Property
*/
INTN
EFIAPI
DTGetProperty (
  CONST DTEntry   Entry,
  CONST CHAR8     *PropertyName,
        VOID      **PropertyValue,
        UINT32    *PropertySize
);

/*
-------------------------------------------------------------------------------
 Iterating Properties
-------------------------------------------------------------------------------
*/
/*
 Create Property Iterator
 Create the property iterator structure. The target entry is defined by entry.
*/

INTN
EFIAPI
DTCreatePropertyIterator (
  CONST DTEntry             Entry,
        DTPropertyIterator  *Iterator
);

/*
 dmazar: version without mem alloc which can be used during or after ExitBootServices.
 caller should not call DTDisposePropertyIterator when using this version.
 */
INTN
EFIAPI
DTCreatePropertyIteratorNoAlloc (
  CONST DTEntry             Entry,
        DTPropertyIterator  Iterator
);

/* Dispose Property Iterator */
INTN
EFIAPI
DTDisposePropertyIterator (
  DTPropertyIterator    Iterator
);

/*
 Iterate Properites
 Iterate and return properties for given entry.
 WhenINTN== kIterationDone, all properties have been exhausted.
*/

INTN
EFIAPI
DTIterateProperties (
  DTPropertyIterator    Iterator,
  CHAR8                 **FoundProperty
);

/*
 Restart Property Iteration
 Used to re-iterate over a list of properties.  The Property Iterator is
 reset to the beginning of the list of properties for an entry.
*/

INTN
EFIAPI
DTRestartPropertyIteration (
  DTPropertyIterator    Iterator
);

/* DevTree may contain /chosen/memory-map
 * with properties with values = UINT32 address, UINT32 length:
 * "BootCLUT" = 8bit boot time colour lookup table
 * "Pict-FailedBoot" = picture shown if booting fails
 * "RAMDisk" = ramdisk
 * "Driver-<hex addr of DriverInfo>" = Kext, UINT32 address points to BooterKextFileInfo
 * "DriversPackage-..." = MKext, UINT32 address points to mkext_header (libkern/libkern/mkext.h), UINT32 length
 *
*/
#define BOOTER_MKEXT_PREFIX             "DriversPackage-"
#define BOOTER_RAMDISK_PREFIX           "RAMDisk"
#define BOOTER_KEXT_PREFIX              "Driver-"
#define BOOTER_KEXT_NOLOAD_PREFIX       "NoLoad-"
#define BOOTER_IOPERSONALITIES_PREFIX   "IOPersonalitiesInjector-"

#endif /* _PEXPERT_DEVICE_TREE_H_ */
