/**
initial concept of DSDT patching by mackerintel

Re-Work by Slice 2011.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <Library/Platform/AmlGenerator.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_ACPI_PATCH
#define DEBUG_ACPI_PATCH -1
#endif
#else
#ifdef DEBUG_ACPI_PATCH
#undef DEBUG_ACPI_PATCH
#endif
#define DEBUG_ACPI_PATCH DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_ACPI_PATCH, __VA_ARGS__)

#define XXXX_SIGN     SIGNATURE_32 ('X','X','X','X')
#define APIC_SIGN     SIGNATURE_32 ('A','P','I','C')
#define SLIC_SIGN     SIGNATURE_32 ('S','L','I','C')

#define ACPI_NAME_SIZE                  4
#define ACPI_OEM_ID_SIZE                6
#define ACPI_OEM_TABLE_ID_SIZE          8
#define ACPI_RSDP_SIG_SIZE              8

// Global pointers
XSDT_TABLE            *Xsdt = NULL;

UINT8                 AcpiCPUCount;
CHAR8                 *AcpiCPUName[32];
CHAR8                 *AcpiCPUScore;

//-----------------------------------

UINT8 PmBlock[] = {
  /*0070: 0xA5, 0x84, 0x00, 0x00,*/ 0x01, 0x08, 0x00, 0x01, 0xF9, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /*0080:*/ 0x06, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x67, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x6F, 0xBF,
  /*0090:*/ 0x00, 0x00, 0x00, 0x00, 0x01, 0x20, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /*00A0:*/ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x02,
  /*00B0:*/ 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /*00C0:*/ 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x50, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /*00D0:*/ 0x01, 0x20, 0x00, 0x03, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x04,
  /*00E0:*/ 0x20, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
  /*00F0:*/ 0x00, 0x00, 0x00, 0x00
};

#if 0
// Slice: Signature compare function
BOOLEAN
TableSign (
        CHAR8   *Table,
  CONST CHAR8   *Sign
) {
  INTN i;

  for (i = 0; i < 4; i++) {
    if ((Table[i] & ~0x20) != (Sign[i] & ~0x20)) {
      return FALSE;
    }
  }

  return TRUE;
}
#endif

EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *
FindAcpiRsdPtr () {
  UINTN     Address, Index;

  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (
      CompareGuid (&gST->ConfigurationTable[Index].VendorGuid, &gEfiAcpi20TableGuid) ||
      CompareGuid (&gST->ConfigurationTable[Index].VendorGuid, &gEfiAcpi10TableGuid)
    ) {
      return (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER*)gST->ConfigurationTable[Index].VendorTable;
    }
  }

  //
  // First Seach 0x0e0000 - 0x0fffff for RSD Ptr
  //
  for (Address = 0xe0000; Address < 0xfffff; Address += 0x10) {
    if (*(UINT64 *)(Address) == EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE) {
      return (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)Address;
    }
  }

  //
  // Search EBDA
  //
  Address = (*(UINT16 *)(UINTN)(EBDA_BASE_ADDRESS)) << 4;
  for (Index = 0; Index < 0x400 ; Index += 16) {
    if (*(UINT64 *)(Address + Index) == EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE) {
      return (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)Address;
    }
  }

  return NULL;
}

UINT8
Checksum8 (
  VOID      *StartPtr,
  UINT32    Len
) {
  UINT8   Value = 0, *Ptr = (UINT8 *)StartPtr;
  UINT32  i = 0;

  for (i = 0; i < Len; i++) {
    Value += *Ptr++;
  }

  return Value;
}

EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *
GetFadt () {
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER    *RsdPtr;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE       *FadtPointer = NULL;

  RsdPtr = FindAcpiRsdPtr ();

  if (RsdPtr != NULL) {
    DBG ("Found UEFI Acpi %d.0 RSDP at %p\n", RsdPtr->Revision, RsdPtr);

    if (RsdPtr->Revision > 0) {
      // xsdt priority
      Xsdt = (XSDT_TABLE *)(UINTN)(RsdPtr->XsdtAddress);

      // xsdt priority
      if (Xsdt) {
        FadtPointer = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)(UINTN)(Xsdt->Entry);
      }
    }
  }

  return FadtPointer;
}

UINTN
FixAsciiTableHeader (
  CHAR8   *Str,
  UINTN   Len,
  CHAR8   ReplaceWith
) {
  UINTN   ReplaceCount = 0;

  while (*Str && Len) {
#if 0
    *Str = 'X';
    ReplaceCount++;
#else
    if ((*Str < 0x20) || (*Str > 0x7E)) {
      *Str = ReplaceWith;
      ReplaceCount++;
    }
#endif

    Len--;
    Str++;
  }

  return ReplaceCount;
}

VOID
PrintTableInfos (
  EFI_ACPI_DESCRIPTION_HEADER   *TableEntry,
  UINT64                        *Address
) {
  CHAR8   Signature[ACPI_NAME_SIZE + 1],
          OemId[ACPI_OEM_ID_SIZE + 1],
          OemTableId[ACPI_OEM_TABLE_ID_SIZE + 1],
          CreatorId[ACPI_NAME_SIZE + 1];

  Signature[ACPI_NAME_SIZE] = 0;
  OemId[ACPI_OEM_ID_SIZE] = 0;
  OemTableId[ACPI_OEM_TABLE_ID_SIZE] = 0;
  CreatorId[ACPI_NAME_SIZE] = 0;

  CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&TableEntry->Signature, ACPI_NAME_SIZE);
  CopyMem ((CHAR8 *)&OemId, (CHAR8 *)&TableEntry->OemId, ACPI_OEM_ID_SIZE);
  CopyMem ((CHAR8 *)&OemTableId, (CHAR8 *)&TableEntry->OemTableId, ACPI_OEM_TABLE_ID_SIZE);
  CopyMem ((CHAR8 *)&CreatorId, (CHAR8 *)&TableEntry->CreatorId, ACPI_NAME_SIZE);

  FixAsciiTableHeader (Signature, ACPI_NAME_SIZE, 0x20);
  FixAsciiTableHeader (OemId, ACPI_OEM_ID_SIZE, 0x20);
  FixAsciiTableHeader (OemTableId, ACPI_OEM_TABLE_ID_SIZE, 0x20);
  FixAsciiTableHeader (CreatorId, ACPI_NAME_SIZE, 0x20);

  MsgLog (
    "%-4a: 0x%8X %06X (v%.2d '%-6a' '%-8a' %08X '%-4a' %08X)\n",
    Signature, Address,
    TableEntry->Length, TableEntry->Revision, OemId,
    OemTableId, TableEntry->OemRevision,
    CreatorId, TableEntry->CreatorRevision
  );
}

VOID
UpdateDropTables (
  EFI_ACPI_DESCRIPTION_HEADER   *TableEntry,
  UINT64                        *Address
) {
  ACPI_DROP_TABLE   *DropTable = AllocateZeroPool (sizeof (ACPI_DROP_TABLE));

  DropTable->Signature = TableEntry->Signature;
  DropTable->TableId = TableEntry->OemTableId;
  DropTable->Length = TableEntry->Length;
  DropTable->MenuItem.BValue = FALSE;
  DropTable->Next = gSettings.ACPIDropTables;
  gSettings.ACPIDropTables = DropTable;

  ACPIDropTablesNum++;

  PrintTableInfos (TableEntry, Address);
}

VOID
GetAcpiTablesList () {
  EFI_ACPI_DESCRIPTION_HEADER   *TableEntry;
  UINTN                         Index;
  UINT32                        EntryCount;
  CHAR8                         *BasePtr;
  UINT64                        Entry64;

  DbgHeader ("GetAcpiTablesList");

  GetFadt (); // this is a first call to acpi, we need it to make a pointer to Xsdt

  if (Xsdt) {
    DBG ("From XSDT:\n");

    EntryCount = (Xsdt->Header.Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT64);
    BasePtr = (CHAR8 *)(&(Xsdt->Entry));

    for (Index = 0; Index < EntryCount; Index ++, BasePtr += sizeof (UINT64)) {
      if (ReadUnaligned64 ((CONST UINT64 *)BasePtr) == 0) {
        continue;
      }

      CopyMem (&Entry64, (VOID *)BasePtr, sizeof (UINT64)); // value from BasePtr->
      TableEntry = (EFI_ACPI_DESCRIPTION_HEADER *)((UINTN)(Entry64));

      UpdateDropTables (TableEntry, &Entry64);
    }
  } else {
    DBG (" - [!] Error! ACPI not found:\n");
  }
}

VOID
DropTableFromXSDT (
  UINT32    ThisSignature,
  UINT64    ThisTableId,
  UINT32    Length
) {
  EFI_ACPI_DESCRIPTION_HEADER   *TableEntry;
  UINTN                         Index, Index2;
  UINT32                        EntryCount;
  CHAR8                         *BasePtr, *Ptr, *Ptr2,
                                Signature[ACPI_NAME_SIZE + 1],
                                OemTableId[ACPI_OEM_TABLE_ID_SIZE + 1];
  UINT64                        Entry64;
  BOOLEAN                       DoubleZero = FALSE, Drop;

  if (!Xsdt || ((ThisSignature == 0) && (ThisTableId == 0))) {
    return;
  }

  Signature[ACPI_NAME_SIZE] = 0;
  OemTableId[ACPI_OEM_TABLE_ID_SIZE] = 0;

  CopyMem ((CHAR8 *)&Signature[0], (CHAR8 *)&ThisSignature, ACPI_NAME_SIZE);
  CopyMem ((CHAR8 *)&OemTableId[0], (CHAR8 *)&ThisTableId, ACPI_OEM_TABLE_ID_SIZE);

  DBG ("Drop tables from Xsdt, SIGN=%a TableID=%a Length=%d\n", Signature, OemTableId, (INT32)Length);

  EntryCount = (Xsdt->Header.Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT64);

  //DBG (" Xsdt has tables count=%d\n", EntryCount);

  CONSTRAIN_MAX (EntryCount, 50);

  BasePtr = (CHAR8 *)(UINTN)(&(Xsdt->Entry));

  for (Index = 0; Index < EntryCount; Index++, BasePtr += sizeof (UINT64)) {
    if (ReadUnaligned64 ((CONST UINT64 *)BasePtr) == 0) {
      if (DoubleZero) {
        Xsdt->Header.Length = (UINT32)(sizeof (UINT64) * Index + sizeof (EFI_ACPI_DESCRIPTION_HEADER));
        DBG ("DoubleZero in XSDT table\n");
        break;
      }

      DBG ("First zero in XSDT table\n");

      DoubleZero = TRUE;
      Xsdt->Header.Length -= sizeof (UINT64);
      continue; // avoid zero field
    }

    DoubleZero = FALSE;
    CopyMem (&Entry64, (VOID *)BasePtr, sizeof (UINT64)); // value from BasePtr->
    TableEntry = (EFI_ACPI_DESCRIPTION_HEADER *)((UINTN)(Entry64));
    CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&TableEntry->Signature, 4);
    CopyMem ((CHAR8 *)&OemTableId, (CHAR8 *)&TableEntry->OemTableId, 8);

    Drop = ((ThisSignature && (TableEntry->Signature == ThisSignature)) &&
            (!ThisTableId  || (TableEntry->OemTableId == ThisTableId))  &&
            (!Length || (TableEntry->Length == Length)));

    if (!Drop) {
      continue;
    }

    DBG (" Table: %a  %a  %d dropped\n", Signature, OemTableId, (INT32)TableEntry->Length);
    Ptr = BasePtr;
    Ptr2 = Ptr + sizeof (UINT64);

    for (Index2 = Index; Index2 < EntryCount - 1; Index2++) {
      // *Ptr++ = *Ptr2++;
      CopyMem (Ptr, Ptr2, sizeof (UINT64));
      Ptr  += sizeof (UINT64);
      Ptr2 += sizeof (UINT64);
    }

    BasePtr -= sizeof (UINT64); // SunKi
    Xsdt->Header.Length -= sizeof (UINT64);
  }

  DBG ("Corrected XSDT length=%d\n", Xsdt->Header.Length);
}

VOID
PatchAllSSDT () {
  EFI_STATUS                      Status = EFI_SUCCESS;
  EFI_ACPI_DESCRIPTION_HEADER     *TableEntry;
  EFI_PHYSICAL_ADDRESS            Ssdt = EFI_SYSTEM_TABLE_MAX_ADDRESS;
  UINTN                           Index, FixedHeaderLen;
  UINT32                          EntryCount, SsdtLen;
  CHAR8                           *BasePtr, *Ptr,
                                  Signature[ACPI_NAME_SIZE + 1],
                                  OemTableId[ACPI_OEM_TABLE_ID_SIZE + 1];
  UINT64                          Entry64;

  Signature[ACPI_NAME_SIZE] = 0;
  OemTableId[ACPI_OEM_TABLE_ID_SIZE] = 0;

  EntryCount = (Xsdt->Header.Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT64);
  BasePtr = (CHAR8 *)(UINTN)(&(Xsdt->Entry));

  for (Index = 0; Index < EntryCount; Index++, BasePtr += sizeof (UINT64)) {
    CopyMem (&Entry64, (VOID *)BasePtr, sizeof (UINT64)); // value from BasePtr->
    TableEntry = (EFI_ACPI_DESCRIPTION_HEADER *)((UINTN)(Entry64));
    //if (TableEntry->Signature == EFI_ACPI_4_0_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
      // will patch here
      CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&TableEntry->Signature, ACPI_NAME_SIZE); // must be SSDT
      CopyMem ((CHAR8 *)&OemTableId, (CHAR8 *)&TableEntry->OemTableId, ACPI_OEM_TABLE_ID_SIZE);
      SsdtLen = TableEntry->Length;

      DBG ("Patch table: %a  %a | Len: 0x%x\n", Signature, OemTableId, SsdtLen);

      Ssdt = EFI_SYSTEM_TABLE_MAX_ADDRESS;
      Status = gBS->AllocatePages (
                      AllocateMaxAddress,
                      EfiACPIReclaimMemory,
                      EFI_SIZE_TO_PAGES (SsdtLen + 4096),
                      &Ssdt
                    );
      if (EFI_ERROR (Status)) {
        DBG (" ... not patched\n");
        continue;
      }

      FixedHeaderLen = 0;

      if (BIT_ISSET (gSettings.FixDsdt, FIX_HEADER)) {
        //FixedHeaderLen += FixAsciiTableHeader ((CHAR8 *)&TableEntry->Signature, ACPI_NAME_SIZE, 0x3F);
        FixedHeaderLen += FixAsciiTableHeader ((CHAR8 *)&TableEntry->OemId, ACPI_OEM_ID_SIZE, 0x3F);
        FixedHeaderLen += FixAsciiTableHeader ((CHAR8 *)&TableEntry->OemTableId, ACPI_OEM_TABLE_ID_SIZE, 0x3F);
        FixedHeaderLen += FixAsciiTableHeader ((CHAR8 *)&TableEntry->CreatorId, ACPI_NAME_SIZE, 0x3F);
      }

      // Only patch SSDT
      if (
        (TableEntry->Signature == EFI_ACPI_4_0_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) &&
        (gSettings.PatchDsdtNum > 0) &&
        gSettings.PatchDsdt
      ) {
        MsgLog ("Patching SSDT:\n");
        SsdtLen = PatchBinACPI ((UINT8 *)(UINTN)Ssdt, SsdtLen);
        FixedHeaderLen++;
      }

      if (!FixedHeaderLen) {
        continue;
      }

      Ptr = (CHAR8 *)(UINTN)Ssdt;
      CopyMem (Ptr, (VOID *)TableEntry, SsdtLen);

      CopyMem ((VOID *)BasePtr, &Ssdt, sizeof (UINT64));
      // Finish SSDT patch and resize SSDT Length
      CopyMem (&Ptr[4], &SsdtLen, 4);
      ((EFI_ACPI_DESCRIPTION_HEADER *)Ptr)->Checksum = 0;
      ((EFI_ACPI_DESCRIPTION_HEADER *)Ptr)->Checksum = (UINT8)(256 - Checksum8 (Ptr, SsdtLen));
    //}
  }
}

EFI_STATUS
InsertTable (
  VOID      *TableEntry,
  UINTN     Length
) {
  EFI_STATUS              Status = EFI_NOT_FOUND;
  EFI_PHYSICAL_ADDRESS    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;

  if (!TableEntry || !Xsdt) {
    return Status;
  }

  Status = gBS->AllocatePages (
                   AllocateMaxAddress,
                   EfiACPIReclaimMemory,
                   EFI_SIZE_TO_PAGES (Length),
                   &BufferPtr
                 );

  // if success insert table pointer into ACPI tables
  if (!EFI_ERROR (Status)) {
    //DBG ("page is allocated, write SSDT into\n");
    CopyMem ((VOID *)(UINTN)BufferPtr, (VOID *)TableEntry, Length);

    // insert into XSDT
    if (Xsdt) {
      UINT64  *XPtr = (UINT64 *)((UINTN)Xsdt + Xsdt->Header.Length);

      WriteUnaligned64 (XPtr, (UINT64)BufferPtr);
      Xsdt->Header.Length += sizeof (UINT64);
      //DBG ("Xsdt->Length = %d\n", Xsdt->Header.Length);
    }
  }

  return Status;
}

#if DUMP_TABLE

/** Saves Buffer of Length to disk as DirName\\FileName. */
EFI_STATUS
SaveBufferToDisk (
  VOID      *Buffer,
  UINTN     Length,
  CHAR16    *DirName,
  CHAR16    *FileName
) {
  EFI_STATUS    Status;

  if ((DirName == NULL) || (FileName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  FileName = PoolPrint (L"%s\\%s", DirName, FileName);
  if (FileName == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = SaveFile (SelfRootDir, FileName, Buffer, Length);
  if (EFI_ERROR (Status)) {
    Status = SaveFile (NULL, FileName, Buffer, Length);
  }

  FreePool (FileName);

  return Status;
}

//
// Remembering saved tables
//
#define SAVED_TABLES_ALLOC_ENTRIES  64

VOID    **mSavedTables = NULL;
UINTN   mSavedTablesEntries = 0;
UINTN   mSavedTablesNum = 0;

/** Returns TRUE is TableEntry is already saved. */
BOOLEAN
IsTableSaved (
  VOID    *TableEntry
) {
  UINTN   Index;

  if (mSavedTables != NULL) {
    for (Index = 0; Index < mSavedTablesNum; Index++) {
      if (mSavedTables[Index] == TableEntry) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/** Adds TableEntry to mSavedTables if not already there. */
VOID
MarkTableAsSaved (
  VOID    *TableEntry
) {
  //
  // If mSavedTables does not exists yet - allocate it
  //
  if (mSavedTables == NULL) {
    //DBG (" Allocaing mSavedTables");
    mSavedTablesEntries = SAVED_TABLES_ALLOC_ENTRIES;
    mSavedTablesNum = 0;
    mSavedTables = AllocateZeroPool (sizeof (*mSavedTables) * mSavedTablesEntries);

    if (mSavedTables == NULL) {
      return;
    }
  }

  //
  // If TableEntry is not in mSavedTables - add it
  //
  //DBG (" MarkTableAsSaved %p", TableEntry);
  if (IsTableSaved (TableEntry)) {
    // already saved
    //DBG (" - already saved\n");
    return;
  }

  //
  // If mSavedTables is full - extend it
  //
  if ((mSavedTablesNum + 1) >= mSavedTablesEntries) {
    // not enough space
    //DBG (" - extending mSavedTables from %d", mSavedTablesEntries);
    mSavedTables = ReallocatePool (
                      sizeof (*mSavedTables) * mSavedTablesEntries,
                      sizeof (*mSavedTables) * (mSavedTablesEntries + SAVED_TABLES_ALLOC_ENTRIES),
                      mSavedTables
                    );

    if (mSavedTables == NULL) {
      return;
    }

    mSavedTablesEntries = mSavedTablesEntries + SAVED_TABLES_ALLOC_ENTRIES;
    //DBG (" to %d", mSavedTablesEntries);
  }

  //
  // Add TableEntry to mSavedTables
  //
  mSavedTables[mSavedTablesNum] = (VOID *)TableEntry;
  //DBG (" - added to index %d\n", mSavedTablesNum);
  mSavedTablesNum++;
}

#define AML_OP_NAME    0x08
#define AML_OP_PACKAGE 0x12

STATIC CHAR8 NameSSDT[] = { AML_OP_NAME, 'S', 'S', 'D', 'T', AML_OP_PACKAGE };
STATIC CHAR8 NameCSDT[] = { AML_OP_NAME, 'C', 'S', 'D', 'T', AML_OP_PACKAGE };
STATIC CHAR8 NameTSDT[] = { AML_OP_NAME, 'T', 'S', 'D', 'T', AML_OP_PACKAGE };

STATIC UINT8 NameSSDT2[] = { 0x80, 0x53, 0x53, 0x44, 0x54 };
STATIC UINT8 NameCSDT2[] = { 0x80, 0x43, 0x53, 0x44, 0x54 };

VOID
DumpChildSsdt (
  EFI_ACPI_DESCRIPTION_HEADER   *TableEntry,
  CHAR16                        *DirName,
  CHAR16                        *FileNamePrefix,
  UINTN                         *SsdtCount
) {
  EFI_STATUS    Status = EFI_SUCCESS;
  UINTN         Adr, Len = 0;
  INTN          j, k, PacLen, PacCount;
  UINT8         *Entry, *End, *PacBody;
  CHAR16        *FileName;
  CHAR8         Signature[5], OemTableId[9];

  Entry = (UINT8 *)TableEntry;
  End = Entry + TableEntry->Length;

  while (Entry < End) {
    if (
      (CompareMem (Entry, NameSSDT, sizeof (NameSSDT)) == 0) ||
      (CompareMem (Entry, NameCSDT, sizeof (NameCSDT)) == 0) ||
      (CompareMem (Entry, NameTSDT, sizeof (NameTSDT)) == 0)
    ) {
      PacLen = AcpiGetSize (Entry, sizeof (NameSSDT));

      PacBody = Entry + sizeof (NameSSDT) + (PacLen > 63 ? 2 : 1); // Our packages are not huge
      PacCount = *PacBody++;

      if ((PacCount > 0) && ((PacCount % 3) == 0)) {
        PacCount /= 3;

        DBG ("\n Found hidden SSDT %d pcs\n", PacCount);

        while (PacCount-- > 0) {
          // Skip text marker and addr type tag
          PacBody += 1 + 8 + 1 + 1;

          Adr = ReadUnaligned32 ((UINT32 *)(PacBody));
          PacBody += 4;

          if (*PacBody == AML_CHUNK_DWORD) {
            Len = ReadUnaligned32 ((UINT32 *)(PacBody + 1));
            PacBody += 5;
          } else if (*PacBody == AML_CHUNK_WORD) {
            Len = ReadUnaligned16 ((UINT16 *)(PacBody + 1));
            PacBody += 3;
          }

          // Take Signature and OemId for printing
          CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->Signature, 4);
          Signature[4] = 0;
          CopyMem ((CHAR8 *)&OemTableId, (CHAR8 *)&((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->OemTableId, 8);
          OemTableId[8] = 0;

          DBG ("      * %p: '%a', '%a', Rev: %d, Len: %d  ", Adr, Signature, OemTableId,
              ((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->Revision, ((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->Length);

          for (k = 0; k < 16; k++) {
            DBG ("%02x ", ((UINT8 *)Adr)[k]);
          }

          if (
            (AsciiStrCmp (Signature, "SSDT") == 0) &&
            (Len < 0x20000) &&
            (DirName != NULL) &&
            !IsTableSaved ((VOID *)Adr)
          ) {
            FileName = PoolPrint (L"%sSSDT-%dx.aml", FileNamePrefix, *SsdtCount);
            Len = ((UINT16 *)Adr)[2];
            DBG ("Internal Length = %d", Len);
            Status = SaveBufferToDisk ((VOID *)Adr, Len, DirName, FileName);

            if (!EFI_ERROR (Status)) {
              DBG (" -> %s\n", FileName);
              MarkTableAsSaved ((VOID *)Adr);
              *SsdtCount += 1;
            } else {
              DBG (" -> %r\n", Status);
            }

            FreePool (FileName);
          }
        }
      }

      Entry += sizeof (NameSSDT) + PacLen;
    } else if (
      (CompareMem (Entry, NameSSDT2, 5) == 0) ||
      (CompareMem (Entry, NameCSDT2, 5) == 0)
    ) {
      Adr = ReadUnaligned32 ((UINT32 *)(Entry + 7));
      Len = 0;
      j = *(Entry + 11);

      if (j == 0x0b) {
        Len = ReadUnaligned16 ((UINT16 *)(Entry + 12));
      } else if (j == 0x0a) {
        Len = *(Entry + 12);
      } else {
        // not a number so skip for security
        Entry += 5;
        continue;
      }

      if (Len > 0) {
        // Take Signature and OemId for printing
        CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->Signature, 4);
        Signature[4] = 0;
        CopyMem ((CHAR8 *)&OemTableId, (CHAR8 *)&((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->OemTableId, 8);
        OemTableId[8] = 0;

        DBG ("      * %p: '%a', '%a', Rev: %d, Len: %d  ", Adr, Signature, OemTableId,
            ((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->Revision, ((EFI_ACPI_DESCRIPTION_HEADER *)Adr)->Length);

        for (k = 0; k < 16; k++) {
          DBG ("%02x ", ((UINT8 *)Adr)[k]);
        }

        if ((AsciiStrCmp (Signature, "SSDT") == 0) && (Len < 0x20000) && (DirName != NULL) && !IsTableSaved ((VOID *)Adr)) {
          FileName = PoolPrint (L"%sSSDT-%dx.aml", FileNamePrefix, *SsdtCount);
          Status = SaveBufferToDisk ((VOID *)Adr, Len, DirName, FileName);

          if (!EFI_ERROR (Status)) {
            DBG (" -> %s", FileName);
            MarkTableAsSaved ((VOID *)Adr);
            *SsdtCount += 1;
          } else {
            DBG (" -> %r", Status);
          }
          FreePool (FileName);
        }

        DBG ("\n");
      }

      Entry += 5;
    } else {
      Entry++;
    }
  }
}

/** Saves Table to disk as DirName\\FileName (DirName != NULL)
 *  or just prints basic table data to log (DirName == NULL).
 */
EFI_STATUS
DumpTable (
  EFI_ACPI_DESCRIPTION_HEADER   *TableEntry,
  CHAR8                         *CheckSignature,
  CHAR16                        *DirName,
  CHAR16                        *FileName,
  CHAR16                        *FileNamePrefix,
  UINTN                         *SsdtCount
) {
  EFI_STATUS    Status;
  CHAR8         Signature[ACPI_NAME_SIZE + 1],
                OemTableId[ACPI_OEM_TABLE_ID_SIZE + 1];
  BOOLEAN       ReleaseFileName = FALSE;

  // Take Signature and OemId for printing
  Signature[ACPI_NAME_SIZE] = 0;
  OemTableId[ACPI_OEM_TABLE_ID_SIZE] = 0;

  CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&TableEntry->Signature, ACPI_NAME_SIZE);
  CopyMem ((CHAR8 *)&OemTableId, (CHAR8 *)&TableEntry->OemTableId, ACPI_OEM_TABLE_ID_SIZE);

  DBG (" %p: '%a', '%a', Rev: %d, Len: %d", TableEntry, Signature, OemTableId, TableEntry->Revision, TableEntry->Length);

  //
  // Additional checks
  //
  if ((CheckSignature != NULL) && (AsciiStrCmp (Signature, CheckSignature) != 0)) {
    DBG (" -> invalid signature, expecting %a\n", CheckSignature);
    return EFI_INVALID_PARAMETER;
  }

  // XSDT checks
  if (TableEntry->Signature == EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
    if (TableEntry->Length < sizeof (XSDT_TABLE)) {
      DBG (" -> invalid length\n");
      return EFI_INVALID_PARAMETER;
    }
  }

  // FADT/FACP checks
  if (TableEntry->Signature == EFI_ACPI_1_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
    if (TableEntry->Length < sizeof (EFI_ACPI_1_0_FIXED_ACPI_DESCRIPTION_TABLE)) {
      DBG (" -> invalid length\n");
      return EFI_INVALID_PARAMETER;
    }
  }

  // DSDT checks
  if (TableEntry->Signature == EFI_ACPI_1_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
    if (TableEntry->Length < sizeof (EFI_ACPI_DESCRIPTION_HEADER)) {
      DBG (" -> invalid length\n");
      return EFI_INVALID_PARAMETER;
    }
  }

  // SSDT checks
  if (TableEntry->Signature == EFI_ACPI_1_0_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
    if (TableEntry->Length < sizeof (EFI_ACPI_DESCRIPTION_HEADER)) {
      DBG (" -> invalid length\n");
      return EFI_INVALID_PARAMETER;
    }
  }

  if ((DirName == NULL) || IsTableSaved (TableEntry)) {
    // just debug log dump
    return EFI_SUCCESS;
  }

  if (FileNamePrefix == NULL) {
    FileNamePrefix = L"";
  }

  if (FileName == NULL) {
    // take the name from the signature
    if (
      (TableEntry->Signature == EFI_ACPI_1_0_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) &&
      (SsdtCount != NULL)
    ) {
      // Ssdt counter
      FileName = PoolPrint (L"%sSSDT-%d.aml", FileNamePrefix, *SsdtCount);
      *SsdtCount = *SsdtCount + 1;
      DumpChildSsdt (TableEntry, DirName, FileNamePrefix, SsdtCount);
    } else {
      FileName = PoolPrint (L"%s%a.aml", FileNamePrefix, Signature);
    }

    if (FileName == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ReleaseFileName = TRUE;
  }

  DBG (" -> %s", FileName);

  // Save it
  Status = SaveBufferToDisk ((VOID *)TableEntry, TableEntry->Length, DirName, FileName);
  MarkTableAsSaved (TableEntry);

  if (ReleaseFileName) {
    FreePool (FileName);
  }

  return Status;
}

/** Saves to disk (DirName != NULL) or prints to log (DirName == NULL) Fadt tables: Dsdt and Facs. */
EFI_STATUS
DumpFadtTables (
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE       *Fadt,
  CHAR16                                          *DirName,
  CHAR16                                          *FileNamePrefix,
  UINTN                                           *SsdtCount
) {
  EFI_ACPI_DESCRIPTION_HEADER                     *TableEntry;
  EFI_ACPI_2_0_FIRMWARE_ACPI_CONTROL_STRUCTURE    *Facs;
  EFI_STATUS                                      Status  = EFI_SUCCESS;
  UINT64                                          DsdtAdr, FacsAdr;
  CHAR8                                           Signature[ACPI_NAME_SIZE + 1];
  CHAR16                                          *FileName;

  //
  // if Fadt->Revision < 3 (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION), then it is Acpi 1.0
  // and fields after Flags are not available
  //

  DBG ("      (Dsdt: %x, Facs: %x", Fadt->Dsdt, Fadt->FirmwareCtrl);

  // for Acpi 1.0
  DsdtAdr = Fadt->Dsdt;
  FacsAdr = Fadt->FirmwareCtrl;

  if (Fadt->Header.Revision >= EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION) {
    // Acpi 2.0 or up
    // may have it in XDsdt or XFirmwareCtrl
    DBG (", XDsdt: %lx, XFacs: %lx", Fadt->XDsdt, Fadt->XFirmwareCtrl);
    if (Fadt->XDsdt != 0) {
      DsdtAdr = Fadt->XDsdt;
    }
    if (Fadt->XFirmwareCtrl != 0) {
      FacsAdr = Fadt->XFirmwareCtrl;
    }
  }

  DBG (")\n");

  //
  // Save Dsdt
  //
  if (DsdtAdr != 0) {
    DBG ("     ");
    TableEntry = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)DsdtAdr;
    Status = DumpTable (TableEntry, "DSDT", DirName,  NULL, FileNamePrefix, NULL);

    if (EFI_ERROR (Status)) {
      DBG (" - %r\n", Status);
      return Status;
    }

    DBG ("\n");
    DumpChildSsdt (TableEntry, DirName, FileNamePrefix, SsdtCount);
  }

  //
  // Save Facs
  //
  if (FacsAdr != 0) {
    // Taking it as structure from Acpi 2.0 just to get Version (it's reserved field in Acpi 1.0 and == 0)
    Facs = (EFI_ACPI_2_0_FIRMWARE_ACPI_CONTROL_STRUCTURE *)(UINTN)FacsAdr;
    // Take Signature for printing
    Signature[ACPI_NAME_SIZE] = 0;
    CopyMem ((CHAR8 *)&Signature, (CHAR8 *)&Facs->Signature, ACPI_NAME_SIZE);

    DBG ("      %p: '%a', Ver: %d, Len: %d", Facs, Signature, Facs->Version, Facs->Length);

    // FACS checks
    if (Facs->Signature != EFI_ACPI_1_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE) {
      DBG (" -> invalid signature, expecting FACS\n");
      return EFI_INVALID_PARAMETER;
    }

    if (Facs->Length < sizeof (EFI_ACPI_1_0_FIRMWARE_ACPI_CONTROL_STRUCTURE)) {
      DBG (" -> invalid length\n");
      return EFI_INVALID_PARAMETER;
    }

    if ((DirName != NULL) && !IsTableSaved ((VOID *)Facs)) {
      FileName = PoolPrint (L"%sFACS.aml", FileNamePrefix);
      DBG (" -> %s", FileName);
      Status = SaveBufferToDisk ((VOID *)Facs, Facs->Length, DirName, FileName);
      MarkTableAsSaved (Facs);
      FreePool (FileName);

      if (EFI_ERROR (Status)) {
        DBG (" - %r\n", Status);
        return Status;
      }
    }

    DBG ("\n");
  }

  return Status;
}

/** Saves to disk (DirName != NULL)
 *  or prints to debug log (DirName == NULL)
 *  ACPI tables given by RsdPtr.
 *  Takes tables from Xsdt if present or from Rsdt if Xsdt is not present.
 */
VOID
DumpTables (
  VOID      *RsdPtrVoid,
  CHAR16    *DirName
) {
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER    *RsdPtr;
  EFI_ACPI_DESCRIPTION_HEADER                     *TableEntry;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE       *Fadt;

  EFI_STATUS    Status;
  CHAR8         *EntryPtr,
                RSDPSignature[ACPI_RSDP_SIG_SIZE + 1]; // "RSD PTR "
  UINTN         EntryCount, Index, SsdtCount, Length;
  CHAR16        *FileNamePrefix;

  //
  // RSDP
  // Take it as Acpi 2.0, but take care that if RsdPtr->Revision == 0
  // then it is actually Acpi 1.0 and fields after RsdtAddress (like XsdtAddress)
  // are not available
  //

  RsdPtr = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)RsdPtrVoid;
  if (DirName != NULL) {
    DBG ("Saving ACPI tables from RSDP %p to %s ...\n", RsdPtr, DirName);
  } else {
    DBG ("Printing ACPI tables from RSDP %p ...\n", RsdPtr);
  }

  if (RsdPtr->Signature != EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE) {
    DBG (" RsdPrt at %p has invaid signature 0x%lx - exiting.\n", RsdPtr, RsdPtr->Signature);
    return;
  }

  // Take Signature and OemId for printing
  RSDPSignature[ACPI_RSDP_SIG_SIZE] = 0;
  CopyMem ((CHAR8 *)&RSDPSignature, (CHAR8 *)&RsdPtr->Signature, ACPI_RSDP_SIG_SIZE);

  DBG (" %p: '%a', Rev: %d", RsdPtr, RSDPSignature, RsdPtr->Revision);
  if (RsdPtr->Revision > 0) {
    // Acpi 2.0 or newer
    Length = RsdPtr->Length;
    DBG (" (Acpi 2.0 or newer)");
  } else {
    DBG (" unsupported RsdPtr->Revision - exiting.\n");
    return;
  }

  DBG (", Len: %d", Length);

  //
  // Save RsdPtr
  //
  if ((DirName != NULL) && !IsTableSaved ((VOID *)RsdPtr)) {
    DBG (" -> RSDP.aml");
    Status = SaveBufferToDisk ((VOID *)RsdPtr, Length, DirName, L"RSDP.aml");
    MarkTableAsSaved (RsdPtr);

    if (EFI_ERROR (Status)) {
      DBG (" - %r\n", Status);
      return;
    }
  }

  DBG ("\n");

  if (Xsdt == NULL) {
    DBG (" No Xsdt - exiting.\n");
    return;
  }

  DBG ("  (Xsdt: %p)\n", Xsdt);

  FileNamePrefix = L"";

  //
  // Save Xsdt
  //

  DBG (" ");

  Status = DumpTable ((EFI_ACPI_DESCRIPTION_HEADER *)Xsdt, "XSDT", DirName,  L"XSDT.aml", FileNamePrefix, NULL);

  if (EFI_ERROR (Status)) {
    DBG (" - %r", Status);
    //Xsdt = NULL;
    return;
  }
  DBG ("\n");

  //
  // Take tables from Xsdt
  //

  EntryCount = (Xsdt->Header.Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT64);
  if (EntryCount > 100) {
    EntryCount = 100; // it's enough
  }

  DBG ("  Tables in Xsdt: %d\n", EntryCount);

  // iterate over table entries
  EntryPtr = (CHAR8 *)&Xsdt->Entry;
  SsdtCount = 0;
  for (Index = 0; Index < EntryCount; Index++, EntryPtr += sizeof (UINT64)) {
    //UINT64 *EntryPtr64 = (UINT64 *)EntryPtr;
    DBG ("  %d.", Index);

    // skip NULL entries
    //if (*EntryPtr64 == 0) {
    if (ReadUnaligned64 ((CONST UINT64 *)EntryPtr) == 0) {
      DBG (" = 0\n", Index);
      continue;
    }

    // Save table with the name from signature
    TableEntry = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)(ReadUnaligned64 ((CONST UINT64 *)EntryPtr));

    if (TableEntry->Signature == EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
      //
      // Fadt - save Dsdt and Facs
      //
      Status = DumpTable (TableEntry, NULL, DirName,  NULL, FileNamePrefix, &SsdtCount);

      if (EFI_ERROR (Status)) {
        DBG (" - %r\n", Status);
        return;
      }

      DBG ("\n");

      Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)TableEntry;
      Status = DumpFadtTables (Fadt, DirName, FileNamePrefix, &SsdtCount);

      if (EFI_ERROR (Status)) {
        return;
      }
    } else {
      Status = DumpTable (TableEntry, NULL, DirName,  NULL /* take the name from the signature */, FileNamePrefix, &SsdtCount);

      if (EFI_ERROR (Status)) {
        DBG (" - %r\n", Status);
        return;
      }

      DBG ("\n");
    }
  }
}

/** Saves OEM ACPI tables to disk.
 *  Searches BIOS, then UEFI Sys.Tables for Acpi 2.0 or newer tables, then for Acpi 1.0 tables
 *  CloverEFI:
 *   - saves first one found, dump others to log
 *  UEFI:
 *   - saves first one found in UEFI Sys.Tables, dump others to log
 *
 * Dumping of other tables to log can be removed if it turns out that there is no value in doing it.
 */
VOID
SaveOemTables () {
  EFI_STATUS    Status;
  VOID          *RsdPtr;
  BOOLEAN       Saved = FALSE;
  CHAR8         *MemLogStart;
  UINTN         MemLogStartLen;

  MemLogStartLen = GetMemLogLen ();
  MemLogStart = GetMemLogBuffer () + MemLogStartLen;

  //
  // Search in BIOS
  // CloverEFI - Save
  // UEFI - just print to log
  //

  RsdPtr = FindAcpiRsdPtr ();

  if (RsdPtr != NULL) {
    DBG ("Found BIOS RSDP at %p\n", RsdPtr);
    //if (gFirmwareClover) {
    //  // Save it
    //  DumpTables (RsdPtr, DIR_ACPI_ORIGIN);
    //  Saved = TRUE;
    //} else {
      // just print to log
      DumpTables (RsdPtr, NULL);
    //}
  }

  //
  // Search Acpi 2.0 or newer in UEFI Sys.Tables
  //
  RsdPtr = NULL;
  Status = EfiGetSystemConfigurationTable (&gEfiAcpi20TableGuid, &RsdPtr);

  if (RsdPtr != NULL) {
    DBG ("Found UEFI Acpi 2.0 RSDP at %p\n", RsdPtr);
    // if tables already saved, then just print to log
    DumpTables (RsdPtr, Saved ? NULL : DIR_ACPI_ORIGIN);
    Saved = TRUE;
  }

  SaveBufferToDisk (MemLogStart, GetMemLogLen () - MemLogStartLen, DIR_ACPI_ORIGIN, DSDT_DUMP_LOG);

  FreePool (mSavedTables);
}

#endif // DUMP_TABLE

VOID
SaveOemDsdt (
  BOOLEAN   FullPatch,
  UINT8     OSType
) {
  EFI_STATUS                                  Status = EFI_NOT_FOUND;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *FadtPointer = NULL;
  EFI_PHYSICAL_ADDRESS                        Dsdt = EFI_SYSTEM_TABLE_MAX_ADDRESS;

  UINTN     Pages, DsdtLen = 0;
  UINT8     *Buffer = NULL;
  UINT64    BiosDsdt;
  CHAR16    *OsSubdir = NULL,
            *OriginDsdtFixed = PoolPrint (DIR_ACPI_ORIGIN L"\\" DSDT_PATCHED_NAME, gSettings.FixDsdt),
            *FixedDsdt = AllocateZeroPool (SVALUE_MAX_SIZE);

  if (!FullPatch) {
    if (OSTYPE_IS_DARWIN_GLOB (OSType)) {
      OsSubdir = OSTYPE_DARWIN_STR;
    } else if (OSTYPE_IS_LINUX_GLOB (OSType)) {
      OsSubdir = OSTYPE_LINUX_STR;
    } else if (OSTYPE_IS_WINDOWS_GLOB (OSType)) {
      OsSubdir = OSTYPE_WINDOWS_STR;
    } else {
      goto Finish;
    }

    FixedDsdt = PoolPrint (DIR_ACPI_PATCHED L"\\%s\\" DSDT_PATCHED_NAME, OsSubdir, gSettings.FixDsdt);

    if (FileExists (SelfRootDir, FixedDsdt)) {
      goto Finish;
    }
  }

  FadtPointer = GetFadt ();
  if (FadtPointer == NULL) {
    DBG ("Cannot found FADT in BIOS or in UEFI!\n");
    goto Finish;
  }

  BiosDsdt = FadtPointer->Dsdt;

  if (
    (FadtPointer->Header.Revision >= EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION) &&
    (FadtPointer->XDsdt != 0)
  ) {
    BiosDsdt = FadtPointer->XDsdt;
  }

  Buffer = (UINT8 *)(UINTN)BiosDsdt;

  if (!Buffer) {
    DBG ("Cannot found DSDT in BIOS or in files!\n");
    goto Finish;
  }

  DsdtLen = ((EFI_ACPI_DESCRIPTION_HEADER *)Buffer)->Length;
  Pages = EFI_SIZE_TO_PAGES (DsdtLen + DsdtLen / 8); // take some extra space for patches
  Status = gBS->AllocatePages (
                   AllocateMaxAddress,
                   EfiBootServicesData,
                   Pages,
                   &Dsdt
                 );

  // if success insert Dsdt pointer into ACPI tables
  if (!EFI_ERROR (Status)) {
    CopyMem ((UINT8 *)(UINTN)Dsdt, Buffer, DsdtLen);
    Buffer = (UINT8 *)(UINTN)Dsdt;

    if (FullPatch) {
      FixBiosDsdt (Buffer, FALSE);
      DsdtLen = ((EFI_ACPI_DESCRIPTION_HEADER *)Buffer)->Length;
    } else if (!StrLen (FixedDsdt)) {
      goto Finish;
    }

    Status = SaveFile (SelfRootDir, FullPatch ? OriginDsdtFixed : FixedDsdt, Buffer, DsdtLen);

    MsgLog ("Saving DSDT to: %s ... %r\n", FullPatch ? OriginDsdtFixed : FixedDsdt, Status);

    gBS->FreePages (Dsdt, Pages);
  }

  Finish:

  FreePool (OriginDsdtFixed);
  FreePool (FixedDsdt);
}

EFI_STATUS
PatchACPI (
  UINT8   OSType
) {
  EFI_STATUS                                              Status = EFI_SUCCESS;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE               *FadtPointer = NULL;
  EFI_ACPI_4_0_FIXED_ACPI_DESCRIPTION_TABLE               *NewFadt   = NULL;
  //EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_TABLE_HEADER      *Hpet    = NULL;
  EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE            *Facs = NULL;
  EFI_PHYSICAL_ADDRESS                                    BufferPtr, Dsdt = EFI_SYSTEM_TABLE_MAX_ADDRESS;
  SSDT_TABLE                                              *Ssdt = NULL;
  UINT64                                                  XFirmwareCtrl, XDsdt; // save values if present
  EFI_ACPI_DESCRIPTION_HEADER                             *TableHeader;
  UINTN                                                   Index, ApicCPUNum, BufferLen = 0;
  UINT8                                                   CPUBase, *Buffer = NULL;
  BOOLEAN                                                 DsdtLoaded = FALSE, OSTypeDarwin = FALSE,
                                                          PatchedDirExists = FALSE;
  CHAR16                                                  PatchedPath[SVALUE_MAX_SIZE], *FullPatchedPath, *OsSubdir = NULL,
                                                          *FixedDsdt = PoolPrint (DSDT_PATCHED_NAME, gSettings.FixDsdt);

  DbgHeader ("PatchACPI");

  FadtPointer = GetFadt ();

  if (!FadtPointer) {
    //DBG ("Null FadtPointer\n");
    return EFI_NOT_FOUND;
  }

  //DBG ("FADT pointer = %x\n", (UINTN)FadtPointer);

  if (OSTYPE_IS_DARWIN_GLOB (OSType)) {
    OsSubdir = OSTYPE_DARWIN_STR;
    OSTypeDarwin = TRUE;
  } else if (OSTYPE_IS_LINUX_GLOB (OSType)) {
    OsSubdir = OSTYPE_LINUX_STR;
  } else if (OSTYPE_IS_WINDOWS_GLOB (OSType)) {
    OsSubdir = OSTYPE_WINDOWS_STR;
  }

  if (OsSubdir != NULL) {
    // prepare dirs that will be searched for custom ACPI tables
    UnicodeSPrint (PatchedPath, ARRAY_SIZE (PatchedPath), DIR_ACPI_PATCHED L"\\%s", OsSubdir);
    PatchedDirExists = FileExists (SelfRootDir, PatchedPath);
  }

  if (!OSTypeDarwin) {
    goto ScanPatchedDir;
  }

  // Slice - then we do FADT patch no matter if we don't have DSDT.aml
  BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;
  Status = gBS->AllocatePages (AllocateMaxAddress, EfiACPIReclaimMemory, 1, &BufferPtr);

  if (!EFI_ERROR (Status)) {
    UINT32    oldLength = ((EFI_ACPI_DESCRIPTION_HEADER *)FadtPointer)->Length;

    NewFadt = (EFI_ACPI_4_0_FIXED_ACPI_DESCRIPTION_TABLE *)(UINTN)BufferPtr;
    DBG ("old FADT length=%x\n", oldLength);
    CopyMem ((UINT8 *)NewFadt, (UINT8 *)FadtPointer, oldLength); // old data
    NewFadt->Header.Length = 0xF4;
    CopyMem ((UINT8 *)NewFadt->Header.OemId, (UINT8 *)BiosVendor, ACPI_OEM_ID_SIZE);
    NewFadt->Header.Revision = EFI_ACPI_4_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION;
    NewFadt->Reserved0 = 0; // ACPIspec said it should be 0, while 1 is possible, but no more
    NewFadt->PreferredPmProfile = 3;
    if (!gSettings.SmartUPS) {
      NewFadt->PreferredPmProfile = gMobile ? 2 : 1; // as calculated before
    }

    if (gSettings.EnableC6 /* || gSettings.EnableISS */) {
      NewFadt->CstCnt = 0x85; // as in Mac
    }

    if (gSettings.EnableC2) {
      NewFadt->PLvl2Lat = 0x65;
    }

    if (gSettings.C3Latency > 0) {
      NewFadt->PLvl3Lat = gSettings.C3Latency;
    } else if (gSettings.EnableC4) {
      NewFadt->PLvl3Lat = 0x3E9;
    }

    if (gSettings.C3Latency == 0) {
      gSettings.C3Latency = NewFadt->PLvl3Lat;
    }

    NewFadt->IaPcBootArch = 0x3;

    NewFadt->Flags |= 0x400; // Reset Register Supported
    XDsdt = NewFadt->XDsdt; // save values if present
    XFirmwareCtrl = NewFadt->XFirmwareCtrl;
    CopyMem ((UINT8 *)&NewFadt->ResetReg, PmBlock, ARRAY_SIZE (PmBlock));

    if (NewFadt->Dsdt) {
      NewFadt->XDsdt = (UINT64)(NewFadt->Dsdt);
    } else if (XDsdt) {
      NewFadt->Dsdt = (UINT32)XDsdt;
    }

    // if (Facs) NewFadt->FirmwareCtrl = (UINT32)(UINTN)Facs;
    // else DBG ("No FACS table ?!\n");
    if (NewFadt->FirmwareCtrl) {
      Facs = (EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE *)(UINTN)NewFadt->FirmwareCtrl;
      NewFadt->XFirmwareCtrl = (UINT64)(UINTN)(Facs);
    } else if (NewFadt->XFirmwareCtrl) {
      NewFadt->FirmwareCtrl = (UINT32)XFirmwareCtrl;
      Facs = (EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE *)(UINTN)XFirmwareCtrl;
    }

    // patch for FACS included here
    Facs->Version = EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_VERSION;

    NewFadt->XPm1aEvtBlk.Address = (UINT64)(NewFadt->Pm1aEvtBlk);
    NewFadt->XPm1bEvtBlk.Address = (UINT64)(NewFadt->Pm1bEvtBlk);
    NewFadt->XPm1aCntBlk.Address = (UINT64)(NewFadt->Pm1aCntBlk);
    NewFadt->XPm1bCntBlk.Address = (UINT64)(NewFadt->Pm1bCntBlk);
    NewFadt->XPm2CntBlk.Address  = (UINT64)(NewFadt->Pm2CntBlk);
    NewFadt->XPmTmrBlk.Address   = (UINT64)(NewFadt->PmTmrBlk);
    NewFadt->XGpe0Blk.Address    = (UINT64)(NewFadt->Gpe0Blk);
    NewFadt->XGpe1Blk.Address    = (UINT64)(NewFadt->Gpe1Blk);
    FadtPointer = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)NewFadt;

    if (Xsdt != NULL) {
      Xsdt->Entry = (UINT64)((UINT32)(UINTN)NewFadt);
      Xsdt->Header.Checksum = 0;
      Xsdt->Header.Checksum = (UINT8)(256 - Checksum8 ((CHAR8 *)Xsdt, Xsdt->Header.Length));
    }

    FadtPointer->Header.Checksum = 0;
    FadtPointer->Header.Checksum = (UINT8)(256 - Checksum8 ((CHAR8 *)FadtPointer, FadtPointer->Header.Length));
  }

  ScanPatchedDir:

  //
  // Inject/drop tables
  //

  if (PatchedDirExists) {
    Status = EFI_NOT_FOUND;

    FullPatchedPath = AllocateZeroPool (SVALUE_MAX_SIZE);

    if (StriCmp (DSDT_NAME, gSettings.DsdtName) == 0) {
      FullPatchedPath = PoolPrint (L"%s\\%s", PatchedPath, FixedDsdt);
      if (FileExists (SelfRootDir, FullPatchedPath)) {
        MsgLog ("Found patched DSDT: %s\n", FullPatchedPath);
        Status = LoadFile (SelfRootDir, FullPatchedPath, &Buffer, &BufferLen);
      }
    }

    if (EFI_ERROR (Status)) {
      FullPatchedPath = PoolPrint (L"%s\\%s", PatchedPath, gSettings.DsdtName);
      if (FileExists (SelfRootDir, FullPatchedPath)) {
        DBG ("Found patched DSDT in Clover volume: %s\n", FullPatchedPath);
        Status = LoadFile (SelfRootDir, FullPatchedPath, &Buffer, &BufferLen);
      }
    }

    FreePool (FullPatchedPath);

    //
    // apply DSDT loaded from a file into Buffer
    //

    if (!EFI_ERROR (Status)) {
      // custom DSDT is loaded so not need to drop _DSM - NO! We need to drop them!
      // if we will apply fixes, allocate additional space
      BufferLen = BufferLen + BufferLen / 8;
      Dsdt = EFI_SYSTEM_TABLE_MAX_ADDRESS;
      Status = gBS->AllocatePages (
                      AllocateMaxAddress,
                      EfiACPIReclaimMemory,
                      EFI_SIZE_TO_PAGES (BufferLen),
                      &Dsdt
                    );

      // if success insert dsdt pointer into ACPI tables
      if (!EFI_ERROR (Status)) {
        //DBG ("page is allocated, write DSDT into\n");
        CopyMem ((VOID *)(UINTN)Dsdt, Buffer, BufferLen);

        FadtPointer->Dsdt  = (UINT32)Dsdt;
        FadtPointer->XDsdt = Dsdt;
        // verify checksum
        FadtPointer->Header.Checksum = 0;
        FadtPointer->Header.Checksum = (UINT8)(256 - Checksum8 ((CHAR8 *)FadtPointer,FadtPointer->Header.Length));
        DsdtLoaded = TRUE;

        goto DebugDSDT;
      }
    }
  }

  // allocate space for fixes
  TableHeader = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)FadtPointer->Dsdt;
  BufferLen = TableHeader->Length;
  //DBG ("DSDT len = 0x%x", BufferLen);
  BufferLen = BufferLen + BufferLen / 8;
  //DBG (" new len = 0x%x\n", BufferLen);

  Dsdt = EFI_SYSTEM_TABLE_MAX_ADDRESS;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiACPIReclaimMemory,
                  EFI_SIZE_TO_PAGES (BufferLen),
                  &Dsdt
                );

  // if success insert dsdt pointer into ACPI tables
  if (!EFI_ERROR (Status)) {
    CopyMem ((VOID *)(UINTN)Dsdt, (VOID *)TableHeader, BufferLen);

    FadtPointer->Dsdt  = (UINT32)Dsdt;
    FadtPointer->XDsdt = Dsdt;
    // verify checksum
    FadtPointer->Header.Checksum = 0;
    FadtPointer->Header.Checksum = (UINT8)(256   - Checksum8 ((CHAR8 *)FadtPointer, FadtPointer->Header.Length));
  }

  DebugDSDT:

  if (PatchedDirExists && gSettings.DebugDSDT) {
    DBG ("Output DSDT before patch to %s\\%s\n", DIR_ACPI_ORIGIN, DSDT_ORIGIN_NAME);
    Status = SaveFile (SelfRootDir, PoolPrint (L"%s\\%s", DIR_ACPI_ORIGIN, DSDT_ORIGIN_NAME), (UINT8 *)(UINTN)FadtPointer->XDsdt, BufferLen);
  }

  if (!OSTypeDarwin) {
    goto InjectSSDT;
  }

  // native DSDT or loaded we want to apply autoFix to this
  // if (gSettings.FixDsdt) { //fix even with zero mask because we want to know PCIRootUID and CPUBase and count (?)
  //DBG ("Apply DsdtFixMask=0x%08x %a way\n", gSettings.FixDsdt, (gSettings.FixDsdt & FIX_NEW_WAY)?"new":"old");
  //MsgLog ("Apply DsdtFixMask: 0x%08x\n", gSettings.FixDsdt);

  DumpFixBiosDsdt ();

  DBG ("Drop _DSM mask=0x%04x\n", gSettings.DropOEM_DSM); // dropDSM

  FixBiosDsdt ((UINT8 *)(UINTN)FadtPointer->XDsdt, DsdtLoaded);

  // Drop tables
  if (gSettings.ACPIDropTables) {
    DbgHeader ("ACPIDropTables");
    ACPI_DROP_TABLE   *DropTable = gSettings.ACPIDropTables;

    while (DropTable) {
      if (DropTable->MenuItem.BValue) {
        //DBG ("Attempting to drop \"%4.4a\" (%8.8X) \"%8.8a\" (%16.16lX) L=%d\n", &(DropTable->Signature),
        //    DropTable->Signature, &(DropTable->TableId), DropTable->TableId, DropTable->Length);
        DropTableFromXSDT (DropTable->Signature, DropTable->TableId, DropTable->Length);
      }

      DropTable = DropTable->Next;
    }
  }

  if (gSettings.DropSSDT) {
    DbgHeader ("DropSSDT");
    // special case if we set into menu drop all SSDT
    DropTableFromXSDT (EFI_ACPI_4_0_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE, 0, 0);
  } else {
    DbgHeader ("PatchAllSSDT");
    // all remaining SSDT tables will be patched
    PatchAllSSDT ();
    // do the empty drop to clean xsdt
    DropTableFromXSDT (XXXX_SIGN, 0, 0);
  }

  InjectSSDT:

  if (ACPIPatchedAML) {
    CHAR16  FullName[AVALUE_MAX_SIZE];

    DbgHeader ("ACPIPatchedAML");
    MsgLog ("Start: Processing Patched AML (s): ");

    if (gSettings.SortedACPICount) {
      MsgLog ("Sorted\n");

      for (Index = 0; Index < gSettings.SortedACPICount; Index++) {
        ACPI_PATCHED_AML    *ACPIPatchedAMLTmp = ACPIPatchedAML;

        while (ACPIPatchedAMLTmp) {
          if (
            (OSTYPE_IS_DARWIN_GLOB (OSType) && OSTYPE_IS_DARWIN_GLOB (ACPIPatchedAMLTmp->OSType)) ||
            (OSTYPE_IS_LINUX_GLOB (OSType) && OSTYPE_IS_LINUX_GLOB (ACPIPatchedAMLTmp->OSType)) ||
            (OSTYPE_IS_WINDOWS_GLOB (OSType) && OSTYPE_IS_WINDOWS_GLOB (ACPIPatchedAMLTmp->OSType))
          ) {
            if (
              (StriCmp (ACPIPatchedAMLTmp->FileName, gSettings.SortedACPI[Index]) == 0) &&
              (ACPIPatchedAMLTmp->MenuItem.BValue)
            ) {
              MsgLog ("Disabled: %s, skip\n", ACPIPatchedAMLTmp->FileName);
              break;
            }
          }

          ACPIPatchedAMLTmp = ACPIPatchedAMLTmp->Next;
        }

        if (!ACPIPatchedAMLTmp) { // NULL when not disabled
          UnicodeSPrint (FullName, ARRAY_SIZE (FullName), L"%s\\%s", PatchedPath, gSettings.SortedACPI[Index]);
          MsgLog (" - [%02d]: %s from %s ... ", Index, gSettings.SortedACPI[Index], PatchedPath);
          Status = LoadFile (SelfRootDir, FullName, &Buffer, &BufferLen);
          if (!EFI_ERROR (Status)) {
            // before insert we should checksum it
            if (Buffer) {
              TableHeader = (EFI_ACPI_DESCRIPTION_HEADER *)Buffer;
              if (TableHeader->Length > 500 * kilo) {
                MsgLog ("wrong table\n");
                continue;
              }

              TableHeader->Checksum = 0;
              TableHeader->Checksum = (UINT8)(256 - Checksum8 ((CHAR8 *)Buffer, TableHeader->Length));
            }

            Status = InsertTable ((VOID *)Buffer, BufferLen);

            if (!EFI_ERROR (Status)) {
              if (!StriCmp (ACPIPatchedAMLTmp->FileName, SSDT_PSTATES_NAME)) {
                if (gSettings.GeneratePStates) {
                  MsgLog (" (GeneratedPStates) ");
                  gSettings.GeneratePStates = FALSE;
                }
              } else if (!StriCmp (ACPIPatchedAMLTmp->FileName, SSDT_CSTATES_NAME)) {
                gSettings.GenerateCStates = FALSE;
                if (gSettings.GenerateCStates) {
                  MsgLog (" (GeneratedCStates) ");
                  gSettings.GenerateCStates = FALSE;
                }
              }
            }
          }

          MsgLog ("%r\n", Status);
        }
      }
    } else {
      ACPI_PATCHED_AML    *ACPIPatchedAMLTmp = ACPIPatchedAML;

      MsgLog ("Unsorted\n");
      Index = 0;

      while (ACPIPatchedAMLTmp) {
        if (ACPIPatchedAMLTmp->MenuItem.BValue == FALSE) {
          if (
            (OSTYPE_IS_DARWIN_GLOB (OSType) && !OSTYPE_IS_DARWIN_GLOB (ACPIPatchedAMLTmp->OSType)) ||
            (OSTYPE_IS_LINUX_GLOB (OSType) && !OSTYPE_IS_LINUX_GLOB (ACPIPatchedAMLTmp->OSType)) ||
            (OSTYPE_IS_WINDOWS_GLOB (OSType) && !OSTYPE_IS_WINDOWS_GLOB (ACPIPatchedAMLTmp->OSType))
          ) {
            goto LoadNext;
          }

          UnicodeSPrint (FullName, ARRAY_SIZE (FullName), L"%s\\%s", PatchedPath, ACPIPatchedAMLTmp->FileName);
          MsgLog (" - [%02d]: %s from %s ... ", Index++, ACPIPatchedAMLTmp->FileName, PatchedPath);
          Status = LoadFile (SelfRootDir, FullName, &Buffer, &BufferLen);

          if (!EFI_ERROR (Status)) {
            // before insert we should checksum it
            if (Buffer) {
              TableHeader = (EFI_ACPI_DESCRIPTION_HEADER *)Buffer;
              if (TableHeader->Length > 500 * kilo) {
                MsgLog ("wrong table\n");
                goto LoadNext;
              }

              TableHeader->Checksum = 0;
              TableHeader->Checksum = (UINT8)(256 - Checksum8 ((CHAR8 *)Buffer, TableHeader->Length));
            }

            Status = InsertTable ((VOID *)Buffer, BufferLen);

            if (!EFI_ERROR (Status)) {
              if (!StriCmp (ACPIPatchedAMLTmp->FileName, SSDT_PSTATES_NAME)) {
                if (gSettings.GeneratePStates) {
                  MsgLog ("(GeneratedPStates) ");
                  gSettings.GeneratePStates = FALSE;
                }
              } else if (!StriCmp (ACPIPatchedAMLTmp->FileName, SSDT_CSTATES_NAME)) {
                if (gSettings.GenerateCStates) {
                  MsgLog ("(GeneratedCStates) ");
                  gSettings.GenerateCStates = FALSE;
                }
              }
            }
          }

          MsgLog ("%r\n", Status);
        } else {
          MsgLog ("Disabled: %s, skip\n", ACPIPatchedAMLTmp->FileName);
        }

        LoadNext:

        ACPIPatchedAMLTmp = ACPIPatchedAMLTmp->Next;
      }
    }

    MsgLog ("End: Processing Patched AML (s)\n");
  }

  if (!OSTypeDarwin || !gSettings.GeneratePStates || !gSettings.GenerateCStates) {
    goto SkipGenStates;
  }

  DbgHeader ("CPU States");

  //if (gCPUStructure.Vendor != CPU_VENDOR_INTEL) {
  //  MsgLog ("Not an Intel platform: P-States will not be generated !!!\n");
  //  goto SkipGenStates;
  //}

  if (BIT_ISUNSET (gCPUStructure.Features, CPUID_FEATURE_MSR)) {
    MsgLog ("Unsupported CPU: P-States will not be generated !!!\n");
    goto SkipGenStates;
  }

  // 1. For CPU base number 0 or 1.  codes from SunKi
  CPUBase = AcpiCPUName[0][3] - '0'; // "CPU0"

  if ((UINT8)CPUBase > 11) {
    DBG ("Abnormal CPUBase=%x will set to 0\n", CPUBase);
    CPUBase = 0;
  }

  ApicCPUNum = AcpiCPUCount
                ? AcpiCPUCount
                : (gCPUStructure.Threads >= gCPUStructure.Cores)
                    ? gCPUStructure.Threads
                    : gCPUStructure.Cores;

  if (gSettings.GeneratePStates) {
    Status = EFI_NOT_FOUND;
    Ssdt = GeneratePssSsdt (CPUBase, ApicCPUNum);

    if (Ssdt) {
      Status = InsertTable ((VOID *)Ssdt, Ssdt->Length);
    }

    //DBG ("GeneratePStates Status=%r\n", Status);

    if (!EFI_ERROR (Status)) {
      Status = SaveFile (SelfRootDir, PoolPrint (L"%s\\%s", PatchedPath, SSDT_PSTATES_NAME), (VOID *)Ssdt, Ssdt->Length);
      if (!EFI_ERROR (Status)) {
        MsgLog ("Generated PStates saved to: %s\\%s\n", PatchedPath, SSDT_PSTATES_NAME);
      }
    }
  }

  if (gSettings.GenerateCStates) {
    Status = EFI_NOT_FOUND;
    Ssdt = GenerateCstSsdt (FadtPointer, CPUBase, ApicCPUNum);

    if (Ssdt) {
      Status = InsertTable ((VOID *)Ssdt, Ssdt->Length);
    }

    //DBG ("GenerateCStates Status=%r\n", Status);

    if (!EFI_ERROR (Status)) {
      Status = SaveFile (SelfRootDir, PoolPrint (L"%s\\%s", PatchedPath, SSDT_CSTATES_NAME), (VOID *)Ssdt, Ssdt->Length);
      if (!EFI_ERROR (Status)) {
        MsgLog ("Generated CStates saved to: %s\\%s\n", PatchedPath, SSDT_CSTATES_NAME);
      }
    }
  }

  SkipGenStates:

  if (Xsdt) {
    Xsdt->Header.Checksum = 0;
    Xsdt->Header.Checksum = (UINT8)(256 - Checksum8 ((CHAR8 *)Xsdt, Xsdt->Header.Length));
  }

  return EFI_SUCCESS;
}
