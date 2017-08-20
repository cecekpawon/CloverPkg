/*
 * Original idea of patching kernel by Evan Lojewsky, 2009
 *
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 * Correction and improvements by Clover team
 */

#include <Library/Platform/KernelPatcher.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_KERNEL_PATCHER
#define DEBUG_KERNEL_PATCHER 0
#endif
#else
#ifdef DEBUG_KERNEL_PATCHER
#undef DEBUG_KERNEL_PATCHER
#endif
#define DEBUG_KERNEL_PATCHER DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_KERNEL_PATCHER, __VA_ARGS__)

BootArgs2     *gBootArgs = NULL;
CHAR8         *gDtRoot = NULL;

KERNEL_INFO   *KernelInfo;

#if 0
UINT32 LOLAddr;
UINT64 LOLVal;

typedef struct _loaded_kext_summary {
    CHAR8     name[64];
    UINT32    uuid;
    UINT64    address;
    UINT64    size;
    UINT64    version;
    UINT32    loadTag;
    UINT32    flags;
    UINT64    reference_list;
} OSKextLoadedKextSummary;

typedef struct _loaded_kext_summary_header {
    UINT32 version;
    UINT32 entry_size;
    UINT32 numSummaries;
    UINT32 reserved; /* explicit alignment for gdb  */
    OSKextLoadedKextSummary summaries[0];
} OSKextLoadedKextSummaryHeader;

OSKextLoadedKextSummaryHeader *loadedKextSummaries = {0};
#endif

VOID
FilterKernelPatches (
  IN LOADER_ENTRY   *Entry
) {
  if (
    gSettings.KernelPatchesAllowed &&
    (Entry->KernelAndKextPatches->KernelPatches != NULL) &&
    Entry->KernelAndKextPatches->NrKernels
  ) {
    UINTN    i = 0;

    MsgLog ("Filtering KernelPatches:\n");

    for (; i < Entry->KernelAndKextPatches->NrKernels; ++i) {
      BOOLEAN   NeedBuildVersion = (
                  (Entry->OSBuildVersion != NULL) &&
                  (Entry->KernelAndKextPatches->KernelPatches[i].MatchBuild != NULL)
                );

      MsgLog (" - [%02d]: %a | [MatchOS: %a | MatchBuild: %a]",
        i,
        Entry->KernelAndKextPatches->KernelPatches[i].Label,
        Entry->KernelAndKextPatches->KernelPatches[i].MatchOS
          ? Entry->KernelAndKextPatches->KernelPatches[i].MatchOS
          : "All",
        NeedBuildVersion
          ? Entry->KernelAndKextPatches->KernelPatches[i].MatchBuild
          : "All"
      );

      if (NeedBuildVersion) {
        Entry->KernelAndKextPatches->KernelPatches[i].Disabled = !IsPatchEnabled (
          Entry->KernelAndKextPatches->KernelPatches[i].MatchBuild, Entry->OSBuildVersion);

        MsgLog (" | Allowed: %a\n", Entry->KernelAndKextPatches->KernelPatches[i].Disabled ? "No" : "Yes");

        //if (!Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
          continue; // If user give MatchOS, should we ignore MatchOS / keep reading 'em?
        //}
      }

      Entry->KernelAndKextPatches->KernelPatches[i].Disabled = !IsPatchEnabled (
        Entry->KernelAndKextPatches->KernelPatches[i].MatchOS, Entry->OSVersion);

      MsgLog (" | Allowed: %a\n", Entry->KernelAndKextPatches->KernelPatches[i].Disabled ? "No" : "Yes");
    }
  }
}

/*
  KernelInfo->RelocBase will normally be 0
  but if OsxAptioFixDrv is used, then it will be > 0
*/
VOID
SetKernelRelocBase () {
  UINTN   DataSize = sizeof (KernelInfo->RelocBase);

  gRT->GetVariable (APTIOFIX_RELOCBASE_VARIABLE_NAME, &(APTIOFIX_RELOCBASE_VARIABLE_GUID), NULL, &DataSize, &KernelInfo->RelocBase);
  DeleteNvramVariable (APTIOFIX_RELOCBASE_VARIABLE_NAME, &(APTIOFIX_RELOCBASE_VARIABLE_GUID)); // clean up the temporary variable
}

/*
  bareBoot: https://github.com/SunnyKi/bareBoot
*/

#if 0
VOID
GetKernelVersion (
  UINT32        Addr,
  UINT32        Size,
  LOADER_ENTRY  *Entry
) {
  CHAR8   *S, *S1;
  UINTN   i, i2, i3, KvBegin;

  if (KernelInfo->Version != NULL) {
    return;
  }

  for (i = Addr; i < Addr + Size; i++) {
    if (AsciiStrnCmp ((CHAR8 *)i, "Darwin Kernel Version", 21) == 0) {
      KvBegin = i + 22;
      i2 = KvBegin;
      S = (CHAR8 *)KvBegin;

      while (AsciiStrnCmp ((CHAR8 *)i2, ":", 1) != 0) {
        i2++;
      }

      KernelInfo->Version = (CHAR8 *)AllocateZeroPool (i2 - KvBegin + 1);
      S1 = KernelInfo->Version;

      for (i3 = KvBegin; i3 < i2; i3++) {
        *S1++ = *S++;
      }

      *S1 = 0;
    }
  }
}
#endif

VOID
InitKernel () {
  struct  nlist_64            *SysTabEntry;
  struct  symtab_command      *ComSymTab;
  struct  load_command        *LoadCommand;
  struct  segment_command_64  *SegCmd64;
  struct  section_64          *Sect64;
          UINT32              NCmds, CmdSize, BinaryIndex,
                              SectionIndex, Addr, Size, Off,
                              LinkeditAddr = 0, LinkeditFileOff = 0,
                              SymOff = 0, NSyms = 0, StrOff = 0, StrSize = 0;
          UINTN               Cnt;
          UINT8               *Data = (UINT8 *)KernelInfo->Bin,
                              *SymBin, *StrBin, ISectionIndex = 0;

  BinaryIndex = sizeof (struct mach_header_64);

  NCmds = MACH_GET_NCMDS (Data);

  for (Cnt = 0; Cnt < NCmds; Cnt++) {
    LoadCommand = (struct load_command *)(Data + BinaryIndex);
    CmdSize = LoadCommand->cmdsize;

    switch (LoadCommand->cmd) {
      case LC_SEGMENT_64:
        SegCmd64 = (struct segment_command_64 *)LoadCommand;
        SectionIndex = sizeof (struct segment_command_64);

        if (SegCmd64->nsects == 0) {
          if (AsciiStrCmp (SegCmd64->segname, kLinkeditSegment) == 0) {
            LinkeditAddr = (UINT32)SegCmd64->vmaddr;
            LinkeditFileOff = (UINT32)SegCmd64->fileoff;
            //DBG ("%a: Segment = %a, Addr = 0x%x, Size = 0x%x, FileOff = 0x%x\n", __FUNCTION__,
            //  SegCmd64->segname, LinkeditAddr, SegCmd64->vmsize, LinkeditFileOff
            //);
            KernelInfo->KernelSize = (UINT32)(SegCmd64->fileoff + SegCmd64->filesize);
          }
        }

        KernelInfo->PrelinkedSize += (UINT32)SegCmd64->filesize;

        if (!ISectionIndex) {
          ISectionIndex++; // Start from 1
        }

        while (SectionIndex < SegCmd64->cmdsize) {
          Sect64 = (struct section_64 *)((UINT8 *)SegCmd64 + SectionIndex);
          SectionIndex += sizeof (struct section_64);

          if (Sect64->size > 0) {
            Addr = (UINT32)(Sect64->addr ? Sect64->addr + KernelInfo->RelocBase : 0);
            Size = (UINT32)Sect64->size;
            Off = Sect64->offset;

            if (
              (AsciiStrCmp (Sect64->segname, kPrelinkTextSegment) == 0) &&
              (AsciiStrCmp (Sect64->sectname, kPrelinkTextSection) == 0)
            ) {
              KernelInfo->PrelinkTextAddr = Addr;
              KernelInfo->PrelinkTextSize = Size;
              KernelInfo->PrelinkTextOff = Off;
              KernelInfo->PrelinkTextIndex = ISectionIndex;
            }
            else if (
              (AsciiStrCmp (Sect64->segname, kPrelinkInfoSegment) == 0) &&
              (AsciiStrCmp (Sect64->sectname, kPrelinkInfoSection) == 0)
            ) {
              KernelInfo->PrelinkInfoAddr = Addr;
              KernelInfo->PrelinkInfoSize = Size;
              KernelInfo->PrelinkInfoOff = Off;
              KernelInfo->PrelinkInfoIndex = ISectionIndex;
            }
            else if (
              (AsciiStrCmp (Sect64->segname, kKldSegment) == 0) &&
              (AsciiStrCmp (Sect64->sectname, kKldTextSection) == 0)
            ) {
              KernelInfo->KldAddr = Addr;
              KernelInfo->KldSize = Size;
              KernelInfo->KldOff = Off;
              KernelInfo->KldIndex = ISectionIndex;
            }
            else if (
              (AsciiStrCmp (Sect64->segname, kDataSegment) == 0) &&
              (AsciiStrCmp (Sect64->sectname, kDataDataSection) == 0)
            ) {
              KernelInfo->DataAddr = Addr;
              KernelInfo->DataSize = Size;
              KernelInfo->DataOff = Off;
              KernelInfo->DataIndex = ISectionIndex;
            }/*
            else if (
              (AsciiStrCmp (Sect64->segname, kDataSegment) == 0) &&
              (AsciiStrCmp (Sect64->sectname, kDataCommonSection) == 0)
            ) {
              KernelInfo->DataCommonAddr = Addr;
              KernelInfo->DataCommonSize = Size;
              KernelInfo->DataCommonOff = Off;
              KernelInfo->DataCommonIndex = ISectionIndex;
            }*/
            else if (AsciiStrCmp (Sect64->segname, kTextSegment) == 0) {
              if (AsciiStrCmp (Sect64->sectname, kTextTextSection) == 0) {
                KernelInfo->TextAddr = Addr;
                KernelInfo->TextSize = Size;
                KernelInfo->TextOff = Off;
                KernelInfo->TextIndex = ISectionIndex;
              }
              else if (AsciiStrCmp (Sect64->sectname, kTextConstSection) == 0) {
                KernelInfo->ConstAddr = Addr;
                KernelInfo->ConstSize = Size;
                KernelInfo->ConstOff = Off;
                KernelInfo->ConstIndex = ISectionIndex;
              }
#if 0
              else if (
                (AsciiStrCmp (Sect64->sectname, kTextConstSection) == 0) ||
                (AsciiStrCmp (Sect64->sectname, kTextCstringSection) == 0)
              ) {
                GetKernelVersion (Addr, Size);
              }
#endif
            }
          }

          ISectionIndex++;
        }
        break;

      case LC_SYMTAB:
        ComSymTab = (struct symtab_command *)LoadCommand;
        SymOff = ComSymTab->symoff;
        NSyms = ComSymTab->nsyms;
        StrOff = ComSymTab->stroff;
        StrSize = ComSymTab->strsize;
        //DBG ("%a: SymOff = 0x%x, NSyms = %d, StrOff = 0x%x, StrSize = %d\n", __FUNCTION__, SymOff, NSyms, StrOff, StrSize);
        break;

      default:
        break;
    }

    BinaryIndex += CmdSize;
  }

  if (ISectionIndex && (LinkeditAddr != 0) && (SymOff != 0)) {
    UINTN     CntPatches = KernelPatchSymbolLookupCount /*+ 2*/;
    CHAR8     *SymbolName = NULL;
    UINT32    PatchLocation;

    Cnt = 0;
    SymBin = (UINT8 *)(UINTN)(LinkeditAddr + (SymOff - LinkeditFileOff) + KernelInfo->RelocBase);
    StrBin = (UINT8 *)(UINTN)(LinkeditAddr + (StrOff - LinkeditFileOff) + KernelInfo->RelocBase);

    //DBG ("%a: symaddr = 0x%x, straddr = 0x%x\n", __FUNCTION__, SymBin, StrBin);

    while ((Cnt < NSyms) && CntPatches) {
      SysTabEntry = (struct nlist_64 *)(SymBin);

      if (SysTabEntry->n_value) {
        SymbolName = (CHAR8 *)(StrBin + SysTabEntry->n_un.n_strx);
        Addr = (UINT32)SysTabEntry->n_value;
        PatchLocation = Addr - (UINT32)(UINTN)KernelInfo->Bin + (UINT32)KernelInfo->RelocBase;

        if (SysTabEntry->n_sect == KernelInfo->TextIndex) {
          if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kLoadEXEStart].Name) == 0) {
            KernelInfo->LoadEXEStart = PatchLocation;
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kLoadEXEEnd].Name) == 0) {
            KernelInfo->LoadEXEEnd = PatchLocation;
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kCPUInfoStart].Name) == 0) {
            KernelInfo->CPUInfoStart = PatchLocation;
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kCPUInfoEnd].Name) == 0) {
            KernelInfo->CPUInfoEnd = PatchLocation;
            CntPatches--;
          }/*
          else if (AsciiStrCmp (SymbolName, "__ZN6OSKext25updateLoadedKextSummariesEv") == 0) {
            LOLAddr = PatchLocation;
            LOLVal = *(PTR_OFFSET (Data, PatchLocation, UINT64 *));
            CntPatches--;
          }*/
        } else if (SysTabEntry->n_sect == KernelInfo->ConstIndex) {
          if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kVersion].Name) == 0) {
            KernelInfo->Version = PTR_OFFSET (Data, PatchLocation, CHAR8 *);
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kVersionMajor].Name) == 0) {
            KernelInfo->VersionMajor = *(PTR_OFFSET (Data, PatchLocation, UINT32 *));
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kVersionMinor].Name) == 0) {
            KernelInfo->VersionMinor = *(PTR_OFFSET (Data, PatchLocation, UINT32 *));
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kRevision].Name) == 0) {
            KernelInfo->Revision = *(PTR_OFFSET (Data, PatchLocation, UINT32 *));
            CntPatches--;
          }
        } else if (SysTabEntry->n_sect == KernelInfo->DataIndex) {
          if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kXCPMStart].Name) == 0) {
            KernelInfo->XCPMStart = PatchLocation;
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kXCPMEnd].Name) == 0) {
            KernelInfo->XCPMEnd = PatchLocation;
            CntPatches--;
          }/*
        } else if (SysTabEntry->n_sect == KernelInfo->DataCommonIndex) {
          if (AsciiStrCmp (SymbolName, "_gLoadedKextSummaries") == 0) {
            loadedKextSummaries = (PTR_OFFSET (Data, PatchLocation, OSKextLoadedKextSummaryHeader *));
            CntPatches--;
          }*/
        } else if (SysTabEntry->n_sect == KernelInfo->KldIndex) {
          if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kStartupExtStart].Name) == 0) {
            KernelInfo->StartupExtStart = PatchLocation;
            CntPatches--;
          }
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kStartupExtEnd].Name) == 0) {
            KernelInfo->StartupExtEnd = PatchLocation;
            //KernelInfo->PrelinkedStart = PatchLocation;
            CntPatches--;
          }/*
          //else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kPrelinkedStart].Name) == 0) {
          //  KernelInfo->PrelinkedStart = PatchLocation;
          //  CntPatches--;
          //}
          else if (AsciiStrCmp (SymbolName, KernelPatchSymbolLookup[kPrelinkedEnd].Name) == 0) {
            KernelInfo->PrelinkedEnd = PatchLocation;
            CntPatches--;
          }*/
        }
      }

      Cnt++;
      SymBin += sizeof (struct nlist_64);
    }
  } else {
    DBG ("%a: symbol table not found\n", __FUNCTION__);
  }

  if (KernelInfo->LoadEXEStart && KernelInfo->LoadEXEEnd && (KernelInfo->LoadEXEEnd > KernelInfo->LoadEXEStart)) {
    KernelInfo->LoadEXESize = (KernelInfo->LoadEXEEnd - KernelInfo->LoadEXEStart);
  }

  if (KernelInfo->StartupExtStart && KernelInfo->StartupExtEnd && (KernelInfo->StartupExtEnd > KernelInfo->StartupExtStart)) {
    KernelInfo->StartupExtSize = (KernelInfo->StartupExtEnd - KernelInfo->StartupExtStart);
  }

  //if (KernelInfo->PrelinkedStart && KernelInfo->PrelinkedEnd && (KernelInfo->PrelinkedEnd > KernelInfo->PrelinkedStart)) {
  //  KernelInfo->PrelinkedSize = (KernelInfo->PrelinkedEnd - KernelInfo->PrelinkedStart);
  //}

  if (KernelInfo->XCPMStart && KernelInfo->XCPMEnd && (KernelInfo->XCPMEnd > KernelInfo->XCPMStart)) {
    KernelInfo->XCPMSize = (KernelInfo->XCPMEnd - KernelInfo->XCPMStart);
  }

  if (KernelInfo->CPUInfoStart && KernelInfo->CPUInfoEnd && (KernelInfo->CPUInfoEnd > KernelInfo->CPUInfoStart)) {
    KernelInfo->CPUInfoSize = (KernelInfo->CPUInfoEnd - KernelInfo->CPUInfoStart);
  }

  DBG ("PrelinkedSize: %d, KernelSize: %d\n", KernelInfo->PrelinkedSize, KernelInfo->KernelSize);
}

//Slice - FakeCPUID substitution, (c)2014

STATIC UINT8 StrMsr8b[]       = { 0xb9, 0x8b, 0x00, 0x00, 0x00, 0x0f, 0x32 };
/*
  This patch searches
    mov ecx, eax
    shr ecx, 0x04   ||  shr ecx, 0x10
  and replaces to
    mov ecx, FakeModel  || mov ecx, FakeExt
 */
//STATIC UINT8 SearchModel107[]  = { 0x89, 0xc1, 0xc1, 0xe9, 0x04 };
//STATIC UINT8 SearchExt107[]    = { 0x89, 0xc1, 0xc1, 0xe9, 0x10 };
STATIC UINT8 ReplaceModel107[] = { 0xb9, 0x07, 0x00, 0x00, 0x00 };

/*
  This patch searches
    mov bl, al     ||   shr eax, 0x10
    shr bl, 0x04   ||   and al,0x0f
  and replaces to
    mov ebx, FakeModel || mov eax, FakeExt
*/
STATIC UINT8 SearchModel109[]   = { 0x88, 0xc3, 0xc0, 0xeb, 0x04 };
STATIC UINT8 SearchExt109[]     = { 0xc1, 0xe8, 0x10, 0x24, 0x0f };
STATIC UINT8 ReplaceModel109[]  = { 0xbb, 0x0a, 0x00, 0x00, 0x00 };
STATIC UINT8 ReplaceExt109[]    = { 0xb8, 0x02, 0x00, 0x00, 0x00 };

/*
  This patch searches
    mov cl, al     ||   mov ecx, eax
    shr cl, 0x04   ||   shr ecx, 0x10
  and replaces to
    mov ecx, FakeModel || mov ecx, FakeExt
*/
STATIC UINT8 SearchModel101[]   = { 0x88, 0xc1, 0xc0, 0xe9, 0x04 };
STATIC UINT8 SearchExt101[]     = { 0x89, 0xc1, 0xc1, 0xe9, 0x10 };

BOOLEAN
PatchCPUID (
  UINT8           *Ptr,
  UINT8           *SearchModel,
  UINT8           *ReplaceModel,
  UINT8           *SearchExt,
  UINT8           *ReplaceExt,
  INT32           Len,
  UINT32          Adr,
  UINT32          Size,
  UINT32          End,
  LOADER_ENTRY    *Entry
) {
  INT32     PatchLocation = 0;
  UINT32    LenPatt = 0;
  UINT8     FakeModel = (Entry->KernelAndKextPatches->FakeCPUID >> 4) & 0x0f,
            FakeExt = (Entry->KernelAndKextPatches->FakeCPUID >> 0x10) & 0x0f;
  BOOLEAN   Ret = FALSE;

  LenPatt = sizeof (StrMsr8b);

  PatchLocation = FindBin (&Ptr[Adr], Size, &StrMsr8b[0], LenPatt);

  if (PatchLocation > 0) {
    Adr += PatchLocation;
    DBG (" - found Pattern at %x, len: %d\n", Adr, LenPatt);

    PatchLocation = FindBin (&Ptr[Adr], (End - Adr), SearchModel, Len);

    if (PatchLocation > 0) {
      Adr += PatchLocation;
      DBG (" - found Model at %x, len: %d\n", Adr, Len);
      CopyMem (&Ptr[Adr], ReplaceModel, Len);
      Ptr[Adr + 1] = FakeModel;

      PatchLocation = FindBin (&Ptr[Adr], (End - Adr), SearchExt, Len);

      if (PatchLocation > 0) {
        Adr += PatchLocation;
        DBG (" - found ExtModel at %x, len: %d\n", Adr, Len);
        CopyMem (&Ptr[Adr], ReplaceExt, Len);
        Ptr[Adr + 1] = FakeExt;
        Ret = TRUE;
      }
    }
  }

  return Ret;
}

BOOLEAN
KernelCPUIDPatch (
  LOADER_ENTRY  *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin;
  UINT32    Adr = 0, Size = 0x800000, End = 0;
  BOOLEAN   Ret = FALSE;

  DBG ("%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG (" - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->CPUInfoSize) {
    Adr = KernelInfo->CPUInfoStart;
    Size = KernelInfo->CPUInfoSize;
    End = Adr + Size;
    //DBG ("CPUInfo: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->CPUInfoStart, KernelInfo->CPUInfoEnd, KernelInfo->CPUInfoSize);
  }
  else if (KernelInfo->TextSize) {
    Adr = KernelInfo->TextOff;
    Size = KernelInfo->TextSize;
    End = Adr + Size;
  }

  if (!Adr || !Size || !End) {
    DBG (" - error 0 range\n");
    goto Finish;
  }

  //Yosemite
  if (
    PatchCPUID (
      Ptr,
      &SearchModel101[0], &ReplaceModel107[0],
      &SearchExt101[0], &ReplaceModel107[0],
      sizeof (SearchModel101), Adr, Size, End, Entry
    )
  ) {
    DBG (" - Yosemite ...done!\n");
    Ret = TRUE;
  }

  //Mavericks
  else if (
    PatchCPUID (
      Ptr,
      &SearchModel109[0], &ReplaceModel109[0],
      &SearchExt109[0], &ReplaceExt109[0],
      sizeof (SearchModel109), Adr, Size, End, Entry
    )
  ) {
    DBG (" - Mavericks ...done!\n");
    Ret = TRUE;
  }

  if (!Ret) {
    DBG (" - error 0 matches\n");
  }

  Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return Ret;
}

BOOLEAN
KernelPatchPm () {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin, *End = NULL, Count = 0;

  DBG ("%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG (" - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->XCPMSize) {
    Ptr += KernelInfo->XCPMStart;
    End = Ptr + KernelInfo->XCPMSize;
    //DBG ("XCPM: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->XCPMStart, KernelInfo->XCPMEnd, KernelInfo->XCPMSize);
  }
  else if (KernelInfo->DataSize) {
    Ptr += KernelInfo->DataOff;
    End = Ptr + KernelInfo->DataSize;
  }

  if (End == NULL) {
    End = Ptr + 0x1000000;
  }

  while ((Count < 2) && (Ptr < End)) {
    if (
      (*(PTR_OFFSET (Ptr, 0, UINT32 *)) == 0xE2) &&
      *(PTR_OFFSET (Ptr, sizeof (UINT32), UINT16 *)) &&
      !(*(PTR_OFFSET (Ptr, sizeof (UINT32) + sizeof (UINT16), UINT16 *)))
    ) {
      DBG (" - Zeroing: 0x%lx (at 0x%p)\n", *((UINT64 *)Ptr), Ptr);
      ZeroMem (Ptr, sizeof (UINT64));
      Count++;
    }

    Ptr += 16;
  }

  if (!Count) {
    DBG (" - error 0 matches\n");
  }

  Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return (Count > 0);
}

BOOLEAN
FindBootArgs (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr, ArchMode = sizeof (UINTN) * 8;
  BOOLEAN   Ret = FALSE;

  // start searching from 0x200000.
  Ptr = (UINT8 *)(UINTN)0x200000;

  while (TRUE) {
    // check bootargs for 10.7 and up
    gBootArgs = (BootArgs2 *)Ptr;

    if (
      (gBootArgs->Version == 2) && (gBootArgs->Revision == 0) &&
      // plus additional checks - some values are not inited by boot.efi yet
      (gBootArgs->efiMode == ArchMode) &&
      (gBootArgs->kaddr == 0) && (gBootArgs->ksize == 0) &&
      (gBootArgs->efiSystemTable == 0)
    ) {
      // set vars
      gDtRoot = (CHAR8 *)(UINTN)gBootArgs->deviceTreeP;
      KernelInfo->Slide = gBootArgs->kslide;

      // Find kernel Mach-O header:
      // for ML: gBootArgs->kslide + 0x00200000
      // for older versions: just 0x200000
      // for AptioFix booting - it's always at 0x200000
      KernelInfo->Bin = (VOID *)(UINTN)(KernelInfo->Slide + KernelInfo->RelocBase + 0x00200000);

      /*
      DBG ("Found gBootArgs at 0x%08x, DevTree at %p\n", Ptr, gDtRoot);
      //DBG ("gBootArgs->kaddr = 0x%08x and gBootArgs->ksize =  0x%08x\n", gBootArgs->kaddr, gBootArgs->ksize);
      //DBG ("gBootArgs->efiMode = 0x%02x\n", gBootArgs->efiMode);
      DBG ("gBootArgs->CommandLine = '%a'\n", gBootArgs->CommandLine);
      DBG ("gBootArgs->flags = 0x%x\n", gBootArgs->flags);
      DBG ("gBootArgs->kslide = 0x%x\n", gBootArgs->kslide);
      DBG ("gBootArgs->bootMemStart = 0x%x\n", gBootArgs->bootMemStart);
      */

      gBootArgs->flags = 0;
      gBootArgs->csrActiveConfig = 0;
      gBootArgs->csrCapabilities = CSR_CAPABILITY_UNLIMITED;

      if (
        //OSX_GE (Entry->OSVersion, DARWIN_OS_VER_STR_ELCAPITAN) ||
        (gSettings.BooterConfig || gSettings.CsrActiveConfig)
      ) {
        // user defined
        if (gSettings.BooterConfig) {
          gBootArgs->flags = gSettings.BooterConfig;
        }

        if (gSettings.CsrActiveConfig) {
          gBootArgs->csrActiveConfig = gSettings.CsrActiveConfig;
        }
      }

      //gBootArgs->boot_SMC_plimit = 0;

      gBootArgs->Video/* V1 */.v_display = BIT_ISSET (Entry->Flags, OSFLAG_USEGRAPHICS) ? GRAPHICS_MODE : FB_TEXT_MODE;
      gBootArgs->Video/* V1 */.v_width = GlobalConfig.UGAWidth;
      gBootArgs->Video/* V1 */.v_height = GlobalConfig.UGAHeight;
      gBootArgs->Video/* V1 */.v_depth = GlobalConfig.UGAColorDepth;
      gBootArgs->Video/* V1 */.v_rowBytes = GlobalConfig.UGABytesPerRow;
      gBootArgs->Video/* V1 */.v_baseAddr = /* (UINT32) */GlobalConfig.UGAFrameBufferBase;

      Ret = TRUE;

      break;
    }

    Ptr += EFI_PAGE_SIZE;
  }

  return Ret;
}

BOOLEAN
KernelUserPatch (
  LOADER_ENTRY  *Entry
) {
  UINTN    Num, i = 0, y = 0;

  DBG ("%a: Start\n", __FUNCTION__);

  for (i = 0; i < Entry->KernelAndKextPatches->NrKernels; ++i) {
    DBG ("KernelUserPatch[%02d]: %a", i, Entry->KernelAndKextPatches->KernelPatches[i].Label);

    if (Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
      DBG (" | DISABLED!\n");
      continue;
    }

    Num = SearchAndReplace (
            KernelInfo->Bin,
            gSettings.KernelPatchesWholePrelinked ? KernelInfo->PrelinkedSize : KernelInfo->KernelSize,
            Entry->KernelAndKextPatches->KernelPatches[i].Data,
            Entry->KernelAndKextPatches->KernelPatches[i].DataLen,
            Entry->KernelAndKextPatches->KernelPatches[i].Patch,
            Entry->KernelAndKextPatches->KernelPatches[i].Wildcard,
            Entry->KernelAndKextPatches->KernelPatches[i].Count
          );

    if (Num) {
      y++;
    }

    DBG (" | %r: %d replaces done\n", Num ? EFI_SUCCESS : EFI_NOT_FOUND, Num);
  }

  DBG ("%a: End\n", __FUNCTION__);

  return (y != 0);
}

BOOLEAN
PatchStartupExt (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin;
  UINT32    PatchLocation = 0, PatchEnd = 0;
  BOOLEAN   Ret = FALSE;

  DBG ("%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG (" - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->StartupExtSize) {
    PatchLocation = KernelInfo->StartupExtStart;
    PatchEnd = KernelInfo->StartupExtEnd;
    //DBG ("Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->StartupExtStart, KernelInfo->StartupExtEnd, KernelInfo->StartupExtSize);
  }
  else if (KernelInfo->KldSize) {
    PatchLocation = KernelInfo->KldOff;
    PatchEnd = PatchLocation + KernelInfo->KldSize;
  }

  if (!PatchEnd) {
    DBG (" - error 0 range\n");
    goto Finish;
  }

  while (PatchLocation < PatchEnd) { // PatchEnd - pattern
    if (
      (Ptr[PatchLocation] == 0xC6) &&
      (Ptr[PatchLocation + 1] == 0xE8) &&
      (Ptr[PatchLocation + 6] == 0xEB)
    ) {
      DBG (" - found at 0x%x\n", PatchLocation);
      PatchLocation += 6;
      Ptr[PatchLocation] = 0x90;
      Ptr[++PatchLocation] = 0x90;
      Ret = TRUE;
      break;
    }

    PatchLocation++;
  }

  if (!Ret) {
    DBG (" - error 0 matches\n");
  }

  Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return Ret;
}

BOOLEAN
PatchLoadEXE (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin;
  UINT32    PatchLocation = 0, PatchEnd = 0;
  BOOLEAN   Ret = FALSE;

  DBG ("%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG (" - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->LoadEXESize) {
    PatchLocation = KernelInfo->LoadEXEStart;
    PatchEnd = KernelInfo->LoadEXEEnd;
    //DBG ("LoadEXE: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->LoadEXEStart, KernelInfo->LoadEXEEnd, KernelInfo->LoadEXESize);
  }
  else if (KernelInfo->TextSize) {
    PatchLocation = KernelInfo->TextOff;
    PatchEnd = PatchLocation + KernelInfo->TextSize;
  }

  if (!PatchEnd) {
    DBG (" - error 0 range\n");
    goto Finish;
  }

  while (PatchLocation < PatchEnd) { // PatchEnd - pattern
    if (
      (Ptr[PatchLocation] == 0xC3) &&
      (Ptr[PatchLocation + 1] == 0x48) &&
      //(Ptr[PatchLocation + 2] == 0x85) &&
      //(Ptr[PatchLocation + 3] == 0xDB) &&
      (Ptr[PatchLocation + 4] == 0x74) &&
      ((Ptr[PatchLocation + 5] == 0x69) || (Ptr[PatchLocation + 5] == 0x70) || (Ptr[PatchLocation + 5] == 0x71)) &&
      (Ptr[PatchLocation + 6] == 0x48)
    ) {
      DBG (" - found at 0x%x\n", PatchLocation);
      PatchLocation += 4;

      if (TRUE) { // Experimmental: Just NOP's it?
        while (PatchLocation < PatchEnd) {
          if (
            (Ptr[PatchLocation] == 0x49) &&
            (Ptr[PatchLocation + 1] == 0x8B)
          ) {
            break;
          }
          Ptr[PatchLocation++] = 0x90;
        }
      } else { // Old: jmp skip
        Ptr[PatchLocation] = 0xEB;
        Ptr[++PatchLocation] = 0x12;
      }

      Ret = TRUE;
      break;
    }

    PatchLocation++;
  }

  if (!Ret) {
    DBG (" - error 0 matches\n");
  }

  Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return Ret;
}

/*
//
// ## Meklort's style
//

BOOLEAN
PatchPrelinked (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin;
  UINT32    PatchLocation = 0, PatchEnd = 0;
  BOOLEAN   Ret = FALSE;

  DBG ("%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG (" - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->PrelinkedSize) {
    PatchLocation = KernelInfo->PrelinkedStart;
    PatchEnd = KernelInfo->PrelinkedEnd;
    //DBG ("Prelinked: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->PrelinkedStart, KernelInfo->PrelinkedEnd, KernelInfo->PrelinkedSize);
  }
  else if (KernelInfo->KldSize) {
    PatchLocation = KernelInfo->KldOff;
    PatchEnd = PatchLocation + KernelInfo->KldSize;
  }

  if (!PatchEnd) {
    DBG (" - error 0 range\n");
    goto Finish;
  }

  //BE8400010031C0E89B11D1FF
  //
  //909090904889DFE87E050000

  while (PatchLocation < PatchEnd) { // PatchEnd - pattern
    if (
      (Ptr[PatchLocation] == 0xBE) &&
      //(Ptr[PatchLocation + 1] == 0x84) &&
      //(Ptr[PatchLocation + 2] == 0x00) &&
      //(Ptr[PatchLocation + 3] == 0x01) &&
      (Ptr[PatchLocation + 4] == 0x00) &&
      (Ptr[PatchLocation + 5] == 0x31) &&
      //(Ptr[PatchLocation + 6] == 0xC0) &&
      (Ptr[PatchLocation + 7] == 0xE8)
    ) {
      UINT32  RelAddr; // Update relative address

      DBG (" - found at 0x%x\n", PatchLocation);

      Ptr[PatchLocation] = 0x90;
      Ptr[++PatchLocation] = 0x90;
      Ptr[++PatchLocation] = 0x90;
      Ptr[++PatchLocation] = 0x90;

      Ptr[++PatchLocation] = 0x48;
      Ptr[++PatchLocation] = 0x89;
      Ptr[++PatchLocation] = 0xDF;

      ++PatchLocation; // 0xE8

      RelAddr = PatchLocation + 5;

      Ptr[++PatchLocation] = (UINT8)((PatchEnd - RelAddr) >> 0);
      Ptr[++PatchLocation] = (UINT8)((PatchEnd - RelAddr) >> 8);
      Ptr[++PatchLocation] = (UINT8)((PatchEnd - RelAddr) >> 16);
      Ptr[++PatchLocation] = (UINT8)((PatchEnd - RelAddr) >> 24);

      Ret = TRUE;
      break;
    }

    PatchLocation++;
  }

  Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return Ret;
}
*/

VOID
EFIAPI
KernelBooterExtensionsPatch (
  LOADER_ENTRY    *Entry
) {
  DBG ("%a: Start\n", __FUNCTION__);

  if (PatchStartupExt (Entry)) {
    PatchLoadEXE (Entry);
  }

  DBG ("%a: End\n", __FUNCTION__);
}

BOOLEAN
KernelAndKextPatcherInit (
  IN LOADER_ENTRY   *Entry
) {
  UINT32  Magic;

  if (KernelInfo->PatcherInited) {
    goto Finish;
  }

  DBG ("%a: Start\n", __FUNCTION__);

  SetKernelRelocBase ();

  DBG ("RelocBase = %lx\n", KernelInfo->RelocBase);

  // Find bootArgs - we need then for proper detection of kernel Mach-O header
  if (!FindBootArgs (Entry)) {
    DBG ("BootArgs not found - skipping patches!\n");
    goto Finish;
  }

  // check that it is Mach-O header and detect architecture
  Magic = MACH_GET_MAGIC (KernelInfo->Bin);

  if ((Magic == MH_MAGIC_64) || (Magic == MH_CIGAM_64)) {
    DBG ("Found 64Bit kernel at 0x%p\n", KernelInfo->Bin);
    KernelInfo->A64Bit = TRUE;
  } else {
    // not valid Mach-O header - exiting
    MsgLog ("Invalid 64Bit kernel at 0x%p - skipping patches!", KernelInfo->Bin);
    KernelInfo->Bin = NULL;
    goto Finish;
  }

  InitKernel ();

  if (KernelInfo->VersionMajor < DARWIN_KERNEL_VER_MAJOR_MINIMUM) {
    MsgLog ("Unsupported kernel version (%d.%d.%d)\n", KernelInfo->VersionMajor, KernelInfo->VersionMinor, KernelInfo->Revision);
    goto Finish;
  }

  KernelInfo->Cached = ((KernelInfo->PrelinkTextSize > 0) && (KernelInfo->PrelinkInfoSize > 0));
  DBG ("Loaded %a | VersionMajor: %d | VersionMinor: %d | Revision: %d\n",
    KernelInfo->Version, KernelInfo->VersionMajor, KernelInfo->VersionMinor, KernelInfo->Revision
  );

  DBG ("Cached: %s\n", KernelInfo->Cached ? L"Yes" : L"No");

  Finish:

  if (!KernelInfo->PatcherInited) {
    KernelInfo->PatcherInited = TRUE;
    DBG ("%a: End\n", __FUNCTION__);
  }

  return (KernelInfo->A64Bit && KernelInfo->Cached && (KernelInfo->Bin != NULL));
}

VOID
KernelAndKextsPatcherStart (
  IN LOADER_ENTRY   *Entry
) {
  BOOLEAN   KextPatchesNeeded = FALSE;

  // we will call KernelAndKextPatcherInit () only if needed
  if ((Entry == NULL) || (Entry->KernelAndKextPatches == NULL)) {
    return;
  }

  KextPatchesNeeded = (
    Entry->KernelAndKextPatches->KPKernelPm ||
    //Entry->KernelAndKextPatches->KPAppleRTC ||
    (Entry->KernelAndKextPatches->KPATIConnectorsPatch != NULL) ||
    ((Entry->KernelAndKextPatches->NrKexts > 0) && (Entry->KernelAndKextPatches->KextPatches != NULL))
  );

  KernelInfo = AllocateZeroPool (sizeof (KERNEL_INFO));

  //
  // KernelToPatch
  //

  if (
    gSettings.KernelPatchesAllowed &&
    (Entry->KernelAndKextPatches->KernelPatches != NULL) &&
    Entry->KernelAndKextPatches->NrKernels
  ) {
    if (!KernelAndKextPatcherInit (Entry)) {
      goto NoKernelData;
    }

    KernelUserPatch (Entry);
  }

  //
  // Kext FakeCPUID
  //

  if (Entry->KernelAndKextPatches->FakeCPUID) {
    if (!KernelAndKextPatcherInit (Entry)) {
      goto NoKernelData;
    }

    KernelCPUIDPatch (Entry);
  }

  //
  // Power Management
  //

  // CPU power management patch for haswell with locked msr
  if (Entry->KernelAndKextPatches->KPKernelPm) {
    gSettings.KextPatchesAllowed = TRUE;
    if (!KernelAndKextPatcherInit (Entry)) {
      goto NoKernelData;
    }

    KernelPatchPm ();
  }

  //
  // Kext patches
  //

  DBG ("Need KextPatches: %a, Allowed: %a ...\n",
    (KextPatchesNeeded ? "Yes" : "No"),
    (gSettings.KextPatchesAllowed ? "Yes" : "No")
  );

  if (KextPatchesNeeded && gSettings.KextPatchesAllowed) {
    if (!KernelAndKextPatcherInit (Entry)) {
      goto NoKernelData;
    }

    KextPatcher (Entry);
  }

  //
  // Kext add
  //

  if (BIT_ISSET (Entry->Flags, OSFLAG_WITHKEXTS)) {
    UINT32        deviceTreeP, deviceTreeLength;
    EFI_STATUS    Status;
    UINTN         DataSize = 0;

    // check if FSInject already injected kexts
    Status = gRT->GetVariable (L"FSInject.KextsInjected", &gEfiGlobalVariableGuid, NULL, &DataSize, NULL);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      // var exists - just exit
      DBG ("InjectKexts: skip, FSInject already injected\n");
      return;
    }

    if (!KernelAndKextPatcherInit (Entry)) {
      goto NoKernelData;
    }

    deviceTreeP = gBootArgs->deviceTreeP;
    deviceTreeLength = gBootArgs->deviceTreeLength;

    Status = InjectKexts (deviceTreeP, &deviceTreeLength, Entry);

    if (!EFI_ERROR (Status)) {
      KernelBooterExtensionsPatch (Entry);
    }
  }

  return;

  NoKernelData:

  DBG ("==> ERROR: in KernelAndKextPatcherInit\n");
}
