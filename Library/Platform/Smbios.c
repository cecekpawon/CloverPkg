/**
 smbios.c
 original idea of SMBIOS patching by mackerintel
 implementation for UEFI smbios table patching
  Slice 2011.

 portion copyright Intel
 Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>
 This program and the accompanying materials
 are licensed and made available under the terms and conditions of the BSD License
 which accompanies this distribution.  The full text of the license may be found at
 http://opensource.org/licenses/bsd-license.php

 THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

 Module Name:

 SmbiosGen.c
 **/

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_SMBIOS
#define DEBUG_SMBIOS -1
#endif
#else
#ifdef DEBUG_SMBIOS
#undef DEBUG_SMBIOS
#endif
#define DEBUG_SMBIOS DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_SMBIOS, __VA_ARGS__)

EFI_GUID                          gUuid;
EFI_GUID                          *gTableGuidArray[] = { &gEfiSmbiosTableGuid };

//EFI_PHYSICAL_ADDRESS            *smbiosTable;
VOID                              *Smbios;  //pointer to SMBIOS data
SMBIOS_TABLE_ENTRY_POINT          *EntryPoint; //SmbiosEps original
SMBIOS_TABLE_ENTRY_POINT          *SmbiosEpsNew; //new SmbiosEps

//for patching
APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTable;
APPLE_SMBIOS_STRUCTURE_POINTER    newSmbiosTable;

UINT16                            NumberOfRecords;
UINT16                            MaxStructureSize;
UINT8                             *Current; //pointer to the current end of tables

EFI_SMBIOS_TABLE_HEADER           *Record;
EFI_SMBIOS_HANDLE                 Handle;
EFI_SMBIOS_TYPE                   Type;

UINTN                             stringNumber;
UINTN                             TableSize;
UINTN                             Index, Size, NewSize, MaxSize;
UINT16                            CoreCache = 0;
UINT16                            TotalCount = 0;
UINT16                            L1, L2, L3;
UINT32                            MaxMemory = 0;
UINT32                            mTotalSystemMemory;
UINT64                            gTotalMemory;
UINT16                            mHandle3;
UINT16                            mHandle17[MAX_RAM_SLOTS];
UINT16                            mHandle19;
UINT16                            mMemory17[MAX_RAM_SLOTS];
UINT64                            mInstalled[MAX_RAM_SLOTS];
UINT64                            mEnabled[MAX_RAM_SLOTS];
BOOLEAN                           gMobile;
UINT8                             gBootStatus;
BOOLEAN                           Once;

MEM_STRUCTURE                     gRAM;
//DMI                             *gDMI;
UINT8                             gRAMCount = 0;

#define MAX_HANDLE                0xFEFF
#define SMBIOS_PTR                SIGNATURE_32 ('_','S','M','_')
#define MAX_TABLE_SIZE            512

#define SmbiosOffsetOf(s,m)      ((SMBIOS_TABLE_STRING) ((UINT8 *)&((s *)0)->m - (UINT8 *)0))

SMBIOS_TABLE_STRING    SMBIOS_TABLE_TYPE0_STR_IDX[] = {
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE0, Vendor),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE0, BiosVersion),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE0, BiosReleaseDate),
  0x00
}; // type 0 Bios

SMBIOS_TABLE_STRING    SMBIOS_TABLE_TYPE1_STR_IDX[] = {
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE1, Manufacturer),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE1, ProductName),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE1, Version),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE1, SerialNumber),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE1, SKUNumber),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE1, Family),
  0x00
}; // type 1 System

SMBIOS_TABLE_STRING    SMBIOS_TABLE_TYPE2_STR_IDX[] = {
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE2, Manufacturer),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE2, ProductName),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE2, Version),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE2, SerialNumber),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE2, AssetTag),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE2, LocationInChassis),
  0x00
}; // type 2 BaseBoard

SMBIOS_TABLE_STRING    SMBIOS_TABLE_TYPE3_STR_IDX[] = {
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE3, Manufacturer),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE3, Version),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE3, SerialNumber),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE3, AssetTag),
  0x00
}; // type 3 Chassis // No SKUNumber?

SMBIOS_TABLE_STRING    SMBIOS_TABLE_TYPE4_STR_IDX[] = {
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE4, Socket),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE4, ProcessorManufacture),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE4, ProcessorVersion),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE4, SerialNumber),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE4, AssetTag),
  SmbiosOffsetOf (SMBIOS_TABLE_TYPE4, PartNumber),
  0x00
}; // type 4 Processor

typedef enum {
  hTable2 = 0x0200,
  hTable9 = 0x0900,
  //hTable11 = 0x0B00,
  hTable16 = 0x1000,
  hTable17 = 0x1100,
  hTable19 = 0x1300,
  hTable128 = 0x8000,
  hTable131 = 0x8300,
  hTable132 = 0x8400,
  hTable133 = 0x8500,
  hTable134 = 0x8600,
} H_APPLE_TABLE_HANDLE;

/* Functions */

// validate the SMBIOS entry point structure
BOOLEAN
IsEntryPointStructureValid (
  IN SMBIOS_TABLE_ENTRY_POINT   *EntryPointStructure
) {
  UINTN     I;
  UINT8     Length, Checksum = 0, *BytePtr;

  if (!EntryPointStructure) {
    return FALSE;
  }

  BytePtr = (UINT8 *)EntryPointStructure;
  Length = EntryPointStructure->EntryPointLength;

  for (I = 0; I < Length; I++) {
    Checksum = Checksum + (UINT8)BytePtr[I];
  }

  // a valid SMBIOS EPS must have checksum of 0
  return (Checksum == 0);
}

VOID *
FindOemSMBIOSPtr () {
  UINTN      Address;

  // Search 0x0f0000 - 0x0fffff for SMBIOS Ptr
  for (Address = 0xf0000; Address < 0xfffff; Address += 0x10) {
    if ((*(UINT32 *)(Address) == SMBIOS_PTR) && IsEntryPointStructureValid ((SMBIOS_TABLE_ENTRY_POINT *)Address)) {
      return (VOID *)Address;
    }
  }

  return NULL;
}

VOID *
GetSmbiosTablesFromHob () {
  EFI_PHYSICAL_ADDRESS    *Table;
  EFI_PEI_HOB_POINTERS    GuidHob;

  GuidHob.Raw = GetFirstGuidHob (&gEfiSmbiosTableGuid);
  if (GuidHob.Raw != NULL) {
    Table = GET_GUID_HOB_DATA (GuidHob.Guid);
    if (Table != NULL) {
      return (VOID *)(UINTN)*Table;
    }
  }

  return NULL;
}

VOID *
GetSmbiosTablesFromConfigTables () {
  EFI_STATUS              Status;
  EFI_PHYSICAL_ADDRESS    *Table;

  Status = EfiGetSystemConfigurationTable (&gEfiSmbiosTableGuid, (VOID **)  &Table);
  if (EFI_ERROR (Status) || Table == NULL) {
    Table = NULL;
  }

  return Table;
}

// Internal functions for flat SMBIOS

UINT16
SmbiosTableLength (
  APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTableN
) {
  CHAR8     *AChar;
  UINT16    Length;

  AChar = (CHAR8 *)(SmbiosTableN.Raw + SmbiosTableN.Hdr->Length);
  while ((*AChar != 0) || (*(AChar + 1) != 0)) {
    AChar ++; //stop at 00 - first 0
  }

  Length = (UINT16)((UINTN)AChar - (UINTN)SmbiosTableN.Raw + 2); //length includes 00

  return Length;
}

EFI_SMBIOS_HANDLE
LogSmbiosTable (
  APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTableN
) {
  UINT16    Length = SmbiosTableLength (SmbiosTableN);

  if (Length > MaxStructureSize) {
    MaxStructureSize = Length;
  }

  CopyMem (Current, SmbiosTableN.Raw, Length);
  Current += Length;
  NumberOfRecords++;

  return SmbiosTableN.Hdr->Handle;
}

EFI_STATUS
UpdateSmbiosString (
  APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTableN,
  SMBIOS_TABLE_STRING               *Field,
  CHAR8                             *Buffer
) {
  CHAR8   *AString, *C1, *C2;
  UINTN   Length = SmbiosTableLength (SmbiosTableN),
          ALength, BLength;
  UINT8   IndexStr = 1;

  if ((SmbiosTableN.Raw == NULL) || !Buffer || !Field) {
    return EFI_NOT_FOUND;
  }

  AString = (CHAR8 *)(SmbiosTableN.Raw + SmbiosTableN.Hdr->Length); //first string
  while (IndexStr != *Field) {
    if (*AString) {
      IndexStr++;
    }

    while (*AString != 0) AString++; //skip string at index

    AString++; //next string
    if (*AString == 0) {
      //this is end of the table
      if (*Field == 0) {
        AString[1] = 0; //one more zero
      }

      *Field = IndexStr; //index of the next string that  is empty

      if (IndexStr == 1) {
        AString--; //first string has no leading zero
      }

      break;
    }
  }

  // AString is at place to copy
  ALength = AsciiTrimStrLen (AString, 0);
  BLength = AsciiTrimStrLen (Buffer, SMBIOS_STRING_MAX_LENGTH);

  //DBG ("Table type %d field %d\n", SmbiosTable.Hdr->Type, *Field);
  //DBG ("Old string length=%d new length=%d\n", ALength, BLength);

  if (BLength > ALength) {
    //Shift right
    C1 = (CHAR8 *)SmbiosTableN.Raw + Length; //old end
    C2 = C1  + BLength - ALength; //new end
    *C2 = 0;

    while (C1 != AString) *(--C2) = *(--C1);
  } else if (BLength < ALength) {
    //Shift left
    C1 = AString + ALength; //old start
    C2 = AString + BLength; //new start

    while (C1 != ((CHAR8 *)SmbiosTableN.Raw + Length)) {
      *C2++ = *C1++;
    }

    *C2 = 0;
    *(--C2) = 0; //end of table
  }

  CopyMem (AString, Buffer, BLength);
  *(AString + BLength) = 0; // not sure there is 0

  return EFI_SUCCESS;
}

APPLE_SMBIOS_STRUCTURE_POINTER
GetSmbiosTableFromType (
  SMBIOS_TABLE_ENTRY_POINT    *SmbiosPoint,
  UINT8                       SmbiosType,
  UINTN                       IndexTable
) {
  APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTableN;
  UINTN                             SmbiosTypeIndex;

  SmbiosTypeIndex = 0;
  SmbiosTableN.Raw = (UINT8 *)((UINTN)SmbiosPoint->TableAddress);
  if (SmbiosTableN.Raw == NULL) {
    return SmbiosTableN;
  }

  while ((SmbiosTypeIndex != IndexTable) || (SmbiosTableN.Hdr->Type != SmbiosType)) {
    if (SmbiosTableN.Hdr->Type == SMBIOS_TYPE_END_OF_TABLE) {
      SmbiosTableN.Raw = NULL;
      return SmbiosTableN;
    }

    if (SmbiosTableN.Hdr->Type == SmbiosType) {
      SmbiosTypeIndex++;
    }

    SmbiosTableN.Raw = (UINT8 *)(SmbiosTableN.Raw + SmbiosTableLength (SmbiosTableN));
  }

  return SmbiosTableN;
}

CHAR8 *
GetSmbiosString (
  APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTableN,
  SMBIOS_TABLE_STRING               StringN
) {
  CHAR8      *AString;
  UINT8      Ind;

  Ind = 1;
  AString = (CHAR8 *)(SmbiosTableN.Raw + SmbiosTableN.Hdr->Length); //first string
  while (Ind != StringN) {
    while (*AString != 0) {
      AString ++;
    }
    AString ++; //skip zero ending
    if (*AString == 0) {
      return AString; //this is end of the table
    }
    Ind++;
  }

  return AString; //return pointer to Ascii string
}

VOID
AddSmbiosEndOfTable () {
  SMBIOS_STRUCTURE  *StructurePtr = (SMBIOS_STRUCTURE *)Current;

  StructurePtr->Type    = SMBIOS_TYPE_END_OF_TABLE;
  StructurePtr->Length  = sizeof (SMBIOS_STRUCTURE);
  StructurePtr->Handle  = SMBIOS_TYPE_INACTIVE; //spec 2.7 p.120
  Current += sizeof (SMBIOS_STRUCTURE);
  *Current++ = 0;
  *Current++ = 0; //double 0 at the end
  NumberOfRecords++;
}

VOID
UniquifySmbiosTableStr (
  APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTableN,
  SMBIOS_TABLE_STRING               *str_idx
) {
  INTN                   i, j;
  SMBIOS_TABLE_STRING    cmp_idx, cmp_str, ref_str;

  if (0 == str_idx[0]) {
    return; // SMBIOS doesn't have string structures, just return;
  }

  for (i = 1; ;i++) {
    cmp_idx = str_idx[i];
    if (0 == cmp_idx) {
      break;
    }

    cmp_str = SmbiosTableN.Raw[cmp_idx];

    if (0 == cmp_str) {
      continue; // if string is undefine, continue
    }

    for (j = 0; j < i; j++) {
      ref_str = SmbiosTableN.Raw[str_idx[j]];
      if (cmp_str == ref_str) {
        SmbiosTableN.Raw[cmp_idx] = 0;    // pretend the string doesn't exist
        // UpdateSmbiosString (SmbiosTableN, &SmbiosTableN.Raw[cmp_idx], GetSmbiosString (SmbiosTableN, ref_str));
        break;
      }
    }
  }
}

/* Patching Functions */
VOID
PatchTableType0 () {
  // BIOS information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_BIOS_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    //DBG ("SmbiosTable: Type 0 (Bios Information) not found!\n");
    return;
  }

  TableSize = SmbiosTableLength (SmbiosTable);
  ZeroMem ((VOID *)newSmbiosTable.Type0, MAX_TABLE_SIZE);
  CopyMem ((VOID *)newSmbiosTable.Type0, (VOID *)SmbiosTable.Type0, TableSize); //can't point to union

  /* Real Mac
    BIOS Information (Type 0)
    Raw Data:
    Header and Data:
    00 18 2E 00 01 02 00 00 03 7F 80 98 01 00 00 00
    00 00 C1 02 00 01 FF FF
    Strings:
    Apple Inc.
    MBP81.88Z.0047.B22.1109281426
    09/28/11
  */

  newSmbiosTable.Type0->BiosSegment = 0; //like in Mac
  newSmbiosTable.Type0->SystemBiosMajorRelease = 0;
  newSmbiosTable.Type0->SystemBiosMinorRelease = 1;

  //newSmbiosTable.Type0->BiosCharacteristics.BiosCharacteristicsNotSupported = 0;
  //newSmbiosTable.Type0->BIOSCharacteristicsExtensionBytes[1] |= 8; //UefiSpecificationSupported;
  //Slice: ----------------------
  //there is a bug in AppleSMBIOS-42 v1.7
  //to eliminate this I have to zero first byte in the field

  *(UINT8 *)&newSmbiosTable.Type0->BiosCharacteristics = 0;
  //dunno about latest version but there is a way to set good characteristics
  //if use patched AppleSMBIOS
  //----------------
  Once = TRUE;

  UniquifySmbiosTableStr (newSmbiosTable, SMBIOS_TABLE_TYPE0_STR_IDX);

  if (AsciiTrimStrLen (gSettings.VendorName, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type0->Vendor, gSettings.VendorName);
  }

  if (AsciiTrimStrLen (gSettings.RomVersion, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type0->BiosVersion, gSettings.RomVersion);
  }

  if (AsciiTrimStrLen (gSettings.ReleaseDate, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type0->BiosReleaseDate, gSettings.ReleaseDate);
  }

  Handle = LogSmbiosTable (newSmbiosTable);
}

VOID
GetTableType1 () {
  CHAR8   *s;

  // System Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 1 (System Information) not found!\n");
    return;
  }

  CopyMem ((VOID *)&gSettings.SmUUID, (VOID *)&SmbiosTable.Type1->Uuid, 16);
  //AsciiStrToUnicodeStr (GetSmbiosString (SmbiosTable, SmbiosTable.Type1->ProductName), gSettings.OEMProduct);
  s = GetSmbiosString (SmbiosTable, SmbiosTable.Type1->ProductName);
  CopyMem (gSettings.OEMProduct, s, AsciiTrimStrLen (s, 64) + 1); //take ending zero
  //s = GetSmbiosString (SmbiosTable, SmbiosTable.Type1->Manufacturer);
  //CopyMem (gSettings.OEMVendor, s, AsciiTrimStrLen (s, 64) + 1);
}

VOID
PatchTableType1 () {
  // System Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    return;
  }

  //Increase table size
  Size = SmbiosTable.Type1->Hdr.Length; //old size
  TableSize = SmbiosTableLength (SmbiosTable); //including strings
  NewSize = 27; //sizeof (SMBIOS_TABLE_TYPE1);
  ZeroMem ((VOID *)newSmbiosTable.Type1, MAX_TABLE_SIZE);
  CopyMem ((VOID *)newSmbiosTable.Type1, (VOID *)SmbiosTable.Type1, Size); //copy main table
  CopyMem ((CHAR8 *)newSmbiosTable.Type1 + NewSize, (CHAR8 *)SmbiosTable.Type1 + Size, TableSize - Size); //copy strings
  newSmbiosTable.Type1->Hdr.Length = (UINT8)NewSize;

  UniquifySmbiosTableStr (newSmbiosTable, SMBIOS_TABLE_TYPE1_STR_IDX);

  newSmbiosTable.Type1->WakeUpType = SystemWakeupTypePowerSwitch;
  Once = TRUE;

  if ((gSettings.SmUUID.Data3 & 0xF000) != 0) {
    CopyMem ((VOID *)&newSmbiosTable.Type1->Uuid, (VOID *)&gSettings.SmUUID, 16);
  }

  if (AsciiTrimStrLen (gSettings.ManufactureName, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type1->Manufacturer, gSettings.ManufactureName);
  }

  if (AsciiTrimStrLen (gSettings.ProductName, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type1->ProductName, gSettings.ProductName);
  }

  if (AsciiTrimStrLen (gSettings.VersionNr, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type1->Version, gSettings.VersionNr);
  }

  if (AsciiTrimStrLen (gSettings.SerialNr, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type1->SerialNumber, gSettings.SerialNr);
  }

  //if (AsciiTrimStrLen (gSettings.BoardNumber, 64) > 0) {
    //UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type1->SKUNumber, gSettings.BoardNumber); //iMac17,1 - there is nothing
    newSmbiosTable.Type1->SKUNumber = 0;
  //}

  if (AsciiTrimStrLen (gSettings.FamilyName, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type1->Family, gSettings.FamilyName);
  }

  Handle = LogSmbiosTable (newSmbiosTable);
}

VOID
GetTableType2 () {
  CHAR8   *s;

  // System Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    return;
  }

  s = GetSmbiosString (SmbiosTable, SmbiosTable.Type2->ProductName);
  CopyMem (gSettings.OEMBoard, s, AsciiTrimStrLen (s, 64) + 1);
  s = GetSmbiosString (SmbiosTable, SmbiosTable.Type2->Manufacturer);
  CopyMem (gSettings.OEMVendor, s, AsciiTrimStrLen (s, 64) + 1);
}

VOID
PatchTableType2 () {
  // BaseBoard Information
  //
  NewSize = 0x10; //sizeof (SMBIOS_TABLE_TYPE2);
  ZeroMem ((VOID *)newSmbiosTable.Type2, MAX_TABLE_SIZE);

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    MsgLog ("SmbiosTable: Type 2 (BaseBoard Information) not found, create new\n");
    //Create new one
    newSmbiosTable.Type2->Hdr.Type = 2;
    newSmbiosTable.Type2->Hdr.Handle = hTable2; //common rule

  } else {
    Size = SmbiosTable.Type2->Hdr.Length; //old size
    TableSize = SmbiosTableLength (SmbiosTable); //including strings

    if (NewSize > Size) {
      CopyMem ((VOID *)newSmbiosTable.Type2, (VOID *)SmbiosTable.Type2, Size); //copy main table
      CopyMem ((CHAR8 *)newSmbiosTable.Type2 + NewSize, (CHAR8 *)SmbiosTable.Type2 + Size, TableSize - Size); //copy strings
    } else {
      CopyMem ((VOID *)newSmbiosTable.Type2, (VOID *)SmbiosTable.Type2, TableSize); //copy full table
    }
  }

  newSmbiosTable.Type2->Hdr.Length = (UINT8)NewSize;
  newSmbiosTable.Type2->ChassisHandle = mHandle3; //from GetTableType3
  newSmbiosTable.Type2->BoardType = gSettings.BoardType;
  ZeroMem ((VOID *)&newSmbiosTable.Type2->FeatureFlag, sizeof (BASE_BOARD_FEATURE_FLAGS));
  newSmbiosTable.Type2->FeatureFlag.Motherboard = 1;
  newSmbiosTable.Type2->FeatureFlag.Replaceable = 1;

  if (gSettings.BoardType == 11) {
    newSmbiosTable.Type2->FeatureFlag.Removable = 1;
  }

  Once = TRUE;

  UniquifySmbiosTableStr (newSmbiosTable, SMBIOS_TABLE_TYPE2_STR_IDX);

  if (AsciiTrimStrLen (gSettings.BoardManufactureName, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type2->Manufacturer, gSettings.BoardManufactureName);
  }

  if (AsciiTrimStrLen (gSettings.BoardNumber, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type2->ProductName, gSettings.BoardNumber);
  }

  if (AsciiTrimStrLen ( gSettings.BoardVersion, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type2->Version, gSettings.BoardVersion); //iMac17,1 - there is ProductName
  }

  if (AsciiTrimStrLen (gSettings.BoardSerialNumber, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type2->SerialNumber, gSettings.BoardSerialNumber);
  }

  if (AsciiTrimStrLen (gSettings.LocationInChassis, 64) > 0) {
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type2->LocationInChassis, gSettings.LocationInChassis);
  }

  newSmbiosTable.Type2->AssetTag = 0;

  //what about Asset Tag??? Not used in real mac. till now.

  //Slice - for the table2 one patch more needed
  /* spec
  Field 0x0E - Identifies the number (0 to 255) of Contained Object Handles that follow
  Field 0x0F - A list of handles of other structures (for example, Baseboard, Processor,
    Port, System Slots, Memory Device) that are contained by this baseboard
  It may be good before our patching but changed after. We should at least check if all
    tables mentioned here are present in final structure
   I just set 0 as in iMac11
  */
  newSmbiosTable.Type2->NumberOfContainedObjectHandles = 0;

  Handle = LogSmbiosTable (newSmbiosTable);
}

VOID
GetTableType3 () {
  // System Chassis Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 3 (System Chassis Information) not found!\n");
    gMobile = FALSE; //default value
    return;
  }

  mHandle3 = SmbiosTable.Type3->Hdr.Handle;
  gMobile = ((SmbiosTable.Type3->Type) >= 8) && (SmbiosTable.Type3->Type != 0x0D); //iMac is desktop!
}

VOID
PatchTableType3 () {
  // System Chassis Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE, 0);
  if (SmbiosTable.Raw == NULL) {
    //DBG ("SmbiosTable: Type 3 (System Chassis Information) not found!\n");
    return;
  }

  Size = SmbiosTable.Type3->Hdr.Length; //old size
  TableSize = SmbiosTableLength (SmbiosTable); //including strings
  NewSize = 0x15; //sizeof (SMBIOS_TABLE_TYPE3);
  ZeroMem ((VOID *)newSmbiosTable.Type3, MAX_TABLE_SIZE);

  if (NewSize > Size) {
    CopyMem ((VOID *)newSmbiosTable.Type3, (VOID *)SmbiosTable.Type3, Size); //copy main table
    CopyMem ((CHAR8 *)newSmbiosTable.Type3 + NewSize, (CHAR8 *)SmbiosTable.Type3 + Size, TableSize - Size); //copy strings
    newSmbiosTable.Type3->Hdr.Length = (UINT8)NewSize;
  } else {
    CopyMem ((VOID *)newSmbiosTable.Type3, (VOID *)SmbiosTable.Type3, TableSize); //copy full table
  }

  newSmbiosTable.Type3->BootupState = ChassisStateSafe;
  newSmbiosTable.Type3->PowerSupplyState = ChassisStateSafe;
  newSmbiosTable.Type3->ThermalState = ChassisStateOther;
  newSmbiosTable.Type3->SecurityStatus = ChassisSecurityStatusOther; //ChassisSecurityStatusNone;
  newSmbiosTable.Type3->NumberofPowerCords = 1;
  newSmbiosTable.Type3->ContainedElementCount = 0;
  newSmbiosTable.Type3->ContainedElementRecordLength = 0;

  Once = TRUE;

  UniquifySmbiosTableStr (newSmbiosTable, SMBIOS_TABLE_TYPE3_STR_IDX);

  if (gSettings.ChassisType != 0) {
    newSmbiosTable.Type3->Type = gSettings.ChassisType;
  }

  if (AsciiTrimStrLen (gSettings.ChassisManufacturer, 64) > 0){
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type3->Manufacturer, gSettings.ChassisManufacturer);
  }
  //SIC! According to iMac there must be the BoardNumber
  if (AsciiTrimStrLen (gSettings.BoardNumber, 64) > 0){
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type3->Version, gSettings.BoardNumber);
  }
  if (AsciiTrimStrLen (gSettings.SerialNr, 64) > 0){
    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type3->SerialNumber, gSettings.SerialNr);
  }
  //if (AsciiTrimStrLen (gSettings.ChassisAssetTag, 64) > 0){
    //UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type3->AssetTag, gSettings.ChassisAssetTag);
    newSmbiosTable.Type3->AssetTag = 0;
    //newSmbiosTable.Type3->SKUNumber = 0;
  //}

  Handle = LogSmbiosTable (newSmbiosTable);
}

VOID
GetTableType4 () {
  // Processor Information
  //
  INTN    res = 0;

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 4 (Processor Information) not found!\n");
    return;
  }

  res = (SmbiosTable.Type4->ExternalClock * 3 + 2) % 100;
  if (res > 2) {
    res = 0;
  } else {
    res = SmbiosTable.Type4->ExternalClock % 10;
  }

  gCPUStructure.ExternalClock = (UINT32)((SmbiosTable.Type4->ExternalClock * 1000) + (res * 110));//MHz->kHz
  //UnicodeSPrint (gSettings.BusSpeed, 10, L"%d", gCPUStructure.ExternalClock);
  //gSettings.BusSpeed = gCPUStructure.ExternalClock; //why duplicate??
  gCPUStructure.CurrentSpeed = SmbiosTable.Type4->CurrentSpeed;
  gCPUStructure.MaxSpeed = SmbiosTable.Type4->MaxSpeed;

  if (Size > 0x23) {  //Smbios <=2.3
    gSettings.EnabledCores = SmbiosTable.Type4->EnabledCoreCount;
  } else {
    gSettings.EnabledCores = 0; //to change later
  }

  //UnicodeSPrint (gSettings.CpuFreqMHz, 10, L"%d", gCPUStructure.CurrentSpeed);
  //gSettings.CpuFreqMHz = gCPUStructure.CurrentSpeed;
}

VOID
PatchTableType4 () {
  // Processor Information
  //
  UINTN     AddBrand = 0, CpuNumber; //Note. iMac11,2 has four tables for CPU i3
  CHAR8     BrandStr[48], *SocketDesignationMac = "U2E1";
  UINT16    ProcChar = 0;

  CopyMem (BrandStr, gCPUStructure.BrandString, 48);
  BrandStr[47] = '\0';

  //DBG ("BrandString=%a\n", BrandStr);

  for (CpuNumber = 0; CpuNumber < gCPUStructure.Cores; CpuNumber++) {
    // Get Table Type4
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION, CpuNumber);
    if (SmbiosTable.Raw == NULL) {
      break;
    }

    // we make SMBios v2.4 while it may be older so we have to increase size
    Size = SmbiosTable.Type4->Hdr.Length; //old size
    TableSize = SmbiosTableLength (SmbiosTable); //including strings

    AddBrand = 0;
    if (SmbiosTable.Type4->ProcessorVersion == 0) { //if no BrandString we can add
      AddBrand = 48;
    }

    NewSize = sizeof (SMBIOS_TABLE_TYPE4);
    ZeroMem ((VOID *)newSmbiosTable.Type4, MAX_TABLE_SIZE);
    CopyMem ((VOID *)newSmbiosTable.Type4, (VOID *)SmbiosTable.Type4, Size); //copy main table
    CopyMem ((CHAR8 *)newSmbiosTable.Type4 + NewSize, (CHAR8 *)SmbiosTable.Type4 + Size, TableSize - Size); //copy strings
    newSmbiosTable.Type4->Hdr.Length = (UINT8)NewSize;

    newSmbiosTable.Type4->MaxSpeed = (UINT16)gCPUStructure.MaxSpeed;
    //old version has no such fields. Fill now
    if (Size <= 0x20){
      //sanity check and clear
      newSmbiosTable.Type4->SerialNumber = 0;
      //newSmbiosTable.Type4->AssetTag = 0;
      //newSmbiosTable.Type4->PartNumber = 0;
    }

    if (Size <= 0x23) {  //Smbios <=2.3
      newSmbiosTable.Type4->CoreCount = gCPUStructure.Cores;
      newSmbiosTable.Type4->ThreadCount = gCPUStructure.Threads;
      newSmbiosTable.Type4->ProcessorCharacteristics = (UINT16)gCPUStructure.Features;
    } //else we propose DMI data is better then cpuid ().
    //if (newSmbiosTable.Type4->CoreCount < newSmbiosTable.Type4->EnabledCoreCount) {
    //  newSmbiosTable.Type4->EnabledCoreCount = gCPUStructure.Cores;
    //}

    newSmbiosTable.Type4->EnabledCoreCount = gSettings.EnabledCores;
    //some verifications
    if ((newSmbiosTable.Type4->ThreadCount < newSmbiosTable.Type4->CoreCount) ||
        newSmbiosTable.Type4->ThreadCount > newSmbiosTable.Type4->CoreCount * 2) {
      newSmbiosTable.Type4->ThreadCount = gCPUStructure.Threads;
    }

    UniquifySmbiosTableStr (newSmbiosTable, SMBIOS_TABLE_TYPE4_STR_IDX);

    // TODO: Set SmbiosTable.Type4->ProcessorFamily for all implemented CPU models
    Once = TRUE;
    if (gCPUStructure.Model == CPU_MODEL_ATOM) {
      newSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelAtom;
    }
     if (gCPUStructure.Model >= CPU_MODEL_NEHALEM) {
       if (AsciiStrStr (gCPUStructure.BrandString, "i3")) {
        newSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCoreI3;
       }

       if (AsciiStrStr (gCPUStructure.BrandString, "i5")) {
        newSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCoreI5;
       }

       if (AsciiStrStr (gCPUStructure.BrandString, "i7")) {
         newSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCoreI7;
       }
    }

    //spec 2.7 page 48 note 3
    if (
      (newSmbiosTable.Type4->ProcessorFamily == ProcessorFamilyIntelCore2) &&
      gCPUStructure.Mobile
    ) {
      newSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCore2DuoMobile;
    }

    // Set CPU Attributes
    newSmbiosTable.Type4->L1CacheHandle = L1;
    newSmbiosTable.Type4->L2CacheHandle = L2;
    newSmbiosTable.Type4->L3CacheHandle = L3;
    newSmbiosTable.Type4->ProcessorType = CentralProcessor;
    newSmbiosTable.Type4->ProcessorId.Signature.ProcessorSteppingId = gCPUStructure.Stepping;
    newSmbiosTable.Type4->ProcessorId.Signature.ProcessorModel    = (gCPUStructure.Model & 0xF);
    newSmbiosTable.Type4->ProcessorId.Signature.ProcessorFamily   = gCPUStructure.Family;
    newSmbiosTable.Type4->ProcessorId.Signature.ProcessorType   = gCPUStructure.Type;
    newSmbiosTable.Type4->ProcessorId.Signature.ProcessorXModel   = gCPUStructure.Extmodel;
    newSmbiosTable.Type4->ProcessorId.Signature.ProcessorXFamily  = gCPUStructure.Extfamily;

    //CopyMem ((VOID *)&newSmbiosTable.Type4->ProcessorId.FeatureFlags, (VOID *)&gCPUStructure.Features, 4);
    //newSmbiosTable.Type4->ProcessorId.FeatureFlags = (PROCESSOR_FEATURE_FLAGS)(UINT32)gCPUStructure.Features;

    if (Size <= 0x26) {
      newSmbiosTable.Type4->ProcessorFamily2 = newSmbiosTable.Type4->ProcessorFamily;
      ProcChar |= (gCPUStructure.ExtFeatures & CPUID_EXTFEATURE_EM64T)?0x04:0;
      ProcChar |= (gCPUStructure.Cores > 1)?0x08:0;
      ProcChar |= (gCPUStructure.Cores < gCPUStructure.Threads)?0x10:0;
      ProcChar |= (gCPUStructure.ExtFeatures & CPUID_EXTFEATURE_XD)?0x20:0;
      ProcChar |= (gCPUStructure.Features & CPUID_FEATURE_VMX)?0x40:0;
      ProcChar |= (gCPUStructure.Features & CPUID_FEATURE_EST)?0x80:0;
      newSmbiosTable.Type4->ProcessorCharacteristics = ProcChar;
    }

    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type4->Socket, SocketDesignationMac);

    if (AddBrand) {
      UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type4->ProcessorVersion, BrandStr);
    }

    //UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type4->AssetTag, BrandStr); //like mac

    newSmbiosTable.Type4->AssetTag = 0;
    newSmbiosTable.Type4->PartNumber = 0;

    // looks to be MicroCode revision
    if (gCPUStructure.MicroCode > 0){
      AsciiSPrint (BrandStr, 20, "%X", gCPUStructure.MicroCode);
      UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type4->SerialNumber, BrandStr);
    }

    Handle = LogSmbiosTable (newSmbiosTable);
  }
}

#if 0
VOID
PatchTableType6 () {
  UINT8   SizeField = 0;
  //
  // MemoryModule (TYPE 6)

  // This table is obsolete accoding to Spec but Apple still using it so
  // copy existing table if found, no patches will be here
  // we can have more then 1 module.
  for (Index = 0; Index < MAX_RAM_SLOTS; Index++) {
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_MODULE_INFORMATON, Index);
    if (SmbiosTable.Raw == NULL) {
      //MsgLog ("SMBIOS Table 6 index %d not found\n", Index);
      continue;
    }

    SizeField = SmbiosTable.Type6->InstalledSize.InstalledOrEnabledSize & 0x7F;
    if (SizeField < 0x7D) {
      mInstalled[Index] =  LShiftU64 (1ULL, 20 + SizeField);
    } else if (SizeField == 0x7F) {
      mInstalled[Index] = 0;
    } else {
      mInstalled[Index] =  4096ULL * (1024ULL * 1024ULL);
    }

    MsgLog ("Table 6 MEMORY_MODULE %d Installed %x ", Index, mInstalled[Index]);

    if (SizeField >= 0x7D) {
      mEnabled[Index]   = 0;
    } else {
      mEnabled[Index]   = LShiftU64 (1ULL, 20 + ((UINT8)SmbiosTable.Type6->EnabledSize.InstalledOrEnabledSize & 0x7F));
    }

    MsgLog ("... enabled %x \n", mEnabled[Index]);
    LogSmbiosTable (SmbiosTable);
  }
}
#endif

VOID
PatchTableType7 () {
  // Cache Information
  //
    //TODO - should be separate table for each CPU core
    //new handle for each core and attach Type4 tables for individual Type7
    // Handle = 0x0700 + CoreN<<2 + CacheN (4-level cache is supported
    // L1[CoreN] = Handle

  CHAR8     *SSocketD;
  BOOLEAN   correctSD = FALSE;

  //according to spec for Smbios v2.0 max handle is 0xFFFE, for v>2.0 (we made 2.6) max handle=0xFEFF.
  // Value 0xFFFF means no cache
  L1 = 0xFFFF; // L1 Cache
  L2 = 0xFFFF; // L2 Cache
  L3 = 0xFFFF; // L3 Cache

  // Get Table Type7 and set CPU Caches
  for (Index = 0; Index < MAX_CACHE_COUNT; Index++) {
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_CACHE_INFORMATION, Index);
    if (SmbiosTable.Raw == NULL) {
      break;
    }

    TableSize = SmbiosTableLength (SmbiosTable);
    ZeroMem ((VOID *)newSmbiosTable.Type7, MAX_TABLE_SIZE);
    CopyMem ((VOID *)newSmbiosTable.Type7, (VOID *)SmbiosTable.Type7, TableSize);
    correctSD = (newSmbiosTable.Type7->SocketDesignation == 0);
    CoreCache = newSmbiosTable.Type7->CacheConfiguration & 3;
    Once = TRUE;

    SSocketD = "L1-Cache";
    if (correctSD) {
      SSocketD[1] = (CHAR8)(0x31 + CoreCache);
      UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type7->SocketDesignation, SSocketD);
    }

    Handle = LogSmbiosTable (newSmbiosTable);
    switch (CoreCache) {
      case 0:
        L1 = Handle;
        break;

      case 1:
        L2 = Handle;
        break;

      case 2:
        L3 = Handle;
        break;

      default:
        break;
    }
  }
}

VOID
PatchTableType9 () {
  //
  // System Slots (Type 9)
  /*
   SlotDesignation: PCI1
   System Slot Type: PCI
   System Slot Data Bus Width:  32 bit
   System Slot Current Usage:  Available
   System Slot Length:  Short length
   System Slot Type: PCI
   Slot Id: the value present in the Slot Number field of the PCI Interrupt Routing table entry that is associated with this slot is: 1
   Slot characteristics 1:  Provides 3.3 Volts |  Slot's opening is shared with another slot, e.g. PCI/EISA shared slot. |
   Slot characteristics 2:  PCI slot supports Power Management Enable (PME#) signal |
   SegmentGroupNum: 0x4350
   BusNum: 0x49
   DevFuncNum: 0x31

   Real Mac always contain Airport table 9 as
   09 0D xx xx 01 A5 08 03 03 00 00 04 06 "AirPort"
  */

  //usage in OSX:
  // SlotID == value of Name (_SUN, SlotID) 8bit
  // SlotDesignation == name to "AAPL,slot-name"
  // SlotType = 32bit PCI/SlotTypePciExpressX1/x4/x16
  // real PC -> PCI, real Mac -> PCIe

  for (Index = 0; Index < 15; Index++) {
    if (SlotDevices[Index].Valid) {
      INTN    Dev, Func;

      ZeroMem ((VOID *)newSmbiosTable.Type9, MAX_TABLE_SIZE);
      newSmbiosTable.Type9->Hdr.Type = EFI_SMBIOS_TYPE_SYSTEM_SLOTS;
      newSmbiosTable.Type9->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE9);
      newSmbiosTable.Type9->Hdr.Handle = (UINT16)(hTable9 + Index);
      newSmbiosTable.Type9->SlotDesignation = 1;
      newSmbiosTable.Type9->SlotType = SlotDevices[Index].SlotType;
      newSmbiosTable.Type9->SlotDataBusWidth = SlotDataBusWidth1X;
      newSmbiosTable.Type9->CurrentUsage = SlotUsageAvailable;
      newSmbiosTable.Type9->SlotLength = SlotLengthShort;
      newSmbiosTable.Type9->SlotID = SlotDevices[Index].SlotID;
      newSmbiosTable.Type9->SlotCharacteristics1.Provides33Volts = 1;
      newSmbiosTable.Type9->SlotCharacteristics2.HotPlugDevicesSupported = 1;
      // take this from PCI bus for WiFi card
      newSmbiosTable.Type9->SegmentGroupNum = SlotDevices[Index].SegmentGroupNum;
      newSmbiosTable.Type9->BusNum = SlotDevices[Index].BusNum;
      newSmbiosTable.Type9->DevFuncNum = SlotDevices[Index].DevFuncNum;
      //
      Dev = SlotDevices[Index].DevFuncNum >> 3;
      Func = SlotDevices[Index].DevFuncNum & 7;
      //DBG ("insert table 9 for dev %x:%x\n", Dev, Func);
      UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type9->SlotDesignation, SlotDevices[Index].SlotName);
      LogSmbiosTable (newSmbiosTable);
    }
  }
}

#if 0
VOID
PatchTableType11 () {
  CHAR8   *OEMString = "Apple inc. uses Clover"; //something else here?

  // System Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_OEM_STRINGS, 0);
  if (SmbiosTable.Raw != NULL) {
    MsgLog ("Table 11 present, but rewritten for us\n");
  }
  //TableSize = SmbiosTableLength (SmbiosTable);
  ZeroMem ((VOID *)newSmbiosTable.Type11, MAX_TABLE_SIZE);
  //CopyMem ((VOID *)newSmbiosTable.Type11, (VOID *)SmbiosTable.Type11, 5); //minimum, other bytes = 0
  newSmbiosTable.Type11->Hdr.Type = EFI_SMBIOS_TYPE_OEM_STRINGS;
  newSmbiosTable.Type11->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 2;
  newSmbiosTable.Type11->Hdr.Handle = hTable11; //common rule

  newSmbiosTable.Type11->StringCount = 1;
  UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type11->StringCount, OEMString);

  LogSmbiosTable (newSmbiosTable);
}
#endif

VOID
PatchTableTypeSome () {
  //some unused but interesting tables. Just log as is
#define NUM_OTHER_TYPES 13
  UINT8 tableTypes[NUM_OTHER_TYPES] = {8, 10, 13, 18, 21, 22, 27, 28, 32, 33, 129, 217, 219};
  UINTN IndexType;
  //
  // Different types
  for (IndexType = 0; IndexType < NUM_OTHER_TYPES; IndexType++) {
    for (Index = 0; Index < 32; Index++) {
      SmbiosTable = GetSmbiosTableFromType (EntryPoint, tableTypes[IndexType], Index);
      if (SmbiosTable.Raw == NULL) {
        continue;
      }

      LogSmbiosTable (SmbiosTable);
    }
  }
}

VOID
GetTableType16 () {
  // Physical Memory Array
  //
  mTotalSystemMemory = 0; //later we will add to the value, here initialize it
  TotalCount = 0;

  // Get Table Type16 and set Device Count
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 16 (Physical Memory Array) not found!\n");
    return;
  }

  TotalCount = SmbiosTable.Type16->NumberOfMemoryDevices;
  if (!TotalCount) {
    TotalCount = MAX_RAM_SLOTS;
  }

  MsgLog ("Total Memory Slots Count = %d\n", TotalCount);
}

VOID
PatchTableType16 () {
  // Physical Memory Array
  //

  // Get Table Type16 and set Device Count
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 16 (Physical Memory Array) not found!\n");
    return;
  }

  TableSize = SmbiosTableLength (SmbiosTable);
  ZeroMem ((VOID *)newSmbiosTable.Type16, MAX_TABLE_SIZE);
  CopyMem ((VOID *)newSmbiosTable.Type16, (VOID *)SmbiosTable.Type16, TableSize);
  newSmbiosTable.Type16->Hdr.Handle = hTable16;
  // Slice - I am not sure I want these values
  // newSmbiosTable.Type16->Location = MemoryArrayLocationProprietaryAddonCard;
  // newSmbiosTable.Type16->Use = MemoryArrayUseSystemMemory;
  // newSmbiosTable.Type16->MemoryErrorCorrection = MemoryErrorCorrectionMultiBitEcc;
  // MemoryErrorInformationHandle
  newSmbiosTable.Type16->MemoryErrorInformationHandle = 0xFFFF;
  newSmbiosTable.Type16->NumberOfMemoryDevices = gRAMCount;
  DBG ("NumberOfMemoryDevices = %d\n", gRAMCount);
  LogSmbiosTable (newSmbiosTable);
}

VOID
GetTableType17 () {
  // Memory Device
  //
  INTN      Index2;
  BOOLEAN   Found;

  // Get Table Type17 and count Size
  gRAMCount = 0;
  for (Index = 0; Index < TotalCount; Index++) {  //how many tables there may be?
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_DEVICE, Index);
    if (SmbiosTable.Raw == NULL) {
      //DBG ("SmbiosTable: Type 17 (Memory Device number %d) not found!\n", Index);
      continue;
    }

    MsgLog ("Type 17 Index = %d\n", Index);

    //gDMI->CntMemorySlots++;
    if (SmbiosTable.Type17->MemoryErrorInformationHandle < 0xFFFE) {
      DBG ("Table has error information, checking\n"); //why skipping?
      // Why trust it if it has an error? I guess we could look
      //  up the error handle and determine certain errors may
      //  be skipped where others may not but it seems easier
      //  to just skip all entries that have an error - apianti
      // will try
      Found = FALSE;

      for (Index2 = 0; Index2 < 24; Index2++) {
        newSmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_32BIT_MEMORY_ERROR_INFORMATION, Index2);
        if (newSmbiosTable.Raw == NULL) {
          continue;
        }

        if (newSmbiosTable.Type18->Hdr.Handle == SmbiosTable.Type17->MemoryErrorInformationHandle) {
          Found = TRUE;
          DBG (" - Found memory information in table 18/%d, type=0x%x, operation=0x%x syndrome=0x%x\n",
              Index2,
              newSmbiosTable.Type18->ErrorType,
              newSmbiosTable.Type18->ErrorOperation,
              newSmbiosTable.Type18->VendorSyndrome);

          switch (newSmbiosTable.Type18->ErrorType) {
            case MemoryErrorOk:
              DBG ("  - ...memory OK\n");
              break;

            case MemoryErrorCorrected:
              DBG ("  - ...memory errors corrected\n");
              break;

            case MemoryErrorChecksum:
              DBG ("  - ...error type: Checksum\n");
              break;

            default:
              DBG ("  - ...error type not shown\n");
              break;
          }

          break;
        }
      }

      if (Found) {
        if ((newSmbiosTable.Type18->ErrorType != MemoryErrorOk) &&
            (newSmbiosTable.Type18->ErrorType != MemoryErrorCorrected)) {
          DBG ("  - skipping wrong module\n");
          continue;
        }
      }
    }

    // Determine if slot has size
    if (SmbiosTable.Type17->Size > 0) {
      gRAM.SMBIOS[Index].InUse = TRUE;
      gRAM.SMBIOS[Index].ModuleSize = SmbiosTable.Type17->Size;
    }

    // Determine if module frequency is sane value
    if ((SmbiosTable.Type17->Speed > 0) && (SmbiosTable.Type17->Speed <= MAX_RAM_FREQUENCY)) {
      gRAM.SMBIOS[Index].InUse = TRUE;
      gRAM.SMBIOS[Index].Frequency = SmbiosTable.Type17->Speed;
    } else {
      MsgLog (" - Ignoring insane frequency value %dMHz\n", SmbiosTable.Type17->Speed);
    }

    // Fill rest of information if in use
    if (gRAM.SMBIOS[Index].InUse) {
      ++(gRAM.SMBIOSInUse);
      gRAM.SMBIOS[Index].Vendor = GetSmbiosString (SmbiosTable, SmbiosTable.Type17->Manufacturer);
      gRAM.SMBIOS[Index].SerialNo = GetSmbiosString (SmbiosTable, SmbiosTable.Type17->SerialNumber);
      gRAM.SMBIOS[Index].PartNo = GetSmbiosString (SmbiosTable, SmbiosTable.Type17->PartNumber);

      //DBG ("CntMemorySlots = %d\n", gDMI->CntMemorySlots)
      //DBG ("gDMI->MemoryModules = %d\n", gDMI->MemoryModules)

      MsgLog (" - Speed = %dMHz, Size = %dMB, Bank/Device = %a/%a, Vendor = %a, SerialNumber = %a, PartNumber = %a\n",
        gRAM.SMBIOS[Index].Frequency,
        gRAM.SMBIOS[Index].ModuleSize,
        GetSmbiosString (SmbiosTable, SmbiosTable.Type17->BankLocator),
        GetSmbiosString (SmbiosTable, SmbiosTable.Type17->DeviceLocator),
        gRAM.SMBIOS[Index].Vendor,
        gRAM.SMBIOS[Index].SerialNo,
        gRAM.SMBIOS[Index].PartNo
      );

      /*
        if ((SmbiosTable.Type17->Size & 0x8000) == 0) {
          mTotalSystemMemory += SmbiosTable.Type17->Size; //Mb
          mMemory17[Index] = (UINT16)(SmbiosTable.Type17->Size > 0 ? mTotalSystemMemory : 0);
        }
        DBG ("mTotalSystemMemory = %d\n", mTotalSystemMemory);
      */
    }
  }
}

VOID
PatchTableType17 () {
  CHAR8           deviceLocator[10], bankLocator[10];
  UINT8           channelMap[MAX_RAM_SLOTS], expectedCount = 0, channels = 2;
  BOOLEAN         insertingEmpty = TRUE, trustSMBIOS = ((gRAM.SPDInUse == 0) || gSettings.TrustSMBIOS),
                  wrongSMBIOSBanks = FALSE, isMacPro = FALSE;
  MACHINE_TYPES   Model = GetModelFromString (gSettings.ProductName);

  if (
    (Model == MacPro31) ||
    (Model == MacPro41) ||
    (Model == MacPro51) ||
    (Model == MacPro61)
  ) {
    isMacPro = TRUE;
  }

  // Inject user memory tables
  if (gSettings.InjectMemoryTables) {
    DBG ("Injecting user memory modules to SMBIOS\n");
    if (gRAM.UserInUse == 0) {
      DBG ("User SMBIOS contains no memory modules\n");
      return;
    }

    // Check channels
    if ((gRAM.UserChannels == 0) || (gRAM.UserChannels > 4)) {
       gRAM.UserChannels = 1;
    }

    if (gRAM.UserInUse >= MAX_RAM_SLOTS) {
      gRAM.UserInUse = MAX_RAM_SLOTS;
    }

    DBG ("Channels: %d\n", gRAM.UserChannels);

    // Setup interleaved channel map
    if (channels >= 2) {
       UINT8  doubleChannels = (UINT8)gRAM.UserChannels << 1;

       for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
          channelMap[Index] = (UINT8)(((Index / doubleChannels) * doubleChannels) +
             ((Index / gRAM.UserChannels) % 2) + ((Index % gRAM.UserChannels) << 1));
       }
    } else {
      for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
        channelMap[Index] = (UINT8)Index;
      }
    }

    DBG ("Interleave:");

    for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
      DBG (" %d", channelMap[Index]);
    }

    DBG ("\n");

    // Memory Device
    //
    gRAMCount = 0;
    // Inject tables
    for (Index = 0; Index < gRAM.UserInUse; Index++) {
      UINTN   UserIndex = channelMap[Index];
      UINT8   bank = (UINT8)Index / gRAM.UserChannels;

      ZeroMem ((VOID *)newSmbiosTable.Type17, MAX_TABLE_SIZE);
      newSmbiosTable.Type17->Hdr.Type = EFI_SMBIOS_TYPE_MEMORY_DEVICE;
      newSmbiosTable.Type17->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE17);
      newSmbiosTable.Type17->TotalWidth = 0xFFFF;
      newSmbiosTable.Type17->DataWidth = 0xFFFF;
      newSmbiosTable.Type17->Hdr.Handle = (UINT16)(hTable17 + UserIndex);
      newSmbiosTable.Type17->FormFactor = gMobile ? MemoryFormFactorSodimm : MemoryFormFactorDimm;
      newSmbiosTable.Type17->TypeDetail.Synchronous = TRUE;
      newSmbiosTable.Type17->DeviceSet = bank + 1;
      newSmbiosTable.Type17->MemoryArrayHandle = hTable16;

      if (isMacPro) {
        AsciiSPrint (deviceLocator, 10, "DIMM%d", gRAMCount + 1);
      } else {
        AsciiSPrint (deviceLocator, 10, "DIMM%d", bank);
        AsciiSPrint (bankLocator, 10, "BANK%d", Index % channels);
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->BankLocator, (CHAR8 *)&bankLocator[0]);
      }

      UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->DeviceLocator, (CHAR8 *)&deviceLocator[0]);

      if ((gRAM.User[UserIndex].InUse) && (gRAM.User[UserIndex].ModuleSize > 0)) {
        if (AsciiTrimStrLen (gRAM.User[UserIndex].Vendor, 64) > 0) {
          UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->Manufacturer, gRAM.User[UserIndex].Vendor);
        } else {
          UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->Manufacturer, "unknown");
        }

        if (AsciiTrimStrLen (gRAM.User[UserIndex].SerialNo, 64) > 0) {
          UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->SerialNumber, gRAM.User[UserIndex].SerialNo);
        } else {
          UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->SerialNumber, "unknown");
        }

        if (AsciiTrimStrLen (gRAM.User[UserIndex].PartNo, 64) > 0) {
          UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->PartNumber, gRAM.User[UserIndex].PartNo);
        } else {
          UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->PartNumber, "unknown");
        }

        newSmbiosTable.Type17->Speed = (UINT16)gRAM.User[UserIndex].Frequency;
        newSmbiosTable.Type17->Size = (UINT16)gRAM.User[UserIndex].ModuleSize;
        newSmbiosTable.Type17->MemoryType = gRAM.User[UserIndex].Type;

        if (
          (newSmbiosTable.Type17->MemoryType != MemoryTypeDdr2) &&
          (newSmbiosTable.Type17->MemoryType != MemoryTypeDdr4) &&
          (newSmbiosTable.Type17->MemoryType != MemoryTypeDdr)
        ) {
          newSmbiosTable.Type17->MemoryType = MemoryTypeDdr3;
        }

        DBG ("%a %a %dMHz %dMB\n", bankLocator, deviceLocator, newSmbiosTable.Type17->Speed, newSmbiosTable.Type17->Size);

        mTotalSystemMemory += newSmbiosTable.Type17->Size; //Mb
        mMemory17[gRAMCount] = (UINT16)mTotalSystemMemory;
        //DBG ("mTotalSystemMemory = %d\n", mTotalSystemMemory);
      } else {
        DBG ("%a %a EMPTY\n", bankLocator, deviceLocator);
      }

      newSmbiosTable.Type17->MemoryErrorInformationHandle = 0xFFFF;
      mHandle17[gRAMCount++] = LogSmbiosTable (newSmbiosTable);
    }

    if (mTotalSystemMemory > 0) {
      DBG ("mTotalSystemMemory = %dMB, %d in use\n", mTotalSystemMemory, gRAMCount);
    }

    return;
  }

  // Prevent inserting empty tables
  if ((gRAM.SPDInUse == 0) && (gRAM.SMBIOSInUse == 0)) {
    DBG ("SMBIOS and SPD contain no modules in use\n");
    return;
  }

  // Detect whether the SMBIOS is trusted information
  if (trustSMBIOS) {
    if (gRAM.SMBIOSInUse != 0) {
      if (gRAM.SPDInUse != 0) {
        if (gRAM.SPDInUse != gRAM.SMBIOSInUse) {
          // Prefer the SPD information
          if (gRAM.SPDInUse > gRAM.SMBIOSInUse) {
            DBG ("Not trusting SMBIOS because SPD reports more modules...\n");
            trustSMBIOS = FALSE;
          } else if (gRAM.SPD[0].InUse || !gRAM.SMBIOS[0].InUse) {
            if (gRAM.SPDInUse > 1) {
              DBG ("Not trusting SMBIOS because SPD reports different modules...\n");
              trustSMBIOS = FALSE;
            } else if (gRAM.SMBIOSInUse == 1) {
              channels = 1;
            }
          } else if (gRAM.SPDInUse == 1) {
            // The SMBIOS may contain table for built-in module
            if (gRAM.SMBIOSInUse <= 2) {
              if (
                !gRAM.SMBIOS[0].InUse || !gRAM.SPD[2].InUse ||
                (gRAM.SMBIOS[0].Frequency != gRAM.SPD[2].Frequency) ||
                (gRAM.SMBIOS[0].ModuleSize != gRAM.SPD[2].ModuleSize)
              ) {
                channels = 1;
              }
            } else {
              DBG ("Not trusting SMBIOS because SPD reports only one module...\n");
              trustSMBIOS = FALSE;
            }
          } else {
            DBG ("Not trusting SMBIOS because SPD reports less modules...\n");
            trustSMBIOS = FALSE;
          }
        } else if (gRAM.SPD[0].InUse != gRAM.SMBIOS[0].InUse) {
          // Never trust a sneaky SMBIOS!
          DBG ("Not trusting SMBIOS because it's being sneaky...\n");
          trustSMBIOS = FALSE;
        }
      } else if (gRAM.SMBIOSInUse == 1) {
        channels = 1;
      }
    }
  }

  if (trustSMBIOS) {
    DBG ("Trusting SMBIOS...\n");
  }

  // Determine expected slot count
  expectedCount = (gRAM.UserInUse != 0) ? gRAM.UserInUse : gRAM.SPDInUse;

  if (trustSMBIOS) {
    // Use the smbios in use count
    if (expectedCount < gRAM.SMBIOSInUse) {
      expectedCount = gRAM.SMBIOSInUse;
    }

    // Check if smbios has a good total count
    if ((!gMobile || (TotalCount == 2)) &&
        (expectedCount < TotalCount)) {
      expectedCount = (UINT8)TotalCount;
    }
  } else {
    // Use default value of two for mobile or four for desktop
    if (gMobile) {
      if (expectedCount < 2) {
        expectedCount = 2;
      }
    } else if (expectedCount < 4) {
      expectedCount = 4;
    }
  }

  // Check for interleaved channels
  if (channels >= 2) {
     wrongSMBIOSBanks = ((gRAM.SMBIOS[1].InUse != gRAM.SPD[1].InUse) ||
                         (gRAM.SMBIOS[1].ModuleSize != gRAM.SPD[1].ModuleSize));
  }

  if (wrongSMBIOSBanks) {
    DBG ("Detected alternating SMBIOS channel banks\n");
  }

  // Determine if using triple or quadruple channel
  if (gRAM.UserChannels != 0) {
    channels = gRAM.UserChannels;
  } else if (gRAM.SPDInUse == 0) {
    if (trustSMBIOS) {
      if ((gRAM.SMBIOSInUse % 4) == 0) {
        // Quadruple channel
        if (
          (
            wrongSMBIOSBanks &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[1].InUse) &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[2].InUse) &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[3].InUse) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[1].ModuleSize) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[2].ModuleSize) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[3].ModuleSize)
          ) ||
          (
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[2].InUse) &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[4].InUse) &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[6].InUse) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[2].ModuleSize) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[4].ModuleSize) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[6].ModuleSize)
          )
        ) {
          channels = 4;
        }
      } else if ((gRAM.SMBIOSInUse % 3) == 0) {
        // Triple channel
        if (
          (
            wrongSMBIOSBanks &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[1].InUse) &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[2].InUse) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[1].ModuleSize) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[2].ModuleSize)
          ) ||
          (
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[2].InUse) &&
            (gRAM.SMBIOS[0].InUse == gRAM.SMBIOS[4].InUse) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[2].ModuleSize) &&
            (gRAM.SMBIOS[0].ModuleSize == gRAM.SMBIOS[4].ModuleSize)
          )
        ) {
          channels = 3;
        }
      } else if (!wrongSMBIOSBanks && ((gRAM.SMBIOSInUse % 2) != 0)) {
         channels = 1;
      }
    }
  } else if ((gRAM.SPDInUse % 4) == 0) {
    // Quadruple channel
    if (
      (gRAM.SPD[0].InUse == gRAM.SPD[2].InUse) &&
      (gRAM.SPD[0].InUse == gRAM.SPD[4].InUse) &&
      (gRAM.SPD[0].InUse == gRAM.SPD[6].InUse) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[2].ModuleSize) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[4].ModuleSize) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[6].ModuleSize)
    ) {
      channels = 4;
    }
  } else if ((gRAM.SPDInUse % 3) == 0) {
    // Triple channel
    if (
      (gRAM.SPD[0].InUse == gRAM.SPD[2].InUse) &&
      (gRAM.SPD[0].InUse == gRAM.SPD[4].InUse) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[2].ModuleSize) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[4].ModuleSize)
    ) {
      channels = 3;
    }
  } else if (
    (gRAM.SPD[0].InUse != gRAM.SPD[2].InUse) ||
    ((gRAM.SPDInUse % 2) != 0)
  ) {
    channels = 1;
  }

  // Can't have less than the number of channels
  if (expectedCount < channels) {
    expectedCount = channels;
  }

  if (expectedCount > 0) {
    --expectedCount;
  }

  MsgLog ("Channels: %d\n", channels);

  // Setup interleaved channel map
  if (channels >= 2) {
    UINT8   doubleChannels = (UINT8)channels << 1;

    for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
      channelMap[Index] = (UINT8)(((Index / doubleChannels) * doubleChannels) +
                                  ((Index / channels) % 2) + ((Index % channels) << 1));
    }
  } else {
    for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
      channelMap[Index] = (UINT8)Index;
    }
  }

  DBG ("Interleave:");

  for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
    DBG (" %d", channelMap[Index]);
  }

  DBG ("\n");

  // Memory Device
  //
  gRAMCount = 0;
  for (Index = 0; Index < TotalCount; Index++) {
    UINTN   SMBIOSIndex = wrongSMBIOSBanks ? Index : channelMap[Index],
            SPDIndex = channelMap[Index];
    UINT8   bank = (UINT8)Index / channels;

    if (
      !insertingEmpty && (gRAMCount > expectedCount) &&
      !gRAM.SPD[SPDIndex].InUse &&
      (!trustSMBIOS || !gRAM.SMBIOS[SMBIOSIndex].InUse)
    ) {
      continue;
    }

    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_DEVICE, SMBIOSIndex);
    if (trustSMBIOS && gRAM.SMBIOS[SMBIOSIndex].InUse && (SmbiosTable.Raw != NULL)) {
      TableSize = SmbiosTableLength (SmbiosTable);
      CopyMem ((VOID *)newSmbiosTable.Type17, (VOID *)SmbiosTable.Type17, TableSize);
      newSmbiosTable.Type17->AssetTag = 0;

      if (AsciiTrimStrLen (gRAM.SMBIOS[SMBIOSIndex].Vendor, 64) > 0) {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->Manufacturer, gRAM.SMBIOS[SMBIOSIndex].Vendor);
        AsciiSPrint (gSettings.MemoryManufacturer, 64, "%a", gRAM.SMBIOS[SMBIOSIndex].Vendor);
      } else {
        //newSmbiosTable.Type17->Manufacturer = 0;
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->Manufacturer, "unknown");
      }

      if (AsciiTrimStrLen (gRAM.SMBIOS[SMBIOSIndex].SerialNo, 64) > 0) {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->SerialNumber, gRAM.SMBIOS[SMBIOSIndex].SerialNo);
        AsciiSPrint (gSettings.MemorySerialNumber, 64, "%a", gRAM.SMBIOS[SMBIOSIndex].SerialNo);
      } else {
        //newSmbiosTable.Type17->SerialNumber = 0;
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->SerialNumber, "unknown");
      }

      if (AsciiTrimStrLen (gRAM.SMBIOS[SMBIOSIndex].PartNo, 64) > 0) {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->PartNumber, gRAM.SMBIOS[SMBIOSIndex].PartNo);
        AsciiSPrint (gSettings.MemoryPartNumber, 64, "%a", gRAM.SMBIOS[SMBIOSIndex].PartNo);
        DBG (" partNum=%a\n", gRAM.SMBIOS[SMBIOSIndex].PartNo);
      } else {
        //newSmbiosTable.Type17->PartNumber = 0;
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->PartNumber,  "unknown");
        DBG (" partNum unknown\n");
      }
    } else {
      ZeroMem ((VOID *)newSmbiosTable.Type17, MAX_TABLE_SIZE);
      newSmbiosTable.Type17->Hdr.Type = EFI_SMBIOS_TYPE_MEMORY_DEVICE;
      newSmbiosTable.Type17->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE17);
      newSmbiosTable.Type17->TotalWidth = 0xFFFF;
      newSmbiosTable.Type17->DataWidth = 0xFFFF;
    }

    Once = TRUE;

    newSmbiosTable.Type17->Hdr.Handle = (UINT16)(hTable17 + Index);
    newSmbiosTable.Type17->FormFactor = gMobile ? MemoryFormFactorSodimm : MemoryFormFactorDimm;
    newSmbiosTable.Type17->TypeDetail.Synchronous = TRUE;
    newSmbiosTable.Type17->DeviceSet = bank + 1;
    newSmbiosTable.Type17->MemoryArrayHandle = hTable16;

    if (gRAM.SPD[SPDIndex].InUse) {
      if (AsciiTrimStrLen (gRAM.SPD[SPDIndex].Vendor, 64) > 0) {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->Manufacturer, gRAM.SPD[SPDIndex].Vendor);
        AsciiSPrint (gSettings.MemoryManufacturer, 64, "%a", gRAM.SPD[SPDIndex].Vendor);
      } else {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->Manufacturer, "unknown");
      }

      if (AsciiTrimStrLen (gRAM.SPD[SPDIndex].SerialNo, 64) > 0) {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->SerialNumber, gRAM.SPD[SPDIndex].SerialNo);
        AsciiSPrint (gSettings.MemorySerialNumber, 64, "%a", gRAM.SPD[SPDIndex].SerialNo);
      } else {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->SerialNumber, "unknown");
      }

      if (AsciiTrimStrLen (gRAM.SPD[SPDIndex].PartNo, 64) > 0) {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->PartNumber, gRAM.SPD[SPDIndex].PartNo);
        AsciiSPrint (gSettings.MemoryPartNumber, 64, "%a", gRAM.SPD[SPDIndex].PartNo);
      } else {
        UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->PartNumber, "unknown");
      }

      newSmbiosTable.Type17->Speed = (UINT16)gRAM.SPD[SPDIndex].Frequency;
      newSmbiosTable.Type17->Size = (UINT16)gRAM.SPD[SPDIndex].ModuleSize;
      newSmbiosTable.Type17->MemoryType = gRAM.SPD[SPDIndex].Type;
    }

    if (
      trustSMBIOS &&
      gRAM.SMBIOS[SMBIOSIndex].InUse &&
      (newSmbiosTable.Type17->Speed < (UINT16)gRAM.SMBIOS[SMBIOSIndex].Frequency)
    ) {
      DBG ("Type17->Speed corrected by SMBIOS from %dMHz to %dMHz\n",
          newSmbiosTable.Type17->Speed, gRAM.SMBIOS[SMBIOSIndex].Frequency);

      newSmbiosTable.Type17->Speed = (UINT16)gRAM.SMBIOS[SMBIOSIndex].Frequency;
    }

    AsciiSPrint (gSettings.MemorySpeed, 64, "%d", newSmbiosTable.Type17->Speed);

    // Assume DDR3 unless explicitly set to DDR2/DDR/DDR4
    if (
      (newSmbiosTable.Type17->MemoryType != MemoryTypeDdr2) &&
      (newSmbiosTable.Type17->MemoryType != MemoryTypeDdr4) &&
      (newSmbiosTable.Type17->MemoryType != MemoryTypeDdr)
    ) {
      newSmbiosTable.Type17->MemoryType = MemoryTypeDdr3;
    }

    //now I want to update deviceLocator and bankLocator
    if (isMacPro) {
      AsciiSPrint (deviceLocator, 10, "DIMM%d", gRAMCount + 1);
      AsciiSPrint (bankLocator, 10, "");
    } else {
      AsciiSPrint (deviceLocator, 10, "DIMM%d", bank);
      AsciiSPrint (bankLocator, 10, "BANK%d", Index % channels);
    }

    UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->DeviceLocator, (CHAR8 *)&deviceLocator[0]);

    if (isMacPro) {
      newSmbiosTable.Type17->BankLocator = 0; //like in MacPro5,1
    } else {
      UpdateSmbiosString (newSmbiosTable, &newSmbiosTable.Type17->BankLocator, (CHAR8 *)&bankLocator[0]);
    }

    MsgLog ("SMBIOS Type 17 Index = %d => %d %d:", gRAMCount, SMBIOSIndex, SPDIndex);

    if (newSmbiosTable.Type17->Size == 0) {
      MsgLog ("%a %a EMPTY\n", bankLocator, deviceLocator);
      newSmbiosTable.Type17->MemoryType = 0; //MemoryTypeUnknown;
    } else {
      insertingEmpty = FALSE;
      MsgLog ("%a %a %dMHz %dMB\n", bankLocator, deviceLocator, newSmbiosTable.Type17->Speed, newSmbiosTable.Type17->Size);
      mTotalSystemMemory += newSmbiosTable.Type17->Size; //Mb
      mMemory17[gRAMCount] = (UINT16)mTotalSystemMemory;
      //DBG ("mTotalSystemMemory = %d\n", mTotalSystemMemory);
    }

    newSmbiosTable.Type17->MemoryErrorInformationHandle = 0xFFFF;
    mHandle17[gRAMCount++] = LogSmbiosTable (newSmbiosTable);
  }

  if (mTotalSystemMemory > 0) {
    MsgLog ("mTotalSystemMemory = %dMB\n", mTotalSystemMemory);
  }
}

VOID
PatchTableType19 () {
  //
  // Generate Memory Array Mapped Address info (TYPE 19)
  //
  /*
   /// This structure provides the address mapping for a Physical Memory Array.
   /// One structure is present for each contiguous address range described.
   ///
   typedef struct {
    SMBIOS_STRUCTURE      Hdr;
    UINT32                StartingAddress;
    UINT32                EndingAddress;
    UINT16                MemoryArrayHandle;
    UINT8                 PartitionWidth;
   } SMBIOS_TABLE_TYPE19;
  */

  //Slice - I created one table as a sum of all other. It is needed for SetupBrowser
  UINT32    TotalEnd = 0;
  UINT8     PartWidth = 1;
  //UINT16    SomeHandle = 0x1300; //as a common rule handle=(type<<8 + index)

  for (Index = 0; Index < TotalCount; Index++) {
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS, Index);
    if (SmbiosTable.Raw == NULL) {
      continue;
    }

    if (SmbiosTable.Type19->EndingAddress > TotalEnd) {
      TotalEnd = SmbiosTable.Type19->EndingAddress;
    }

    PartWidth = SmbiosTable.Type19->PartitionWidth;
    //SomeHandle = SmbiosTable.Type19->Hdr.Handle;
  }

  if (TotalEnd == 0) {
    TotalEnd = (UINT32)(LShiftU64 (mTotalSystemMemory, 10) - 1);
  }

  gTotalMemory = LShiftU64 (mTotalSystemMemory, 20);
  ZeroMem ((VOID *)newSmbiosTable.Type19, MAX_TABLE_SIZE);
  newSmbiosTable.Type19->Hdr.Type = EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS;
  newSmbiosTable.Type19->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE19);
  newSmbiosTable.Type19->Hdr.Handle = hTable19;
  newSmbiosTable.Type19->MemoryArrayHandle = hTable16;
  newSmbiosTable.Type19->StartingAddress = 0;
  newSmbiosTable.Type19->EndingAddress = TotalEnd;
  newSmbiosTable.Type19->PartitionWidth = PartWidth;
  mHandle19 = LogSmbiosTable (newSmbiosTable);
}

VOID
PatchTableType20 () {
  UINTN   j = 0, k = 0, m = 0;
  //
  // Generate Memory Array Mapped Address info (TYPE 20)
  // not needed neither for Apple nor for EFI
  m = 0;
  for (Index = 0; Index < TotalCount; Index++) {
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS, Index);
    if (SmbiosTable.Raw == NULL) {
      return ;
    }

    TableSize = SmbiosTableLength (SmbiosTable);
    ZeroMem ((VOID *)newSmbiosTable.Type20, MAX_TABLE_SIZE);
    CopyMem ((VOID *)newSmbiosTable.Type20, (VOID *)SmbiosTable.Type20, TableSize);

    for (j=0; j < TotalCount; j++) {
      //EndingAddress in kb while mMemory in Mb
      if ((UINT32)(mMemory17[j] << 10) > newSmbiosTable.Type20->EndingAddress) {
        newSmbiosTable.Type20->MemoryDeviceHandle = mHandle17[j];
        k = newSmbiosTable.Type20->EndingAddress;
        m += mMemory17[j];

        //DBG ("Type20[%02d]->End = 0x%x, Type17[%02d] = 0x%x\n",
        //    Index, k, j, m);

        //DBG (" MemoryDeviceHandle = 0x%x\n", newSmbiosTable.Type20->MemoryDeviceHandle);
        mMemory17[j] = 0; // used
        break;
      }
      //DBG ("\n");
    }

    newSmbiosTable.Type20->MemoryArrayMappedAddressHandle = mHandle19;
    //
    // Record Smbios Type 20
    //
    LogSmbiosTable (newSmbiosTable);
  }
}

VOID
GetTableType32 () {
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    return;
  }

  gBootStatus = SmbiosTable.Type32->BootStatus;
}

// Apple Specific Structures
VOID
PatchTableType128 () {
  //
  // FirmwareVolume (TYPE 128)
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, 128, 0);
  if (SmbiosTable.Raw == NULL) {
    //DEBUG ((EFI_D_ERROR, "SmbiosTable: Type 128 (FirmwareVolume) not found!\n"));

    ZeroMem ((VOID *)newSmbiosTable.Type128, MAX_TABLE_SIZE);
    newSmbiosTable.Type128->Hdr.Type = 128;
    newSmbiosTable.Type128->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE128);
    newSmbiosTable.Type128->Hdr.Handle = hTable128; //common rule
    newSmbiosTable.Type128->FirmwareFeatures = gFwFeatures; //0x80001417; //imac112 -> 0x1403
    newSmbiosTable.Type128->FirmwareFeaturesMask = 0xC003ff37; // 0xffff

    /*
      FW_REGION_RESERVED   = 0,
      FW_REGION_RECOVERY   = 1,
      FW_REGION_MAIN       = 2, gHob->MemoryAbove1MB.PhysicalStart + ResourceLength
      or fix as 0x200000 - 0x600000
      FW_REGION_NVRAM      = 3,
      FW_REGION_CONFIG     = 4,
      FW_REGION_DIAGVAULT  = 5,
    */

    //TODO - Slice - I have an idea that region should be the same as Efivar.bin
    newSmbiosTable.Type128->RegionCount = 1;
    newSmbiosTable.Type128->RegionType[0] = FW_REGION_MAIN;
    //UpAddress = mTotalSystemMemory << 20; //Mb -> b
    //      gHob->MemoryAbove1MB.PhysicalStart;
    newSmbiosTable.Type128->FlashMap[0].StartAddress = 0xFFE00000; //0xF0000;
    //      gHob->MemoryAbove1MB.PhysicalStart + gHob->MemoryAbove1MB.ResourceLength - 1;
    newSmbiosTable.Type128->FlashMap[0].EndAddress = 0xFFEFFFFF;
    //newSmbiosTable.Type128->RegionType[1] = FW_REGION_NVRAM; //Efivar
    //newSmbiosTable.Type128->FlashMap[1].StartAddress = 0x15000; //0xF0000;
    //newSmbiosTable.Type128->FlashMap[1].EndAddress = 0x1FFFF;
    //region type=1 also present in mac
    LogSmbiosTable (newSmbiosTable);

    return ;
  }
  //
  // Log Smbios Record Type128
  //
  LogSmbiosTable (SmbiosTable);
}

VOID
PatchTableType130 () {
  //
  // MemorySPD (TYPE 130)
  // TODO:  read SPD and place here. But for a what?
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, 130, 0);
  if (SmbiosTable.Raw == NULL) {
    return ;
  }

  //
  // Log Smbios Record Type130
  //
  LogSmbiosTable (SmbiosTable);
}

VOID
PatchTableType131 () {
  // Get Table Type131
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, 131, 0);
  if (SmbiosTable.Raw != NULL) {
    MsgLog ("Table 131 is present, CPUType=%x\n", SmbiosTable.Type131->ProcessorType);
    MsgLog ("Change to: %x\n", gSettings.CpuType);
  }

  ZeroMem ((VOID *)newSmbiosTable.Type131, MAX_TABLE_SIZE);
  newSmbiosTable.Type131->Hdr.Type = 131;
  newSmbiosTable.Type131->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 2;
  newSmbiosTable.Type131->Hdr.Handle = hTable131; //common rule
  // Patch ProcessorType
  newSmbiosTable.Type131->ProcessorType = gSettings.CpuType;
  Handle = LogSmbiosTable (newSmbiosTable);
}

VOID
PatchTableType132 () {
  if (gSettings.QPI == 0xFFFF) {
    return;
  }

  // Get Table Type132
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, 132, 0);
  if (SmbiosTable.Raw != NULL) {
    MsgLog ("Table 132 is present, QPI=%x\n", SmbiosTable.Type132->ProcessorBusSpeed);
    MsgLog ("Change to: %x\n", gSettings.QPI);
  }

  ZeroMem ((VOID *)newSmbiosTable.Type132, MAX_TABLE_SIZE);
  newSmbiosTable.Type132->Hdr.Type = 132;
  newSmbiosTable.Type132->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 2;
  newSmbiosTable.Type132->Hdr.Handle = hTable132;

  // Patch ProcessorBusSpeed
  if (gSettings.QPI){
    newSmbiosTable.Type132->ProcessorBusSpeed = gSettings.QPI;
  } else {
    newSmbiosTable.Type132->ProcessorBusSpeed = (UINT16)(LShiftU64 (DivU64x32 (gSettings.BusSpeed, kilo), 2));
  }

  Handle = LogSmbiosTable (newSmbiosTable);
}

VOID
PatchTableType133 () {
  if (gSettings.PlatformFeature == 0xFFFF) {
    return;
  }

  // Get Table Type133
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, 133, 0);
  if (SmbiosTable.Raw != NULL) {
    MsgLog ("Table 133 is present, PlatformFeature=%x\n", SmbiosTable.Type133->PlatformFeature);
    MsgLog ("Change to: %x\n", gSettings.PlatformFeature);
  }

  ZeroMem ((VOID *)newSmbiosTable.Type133, MAX_TABLE_SIZE);
  newSmbiosTable.Type133->Hdr.Type = 133;
  newSmbiosTable.Type133->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 8;
  newSmbiosTable.Type133->Hdr.Handle = hTable133;
  //newSmbiosTable.Type133->PlatformFeature = gSettings.PlatformFeature;
  CopyMem ((VOID *)&newSmbiosTable.Type133->PlatformFeature, (VOID *)&gSettings.PlatformFeature, 8);
  Handle = LogSmbiosTable (newSmbiosTable);
}

CHAR8 *
EFIAPI
Rev2HexStr (
  UINT8   *Bytes,
  UINTN   Len
) {
  INT32     i, y, b1, b2;
  CHAR8     *Nib = "0123456789ABCDEF",
            *Res = AllocatePool ((Len * 2) + 1);

  for (i = 0, y = 0; i < Len; i++) {
    BOOLEAN   Skip = FALSE;

    switch (i) {
      case 1:
        Res[y++] = 0x2E;
        break;
      case 3:
        Res[y++] = 0x30;
      case 4:
        Skip = TRUE;
        break;
    }

    if (Skip) {
      continue;
    }

    b2 = Bytes[i];
    b1 = (b2 >> 4);

    if (b1 == 0) {
      if (b2 != 0) {
        goto next;
      }

      continue;
    }

    Res[y++] = Nib[b1 & 0x0F];

    next:

    Res[y++] = Nib[b2 & 0x0F];
  }

  Res[y] = '\0';

  return Res;
}

VOID
PatchTableType134 () {
  CHAR8   *REV;

  // Get Table Type134
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, 134, 0);
  if (SmbiosTable.Raw != NULL) {
    MsgLog ("Table 134 is present, SMCVersion=%x\n", SmbiosTable.Type134->SMCVersion);
    MsgLog ("Change to: %x\n", gSettings.REV);
  }

  ZeroMem ((VOID *)newSmbiosTable.Type134, MAX_TABLE_SIZE);
  newSmbiosTable.Type134->Hdr.Type = 134;
  newSmbiosTable.Type134->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 16;
  newSmbiosTable.Type134->Hdr.Handle = hTable134;
  REV = Rev2HexStr (gSettings.REV, sizeof (gSettings.REV));
  CopyMem ((VOID *)&newSmbiosTable.Type134->SMCVersion, (VOID *)REV, AsciiStrSize (REV));
  Handle = LogSmbiosTable (newSmbiosTable);
}

EFI_STATUS
PrepatchSmbios () {
  EFI_STATUS              Status = EFI_SUCCESS;
  UINTN                   BufferLen;
  EFI_PHYSICAL_ADDRESS    BufferPtr;

  DbgHeader ("Get Smbios");

  // Get SMBIOS Tables
  Smbios = FindOemSMBIOSPtr ();

  //DBG ("OEM SMBIOS EPS=%p\n", Smbios);
  //DBG ("OEM Tables = %x\n", ((SMBIOS_TABLE_ENTRY_POINT *)Smbios)->TableAddress);

  if (!Smbios) {
    Status = EFI_NOT_FOUND;
    //DBG ("Original SMBIOS System Table not found! Getting from Hob...\n");
    Smbios = GetSmbiosTablesFromHob ();
    //DBG ("HOB SMBIOS EPS=%p\n", Smbios);
    if (!Smbios) {
      //DBG ("And here SMBIOS System Table not found! Trying System table ...\n");
      // this should work on any UEFI
      Smbios = GetSmbiosTablesFromConfigTables ();
      //DBG ("ConfigTables SMBIOS EPS=%p\n", Smbios);
      if (!Smbios) {
        //DBG ("And here SMBIOS System Table not found! Exiting...\n");
        return EFI_NOT_FOUND;
      }
    }
  }

  //original EPS and tables
  EntryPoint = (SMBIOS_TABLE_ENTRY_POINT *)Smbios; //yes, it is old SmbiosEPS
  //Smbios = (VOID *)(UINT32)EntryPoint->TableAddress; // here is flat Smbios database. Work with it
  //how many we need to add for tables 128, 130, 131, 132 and for strings?
  BufferLen = 0x20 + EntryPoint->TableLength + 64 * 10;
  //new place for EPS and tables. Allocated once for both
  BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;

  Status = gBS->AllocatePages (AllocateMaxAddress, EfiACPIMemoryNVS, /* EfiACPIReclaimMemory, */

  EFI_SIZE_TO_PAGES (BufferLen), &BufferPtr);

  if (EFI_ERROR (Status)) {
    //DBG ("There is error allocating pages in EfiACPIMemoryNVS!\n");
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,  /*EfiACPIMemoryNVS, */EfiACPIReclaimMemory,
                    ROUND_PAGE (BufferLen)/EFI_PAGE_SIZE, &BufferPtr
                  );

    //if (EFI_ERROR (Status)) {
      //DBG ("There is error allocating pages in EfiACPIReclaimMemory!\n");
    //}
  }

  //DBG ("Buffer @ %p\n", BufferPtr);

  if (BufferPtr) {
    SmbiosEpsNew = (SMBIOS_TABLE_ENTRY_POINT *)(UINTN)BufferPtr; //this is new EPS
  } else {
    SmbiosEpsNew = EntryPoint; //is it possible?!
  }

  ZeroMem (SmbiosEpsNew, BufferLen);
  //DBG ("New EntryPoint = %p\n", SmbiosEpsNew);
  NumberOfRecords = 0;
  MaxStructureSize = 0;
  //preliminary fill EntryPoint with some data
  CopyMem ((VOID *)SmbiosEpsNew, (VOID *)EntryPoint, sizeof (SMBIOS_TABLE_ENTRY_POINT));

  Smbios = (VOID *)(SmbiosEpsNew + 1); //this is a C-language trick. I hate it but use. +1 means +sizeof (SMBIOS_TABLE_ENTRY_POINT)
  Current = (UINT8 *)Smbios; //begin fill tables from here
  SmbiosEpsNew->TableAddress = (UINT32)(UINTN)Current;
  SmbiosEpsNew->EntryPointLength = sizeof (SMBIOS_TABLE_ENTRY_POINT); // no matter on other versions
  SmbiosEpsNew->MajorVersion = 2;
  SmbiosEpsNew->MinorVersion = 4;
  SmbiosEpsNew->SmbiosBcdRevision = 0x24; //Slice - we want to have v2.6 but Apple still uses 2.4

  //Create space for SPD
  //gRAM = AllocateZeroPool (sizeof (MEM_STRUCTURE));
  //gDMI = AllocateZeroPool (sizeof (DMI));

  //Collect information for use in menu
  GetTableType1 ();
  GetTableType2 ();
  GetTableType3 ();
  GetTableType4 ();
  GetTableType16 ();
  GetTableType17 ();
  GetTableType32 (); //get BootStatus here to decide what to do
  DBG ("Boot status=%x\n", gBootStatus);

  //for example the bootloader may go to Recovery is BootStatus is Fail
  return  Status;
}

VOID
PatchSmbios () {
  DbgHeader ("PatchSmbios");

  newSmbiosTable.Raw = (UINT8 *)AllocateZeroPool (MAX_TABLE_SIZE);
  //Slice - order of patching is significant
  PatchTableType0 ();
  PatchTableType1 ();
  PatchTableType2 ();
  PatchTableType3 ();
  PatchTableType7 (); //we should know handles before patch Table4
  PatchTableType4 ();
  //PatchTableType6 ();
  PatchTableType9 ();
  //PatchTableType11 ();
  PatchTableTypeSome ();
  PatchTableType17 ();
  PatchTableType16 ();
  PatchTableType19 ();
  PatchTableType20 ();
  PatchTableType128 ();
  PatchTableType130 ();
  PatchTableType131 ();
  PatchTableType132 ();
  PatchTableType133 ();
  PatchTableType134 ();
  AddSmbiosEndOfTable ();

  //if (MaxStructureSize > MAX_TABLE_SIZE) {
    //DBG ("Too long SMBIOS!\n");
  //}

  FreePool ((VOID *)newSmbiosTable.Raw);

  // there is no need to keep all tables in numeric order. It is not needed
  // neither by specs nor by AppleSmbios.kext
}

VOID
FinalizeSmbios () {
  EFI_PEI_HOB_POINTERS    GuidHob, HobStart;
  EFI_PHYSICAL_ADDRESS    *Table = NULL;
  //UINTN                 TableLength = 0;

  // Get Hob List
  HobStart.Raw = GetHobList ();

  if (HobStart.Raw != NULL) {
    // find SMBIOS in hob
    Size = ARRAY_SIZE (gTableGuidArray);
    for (Index = 0; Index < Size; ++Index) {
      GuidHob.Raw = GetNextGuidHob (gTableGuidArray[Index], HobStart.Raw);
      if (GuidHob.Raw != NULL) {
        Table = GET_GUID_HOB_DATA (GuidHob.Guid);
        //TableLength = GET_GUID_HOB_DATA_SIZE (GuidHob);
        if (Table != NULL) {
          break;
        }
      }
    }
  }

  //
  // Install SMBIOS in Config table
  SmbiosEpsNew->TableLength = (UINT16)((UINT32)(UINTN)Current - (UINT32)(UINTN)Smbios);
  SmbiosEpsNew->NumberOfSmbiosStructures = NumberOfRecords;
  SmbiosEpsNew->MaxStructureSize = MaxStructureSize;
  SmbiosEpsNew->IntermediateChecksum = 0;
  SmbiosEpsNew->IntermediateChecksum = (UINT8)(256 - Checksum8 (
                                                        (UINT8 *)SmbiosEpsNew + 0x10,
                                                        SmbiosEpsNew->EntryPointLength - 0x10)
                                                      );

  SmbiosEpsNew->EntryPointStructureChecksum = 0;
  SmbiosEpsNew->EntryPointStructureChecksum = (UINT8)(256 - Checksum8 ((UINT8 *)SmbiosEpsNew, SmbiosEpsNew->EntryPointLength));

  //DBG ("SmbiosEpsNew->EntryPointLength = %d\n", SmbiosEpsNew->EntryPointLength);
  //DBG ("DMI checksum = %d\n", Checksum8 ((UINT8 *)SmbiosEpsNew, SmbiosEpsNew->EntryPointLength));

  gBS->InstallConfigurationTable (&gEfiSmbiosTableGuid, (VOID *)SmbiosEpsNew);
  gST->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 ((UINT8 *)&gST->Hdr, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);

  //
  // Fix it in Hob list
  //
  // No smbios in Hob list on Aptio, so no need to update it there.
  // But even if it would be there, loading of OSX would overwrite it
  // since this list on my board is inside space needed for kernel
  // (ha! like many other UEFI stuff).
  // It's enough to add it to Conf.table.
  //
  if (Table != NULL) {
    //PauseForKey (L"installing SMBIOS in Hob\n");
    *Table = (UINT32)(UINTN)SmbiosEpsNew;
  }
}
