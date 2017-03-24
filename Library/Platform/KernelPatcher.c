/*
 * Original idea of patching kernel by Evan Lojewsky, 2009
 *
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 * Correction and improvements by Clover team
 */

#include <Library/Platform/KernelPatcher.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_KERNEL
#define DEBUG_KERNEL 0
#endif
#else
#ifdef DEBUG_KERNEL
#undef DEBUG_KERNEL
#endif
#define DEBUG_KERNEL DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_KERNEL, __VA_ARGS__)

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
    INTN    i = 0;

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

        MsgLog (" ==> %a\n", Entry->KernelAndKextPatches->KernelPatches[i].Disabled ? "not allowed" : "allowed");

        //if (!Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
          continue; // If user give MatchOS, should we ignore MatchOS / keep reading 'em?
        //}
      }

      Entry->KernelAndKextPatches->KernelPatches[i].Disabled = !IsPatchEnabled (
        Entry->KernelAndKextPatches->KernelPatches[i].MatchOS, Entry->OSVersion);

      MsgLog (" ==> %a\n", Entry->KernelAndKextPatches->KernelPatches[i].Disabled ? "not allowed" : "allowed");
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

  gRT->GetVariable (L"OsxAptioFixDrv-RelocBase", &gEfiGlobalVariableGuid, NULL, &DataSize, &KernelInfo->RelocBase);
  DeleteNvramVariable (L"OsxAptioFixDrv-RelocBase", &gEfiGlobalVariableGuid); // clean up the temporary variable
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
InitKernel (
  IN LOADER_ENTRY   *Entry
) {
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
          }
        }

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
                GetKernelVersion (Addr, Size, Entry);
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
STATIC UINT8 SearchModel107[]  = { 0x89, 0xc1, 0xc1, 0xe9, 0x04 };
STATIC UINT8 SearchExt107[]    = { 0x89, 0xc1, 0xc1, 0xe9, 0x10 };
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
  UINT8           *Search4,
  UINT8           *Search10,
  UINT8           *ReplaceModel,
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

    PatchLocation = FindBin (&Ptr[Adr], (End - Adr), Search4, Len);

    if (PatchLocation > 0) {
      Adr += PatchLocation;
      DBG (" - found Model at %x, len: %d\n", Adr, Len);
      CopyMem (&Ptr[Adr], ReplaceModel, Len);
      Ptr[Adr + 1] = FakeModel;

      PatchLocation = FindBin (&Ptr[Adr], (End - Adr), Search10, Len);

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
      Ptr, /*&StrMsr8b[0], sizeof (StrMsr8b),*/ &SearchModel101[0],
      &SearchExt101[0], &ReplaceModel107[0], &ReplaceModel107[0],
      sizeof (SearchModel107), Adr, Size, End, Entry
    )
  ) {
    DBG (" - Yosemite ...done!\n");
    Ret = TRUE;
  }

  //Mavericks
  else if (
    PatchCPUID (
      Ptr, /*&StrMsr8b[0], sizeof (StrMsr8b),*/ &SearchModel109[0],
      &SearchExt109[0], &ReplaceModel109[0], &ReplaceExt109[0],
      sizeof (SearchModel109), Adr, Size, End, Entry
    )
  ) {
    DBG (" - Mavericks ...done!\n");
    Ret = TRUE;
  }

  //Lion patterns
  else if (
    PatchCPUID (
      Ptr, /*&StrMsr8b[0], sizeof (StrMsr8b),*/ &SearchModel107[0],
      &SearchExt107[0], &ReplaceModel107[0], &ReplaceModel107[0],
      sizeof (SearchModel107), Adr, Size, End, Entry
    )
  ) {
    DBG (" - Lion ...done!\n");
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
KernelPatchPm (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin, *End = NULL;
  BOOLEAN   Found = FALSE, Ret = FALSE;
  UINT64    KernelPatchPMNull = 0x0000000000000000ULL;

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

  // Credits to RehabMan for the kernel patch information

  while (Ptr < End) {
    Found = Ret = FALSE;

    switch (*((UINT64 *)Ptr)) {
      case 0x00003390000000E2ULL:
      case 0x00001390000000E2ULL:
      case 0x00000190000000E2ULL:
        Found = TRUE;
        Ret = TRUE;
        break;
      case 0x0000004C000000E2ULL:
      case 0x00000002000000E2ULL:
        Found = TRUE;
        break;
    }

    if (Found) {
      DBG (" - found: 0x%lx\n", *((UINT64 *)Ptr));
      (*((UINT64 *)Ptr)) = KernelPatchPMNull;

      if (Ret) {
        goto Finish;
      }
    }

    Ptr += 16;
  }

  if (!Ret) {
    DBG (" - error 0 matches\n");
  }

Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return Ret;
}

VOID
FindBootArgs (
  IN LOADER_ENTRY   *Entry
) {
  UINT8   *Ptr, ArchMode = sizeof (UINTN) * 8;

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

      /*
        DBG ("Found gBootArgs at 0x%08x, DevTree at %p\n", Ptr, dtRoot);
        //DBG ("gBootArgs->kaddr = 0x%08x and gBootArgs->ksize =  0x%08x\n", gBootArgs->kaddr, gBootArgs->ksize);
        //DBG ("gBootArgs->efiMode = 0x%02x\n", gBootArgs->efiMode);
        DBG ("gBootArgs->CommandLine = %a\n", gBootArgs->CommandLine);
        DBG ("gBootArgs->flags = 0x%x\n", gBootArgs->flags);
        DBG ("gBootArgs->kslide = 0x%x\n", gBootArgs->kslide);
        DBG ("gBootArgs->bootMemStart = 0x%x\n", gBootArgs->bootMemStart);
      */

#ifdef NO_NVRAM_SIP
      gBootArgs->flags = kBootArgsFlagCSRActiveConfig;

      switch (Entry->LoaderType) {
        case OSTYPE_DARWIN_RECOVERY:
        case OSTYPE_DARWIN_INSTALLER:
          gBootArgs->flags |= DEF_NOSIP_BOOTER_CONFIG;
          gBootArgs->csrActiveConfig |= DEF_NOSIP_CSR_ACTIVE_CONFIG;
          break;
        default:
          // user defined?
          break;
      }

      //
      // user defined
      //

      if (gSettings.BooterConfig != 0xFFFF) {
        gBootArgs->flags |= gSettings.BooterConfig;
      }

      if (gSettings.CsrActiveConfig != 0xFFFF) {
        gBootArgs->csrActiveConfig = gSettings.CsrActiveConfig;
      }

      //gBootArgs->csrCapabilities = CSR_CAPABILITY_UNLIMITED;
      //gBootArgs->boot_SMC_plimit = 0;

      gBootArgs->Video/* V1 */.v_display = OSFLAG_ISSET(Entry->Flags, OSFLAG_USEGRAPHICS) ? GRAPHICS_MODE : FB_TEXT_MODE;
      gBootArgs->Video/* V1 */.v_width = UGAWidth;
      gBootArgs->Video/* V1 */.v_height = UGAHeight;
      gBootArgs->Video/* V1 */.v_depth = UGAColorDepth;
      gBootArgs->Video/* V1 */.v_rowBytes = UGABytesPerRow;
      gBootArgs->Video/* V1 */.v_baseAddr = /* (UINT32) */UGAFrameBufferBase;

      DeleteNvramVariable (L"csr-active-config", &gEfiAppleBootGuid);
      DeleteNvramVariable (L"bootercfg", &gEfiAppleBootGuid);
#endif

      break;
    }

    Ptr += EFI_PAGE_SIZE;
  }
}

BOOLEAN
KernelUserPatch (
  LOADER_ENTRY  *Entry
) {
  INTN    Num, i = 0, y = 0;

  DBG ("%a: Start\n", __FUNCTION__);

  for (i = 0; i < Entry->KernelAndKextPatches->NrKernels; ++i) {
    DBG ("Patch[%02d]: %a", i, Entry->KernelAndKextPatches->KernelPatches[i].Label);

    if (Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
      DBG (" | DISABLED!\n");
      continue;
    }

    /*
      Num = SearchAndCount (
        KernelInfo->Bin,
        KERNEL_MAX_SIZE,
        Entry->KernelAndKextPatches->KernelPatches[i].Data,
        Entry->KernelAndKextPatches->KernelPatches[i].DataLen
      );

      if (!Num) {
        DBG (" | pattern (s) not found.\n");
        continue;
      }
    */

    Num = SearchAndReplace (
      KernelInfo->Bin,
      KERNEL_MAX_SIZE,
      Entry->KernelAndKextPatches->KernelPatches[i].Data,
      Entry->KernelAndKextPatches->KernelPatches[i].DataLen,
      Entry->KernelAndKextPatches->KernelPatches[i].Patch,
      Entry->KernelAndKextPatches->KernelPatches[i].Wildcard,
      Entry->KernelAndKextPatches->KernelPatches[i].Count
    );

    if (Num) {
      y++;
    }

    DBG (" | %a : %d replaces done\n", Num ? "Success" : "Error", Num);
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
      ((Ptr[PatchLocation + 5] == 0x70) || (Ptr[PatchLocation + 5] == 0x71)) &&
      (Ptr[PatchLocation + 6] == 0x48)
    ) {
      DBG (" - found at 0x%x\n", PatchLocation);
      PatchLocation += 4;

      if (TRUE) { // Just NOP's it?
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
## Meklort's style

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

#if 0
////////////////////////////////////
//
// KernelBooterExtensionsPatch to load extra kexts besides kernelcache
//
//
// -- StartupExt
UINT8 KBStartupExt_S1[]   = { 0xC6, 0xE8, 0x0C, 0xFD, 0xFF, 0xFF, 0xEB, 0x08, 0x48, 0x89, 0xDF }; // 10.7
UINT8 KBStartupExt_R1[]   = { 0xC6, 0xE8, 0x0C, 0xFD, 0xFF, 0xFF, 0x90, 0x90, 0x48, 0x89, 0xDF };

UINT8 KBStartupExt_S2[]   = { 0xC6, 0xE8, 0x30, 0x00, 0x00, 0x00, 0xEB, 0x08, 0x48, 0x89, 0xDF }; // 10.8 - 10.9
UINT8 KBStartupExt_R2[]   = { 0xC6, 0xE8, 0x30, 0x00, 0x00, 0x00, 0x90, 0x90, 0x48, 0x89, 0xDF };

UINT8 KBStartupExt_S3[]   = { 0xC6, 0xE8, 0x25, 0x00, 0x00, 0x00, 0xEB, 0x05, 0xE8, 0xCE, 0x02 }; // 10.10 - 10.11
UINT8 KBStartupExt_R3[]   = { 0xC6, 0xE8, 0x25, 0x00, 0x00, 0x00, 0x90, 0x90, 0xE8, 0xCE, 0x02 };

UINT8 KBStartupExt_S4[]   = { 0xC6, 0xE8, 0x25, 0x00, 0x00, 0x00, 0xEB, 0x05, 0xE8, 0x7E, 0x05 }; // 10.12
UINT8 KBStartupExt_R4[]   = { 0xC6, 0xE8, 0x25, 0x00, 0x00, 0x00, 0x90, 0x90, 0xE8, 0x7E, 0x05 };

// -- LoadExec + Entitlement
UINT8 KBLoadExec_S1[]     = { 0xC3, 0x48, 0x85, 0xDB, 0x74, 0x70, 0x48, 0x8B, 0x03, 0x48, 0x89, 0xDF, 0xFF, 0x50, 0x28, 0x48 }; // 10.11
UINT8 KBLoadExec_R1[]     = { 0xC3, 0x48, 0x85, 0xDB, 0xEB, 0x12, 0x48, 0x8B, 0x03, 0x48, 0x89, 0xDF, 0xFF, 0x50, 0x28, 0x48 };

UINT8 KBLoadExec_S2[]     = { 0xC3, 0x48, 0x85, 0xDB, 0x74, 0x71, 0x48, 0x8B, 0x03, 0x48, 0x89, 0xDF, 0xFF, 0x50, 0x28, 0x48 }; // 10.12
UINT8 KBLoadExec_R2[]     = { 0xC3, 0x48, 0x85, 0xDB, 0xEB, 0x12, 0x48, 0x8B, 0x03, 0x48, 0x89, 0xDF, 0xFF, 0x50, 0x28, 0x48 };

//
// We can not rely on OSVersion global variable for OS version detection,
// since in some cases it is not correct (install of ML from Lion, for example).
// So, we'll use "brute-force" method - just try to patch.
// Actually, we'll at least check that if we can find only one instance of code that
// we are planning to patch.
//
VOID
EFIAPI
KernelBooterExtensionsPatchBruteForce (
  LOADER_ENTRY    *Entry
) {
  UINTN   Num = 0;

  if (!KernelInfo->A64Bit) {
    return;
  }

  DBG ("Patching kernel for injected kexts:\n");

  // LoadExec + Entitlement
  Num = FSearchReplace (KernelInfo->TextAddr, KernelInfo->TextSize, KBLoadExec_S2, KBLoadExec_R2) +
        FSearchReplace (KernelInfo->TextAddr, KernelInfo->TextSize, KBLoadExec_S1, KBLoadExec_R1);

  // StartupExt
  if (Num) { // >= 10.11
    Num += FSearchReplace (KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S4, KBStartupExt_R4) +
           FSearchReplace (KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S3, KBStartupExt_R3);
    DBG ("==> OS: >= 10.11\n");
  } else { // <= 10.10
    Num = FSearchReplace (KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S3, KBStartupExt_R3) +
          FSearchReplace (KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S2, KBStartupExt_R2) +
          FSearchReplace (KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S1, KBStartupExt_R1);
    DBG ("==> OS: <= 10.10\n");
  }

  DBG_("==> %a : %d replaces done\n", Num ? "Success" : "Error", Num);
}
#endif

#if 0
/**
cc: @bs0d

Here Im trying to create 'trampoline' like, but just FAILED, to catch all kexts after fully loaded
(in installer / recovery mode) and then patch it like Lilu (https://github.com/vit9696/Lilu).

Method below was taken from UEFI-Bootkit for Windows (https://github.com/dude719/UEFI-Bootkit).
**/

//
// TargetCall hook
//
typedef EFI_STATUS(EFIAPI *tTargetCall)(VOID);

UINT8         OriginalCallPatterns[] = { 0xE8, 0xCC, 0x7C, 0x00, 0x00, 0x85, 0xC0, 0x0F, 0x84, 0x91, 0x00, 0x00, 0x00 };

UINT8         *TargetCallPatchLocation = NULL;
UINT8         TargetCallBackup[5] = { 0 };
tTargetCall   oTargetCall = NULL;

//ffffff8000842f91 488D3558662C00     lea        rsi, qword [ds:0xffffff8000b095f0]
//ffffff8000842f98 E8137C0000         call       sub_ffffff800084abb0
//ffffff8000842f9d 85C0               test       eax, eax
//ffffff8000842f9f 0F8491000000       je         0xffffff8000843036

//
// Our TargetCall hook which takes the winload Image Base as a parameter so we can patch the kernel
//
EFI_STATUS
EFIAPI
OnCalledFromKernel (VOID) {
  // Restore original bytes to call
  CopyMem (TargetCallPatchLocation, TargetCallBackup, 5);
/*
  // Clear the screen
  gST->ConOut->ClearScreen (gST->ConOut);

  Print( L"Called..." );
  //DBG_PAUSE (Entry, 10);

  // Clear screen
  gST->ConOut->ClearScreen (gST->ConOut);
*/

  return oTargetCall();
}

VOID
PatchKeat (
  LOADER_ENTRY    *Entry
) {
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       *Found = NULL;

  // Find right location to patch
  Status = FindPatternAddr (
    OriginalCallPatterns,
    0xCC,
    sizeof (OriginalCallPatterns),
    KernelInfo->Bin + KernelInfo->TextOff,
    KernelInfo->TextSize,
    (VOID**)&Found
  );

  if (!EFI_ERROR (Status)) {
    // Found address, now let's do our patching
    UINT32  NewCallRelative = 0, PatchLocation = 0;

    DBG ("Found TargetCall at %lx\n", Found);

    // Save original call
    oTargetCall = (tTargetCall)UtilCallAddress (Found);

    // Backup original bytes and patch location before patching
    TargetCallPatchLocation = (VOID*)Found;
    CopyMem (TargetCallBackup, TargetCallPatchLocation, 5);

    // Patch call to jump to our OnCalledFromKernel hook
    NewCallRelative = UtilCalcRelativeCallOffset ((VOID*)Found, (VOID*)&OnCalledFromKernel);

    //DBG ("1: %02x - ",  (UINT8)Found[PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x\n",      (UINT8)Found[++PatchLocation]);

    //Found
    //*(UINT8*)Found = 0xE8; // Write call opcode
    *(UINT32*)(Found + 1) = NewCallRelative; // Write the new relative call offset

    //PatchLocation = 0;
    //DBG ("2: %02x - ",  (UINT8)Found[PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x - ",     (UINT8)Found[++PatchLocation]);
    //DBG ("%02x\n",      (UINT8)Found[++PatchLocation]);
  }
}
#endif

VOID
EFIAPI
KernelBooterExtensionsPatch (
  LOADER_ENTRY    *Entry
) {
  DBG ("%a: Start\n", __FUNCTION__);

  if (PatchStartupExt (Entry)) {
    PatchLoadEXE (Entry);
  }/* else {
    KernelBooterExtensionsPatchBruteForce (Entry);
  }*/

  //PatchKeat (Entry);

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

  KernelInfo->PatcherInited = TRUE;

  SetKernelRelocBase ();

  DBG ("RelocBase = %lx\n", KernelInfo->RelocBase);

  // Find bootArgs - we need then for proper detection of kernel Mach-O header
  FindBootArgs (Entry);

  if (gBootArgs == NULL) {
    DBG ("BootArgs not found - skipping patches!\n");
    goto Finish;
  }

  // Find kernel Mach-O header:
  // for ML: gBootArgs->kslide + 0x00200000
  // for older versions: just 0x200000
  // for AptioFix booting - it's always at KernelInfo->RelocBase + 0x200000
  KernelInfo->Bin = (VOID *)(UINTN)(KernelInfo->Slide + KernelInfo->RelocBase + 0x00200000);

  // check that it is Mach-O header and detect architecture
  Magic = MACH_GET_MAGIC (KernelInfo->Bin);

  if ((Magic == MH_MAGIC_64) || (Magic == MH_CIGAM_64)) {
    DBG ("Found 64 bit kernel at 0x%p\n", KernelInfo->Bin);
    KernelInfo->A64Bit = TRUE;
  } else {
    // not valid Mach-O header - exiting
    DBG ("64Bit Kernel not found at 0x%p - skipping patches!", KernelInfo->Bin);
    KernelInfo->Bin = NULL;
    goto Finish;
  }

  InitKernel (Entry);

  if (KernelInfo->VersionMajor < DARWIN_KERNEL_VER_MAJOR_MINIMUM) {
    MsgLog ("Unsupported kernel version (%d.%d.%d)\n", KernelInfo->VersionMajor, KernelInfo->VersionMinor, KernelInfo->Revision);
    goto Finish;
  }

  KernelInfo->Cached = ((KernelInfo->PrelinkTextSize > 0) && (KernelInfo->PrelinkInfoSize > 0));
  DBG ("Loaded %a | VersionMajor: %d | VersionMinor: %d | Revision: %d\n",
    KernelInfo->Version, KernelInfo->VersionMajor, KernelInfo->VersionMinor, KernelInfo->Revision
  );

  DBG ("Cached: %s\n", KernelInfo->Cached ? L"Yes" : L"No");
  DBG ("%a: End\n", __FUNCTION__);

Finish:

  return (KernelInfo->A64Bit && (KernelInfo->Bin != NULL));
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
    Entry->KernelAndKextPatches->KPAsusAICPUPM ||
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
    if (!KernelAndKextPatcherInit (Entry)) {
      goto NoKernelData;
    }

    KernelPatchPm (Entry);
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

  //DBG_PAUSE (Entry, 5);

  //
  // Kext add
  //

  if (OSFLAG_ISSET (Entry->Flags, OSFLAG_WITHKEXTS)) {
    UINT32        deviceTreeP, deviceTreeLength;
    EFI_STATUS    Status;
    UINTN         DataSize;

    // check if FSInject already injected kexts
    DataSize = 0;
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
