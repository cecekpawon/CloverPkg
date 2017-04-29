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

//EFI_GUID                          gUuid;
EFI_GUID                          *gTableGuidArray[] = { &gEfiSmbiosTableGuid };

//EFI_PHYSICAL_ADDRESS            *smbiosTable;
VOID                              *Smbios;  //pointer to SMBIOS data
SMBIOS_TABLE_ENTRY_POINT          *EntryPoint; //SmbiosEps original
SMBIOS_TABLE_ENTRY_POINT          *SmbiosEpsNew; //new SmbiosEps

//for patching
APPLE_SMBIOS_STRUCTURE_POINTER    SmbiosTable;
APPLE_SMBIOS_STRUCTURE_POINTER    NewSmbiosTable;

UINT16                            NumberOfRecords, MaxStructureSize, CoreCache = 0, TotalCount = 0,
                                  L1, L2, L3,
                                  gSmHandle3, gSmHandle17[MAX_RAM_SLOTS], gSmHandle19,
                                  mMemory17[MAX_RAM_SLOTS];

UINT8                             *Current, //pointer to the current end of tables
                                  gBootStatus,
                                  gRAMCount = 0;

//EFI_SMBIOS_TABLE_HEADER           *Record;
EFI_SMBIOS_HANDLE                 Handle;
//EFI_SMBIOS_TYPE                   Type;

//UINTN                             stringNumber;
//UINT32                            MaxMemory = 0;
UINT32                            mTotalSystemMemory;
UINT64                            gTotalMemory;
//UINT64                            mInstalled[MAX_RAM_SLOTS];
//UINT64                            mEnabled[MAX_RAM_SLOTS];
BOOLEAN                           gMobile;
//BOOLEAN                           Once;

MEM_STRUCTURE                     gRAM;
//DMI                             *gDMI;

#define MAX_HANDLE                0xFEFF
#define SMBIOS_PTR                SIGNATURE_32 ('_','S','M','_')
#define MAX_TABLE_SIZE            512
#define STR_A_UNKNOWN             "unknown"

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
  UINTN     i;
  UINT8     Length, Checksum = 0, *BytePtr;

  if (!EntryPointStructure) {
    return FALSE;
  }

  BytePtr = (UINT8 *)EntryPointStructure;
  Length = EntryPointStructure->EntryPointLength;

  for (i = 0; i < Length; i++) {
    Checksum = Checksum + (UINT8)BytePtr[i];
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
  SMBIOS_TABLE_STRING               *StrIdx
) {
  INTN                   i, j;
  SMBIOS_TABLE_STRING    CmpIdx, CmpStr, RefStr;

  if (0 == StrIdx[0]) {
    return; // SMBIOS doesn't have string structures, just return;
  }

  for (i = 1; ;i++) {
    CmpIdx = StrIdx[i];
    if (0 == CmpIdx) {
      break;
    }

    CmpStr = SmbiosTableN.Raw[CmpIdx];

    if (0 == CmpStr) {
      continue; // if string is undefine, continue
    }

    for (j = 0; j < i; j++) {
      RefStr = SmbiosTableN.Raw[StrIdx[j]];
      if (CmpStr == RefStr) {
        SmbiosTableN.Raw[CmpIdx] = 0;    // pretend the string doesn't exist
        // UpdateSmbiosString (SmbiosTableN, &SmbiosTableN.Raw[CmpIdx], GetSmbiosString (SmbiosTableN, RefStr));
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
  UINTN   TableSize;

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_BIOS_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    //DBG ("SmbiosTable: Type 0 (Bios Information) not found!\n");
    return;
  }

  TableSize = SmbiosTableLength (SmbiosTable);
  ZeroMem ((VOID *)NewSmbiosTable.Type0, MAX_TABLE_SIZE);
  CopyMem ((VOID *)NewSmbiosTable.Type0, (VOID *)SmbiosTable.Type0, TableSize); //can't point to union

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

  NewSmbiosTable.Type0->BiosSegment = 0; //like in Mac
  NewSmbiosTable.Type0->SystemBiosMajorRelease = 0;
  NewSmbiosTable.Type0->SystemBiosMinorRelease = 1;

  //NewSmbiosTable.Type0->BiosCharacteristics.BiosCharacteristicsNotSupported = 0;
  //NewSmbiosTable.Type0->BIOSCharacteristicsExtensionBytes[1] |= 8; //UefiSpecificationSupported;
  //Slice: ----------------------
  //there is a bug in AppleSMBIOS-42 v1.7
  //to eliminate this I have to zero first byte in the field

  *(UINT8 *)&NewSmbiosTable.Type0->BiosCharacteristics = 0;
  //dunno about latest version but there is a way to set good characteristics
  //if use patched AppleSMBIOS
  //----------------
  //Once = TRUE;

  UniquifySmbiosTableStr (NewSmbiosTable, SMBIOS_TABLE_TYPE0_STR_IDX);

  if (AsciiTrimStrLen (gSettings.VendorName, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type0->Vendor, gSettings.VendorName);
  }

  if (AsciiTrimStrLen (gSettings.RomVersion, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type0->BiosVersion, gSettings.RomVersion);
  }

  if (AsciiTrimStrLen (gSettings.ReleaseDate, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type0->BiosReleaseDate, gSettings.ReleaseDate);
  }

  Handle = LogSmbiosTable (NewSmbiosTable);
}

VOID
GetTableType1 () {
  CHAR8   *Str;

  // System Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 1 (System Information) not found!\n");
    return;
  }

  CopyMem ((VOID *)&gSettings.SmUUID, (VOID *)&SmbiosTable.Type1->Uuid, sizeof (EFI_GUID));
  //AsciiStrToUnicodeStr (GetSmbiosString (SmbiosTable, SmbiosTable.Type1->ProductName), gSettings.OEMProduct);
  Str = GetSmbiosString (SmbiosTable, SmbiosTable.Type1->ProductName);
  CopyMem (gSettings.OEMProduct, Str, AsciiTrimStrLen (Str, 64) + 1); //take ending zero
  //s = GetSmbiosString (SmbiosTable, SmbiosTable.Type1->Manufacturer);
  //CopyMem (gSettings.OEMVendor, s, AsciiTrimStrLen (s, 64) + 1);
}

VOID
PatchTableType1 () {
  // System Information
  //

  UINTN   Size, NewSize, TableSize;

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    return;
  }

  //Increase table size
  Size = SmbiosTable.Type1->Hdr.Length; //old size
  TableSize = SmbiosTableLength (SmbiosTable); //including strings
  NewSize = 27; //sizeof (SMBIOS_TABLE_TYPE1);
  ZeroMem ((VOID *)NewSmbiosTable.Type1, MAX_TABLE_SIZE);
  CopyMem ((VOID *)NewSmbiosTable.Type1, (VOID *)SmbiosTable.Type1, Size); //copy main table
  CopyMem ((CHAR8 *)NewSmbiosTable.Type1 + NewSize, (CHAR8 *)SmbiosTable.Type1 + Size, TableSize - Size); //copy strings
  NewSmbiosTable.Type1->Hdr.Length = (UINT8)NewSize;

  UniquifySmbiosTableStr (NewSmbiosTable, SMBIOS_TABLE_TYPE1_STR_IDX);

  NewSmbiosTable.Type1->WakeUpType = SystemWakeupTypePowerSwitch;
  //Once = TRUE;

  if ((gSettings.SmUUID.Data3 & 0xF000) != 0) {
    CopyMem ((VOID *)&NewSmbiosTable.Type1->Uuid, (VOID *)&gSettings.SmUUID, 16);
  }

  if (AsciiTrimStrLen (gSettings.ManufactureName, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type1->Manufacturer, gSettings.ManufactureName);
  }

  if (AsciiTrimStrLen (gSettings.ProductName, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type1->ProductName, gSettings.ProductName);
  }

  if (AsciiTrimStrLen (gSettings.VersionNr, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type1->Version, gSettings.VersionNr);
  }

  if (AsciiTrimStrLen (gSettings.SerialNr, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type1->SerialNumber, gSettings.SerialNr);
  }

  //if (AsciiTrimStrLen (gSettings.BoardNumber, 64) > 0) {
    //UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type1->SKUNumber, gSettings.BoardNumber); //iMac17,1 - there is nothing
    NewSmbiosTable.Type1->SKUNumber = 0;
  //}

  if (AsciiTrimStrLen (gSettings.FamilyName, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type1->Family, gSettings.FamilyName);
  }

  Handle = LogSmbiosTable (NewSmbiosTable);
}

VOID
GetTableType2 () {
  CHAR8   *Str;

  // System Information
  //
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    return;
  }

  Str = GetSmbiosString (SmbiosTable, SmbiosTable.Type2->ProductName);
  CopyMem (gSettings.OEMBoard, Str, AsciiTrimStrLen (Str, 64) + 1);
  Str = GetSmbiosString (SmbiosTable, SmbiosTable.Type2->Manufacturer);
  CopyMem (gSettings.OEMVendor, Str, AsciiTrimStrLen (Str, 64) + 1);
}

VOID
PatchTableType2 () {
  // BaseBoard Information
  //

  UINTN   Size, NewSize, TableSize;

  NewSize = 0x10; //sizeof (SMBIOS_TABLE_TYPE2);
  ZeroMem ((VOID *)NewSmbiosTable.Type2, MAX_TABLE_SIZE);

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    MsgLog ("SmbiosTable: Type 2 (BaseBoard Information) not found, create new\n");
    //Create new one
    NewSmbiosTable.Type2->Hdr.Type = 2;
    NewSmbiosTable.Type2->Hdr.Handle = hTable2; //common rule

  } else {
    Size = SmbiosTable.Type2->Hdr.Length; //old size
    TableSize = SmbiosTableLength (SmbiosTable); //including strings

    if (NewSize > Size) {
      CopyMem ((VOID *)NewSmbiosTable.Type2, (VOID *)SmbiosTable.Type2, Size); //copy main table
      CopyMem ((CHAR8 *)NewSmbiosTable.Type2 + NewSize, (CHAR8 *)SmbiosTable.Type2 + Size, TableSize - Size); //copy strings
    } else {
      CopyMem ((VOID *)NewSmbiosTable.Type2, (VOID *)SmbiosTable.Type2, TableSize); //copy full table
    }
  }

  NewSmbiosTable.Type2->Hdr.Length = (UINT8)NewSize;
  NewSmbiosTable.Type2->ChassisHandle = gSmHandle3; //from GetTableType3
  NewSmbiosTable.Type2->BoardType = gSettings.BoardType;
  ZeroMem ((VOID *)&NewSmbiosTable.Type2->FeatureFlag, sizeof (BASE_BOARD_FEATURE_FLAGS));
  NewSmbiosTable.Type2->FeatureFlag.Motherboard = 1;
  NewSmbiosTable.Type2->FeatureFlag.Replaceable = 1;

  if (gSettings.BoardType == 11) {
    NewSmbiosTable.Type2->FeatureFlag.Removable = 1;
  }

  //Once = TRUE;

  UniquifySmbiosTableStr (NewSmbiosTable, SMBIOS_TABLE_TYPE2_STR_IDX);

  if (AsciiTrimStrLen (gSettings.BoardManufactureName, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type2->Manufacturer, gSettings.BoardManufactureName);
  }

  if (AsciiTrimStrLen (gSettings.BoardNumber, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type2->ProductName, gSettings.BoardNumber);
  }

  if (AsciiTrimStrLen ( gSettings.BoardVersion, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type2->Version, gSettings.BoardVersion); //iMac17,1 - there is ProductName
  }

  if (AsciiTrimStrLen (gSettings.BoardSerialNumber, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type2->SerialNumber, gSettings.BoardSerialNumber);
  }

  if (AsciiTrimStrLen (gSettings.LocationInChassis, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type2->LocationInChassis, gSettings.LocationInChassis);
  }

  NewSmbiosTable.Type2->AssetTag = 0;

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
  NewSmbiosTable.Type2->NumberOfContainedObjectHandles = 0;

  Handle = LogSmbiosTable (NewSmbiosTable);
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

  gSmHandle3 = SmbiosTable.Type3->Hdr.Handle;
  gMobile = ((SmbiosTable.Type3->Type) >= 8) && (SmbiosTable.Type3->Type != 0x0D); //iMac is desktop!
}

VOID
PatchTableType3 () {
  // System Chassis Information
  //

  UINTN   Size, NewSize, TableSize;

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE, 0);
  if (SmbiosTable.Raw == NULL) {
    //DBG ("SmbiosTable: Type 3 (System Chassis Information) not found!\n");
    return;
  }

  Size = SmbiosTable.Type3->Hdr.Length; //old size
  TableSize = SmbiosTableLength (SmbiosTable); //including strings
  NewSize = 0x15; //sizeof (SMBIOS_TABLE_TYPE3);
  ZeroMem ((VOID *)NewSmbiosTable.Type3, MAX_TABLE_SIZE);

  if (NewSize > Size) {
    CopyMem ((VOID *)NewSmbiosTable.Type3, (VOID *)SmbiosTable.Type3, Size); //copy main table
    CopyMem ((CHAR8 *)NewSmbiosTable.Type3 + NewSize, (CHAR8 *)SmbiosTable.Type3 + Size, TableSize - Size); //copy strings
    NewSmbiosTable.Type3->Hdr.Length = (UINT8)NewSize;
  } else {
    CopyMem ((VOID *)NewSmbiosTable.Type3, (VOID *)SmbiosTable.Type3, TableSize); //copy full table
  }

  NewSmbiosTable.Type3->BootupState = ChassisStateSafe;
  NewSmbiosTable.Type3->PowerSupplyState = ChassisStateSafe;
  NewSmbiosTable.Type3->ThermalState = ChassisStateOther;
  NewSmbiosTable.Type3->SecurityStatus = ChassisSecurityStatusOther; //ChassisSecurityStatusNone;
  NewSmbiosTable.Type3->NumberofPowerCords = 1;
  NewSmbiosTable.Type3->ContainedElementCount = 0;
  NewSmbiosTable.Type3->ContainedElementRecordLength = 0;

  //Once = TRUE;

  UniquifySmbiosTableStr (NewSmbiosTable, SMBIOS_TABLE_TYPE3_STR_IDX);

  if (gSettings.ChassisType != 0) {
    NewSmbiosTable.Type3->Type = gSettings.ChassisType;
  }

  if (AsciiTrimStrLen (gSettings.ChassisManufacturer, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type3->Manufacturer, gSettings.ChassisManufacturer);
  }
  //SIC! According to iMac there must be the BoardNumber
  if (AsciiTrimStrLen (gSettings.BoardNumber, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type3->Version, gSettings.BoardNumber);
  }
  if (AsciiTrimStrLen (gSettings.SerialNr, 64) > 0) {
    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type3->SerialNumber, gSettings.SerialNr);
  }
  //if (AsciiTrimStrLen (gSettings.ChassisAssetTag, 64) > 0) {
    //UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type3->AssetTag, gSettings.ChassisAssetTag);
    NewSmbiosTable.Type3->AssetTag = 0;
    //NewSmbiosTable.Type3->SKUNumber = 0;
  //}

  Handle = LogSmbiosTable (NewSmbiosTable);
}

VOID
GetTableType4 () {
  // Processor Information
  //
  UINTN    Size = 0, Res = 0;

  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 4 (Processor Information) not found!\n");
    return;
  }

  Res = (SmbiosTable.Type4->ExternalClock * 3 + 2) % 100;
  Res = (Res > 2) ? 0 : SmbiosTable.Type4->ExternalClock % 10;

  gCPUStructure.ExternalClock = (UINT32)((SmbiosTable.Type4->ExternalClock * 1000) + (Res * 110));//MHz->kHz
  //UnicodeSPrint (gSettings.BusSpeed, 10, L"%d", gCPUStructure.ExternalClock);
  //gSettings.BusSpeed = gCPUStructure.ExternalClock; //why duplicate??
  gCPUStructure.CurrentSpeed = SmbiosTable.Type4->CurrentSpeed;
  gCPUStructure.MaxSpeed = SmbiosTable.Type4->MaxSpeed;

  gSettings.EnabledCores = (Size > 0x23) ? SmbiosTable.Type4->EnabledCoreCount : 0;

  //UnicodeSPrint (gSettings.CpuFreqMHz, 10, L"%d", gCPUStructure.CurrentSpeed);
  //gSettings.CpuFreqMHz = gCPUStructure.CurrentSpeed;
}

VOID
PatchTableType4 () {
  // Processor Information
  //
  UINTN     AddBrand = 0, CpuNumber, //Note. iMac11,2 has four tables for CPU i3
            Size, NewSize, TableSize;
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

    AddBrand = (SmbiosTable.Type4->ProcessorVersion == 0) ? 48 : 0; //if no BrandString we can add

    NewSize = sizeof (SMBIOS_TABLE_TYPE4);
    ZeroMem ((VOID *)NewSmbiosTable.Type4, MAX_TABLE_SIZE);
    CopyMem ((VOID *)NewSmbiosTable.Type4, (VOID *)SmbiosTable.Type4, Size); //copy main table
    CopyMem ((CHAR8 *)NewSmbiosTable.Type4 + NewSize, (CHAR8 *)SmbiosTable.Type4 + Size, TableSize - Size); //copy strings
    NewSmbiosTable.Type4->Hdr.Length = (UINT8)NewSize;

    NewSmbiosTable.Type4->MaxSpeed = (UINT16)gCPUStructure.MaxSpeed;
    //old version has no such fields. Fill now
    if (Size <= 0x20) {
      //sanity check and clear
      NewSmbiosTable.Type4->SerialNumber = 0;
      //NewSmbiosTable.Type4->AssetTag = 0;
      //NewSmbiosTable.Type4->PartNumber = 0;
    }

    if (Size <= 0x23) {  //Smbios <=2.3
      NewSmbiosTable.Type4->CoreCount = gCPUStructure.Cores;
      NewSmbiosTable.Type4->ThreadCount = gCPUStructure.Threads;
      NewSmbiosTable.Type4->ProcessorCharacteristics = (UINT16)gCPUStructure.Features;
    } //else we propose DMI data is better then cpuid ().
    //if (NewSmbiosTable.Type4->CoreCount < NewSmbiosTable.Type4->EnabledCoreCount) {
    //  NewSmbiosTable.Type4->EnabledCoreCount = gCPUStructure.Cores;
    //}

    NewSmbiosTable.Type4->EnabledCoreCount = gSettings.EnabledCores;
    //some verifications
    if (
      (NewSmbiosTable.Type4->ThreadCount < NewSmbiosTable.Type4->CoreCount) ||
      (NewSmbiosTable.Type4->ThreadCount > NewSmbiosTable.Type4->CoreCount * 2)
    ) {
      NewSmbiosTable.Type4->ThreadCount = gCPUStructure.Threads;
    }

    UniquifySmbiosTableStr (NewSmbiosTable, SMBIOS_TABLE_TYPE4_STR_IDX);

    // TODO: Set SmbiosTable.Type4->ProcessorFamily for all implemented CPU models
    //Once = TRUE;

    if (gCPUStructure.Model >= CPUID_MODEL_SANDYBRIDGE) {
      if (AsciiStrStr (gCPUStructure.BrandString, "i3")) {
        NewSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCoreI3;
      }

      if (AsciiStrStr (gCPUStructure.BrandString, "i5")) {
        NewSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCoreI5;
      }

      if (AsciiStrStr (gCPUStructure.BrandString, "i7")) {
        NewSmbiosTable.Type4->ProcessorFamily = ProcessorFamilyIntelCoreI7;
      }
    }

    // Set CPU Attributes
    NewSmbiosTable.Type4->L1CacheHandle = L1;
    NewSmbiosTable.Type4->L2CacheHandle = L2;
    NewSmbiosTable.Type4->L3CacheHandle = L3;
    NewSmbiosTable.Type4->ProcessorType = CentralProcessor;
    NewSmbiosTable.Type4->ProcessorId.Signature.ProcessorSteppingId = gCPUStructure.Stepping;
    NewSmbiosTable.Type4->ProcessorId.Signature.ProcessorModel      = (gCPUStructure.Model & 0xF);
    NewSmbiosTable.Type4->ProcessorId.Signature.ProcessorFamily     = gCPUStructure.Family;
    NewSmbiosTable.Type4->ProcessorId.Signature.ProcessorType       = gCPUStructure.Type;
    NewSmbiosTable.Type4->ProcessorId.Signature.ProcessorXModel     = gCPUStructure.Extmodel;
    NewSmbiosTable.Type4->ProcessorId.Signature.ProcessorXFamily    = gCPUStructure.Extfamily;

    //CopyMem ((VOID *)&NewSmbiosTable.Type4->ProcessorId.FeatureFlags, (VOID *)&gCPUStructure.Features, 4);
    //NewSmbiosTable.Type4->ProcessorId.FeatureFlags = (PROCESSOR_FEATURE_FLAGS)(UINT32)gCPUStructure.Features;

    if (Size <= 0x26) {
      NewSmbiosTable.Type4->ProcessorFamily2 = NewSmbiosTable.Type4->ProcessorFamily;
      ProcChar |= (gCPUStructure.ExtFeatures & CPUID_EXTFEATURE_EM64T) ? 0x04 : 0;
      ProcChar |= (gCPUStructure.Cores > 1) ? 0x08 : 0;
      ProcChar |= (gCPUStructure.Cores < gCPUStructure.Threads) ? 0x10 : 0;
      ProcChar |= (gCPUStructure.ExtFeatures & CPUID_EXTFEATURE_XD) ? 0x20 : 0;
      ProcChar |= (gCPUStructure.Features & CPUID_FEATURE_VMX) ? 0x40 : 0;
      ProcChar |= (gCPUStructure.Features & CPUID_FEATURE_EST) ? 0x80 : 0;
      NewSmbiosTable.Type4->ProcessorCharacteristics = ProcChar;
    }

    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type4->Socket, SocketDesignationMac);

    if (AddBrand) {
      UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type4->ProcessorVersion, BrandStr);
    }

    //UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type4->AssetTag, BrandStr); //like mac

    NewSmbiosTable.Type4->AssetTag = 0;
    NewSmbiosTable.Type4->PartNumber = 0;

    // looks to be MicroCode revision
    if (gCPUStructure.MicroCode > 0) {
      AsciiSPrint (BrandStr, 20, "%X", gCPUStructure.MicroCode);
      UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type4->SerialNumber, BrandStr);
    }

    Handle = LogSmbiosTable (NewSmbiosTable);
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
  BOOLEAN   CorrectSD = FALSE;
  UINTN     Index, TableSize;

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
    ZeroMem ((VOID *)NewSmbiosTable.Type7, MAX_TABLE_SIZE);
    CopyMem ((VOID *)NewSmbiosTable.Type7, (VOID *)SmbiosTable.Type7, TableSize);
    CorrectSD = (NewSmbiosTable.Type7->SocketDesignation == 0);
    CoreCache = NewSmbiosTable.Type7->CacheConfiguration & 3;
    //Once = TRUE;

    SSocketD = "L1-Cache";
    if (CorrectSD) {
      SSocketD[1] = (CHAR8)(0x31 + CoreCache);
      UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type7->SocketDesignation, SSocketD);
    }

    Handle = LogSmbiosTable (NewSmbiosTable);
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
  UINTN   Index, Count = ARRAY_SIZE (SlotDevices);
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

  for (Index = 0; Index < Count; Index++) {
    if (SlotDevices[Index].Valid) {
      INTN    Dev, Func;

      ZeroMem ((VOID *)NewSmbiosTable.Type9, MAX_TABLE_SIZE);
      NewSmbiosTable.Type9->Hdr.Type = EFI_SMBIOS_TYPE_SYSTEM_SLOTS;
      NewSmbiosTable.Type9->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE9);
      NewSmbiosTable.Type9->Hdr.Handle = (UINT16)(hTable9 + Index);
      NewSmbiosTable.Type9->SlotDesignation = 1;
      NewSmbiosTable.Type9->SlotType = SlotDevices[Index].SlotType;
      NewSmbiosTable.Type9->SlotDataBusWidth = SlotDataBusWidth1X;
      NewSmbiosTable.Type9->CurrentUsage = SlotUsageAvailable;
      NewSmbiosTable.Type9->SlotLength = SlotLengthShort;
      NewSmbiosTable.Type9->SlotID = SlotDevices[Index].SlotID;
      NewSmbiosTable.Type9->SlotCharacteristics1.Provides33Volts = 1;
      NewSmbiosTable.Type9->SlotCharacteristics2.HotPlugDevicesSupported = 1;
      // take this from PCI bus for WiFi card
      NewSmbiosTable.Type9->SegmentGroupNum = SlotDevices[Index].SegmentGroupNum;
      NewSmbiosTable.Type9->BusNum = SlotDevices[Index].BusNum;
      NewSmbiosTable.Type9->DevFuncNum = SlotDevices[Index].DevFuncNum;
      //
      Dev = SlotDevices[Index].DevFuncNum >> 3;
      Func = SlotDevices[Index].DevFuncNum & 7;
      //DBG ("insert table 9 for dev %x:%x\n", Dev, Func);
      UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type9->SlotDesignation, SlotDevices[Index].SlotName);
      LogSmbiosTable (NewSmbiosTable);
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
  ZeroMem ((VOID *)NewSmbiosTable.Type11, MAX_TABLE_SIZE);
  //CopyMem ((VOID *)NewSmbiosTable.Type11, (VOID *)SmbiosTable.Type11, 5); //minimum, other bytes = 0
  NewSmbiosTable.Type11->Hdr.Type = EFI_SMBIOS_TYPE_OEM_STRINGS;
  NewSmbiosTable.Type11->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 2;
  NewSmbiosTable.Type11->Hdr.Handle = hTable11; //common rule

  NewSmbiosTable.Type11->StringCount = 1;
  UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type11->StringCount, OEMString);

  LogSmbiosTable (NewSmbiosTable);
}
#endif

VOID
PatchTableTypeSome () {
  //some unused but interesting tables. Just log as is
  UINT8   TableTypes[] = { 8, 10, 13, 18, 21, 22, 27, 28, 32, 33, 129, 217, 219 };
  UINTN   Index, IndexType, Count = ARRAY_SIZE (TableTypes);

  // Different types
  for (IndexType = 0; IndexType < Count; IndexType++) {
    for (Index = 0; Index < 32; Index++) {
      SmbiosTable = GetSmbiosTableFromType (EntryPoint, TableTypes[IndexType], Index);
      if (SmbiosTable.Raw != NULL) {
        LogSmbiosTable (SmbiosTable);
      }
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

  UINTN   TableSize;

  // Get Table Type16 and set Device Count
  SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, 0);
  if (SmbiosTable.Raw == NULL) {
    DBG ("SmbiosTable: Type 16 (Physical Memory Array) not found!\n");
    return;
  }

  TableSize = SmbiosTableLength (SmbiosTable);
  ZeroMem ((VOID *)NewSmbiosTable.Type16, MAX_TABLE_SIZE);
  CopyMem ((VOID *)NewSmbiosTable.Type16, (VOID *)SmbiosTable.Type16, TableSize);
  NewSmbiosTable.Type16->Hdr.Handle = hTable16;
  // Slice - I am not sure I want these values
  // NewSmbiosTable.Type16->Location = MemoryArrayLocationProprietaryAddonCard;
  // NewSmbiosTable.Type16->Use = MemoryArrayUseSystemMemory;
  // NewSmbiosTable.Type16->MemoryErrorCorrection = MemoryErrorCorrectionMultiBitEcc;
  // MemoryErrorInformationHandle
  NewSmbiosTable.Type16->MemoryErrorInformationHandle = 0xFFFF;
  NewSmbiosTable.Type16->NumberOfMemoryDevices = gRAMCount;
  DBG ("NumberOfMemoryDevices = %d\n", gRAMCount);
  LogSmbiosTable (NewSmbiosTable);
}

VOID
GetTableType17 () {
  // Memory Device
  //
  UINTN     Index, Index2;
  BOOLEAN   Found;

  // Get Table Type17 and count Size
  gRAMCount = 0;
  for (Index = 0; Index < TotalCount; Index++) {  //how many tables there may be?
    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_DEVICE, Index);
    if (SmbiosTable.Raw == NULL) {
      //DBG ("SmbiosTable: Type 17 (Memory Device number %d) not found!\n", Index);
      continue;
    }

    DBG ("Type 17 Index = %d\n", Index);

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
        NewSmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_32BIT_MEMORY_ERROR_INFORMATION, Index2);
        if (NewSmbiosTable.Raw == NULL) {
          continue;
        }

        if (NewSmbiosTable.Type18->Hdr.Handle == SmbiosTable.Type17->MemoryErrorInformationHandle) {
          Found = TRUE;
          DBG (" - Found memory information in table 18/%d, type=0x%x, operation=0x%x syndrome=0x%x",
              Index2,
              NewSmbiosTable.Type18->ErrorType,
              NewSmbiosTable.Type18->ErrorOperation,
              NewSmbiosTable.Type18->VendorSyndrome);

          switch (NewSmbiosTable.Type18->ErrorType) {
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
        if (
          (NewSmbiosTable.Type18->ErrorType != MemoryErrorOk) &&
          (NewSmbiosTable.Type18->ErrorType != MemoryErrorCorrected)
        ) {
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
      DBG (" - Ignoring insane frequency value %dMHz\n", SmbiosTable.Type17->Speed);
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
  CHAR8     DeviceLocator[10], BankLocator[10];
  UINT8     ChannelMap[MAX_RAM_SLOTS], ExpectedCount = 0, Channels = 2;
  UINTN     Index;
  BOOLEAN   InsertingEmpty = TRUE, TrustSMBIOS = ((gRAM.SPDInUse == 0) || gSettings.TrustSMBIOS),
            WrongSMBIOSBanks = FALSE, IsMacPro = (AsciiStriStr (gSettings.ProductName, "MacPro") != NULL);

  // Inject user memory tables
  if (gSettings.InjectMemoryTables) {
    DBG ("Injecting user memory modules to SMBIOS\n");
    if (gRAM.UserInUse == 0) {
      DBG ("User SMBIOS contains no memory modules\n");
      return;
    }

    // Check Channels
    if ((gRAM.UserChannels == 0) || (gRAM.UserChannels > 4)) {
       gRAM.UserChannels = 1;
    }

    if (gRAM.UserInUse >= MAX_RAM_SLOTS) {
      gRAM.UserInUse = MAX_RAM_SLOTS;
    }

    DBG ("Channels: %d\n", gRAM.UserChannels);

    // Setup interleaved channel map
    if (Channels >= 2) {
       UINT8  DoubleChannels = (UINT8)gRAM.UserChannels << 1;

       for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
          ChannelMap[Index] = (UINT8)(((Index / DoubleChannels) * DoubleChannels) +
                                      ((Index / gRAM.UserChannels) % 2) + ((Index % gRAM.UserChannels) << 1));
       }
    } else {
      for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
        ChannelMap[Index] = (UINT8)Index;
      }
    }

    DBG ("Interleave:");

    for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
      DBG (" %d", ChannelMap[Index]);
    }

    DBG ("\n");

    // Memory Device
    //

    gRAMCount = 0;

    // Inject tables

    for (Index = 0; Index < gRAM.UserInUse; Index++) {
      UINTN   UserIndex = ChannelMap[Index];
      UINT8   Bank = (UINT8)Index / gRAM.UserChannels;

      ZeroMem ((VOID *)NewSmbiosTable.Type17, MAX_TABLE_SIZE);
      NewSmbiosTable.Type17->Hdr.Type = EFI_SMBIOS_TYPE_MEMORY_DEVICE;
      NewSmbiosTable.Type17->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE17);
      NewSmbiosTable.Type17->TotalWidth = 0xFFFF;
      NewSmbiosTable.Type17->DataWidth = 0xFFFF;
      NewSmbiosTable.Type17->Hdr.Handle = (UINT16)(hTable17 + UserIndex);
      NewSmbiosTable.Type17->FormFactor = gMobile ? MemoryFormFactorSodimm : MemoryFormFactorDimm;
      NewSmbiosTable.Type17->TypeDetail.Synchronous = TRUE;
      NewSmbiosTable.Type17->DeviceSet = Bank + 1;
      NewSmbiosTable.Type17->MemoryArrayHandle = hTable16;

      if (IsMacPro) {
        AsciiSPrint (DeviceLocator, 10, "DIMM%d", gRAMCount + 1);
      } else {
        AsciiSPrint (DeviceLocator, 10, "DIMM%d", Bank);
        AsciiSPrint (BankLocator, 10, "BANK%d", Index % Channels);
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->BankLocator, (CHAR8 *)&BankLocator[0]);
      }

      UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->DeviceLocator, (CHAR8 *)&DeviceLocator[0]);

      if ((gRAM.User[UserIndex].InUse) && (gRAM.User[UserIndex].ModuleSize > 0)) {
        if (AsciiTrimStrLen (gRAM.User[UserIndex].Vendor, 64) > 0) {
          UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->Manufacturer, gRAM.User[UserIndex].Vendor);
        } else {
          UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->Manufacturer, STR_A_UNKNOWN);
        }

        if (AsciiTrimStrLen (gRAM.User[UserIndex].SerialNo, 64) > 0) {
          UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->SerialNumber, gRAM.User[UserIndex].SerialNo);
        } else {
          UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->SerialNumber, STR_A_UNKNOWN);
        }

        if (AsciiTrimStrLen (gRAM.User[UserIndex].PartNo, 64) > 0) {
          UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->PartNumber, gRAM.User[UserIndex].PartNo);
        } else {
          UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->PartNumber, STR_A_UNKNOWN);
        }

        NewSmbiosTable.Type17->Speed = (UINT16)gRAM.User[UserIndex].Frequency;
        NewSmbiosTable.Type17->Size = (UINT16)gRAM.User[UserIndex].ModuleSize;
        NewSmbiosTable.Type17->MemoryType = gRAM.User[UserIndex].Type;

        if (
          (NewSmbiosTable.Type17->MemoryType != MemoryTypeDdr2) &&
          (NewSmbiosTable.Type17->MemoryType != MemoryTypeDdr4) &&
          (NewSmbiosTable.Type17->MemoryType != MemoryTypeDdr)
        ) {
          NewSmbiosTable.Type17->MemoryType = MemoryTypeDdr3;
        }

        DBG ("%a %a %dMHz %dMB\n", BankLocator, DeviceLocator, NewSmbiosTable.Type17->Speed, NewSmbiosTable.Type17->Size);

        mTotalSystemMemory += NewSmbiosTable.Type17->Size; //Mb
        mMemory17[gRAMCount] = (UINT16)mTotalSystemMemory;
        //DBG ("mTotalSystemMemory = %d\n", mTotalSystemMemory);
      } else {
        DBG ("%a %a EMPTY\n", BankLocator, DeviceLocator);
      }

      NewSmbiosTable.Type17->MemoryErrorInformationHandle = 0xFFFF;
      gSmHandle17[gRAMCount++] = LogSmbiosTable (NewSmbiosTable);
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
  if (TrustSMBIOS) {
    if (gRAM.SMBIOSInUse != 0) {
      if (gRAM.SPDInUse != 0) {
        if (gRAM.SPDInUse != gRAM.SMBIOSInUse) {
          // Prefer the SPD information
          if (gRAM.SPDInUse > gRAM.SMBIOSInUse) {
            DBG ("Not trusting SMBIOS because SPD reports more modules...\n");
            TrustSMBIOS = FALSE;
          } else if (gRAM.SPD[0].InUse || !gRAM.SMBIOS[0].InUse) {
            if (gRAM.SPDInUse > 1) {
              DBG ("Not trusting SMBIOS because SPD reports different modules...\n");
              TrustSMBIOS = FALSE;
            } else if (gRAM.SMBIOSInUse == 1) {
              Channels = 1;
            }
          } else if (gRAM.SPDInUse == 1) {
            // The SMBIOS may contain table for built-in module
            if (gRAM.SMBIOSInUse <= 2) {
              if (
                !gRAM.SMBIOS[0].InUse || !gRAM.SPD[2].InUse ||
                (gRAM.SMBIOS[0].Frequency != gRAM.SPD[2].Frequency) ||
                (gRAM.SMBIOS[0].ModuleSize != gRAM.SPD[2].ModuleSize)
              ) {
                Channels = 1;
              }
            } else {
              DBG ("Not trusting SMBIOS because SPD reports only one module...\n");
              TrustSMBIOS = FALSE;
            }
          } else {
            DBG ("Not trusting SMBIOS because SPD reports less modules...\n");
            TrustSMBIOS = FALSE;
          }
        } else if (gRAM.SPD[0].InUse != gRAM.SMBIOS[0].InUse) {
          // Never trust a sneaky SMBIOS!
          DBG ("Not trusting SMBIOS because it's being sneaky...\n");
          TrustSMBIOS = FALSE;
        }
      } else if (gRAM.SMBIOSInUse == 1) {
        Channels = 1;
      }
    }
  }

  if (TrustSMBIOS) {
    DBG ("Trusting SMBIOS...\n");
  }

  // Determine expected slot count
  ExpectedCount = (gRAM.UserInUse != 0) ? gRAM.UserInUse : gRAM.SPDInUse;

  if (TrustSMBIOS) {
    // Use the smbios in use count
    if (ExpectedCount < gRAM.SMBIOSInUse) {
      ExpectedCount = gRAM.SMBIOSInUse;
    }

    // Check if smbios has a good total count
    if (
      (!gMobile || (TotalCount == 2)) &&
      (ExpectedCount < TotalCount)
    ) {
      ExpectedCount = (UINT8)TotalCount;
    }
  } else {
    // Use default value of two for mobile or four for desktop
    if (gMobile) {
      if (ExpectedCount < 2) {
        ExpectedCount = 2;
      }
    } else if (ExpectedCount < 4) {
      ExpectedCount = 4;
    }
  }

  // Check for interleaved Channels
  if (Channels >= 2) {
     WrongSMBIOSBanks = (
                          (gRAM.SMBIOS[1].InUse != gRAM.SPD[1].InUse) ||
                          (gRAM.SMBIOS[1].ModuleSize != gRAM.SPD[1].ModuleSize)
                        );
  }

  if (WrongSMBIOSBanks) {
    DBG ("Detected alternating SMBIOS channel Banks\n");
  }

  // Determine if using triple or quadruple channel
  if (gRAM.UserChannels != 0) {
    Channels = gRAM.UserChannels;
  } else if (gRAM.SPDInUse == 0) {
    if (TrustSMBIOS) {
      if ((gRAM.SMBIOSInUse % 4) == 0) {
        // Quadruple channel
        if (
          (
            WrongSMBIOSBanks &&
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
          Channels = 4;
        }
      } else if ((gRAM.SMBIOSInUse % 3) == 0) {
        // Triple channel
        if (
          (
            WrongSMBIOSBanks &&
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
          Channels = 3;
        }
      } else if (!WrongSMBIOSBanks && ((gRAM.SMBIOSInUse % 2) != 0)) {
         Channels = 1;
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
      Channels = 4;
    }
  } else if ((gRAM.SPDInUse % 3) == 0) {
    // Triple channel
    if (
      (gRAM.SPD[0].InUse == gRAM.SPD[2].InUse) &&
      (gRAM.SPD[0].InUse == gRAM.SPD[4].InUse) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[2].ModuleSize) &&
      (gRAM.SPD[0].ModuleSize == gRAM.SPD[4].ModuleSize)
    ) {
      Channels = 3;
    }
  } else if (
    (gRAM.SPD[0].InUse != gRAM.SPD[2].InUse) ||
    ((gRAM.SPDInUse % 2) != 0)
  ) {
    Channels = 1;
  }

  // Can't have less than the number of Channels
  if (ExpectedCount < Channels) {
    ExpectedCount = Channels;
  }

  if (ExpectedCount > 0) {
    --ExpectedCount;
  }

  DBG ("Channels: %d\n", Channels);

  // Setup interleaved channel map
  if (Channels >= 2) {
    UINT8   DoubleChannels = (UINT8)Channels << 1;

    for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
      ChannelMap[Index] = (UINT8)(((Index / DoubleChannels) * DoubleChannels) +
                                  ((Index / Channels) % 2) + ((Index % Channels) << 1));
    }
  } else {
    for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
      ChannelMap[Index] = (UINT8)Index;
    }
  }

  DBG ("Interleave:");

  for (Index = 0; Index < MAX_RAM_SLOTS; ++Index) {
    DBG (" %d", ChannelMap[Index]);
  }

  DBG ("\n");

  // Memory Device
  //
  gRAMCount = 0;
  for (Index = 0; Index < TotalCount; Index++) {
    UINTN   SMBIOSIndex = WrongSMBIOSBanks ? Index : ChannelMap[Index],
            SPDIndex = ChannelMap[Index], TableSize;
    UINT8   Bank = (UINT8)Index / Channels;

    if (
      !InsertingEmpty && (gRAMCount > ExpectedCount) &&
      !gRAM.SPD[SPDIndex].InUse &&
      (!TrustSMBIOS || !gRAM.SMBIOS[SMBIOSIndex].InUse)
    ) {
      continue;
    }

    SmbiosTable = GetSmbiosTableFromType (EntryPoint, EFI_SMBIOS_TYPE_MEMORY_DEVICE, SMBIOSIndex);
    if (TrustSMBIOS && gRAM.SMBIOS[SMBIOSIndex].InUse && (SmbiosTable.Raw != NULL)) {
      TableSize = SmbiosTableLength (SmbiosTable);
      CopyMem ((VOID *)NewSmbiosTable.Type17, (VOID *)SmbiosTable.Type17, TableSize);
      NewSmbiosTable.Type17->AssetTag = 0;

      if (AsciiTrimStrLen (gRAM.SMBIOS[SMBIOSIndex].Vendor, 64) > 0) {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->Manufacturer, gRAM.SMBIOS[SMBIOSIndex].Vendor);
        AsciiSPrint (gSettings.MemoryManufacturer, 64, "%a", gRAM.SMBIOS[SMBIOSIndex].Vendor);
      } else {
        //NewSmbiosTable.Type17->Manufacturer = 0;
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->Manufacturer, STR_A_UNKNOWN);
      }

      if (AsciiTrimStrLen (gRAM.SMBIOS[SMBIOSIndex].SerialNo, 64) > 0) {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->SerialNumber, gRAM.SMBIOS[SMBIOSIndex].SerialNo);
        AsciiSPrint (gSettings.MemorySerialNumber, 64, "%a", gRAM.SMBIOS[SMBIOSIndex].SerialNo);
      } else {
        //NewSmbiosTable.Type17->SerialNumber = 0;
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->SerialNumber, STR_A_UNKNOWN);
      }

      if (AsciiTrimStrLen (gRAM.SMBIOS[SMBIOSIndex].PartNo, 64) > 0) {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->PartNumber, gRAM.SMBIOS[SMBIOSIndex].PartNo);
        AsciiSPrint (gSettings.MemoryPartNumber, 64, "%a", gRAM.SMBIOS[SMBIOSIndex].PartNo);
        DBG (" partNum=%a\n", gRAM.SMBIOS[SMBIOSIndex].PartNo);
      } else {
        //NewSmbiosTable.Type17->PartNumber = 0;
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->PartNumber,  STR_A_UNKNOWN);
        DBG (" partNum unknown\n");
      }
    } else {
      ZeroMem ((VOID *)NewSmbiosTable.Type17, MAX_TABLE_SIZE);
      NewSmbiosTable.Type17->Hdr.Type = EFI_SMBIOS_TYPE_MEMORY_DEVICE;
      NewSmbiosTable.Type17->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE17);
      NewSmbiosTable.Type17->TotalWidth = 0xFFFF;
      NewSmbiosTable.Type17->DataWidth = 0xFFFF;
    }

    //Once = TRUE;

    NewSmbiosTable.Type17->Hdr.Handle = (UINT16)(hTable17 + Index);
    NewSmbiosTable.Type17->FormFactor = gMobile ? MemoryFormFactorSodimm : MemoryFormFactorDimm;
    NewSmbiosTable.Type17->TypeDetail.Synchronous = TRUE;
    NewSmbiosTable.Type17->DeviceSet = Bank + 1;
    NewSmbiosTable.Type17->MemoryArrayHandle = hTable16;

    if (gRAM.SPD[SPDIndex].InUse) {
      if (AsciiTrimStrLen (gRAM.SPD[SPDIndex].Vendor, 64) > 0) {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->Manufacturer, gRAM.SPD[SPDIndex].Vendor);
        AsciiSPrint (gSettings.MemoryManufacturer, 64, "%a", gRAM.SPD[SPDIndex].Vendor);
      } else {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->Manufacturer, STR_A_UNKNOWN);
      }

      if (AsciiTrimStrLen (gRAM.SPD[SPDIndex].SerialNo, 64) > 0) {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->SerialNumber, gRAM.SPD[SPDIndex].SerialNo);
        AsciiSPrint (gSettings.MemorySerialNumber, 64, "%a", gRAM.SPD[SPDIndex].SerialNo);
      } else {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->SerialNumber, STR_A_UNKNOWN);
      }

      if (AsciiTrimStrLen (gRAM.SPD[SPDIndex].PartNo, 64) > 0) {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->PartNumber, gRAM.SPD[SPDIndex].PartNo);
        AsciiSPrint (gSettings.MemoryPartNumber, 64, "%a", gRAM.SPD[SPDIndex].PartNo);
      } else {
        UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->PartNumber, STR_A_UNKNOWN);
      }

      NewSmbiosTable.Type17->Speed = (UINT16)gRAM.SPD[SPDIndex].Frequency;
      NewSmbiosTable.Type17->Size = (UINT16)gRAM.SPD[SPDIndex].ModuleSize;
      NewSmbiosTable.Type17->MemoryType = gRAM.SPD[SPDIndex].Type;
    }

    if (
      TrustSMBIOS &&
      gRAM.SMBIOS[SMBIOSIndex].InUse &&
      (NewSmbiosTable.Type17->Speed < (UINT16)gRAM.SMBIOS[SMBIOSIndex].Frequency)
    ) {
      DBG ("Type17->Speed corrected by SMBIOS from %dMHz to %dMHz\n",
          NewSmbiosTable.Type17->Speed, gRAM.SMBIOS[SMBIOSIndex].Frequency);

      NewSmbiosTable.Type17->Speed = (UINT16)gRAM.SMBIOS[SMBIOSIndex].Frequency;
    }

    AsciiSPrint (gSettings.MemorySpeed, 64, "%d", NewSmbiosTable.Type17->Speed);

    // Assume DDR3 unless explicitly set to DDR2/DDR/DDR4
    if (
      (NewSmbiosTable.Type17->MemoryType != MemoryTypeDdr2) &&
      (NewSmbiosTable.Type17->MemoryType != MemoryTypeDdr4) &&
      (NewSmbiosTable.Type17->MemoryType != MemoryTypeDdr)
    ) {
      NewSmbiosTable.Type17->MemoryType = MemoryTypeDdr3;
    }

    //now I want to update DeviceLocator and BankLocator
    if (IsMacPro) {
      AsciiSPrint (DeviceLocator, 10, "DIMM%d", gRAMCount + 1);
      AsciiSPrint (BankLocator, 10, "");
    } else {
      AsciiSPrint (DeviceLocator, 10, "DIMM%d", Bank);
      AsciiSPrint (BankLocator, 10, "BANK%d", Index % Channels);
    }

    UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->DeviceLocator, (CHAR8 *)&DeviceLocator[0]);

    if (IsMacPro) {
      NewSmbiosTable.Type17->BankLocator = 0; //like in MacPro5,1
    } else {
      UpdateSmbiosString (NewSmbiosTable, &NewSmbiosTable.Type17->BankLocator, (CHAR8 *)&BankLocator[0]);
    }

    DBG ("SMBIOS Type 17 Index = %d => %d %d:", gRAMCount, SMBIOSIndex, SPDIndex);

    if (NewSmbiosTable.Type17->Size == 0) {
      DBG ("%a %a EMPTY\n", BankLocator, DeviceLocator);
      NewSmbiosTable.Type17->MemoryType = 0; //MemoryTypeUnknown;
    } else {
      InsertingEmpty = FALSE;
      DBG ("%a %a %dMHz %dMB\n", BankLocator, DeviceLocator, NewSmbiosTable.Type17->Speed, NewSmbiosTable.Type17->Size);
      mTotalSystemMemory += NewSmbiosTable.Type17->Size; //Mb
      mMemory17[gRAMCount] = (UINT16)mTotalSystemMemory;
      //DBG ("mTotalSystemMemory = %d\n", mTotalSystemMemory);
    }

    NewSmbiosTable.Type17->MemoryErrorInformationHandle = 0xFFFF;
    gSmHandle17[gRAMCount++] = LogSmbiosTable (NewSmbiosTable);
  }

  if (mTotalSystemMemory > 0) {
    DBG ("mTotalSystemMemory = %dMB\n", mTotalSystemMemory);
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
  UINTN     Index;
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
  ZeroMem ((VOID *)NewSmbiosTable.Type19, MAX_TABLE_SIZE);
  NewSmbiosTable.Type19->Hdr.Type = EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS;
  NewSmbiosTable.Type19->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE19);
  NewSmbiosTable.Type19->Hdr.Handle = hTable19;
  NewSmbiosTable.Type19->MemoryArrayHandle = hTable16;
  NewSmbiosTable.Type19->StartingAddress = 0;
  NewSmbiosTable.Type19->EndingAddress = TotalEnd;
  NewSmbiosTable.Type19->PartitionWidth = PartWidth;
  gSmHandle19 = LogSmbiosTable (NewSmbiosTable);
}

VOID
PatchTableType20 () {
  UINTN   j = 0, k = 0, m = 0, Index, TableSize;
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
    ZeroMem ((VOID *)NewSmbiosTable.Type20, MAX_TABLE_SIZE);
    CopyMem ((VOID *)NewSmbiosTable.Type20, (VOID *)SmbiosTable.Type20, TableSize);

    for (j = 0; j < TotalCount; j++) {
      //EndingAddress in kb while mMemory in Mb
      if ((UINT32)(mMemory17[j] << 10) > NewSmbiosTable.Type20->EndingAddress) {
        NewSmbiosTable.Type20->MemoryDeviceHandle = gSmHandle17[j];
        k = NewSmbiosTable.Type20->EndingAddress;
        m += mMemory17[j];

        //DBG ("Type20[%02d]->End = 0x%x, Type17[%02d] = 0x%x\n",
        //    Index, k, j, m);

        //DBG (" MemoryDeviceHandle = 0x%x\n", NewSmbiosTable.Type20->MemoryDeviceHandle);
        mMemory17[j] = 0; // used
        break;
      }
      //DBG ("\n");
    }

    NewSmbiosTable.Type20->MemoryArrayMappedAddressHandle = gSmHandle19;
    //
    // Record Smbios Type 20
    //
    LogSmbiosTable (NewSmbiosTable);
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

    ZeroMem ((VOID *)NewSmbiosTable.Type128, MAX_TABLE_SIZE);
    NewSmbiosTable.Type128->Hdr.Type = 128;
    NewSmbiosTable.Type128->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE128);
    NewSmbiosTable.Type128->Hdr.Handle = hTable128; //common rule
    NewSmbiosTable.Type128->FirmwareFeatures = gFwFeatures; //0x80001417; //imac112 -> 0x1403
    NewSmbiosTable.Type128->FirmwareFeaturesMask = gFwFeaturesMask;

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
    NewSmbiosTable.Type128->RegionCount = 1;
    NewSmbiosTable.Type128->RegionType[0] = FW_REGION_MAIN;
    //UpAddress = mTotalSystemMemory << 20; //Mb -> b
    //      gHob->MemoryAbove1MB.PhysicalStart;
    NewSmbiosTable.Type128->FlashMap[0].StartAddress = 0xFFE00000; //0xF0000;
    //      gHob->MemoryAbove1MB.PhysicalStart + gHob->MemoryAbove1MB.ResourceLength - 1;
    NewSmbiosTable.Type128->FlashMap[0].EndAddress = 0xFFEFFFFF;
    //NewSmbiosTable.Type128->RegionType[1] = FW_REGION_NVRAM; //Efivar
    //NewSmbiosTable.Type128->FlashMap[1].StartAddress = 0x15000; //0xF0000;
    //NewSmbiosTable.Type128->FlashMap[1].EndAddress = 0x1FFFF;
    //region type=1 also present in mac

  /*
    NewSmbiosTable.Type128->RegionCount = 4;
    NewSmbiosTable.Type128->RegionType[0] = FW_REGION_MAIN;
    NewSmbiosTable.Type128->FlashMap[0].StartAddress = 0xFF990000;
    NewSmbiosTable.Type128->FlashMap[0].EndAddress = 0xFFB2FFFF;
    NewSmbiosTable.Type128->RegionType[1] = FW_REGION_RESERVED;
    NewSmbiosTable.Type128->FlashMap[1].StartAddress = 0xFFB30000;
    NewSmbiosTable.Type128->FlashMap[1].EndAddress = 0xFFB5FFFF;
    NewSmbiosTable.Type128->RegionType[2] = FW_REGION_RESERVED;
    NewSmbiosTable.Type128->FlashMap[2].StartAddress = 0xFFB60000;
    NewSmbiosTable.Type128->FlashMap[2].EndAddress = 0xFFDFFFFF;
    NewSmbiosTable.Type128->RegionType[3] = FW_REGION_NVRAM;
    NewSmbiosTable.Type128->FlashMap[3].StartAddress = 0xFFE70000;
    NewSmbiosTable.Type128->FlashMap[3].EndAddress = 0xFFE9FFFF;
  */

    LogSmbiosTable (NewSmbiosTable);

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

  ZeroMem ((VOID *)NewSmbiosTable.Type131, MAX_TABLE_SIZE);
  NewSmbiosTable.Type131->Hdr.Type = 131;
  NewSmbiosTable.Type131->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 2;
  NewSmbiosTable.Type131->Hdr.Handle = hTable131; //common rule
  // Patch ProcessorType
  NewSmbiosTable.Type131->ProcessorType = gSettings.CpuType;
  Handle = LogSmbiosTable (NewSmbiosTable);
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

  ZeroMem ((VOID *)NewSmbiosTable.Type132, MAX_TABLE_SIZE);
  NewSmbiosTable.Type132->Hdr.Type = 132;
  NewSmbiosTable.Type132->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 2;
  NewSmbiosTable.Type132->Hdr.Handle = hTable132;

  // Patch ProcessorBusSpeed
  NewSmbiosTable.Type132->ProcessorBusSpeed = gSettings.QPI
    ? gSettings.QPI
    : (UINT16)(LShiftU64 (DivU64x32 (gSettings.BusSpeed, kilo), 2));

  Handle = LogSmbiosTable (NewSmbiosTable);
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

  ZeroMem ((VOID *)NewSmbiosTable.Type133, MAX_TABLE_SIZE);
  NewSmbiosTable.Type133->Hdr.Type = 133;
  NewSmbiosTable.Type133->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 8;
  NewSmbiosTable.Type133->Hdr.Handle = hTable133;
  //NewSmbiosTable.Type133->PlatformFeature = gSettings.PlatformFeature;
  CopyMem ((VOID *)&NewSmbiosTable.Type133->PlatformFeature, (VOID *)&gSettings.PlatformFeature, 8);
  Handle = LogSmbiosTable (NewSmbiosTable);
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

  ZeroMem ((VOID *)NewSmbiosTable.Type134, MAX_TABLE_SIZE);
  NewSmbiosTable.Type134->Hdr.Type = 134;
  NewSmbiosTable.Type134->Hdr.Length = sizeof (SMBIOS_STRUCTURE) + 16;
  NewSmbiosTable.Type134->Hdr.Handle = hTable134;
  REV = Rev2HexStr (gSettings.REV, sizeof (gSettings.REV));
  CopyMem ((VOID *)&NewSmbiosTable.Type134->SMCVersion, (VOID *)REV, AsciiStrSize (REV));
  Handle = LogSmbiosTable (NewSmbiosTable);
}

EFI_STATUS
PrePatchSmbios () {
  EFI_STATUS              Status = EFI_SUCCESS;
  UINTN                   BufferLen;
  EFI_PHYSICAL_ADDRESS    BufferPtr;

  DbgHeader ("GetSmbios");

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

  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiACPIMemoryNVS, /* EfiACPIReclaimMemory, */
                  EFI_SIZE_TO_PAGES (BufferLen),
                  &BufferPtr
                );

  if (EFI_ERROR (Status)) {
    //DBG ("There is error allocating pages in EfiACPIMemoryNVS!\n");
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    /*EfiACPIMemoryNVS, */ EfiACPIReclaimMemory,
                    ROUND_PAGE (BufferLen) / EFI_PAGE_SIZE,
                    &BufferPtr
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

  NewSmbiosTable.Raw = (UINT8 *)AllocateZeroPool (MAX_TABLE_SIZE);
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

  FreePool ((VOID *)NewSmbiosTable.Raw);

  // there is no need to keep all tables in numeric order. It is not needed
  // neither by specs nor by AppleSmbios.kext
}

VOID
FinalizeSmbios () {
  EFI_PEI_HOB_POINTERS    GuidHob, HobStart;
  EFI_PHYSICAL_ADDRESS    *Table = NULL;
  UINTN                   Index, Size;

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
