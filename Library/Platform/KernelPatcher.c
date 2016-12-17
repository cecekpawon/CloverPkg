/*
 * Original idea of patching kernel by Evan Lojewsky, 2009
 *
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 * Correction and improvements by Clover team
 */

#include <Library/Platform/KernelPatcher.h>

//#define KERNEL_DEBUG 0

//#if KERNEL_DEBUG
//#define DBG(...)    AsciiPrint(__VA_ARGS__);
//#else
//#define DBG(...)
//#endif

// runtime debug
#define DBG_ON(entry) \
  ((entry != NULL) && (entry->KernelAndKextPatches != NULL) \
  /*&& entry->KernelAndKextPatches->KPDebug*/ && (OSFLAG_ISSET(gSettings.FlagsBits, OSFLAG_DBGPATCHES) || gSettings.DebugKP))
#define DBG_RT(entry, ...) \
  if (DBG_ON(entry)) AsciiPrint(__VA_ARGS__)
#define DBG_PAUSE(entry, s) \
  if (DBG_ON(entry)) gBS->Stall(s * 1000000)

BootArgs2     *bootArgs2 = NULL;
CHAR8         *dtRoot = NULL;

KERNEL_INFO   *KernelInfo;


VOID
SetKernelRelocBase() {
  UINTN   DataSize = sizeof(KernelInfo->RelocBase);

  // OsxAptioFixDrv will set this
  gRT->GetVariable(L"OsxAptioFixDrv-RelocBase", &gEfiAppleBootGuid, NULL, &DataSize, &KernelInfo->RelocBase);
  DeleteNvramVariable(L"OsxAptioFixDrv-RelocBase", &gEfiAppleBootGuid); // clean up the temporary variable
  // KernelInfo->RelocBase is now either read or 0

  return;
}

/*
  bareBoot: https://github.com/SunnyKi/bareBoot
*/

#if 0
VOID
GetKernelVersion (
  UINT32        addr,
  UINT32        size,
  LOADER_ENTRY  *Entry
) {
  CHAR8   *s, *s1;
  UINTN   i, i2, i3, kvBegin;

  if (KernelInfo->Version != NULL) {
    return;
  }

  for (i = addr; i < addr + size; i++) {
    if (AsciiStrnCmp ((CHAR8 *) i, "Darwin Kernel Version", 21) == 0) {
      kvBegin = i + 22;
      i2 = kvBegin;
      s = (CHAR8 *) kvBegin;

      while (AsciiStrnCmp ((CHAR8 *) i2, ":", 1) != 0) {
        i2++;
      }

      KernelInfo->Version = (CHAR8 *) AllocateZeroPool (i2 - kvBegin + 1);
      s1 = KernelInfo->Version;

      for (i3 = kvBegin; i3 < i2; i3++) {
        *s1++ = *s++;
      }

      *s1 = 0;
    }
  }
}
#endif

VOID
InitKernel (
  IN LOADER_ENTRY   *Entry
) {
          UINT32              ncmds, cmdsize, binaryIndex,
                              sectionIndex, Addr, Size, Off,
                              linkeditaddr = 0, linkeditfileoff = 0,
                              symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
          UINTN               cnt;
          UINT8               *Data = (UINT8*)KernelInfo->Bin,
                              *symbin, *strbin;
  struct  nlist_64            *systabentry;
  struct  symtab_command      *comSymTab;
  struct  load_command        *loadCommand;
  struct  segment_command_64  *segCmd64;
  struct  section_64          *sect64;

  binaryIndex = sizeof(struct mach_header_64);

  ncmds = MACH_GET_NCMDS(Data);

  for (cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct load_command *)(Data + binaryIndex);
    cmdsize = loadCommand->cmdsize;

    switch (loadCommand->cmd) {
      case LC_SEGMENT_64:
        segCmd64 = (struct segment_command_64 *)loadCommand;
        sectionIndex = sizeof(struct segment_command_64);

        if (segCmd64->nsects == 0) {
          if (AsciiStrCmp (segCmd64->segname, kLinkeditSegment) == 0) {
            linkeditaddr = (UINT32) segCmd64->vmaddr;
            linkeditfileoff = (UINT32) segCmd64->fileoff;
            //DBG_RT(Entry, "%a: Segment = %a, Addr = 0x%x, Size = 0x%x, FileOff = 0x%x\n", __FUNCTION__,
            //  segCmd64->segname, linkeditaddr, segCmd64->vmsize, linkeditfileoff
            //);
          }
        }

        while (sectionIndex < segCmd64->cmdsize) {
          sect64 = (struct section_64 *)((UINT8*)segCmd64 + sectionIndex);
          sectionIndex += sizeof(struct section_64);

          if (sect64->size > 0) {
            Addr = (UINT32)(sect64->addr ? sect64->addr + KernelInfo->RelocBase : 0);
            Size = (UINT32)sect64->size;
            Off = sect64->offset;

            if (
              (AsciiStrCmp(sect64->segname, kPrelinkTextSegment) == 0) &&
              (AsciiStrCmp(sect64->sectname, kPrelinkTextSection) == 0)
            ) {
              KernelInfo->PrelinkTextAddr = Addr;
              KernelInfo->PrelinkTextSize = Size;
            }
            else if (
              (AsciiStrCmp(sect64->segname, kPrelinkInfoSegment) == 0) &&
              (AsciiStrCmp(sect64->sectname, kPrelinkInfoSection) == 0)
            ) {
              KernelInfo->PrelinkInfoAddr = Addr;
              KernelInfo->PrelinkInfoSize = Size;
            }
            else if (
              (AsciiStrCmp(sect64->segname, kKldSegment) == 0) &&
              (AsciiStrCmp(sect64->sectname, kKldTextSection) == 0)
            ) {
              KernelInfo->KldAddr = Addr;
              KernelInfo->KldSize = Size;
              KernelInfo->KldOff = Off;
            }
            else if (
              (AsciiStrCmp(sect64->segname, kDataSegment) == 0) &&
              (AsciiStrCmp(sect64->sectname, kDataDataSection) == 0)
            ) {
              KernelInfo->DataAddr = Addr;
              KernelInfo->DataSize = Size;
              KernelInfo->DataOff = Off;
            }
            else if (AsciiStrCmp(sect64->segname, kTextSegment) == 0) {
              if (AsciiStrCmp(sect64->sectname, kTextTextSection) == 0) {
                KernelInfo->TextAddr = Addr;
                KernelInfo->TextSize = Size;
                KernelInfo->TextOff = Off;
              }
#if 0
              else if (
                (AsciiStrCmp(sect64->sectname, kTextConstSection) == 0) ||
                (AsciiStrCmp(sect64->sectname, kTextCstringSection) == 0)
              ) {
                GetKernelVersion(Addr, Size, Entry);
              }
#endif
            }
          }
        }
        break;

      case LC_SYMTAB:
        comSymTab = (struct symtab_command *) loadCommand;
        symoff = comSymTab->symoff;
        nsyms = comSymTab->nsyms;
        stroff = comSymTab->stroff;
        strsize = comSymTab->strsize;
        //DBG_RT(Entry, "%a: symoff = 0x%x, nsyms = %d, stroff = 0x%x, strsize = %d\n", __FUNCTION__, symoff, nsyms, stroff, strsize);
        break;

      default:
        break;
    }

    binaryIndex += cmdsize;
  }

  if ((linkeditaddr != 0) && (symoff != 0)) {
    UINTN     CntPatches = 12; // Max get / patches values. TODO: to use bits like Revoboot
    CHAR8     *symbolName = NULL;
    UINT32    patchLocation;

    cnt = 0;
    symbin = (UINT8 *)(UINTN) (linkeditaddr + (symoff - linkeditfileoff) + KernelInfo->RelocBase);
    strbin = (UINT8 *)(UINTN) (linkeditaddr + (stroff - linkeditfileoff) + KernelInfo->RelocBase);

    //DBG_RT(Entry, "%a: symaddr = 0x%x, straddr = 0x%x\n", __FUNCTION__, symbin, strbin);

    while ((cnt < nsyms) && CntPatches) {
      systabentry = (struct nlist_64 *) (symbin);

      if (systabentry->n_value) {
        symbolName = (CHAR8 *) (strbin + systabentry->n_un.n_strx);
        Addr = (UINT32) systabentry->n_value;
        patchLocation = Addr - (UINT32)(UINTN)KernelInfo->Bin + (UINT32)KernelInfo->RelocBase;

        switch (systabentry->n_sect) {
          case 1: // __TEXT, __text
            if (AsciiStrCmp (symbolName, "__ZN6OSKext14loadExecutableEv") == 0) {
              KernelInfo->LoadEXEStart = patchLocation;
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "__ZN6OSKext23jettisonLinkeditSegmentEv") == 0) {
              KernelInfo->LoadEXEEnd = patchLocation;
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "_cpuid_set_info") == 0) {
              KernelInfo->CPUInfoStart = patchLocation;
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "_cpuid_info") == 0) {
              KernelInfo->CPUInfoEnd = patchLocation;
              CntPatches--;
            }
            break;

          case 2: // __TEXT, __const
            if (AsciiStrCmp (symbolName, "_version") == 0) {
              KernelInfo->Version = PTR_OFFSET (Data, patchLocation, CHAR8 *);
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "_version_major") == 0) {
              KernelInfo->VersionMajor = *(PTR_OFFSET (Data, patchLocation, UINT32 *));
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "_version_minor") == 0) {
              KernelInfo->VersionMinor = *(PTR_OFFSET (Data, patchLocation, UINT32 *));
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "_version_revision") == 0) {
              KernelInfo->Revision = *(PTR_OFFSET (Data, patchLocation, UINT32 *));
              CntPatches--;
            }
            break;

          case 8: // __DATA, __data
            if (AsciiStrCmp (symbolName, "_xcpm_core_scope_msrs") == 0) {
              KernelInfo->XCPMStart = patchLocation;
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "_xcpm_SMT_scope_msrs") == 0) {
              KernelInfo->XCPMEnd = patchLocation;
              CntPatches--;
            }

          case 25: // __KLD, __text
            if (AsciiStrCmp (symbolName, "__ZN12KLDBootstrap21readStartupExtensionsEv") == 0) {
              KernelInfo->StartupExtStart = patchLocation;
              CntPatches--;
            }
            else if (AsciiStrCmp (symbolName, "__ZN12KLDBootstrap23readPrelinkedExtensionsEP10section_64") == 0) {
              KernelInfo->StartupExtEnd = patchLocation;
              CntPatches--;
            }
            break;
        }
      }

      cnt++;
      symbin += sizeof (struct nlist_64);
    }
  } /*else {
    DBG_RT(Entry, "%a: symbol table not found\n", __FUNCTION__);
  }*/

  if (KernelInfo->LoadEXEStart && KernelInfo->LoadEXEEnd && (KernelInfo->LoadEXEEnd > KernelInfo->LoadEXEStart)) {
    KernelInfo->LoadEXESize = (KernelInfo->LoadEXEEnd - KernelInfo->LoadEXEStart);
  }

  if (KernelInfo->StartupExtStart && KernelInfo->StartupExtEnd && (KernelInfo->StartupExtEnd > KernelInfo->StartupExtStart)) {
    KernelInfo->StartupExtSize = (KernelInfo->StartupExtEnd - KernelInfo->StartupExtStart);
  }

  if (KernelInfo->XCPMStart && KernelInfo->XCPMEnd && (KernelInfo->XCPMEnd > KernelInfo->XCPMStart)) {
    KernelInfo->XCPMSize = (KernelInfo->XCPMEnd - KernelInfo->XCPMStart);
  }

  if (KernelInfo->CPUInfoStart && KernelInfo->CPUInfoEnd && (KernelInfo->CPUInfoEnd > KernelInfo->CPUInfoStart)) {
    KernelInfo->CPUInfoSize = (KernelInfo->CPUInfoEnd - KernelInfo->CPUInfoStart);
  }

  //DBG_PAUSE(Entry, 10);
  //return;
}

//Slice - FakeCPUID substitution, (c)2014

STATIC UINT8 StrMsr8b[]       = {0xb9, 0x8b, 0x00, 0x00, 0x00, 0x0f, 0x32};

/*
 This patch searches
  mov ecx, eax
  shr ecx, 0x04   ||  shr ecx, 0x10
 and replaces to
  mov ecx, FakeModel  || mov ecx, FakeExt
 */
STATIC UINT8 SearchModel107[]  = {0x89, 0xc1, 0xc1, 0xe9, 0x04};
STATIC UINT8 SearchExt107[]    = {0x89, 0xc1, 0xc1, 0xe9, 0x10};
STATIC UINT8 ReplaceModel107[] = {0xb9, 0x07, 0x00, 0x00, 0x00};

/*
 This patch searches
  mov bl, al     ||   shr eax, 0x10
  shr bl, 0x04   ||   and al,0x0f
 and replaces to
  mov ebx, FakeModel || mov eax, FakeExt
*/
STATIC UINT8 SearchModel109[]   = {0x88, 0xc3, 0xc0, 0xeb, 0x04};
STATIC UINT8 SearchExt109[]     = {0xc1, 0xe8, 0x10, 0x24, 0x0f};
STATIC UINT8 ReplaceModel109[]  = {0xbb, 0x0a, 0x00, 0x00, 0x00};
STATIC UINT8 ReplaceExt109[]    = {0xb8, 0x02, 0x00, 0x00, 0x00};

/*
 This patch searches
  mov cl, al     ||   mov ecx, eax
  shr cl, 0x04   ||   shr ecx, 0x10
 and replaces to
  mov ecx, FakeModel || mov ecx, FakeExt
*/
STATIC UINT8 SearchModel101[]   = {0x88, 0xc1, 0xc0, 0xe9, 0x04};
STATIC UINT8 SearchExt101[]     = {0x89, 0xc1, 0xc1, 0xe9, 0x10};

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
  INT32     patchLocation = 0;
  UINT32    LenPatt = 0;
  UINT8     FakeModel = (Entry->KernelAndKextPatches->FakeCPUID >> 4) & 0x0f,
            FakeExt = (Entry->KernelAndKextPatches->FakeCPUID >> 0x10) & 0x0f;
  BOOLEAN   Ret = FALSE;

  LenPatt = sizeof(StrMsr8b);

  patchLocation = FindBin(&Ptr[Adr], Size, &StrMsr8b[0], LenPatt);

  if (patchLocation > 0) {
    Adr += patchLocation;
    DBG_RT(Entry, " - found Pattern at %x, len: %d\n", Adr, LenPatt);

    patchLocation = FindBin(&Ptr[Adr], (End - Adr), Search4, Len);

    if (patchLocation > 0) {
      Adr += patchLocation;
      DBG_RT(Entry, " - found Model at %x, len: %d\n", Adr, Len);
      CopyMem(&Ptr[Adr], ReplaceModel, Len);
      Ptr[Adr + 1] = FakeModel;

      patchLocation = FindBin(&Ptr[Adr], (End - Adr), Search10, Len);

      if (patchLocation > 0) {
        Adr += patchLocation;
        DBG_RT(Entry, " - found ExtModel at %x, len: %d\n", Adr, Len);
        CopyMem(&Ptr[Adr], ReplaceExt, Len);
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

  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG_RT(Entry, " - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->CPUInfoSize) {
    Adr = KernelInfo->CPUInfoStart;
    Size = KernelInfo->CPUInfoSize;
    End = Adr + Size;
    //DBG_RT(Entry, "CPUInfo: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->CPUInfoStart, KernelInfo->CPUInfoEnd, KernelInfo->CPUInfoSize);
  }
  else if (KernelInfo->TextSize) {
    Adr = KernelInfo->TextOff;
    Size = KernelInfo->TextSize;
    End = Adr + Size;
  }

  if (!Adr || !Size || !End) {
    DBG_RT(Entry, " - error 0 range\n");
    goto Finish;
  }

  //Yosemite
  if (
    PatchCPUID (
      Ptr, /*&StrMsr8b[0], sizeof(StrMsr8b),*/ &SearchModel101[0],
      &SearchExt101[0], &ReplaceModel107[0], &ReplaceModel107[0],
      sizeof(SearchModel107), Adr, Size, End, Entry
    )
  ) {
    DBG_RT(Entry, " - Yosemite ...done!\n");
    Ret = TRUE;
  }

  //Mavericks
  else if (
    PatchCPUID (
      Ptr, /*&StrMsr8b[0], sizeof(StrMsr8b),*/ &SearchModel109[0],
      &SearchExt109[0], &ReplaceModel109[0], &ReplaceExt109[0],
      sizeof(SearchModel109), Adr, Size, End, Entry
    )
  ) {
    DBG_RT(Entry, " - Mavericks ...done!\n");
    Ret = TRUE;
  }

  //Lion patterns
  else if (
    PatchCPUID (
      Ptr, /*&StrMsr8b[0], sizeof(StrMsr8b),*/ &SearchModel107[0],
      &SearchExt107[0], &ReplaceModel107[0], &ReplaceModel107[0],
      sizeof(SearchModel107), Adr, Size, End, Entry
    )
  ) {
    DBG_RT(Entry, " - Lion ...done!\n");
    Ret = TRUE;
  }

  if (!Ret) {
    DBG_RT(Entry, " - error 0 matches\n");
  }

Finish:

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
  //DBG_PAUSE(Entry, 10);

  return Ret;
}

#define KERNEL_PATCH_PM_NULL    0x0000000000000000ULL

BOOLEAN
KernelPatchPm (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin, *End = NULL;
  BOOLEAN   Found = FALSE, Ret = FALSE;

  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG_RT(Entry, " - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->XCPMSize) {
    Ptr += KernelInfo->XCPMStart;
    End = Ptr + KernelInfo->XCPMSize;
    //DBG_RT(Entry, "XCPM: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->XCPMStart, KernelInfo->XCPMEnd, KernelInfo->XCPMSize);
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
      DBG_RT(Entry, " - found: 0x%lx\n", *((UINT64 *)Ptr));
      (*((UINT64 *)Ptr)) = KERNEL_PATCH_PM_NULL;

      if (Ret) {
        goto Finish;
      }
    }

    Ptr += 16;
  }

  if (!Ret) {
    DBG_RT(Entry, " - error 0 matches\n");
  }

Finish:

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
  //DBG_PAUSE(Entry, 10);

  return Ret;
}

VOID
FindBootArgs (
  IN LOADER_ENTRY   *Entry
) {
  UINT8   *ptr, archMode = sizeof(UINTN) * 8;

  // start searching from 0x200000.
  ptr = (UINT8*)(UINTN)0x200000;

  while (TRUE) {
    // check bootargs for 10.7 and up
    bootArgs2 = (BootArgs2*)ptr;

    if (
      (bootArgs2->Version == 2) && (bootArgs2->Revision == 0) &&
      // plus additional checks - some values are not inited by boot.efi yet
      (bootArgs2->efiMode == archMode) &&
      (bootArgs2->kaddr == 0) && (bootArgs2->ksize == 0) &&
      (bootArgs2->efiSystemTable == 0)
    ) {
      // set vars
      dtRoot = (CHAR8*)(UINTN)bootArgs2->deviceTreeP;
      KernelInfo->Slide = bootArgs2->kslide;
      /*
        DBG_RT(Entry, "Found bootArgs2 at 0x%08x, DevTree at %p\n", ptr, dtRoot);
        //DBG_RT(Entry, "bootArgs2->kaddr = 0x%08x and bootArgs2->ksize =  0x%08x\n", bootArgs2->kaddr, bootArgs2->ksize);
        //DBG_RT(Entry, "bootArgs2->efiMode = 0x%02x\n", bootArgs2->efiMode);
        DBG_RT(Entry, "bootArgs2->CommandLine = %a\n", bootArgs2->CommandLine);
        DBG_RT(Entry, "bootArgs2->flags = 0x%x\n", bootArgs2->flags);
        DBG_RT(Entry, "bootArgs2->kslide = 0x%x\n", bootArgs2->kslide);
        DBG_RT(Entry, "bootArgs2->bootMemStart = 0x%x\n", bootArgs2->bootMemStart);
        DBG_PAUSE(Entry, 2);
      */
      break;
    }

    ptr += 0x1000;
  }
}

BOOLEAN
KernelUserPatch (
  LOADER_ENTRY  *Entry
) {
  INTN    Num, i = 0, y = 0;

  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  for (; i < Entry->KernelAndKextPatches->NrKernels; ++i) {
    DBG_RT(Entry, "Patch[%02d]: %a\n", i, Entry->KernelAndKextPatches->KernelPatches[i].Label);

    if (Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
      DBG_RT(Entry, "==> DISABLED!\n");
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
        DBG_RT(Entry, "==> pattern(s) not found.\n");
        continue;
      }
    */

    Num = SearchAndReplace (
      KernelInfo->Bin,
      KERNEL_MAX_SIZE,
      Entry->KernelAndKextPatches->KernelPatches[i].Data,
      Entry->KernelAndKextPatches->KernelPatches[i].DataLen,
      Entry->KernelAndKextPatches->KernelPatches[i].Patch,
      Entry->KernelAndKextPatches->KernelPatches[i].Count
    );

    if (Num) {
      y++;
    }

    DBG_RT(Entry, "==> %a : %d replaces done\n", Num ? "Success" : "Error", Num);
  }

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);

  return (y != 0);
}

BOOLEAN
PatchStartupExt (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin;
  UINT32    patchLocation = 0, patchEnd = 0;
  BOOLEAN   Ret = FALSE;

  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG_RT(Entry, " - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->StartupExtSize) {
    patchLocation = KernelInfo->StartupExtStart;
    patchEnd = KernelInfo->StartupExtEnd;
    //DBG_RT(Entry, "Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->StartupExtStart, KernelInfo->StartupExtEnd, KernelInfo->StartupExtSize);
  }
  else if (KernelInfo->KldSize) {
    patchLocation = KernelInfo->KldOff;
    patchEnd = patchLocation + KernelInfo->KldSize;
  }

  if (!patchEnd) {
    DBG_RT(Entry, " - error 0 range\n");
    goto Finish;
  }

  while (patchLocation < patchEnd) { // patchEnd - pattern
    if (
      (Ptr[patchLocation] == 0xC6) &&
      (Ptr[patchLocation + 1] == 0xE8) &&
      (Ptr[patchLocation + 6] == 0xEB)
    ) {
      DBG_RT(Entry, " - found at 0x%x\n", patchLocation);
      patchLocation += 6;
      Ptr[patchLocation] = 0x90;
      Ptr[++patchLocation] = 0x90;
      Ret = TRUE;
      break;
    }

    patchLocation++;
  }

  if (!Ret) {
    DBG_RT(Entry, " - error 0 matches\n");
  }

Finish:

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
  //DBG_PAUSE(Entry, 10);

  return Ret;
}

BOOLEAN
PatchLoadEXE (
  IN LOADER_ENTRY   *Entry
) {
  UINT8     *Ptr = (UINT8 *)KernelInfo->Bin;
  UINT32    patchLocation = 0, patchEnd = 0;
  BOOLEAN   Ret = FALSE;

  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (Ptr == NULL) {
    DBG_RT(Entry, " - error 0 bin\n");
    goto Finish;
  }

  if (KernelInfo->LoadEXESize) {
    patchLocation = KernelInfo->LoadEXEStart;
    patchEnd = KernelInfo->LoadEXEEnd;
    //DBG_RT(Entry, "LoadEXE: Start=0x%x | End=0x%x | Size=0x%x\n", KernelInfo->LoadEXEStart, KernelInfo->LoadEXEEnd, KernelInfo->LoadEXESize);
  }
  else if (KernelInfo->TextSize) {
    patchLocation = KernelInfo->TextOff;
    patchEnd = patchLocation + KernelInfo->TextSize;
  }

  if (!patchEnd) {
    DBG_RT(Entry, " - error 0 range\n");
    goto Finish;
  }

  while (patchLocation < patchEnd) { // patchEnd - pattern
    if (
      (Ptr[patchLocation] == 0xC3) &&
      (Ptr[patchLocation + 1] == 0x48) &&
      //(Ptr[patchLocation + 2] == 0x85) &&
      //(Ptr[patchLocation + 3] == 0xDB) &&
      (Ptr[patchLocation + 4] == 0x74) &&
      ((Ptr[patchLocation + 5] == 0x70) || (Ptr[patchLocation + 5] == 0x71)) &&
      (Ptr[patchLocation + 6] == 0x48)
    ) {
      DBG_RT(Entry, " - found at 0x%x\n", patchLocation);
      patchLocation += 4;
      Ptr[patchLocation] = 0xEB;
      Ptr[++patchLocation] = 0x12;
      Ret = TRUE;
      break;
    }

    patchLocation++;
  }

  if (!Ret) {
    DBG_RT(Entry, " - error 0 matches\n");
  }

Finish:

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
  //DBG_PAUSE(Entry, 10);

  return Ret;
}

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
KernelBooterExtensionsPatch (
  LOADER_ENTRY    *Entry
) {
  UINTN   Num = 0;

  if (!KernelInfo->is64Bit) {
    return;
  }

  DBG_RT(Entry, "\n\nPatching kernel for injected kexts:\n");

  // LoadExec + Entitlement
  Num = FSearchReplace(KernelInfo->TextAddr, KernelInfo->TextSize, KBLoadExec_S2, KBLoadExec_R2) +
        FSearchReplace(KernelInfo->TextAddr, KernelInfo->TextSize, KBLoadExec_S1, KBLoadExec_R1);

  // StartupExt
  if (Num) { // >= 10.11
    Num += FSearchReplace(KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S4, KBStartupExt_R4) +
           FSearchReplace(KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S3, KBStartupExt_R3);
    DBG_RT(Entry, "==> OS: >= 10.11\n");
  } else { // <= 10.10
    Num = FSearchReplace(KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S3, KBStartupExt_R3) +
          FSearchReplace(KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S2, KBStartupExt_R2) +
          FSearchReplace(KernelInfo->KldAddr, KernelInfo->KldSize, KBStartupExt_S1, KBStartupExt_R1);
    DBG_RT(Entry, "==> OS: <= 10.10\n");
  }

  DBG_RT(Entry, "==> %a : %d replaces done\n", Num ? "Success" : "Error", Num);
  DBG_RT(Entry, "Pausing 5 secs ...\n");
  DBG_PAUSE(Entry, 5);
}
#endif

VOID
EFIAPI
KernelBooterExtensionsPatch (
  LOADER_ENTRY    *Entry
) {
  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (PatchStartupExt(Entry)) {
    PatchLoadEXE(Entry);
  }

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
  DBG_PAUSE(Entry, 5);
}

BOOLEAN
KernelAndKextPatcherInit (
  IN LOADER_ENTRY   *Entry
) {
  if (KernelInfo->PatcherInited) {
    goto Finish;
  }

  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  KernelInfo->PatcherInited = TRUE;

  // KernelInfo->RelocBase will normally be 0
  // but if OsxAptioFixDrv is used, then it will be > 0
  SetKernelRelocBase();

  DBG_RT(Entry, "RelocBase = %lx\n", KernelInfo->RelocBase);

  // Find bootArgs - we need then for proper detection
  // of kernel Mach-O header
  FindBootArgs(Entry);

  if (bootArgs2 == NULL) {
    DBG_RT(Entry, "BootArgs not found - skipping patches!\n");
    goto Finish;
  }

  // Find kernel Mach-O header:
  // for ML: bootArgs2->kslide + 0x00200000
  // for older versions: just 0x200000
  // for AptioFix booting - it's always at KernelInfo->RelocBase + 0x200000
  KernelInfo->Bin = (VOID*)(UINTN)(KernelInfo->Slide + KernelInfo->RelocBase + 0x00200000);

  // check that it is Mach-O header and detect architecture
  if (MACH_GET_MAGIC(KernelInfo->Bin) == MH_MAGIC_64) { // (MACH_GET_MAGIC(KernelInfo->Bin) == MH_CIGAM_64)
    DBG_RT(Entry, "Found 64 bit kernel at 0x%p\n", KernelInfo->Bin);
    KernelInfo->is64Bit = TRUE;
  } else {
    // not valid Mach-O header - exiting
    DBG_RT(Entry, "64Bit Kernel not found at 0x%p - skipping patches!", KernelInfo->Bin);
    KernelInfo->Bin = NULL;
    goto Finish;
  }

  InitKernel(Entry);

  KernelInfo->isCache = ((KernelInfo->PrelinkTextSize > 0) && (KernelInfo->PrelinkInfoSize > 0));
  DBG_RT(Entry, "Darwin: Version = %a | VersionMajor: %d | VersionMinor: %d | Revision: %d\n",
    KernelInfo->Version, KernelInfo->VersionMajor, KernelInfo->VersionMinor, KernelInfo->Revision
  );

  DBG_RT(Entry, "isCache: %s\n", KernelInfo->isCache ? L"Yes" : L"No");
  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
  //DBG_PAUSE(Entry, 10);

Finish:

  return (KernelInfo->is64Bit && (KernelInfo->Bin != NULL));
}

VOID
KernelAndKextsPatcherStart (
  IN LOADER_ENTRY   *Entry
) {
  BOOLEAN   KextPatchesNeeded;

  // we will call KernelAndKextPatcherInit() only if needed
  if ((Entry == NULL) || (Entry->KernelAndKextPatches == NULL)) {
    return;
  }

  KernelInfo = AllocateZeroPool (sizeof(KERNEL_INFO));

  KernelInfo->Slide = 0;
  KernelInfo->KldAddr = 0;
  KernelInfo->KldSize = 0;
  KernelInfo->KldOff = 0;
  KernelInfo->TextAddr = 0;
  KernelInfo->TextSize = 0;
  KernelInfo->TextOff = 0;
  //KernelInfo->ConstAddr = 0;
  //KernelInfo->ConstSize = 0;
  //KernelInfo->ConstOff = 0;
  //KernelInfo->CStringAddr = 0;
  //KernelInfo->CStringSize = 0;
  //KernelInfo->CStringOff = 0;
  KernelInfo->DataAddr = 0;
  KernelInfo->DataSize = 0;
  KernelInfo->DataOff = 0;
  KernelInfo->PrelinkTextAddr = 0;
  KernelInfo->PrelinkTextSize = 0;
  KernelInfo->PrelinkInfoAddr = 0;
  KernelInfo->PrelinkInfoSize = 0;
  KernelInfo->LoadEXEStart = 0;
  KernelInfo->LoadEXEEnd = 0;
  KernelInfo->LoadEXESize = 0;
  KernelInfo->StartupExtStart = 0;
  KernelInfo->StartupExtEnd = 0;
  KernelInfo->StartupExtSize = 0;
  KernelInfo->XCPMStart = 0;
  KernelInfo->XCPMEnd = 0;
  KernelInfo->XCPMSize = 0;
  KernelInfo->CPUInfoStart = 0;
  KernelInfo->CPUInfoEnd = 0;
  KernelInfo->CPUInfoSize = 0;
  KernelInfo->VersionMajor = 0;
  KernelInfo->VersionMinor = 0;
  KernelInfo->Revision = 0;
  KernelInfo->Version = NULL;
  KernelInfo->isCache = FALSE;
  KernelInfo->is64Bit = FALSE;
  KernelInfo->PatcherInited = FALSE;
  KernelInfo->RelocBase = 0;
  KernelInfo->Bin = NULL;

  KextPatchesNeeded = (
    Entry->KernelAndKextPatches->KPAsusAICPUPM ||
    //Entry->KernelAndKextPatches->KPAppleRTC ||
    (Entry->KernelAndKextPatches->KPATIConnectorsPatch != NULL) ||
    ((Entry->KernelAndKextPatches->NrKexts > 0) && (Entry->KernelAndKextPatches->KextPatches != NULL))
  );

  //
  // KernelToPatch
  //

  if (
    gSettings.KernelPatchesAllowed &&
    (Entry->KernelAndKextPatches->KernelPatches != NULL) &&
    Entry->KernelAndKextPatches->NrKernels
  ) {
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    KernelUserPatch(Entry);
  }

  //
  // Kext FakeCPUID
  //

  if (Entry->KernelAndKextPatches->FakeCPUID) {
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    KernelCPUIDPatch(Entry);
  }

  //
  // Power Management
  //

  // CPU power management patch for haswell with locked msr
  if (Entry->KernelAndKextPatches->KPKernelPm) {
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    KernelPatchPm(Entry);
  }

  //
  // Kext patches
  //

  DBG_RT(Entry, "\nKextPatches Needed: %a, Allowed: %a ... ",
    (KextPatchesNeeded ? "Yes" : "No"),
    (gSettings.KextPatchesAllowed ? "Yes" : "No")
  );

  if (KextPatchesNeeded && gSettings.KextPatchesAllowed) {
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    KextPatcherStart(Entry);
  }

  //DBG_PAUSE(Entry, 5);

  //
  // Kext add
  //

  if (/*(Entry != 0) &&*/ OSFLAG_ISSET(Entry->Flags, OSFLAG_WITHKEXTS)) {
    UINT32        deviceTreeP, deviceTreeLength;
    EFI_STATUS    Status;
    UINTN         DataSize;

    // check if FSInject already injected kexts
    DataSize = 0;
    Status = gRT->GetVariable (L"FSInject.KextsInjected", &gEfiGlobalVariableGuid, NULL, &DataSize, NULL);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      // var exists - just exit
      DBG_RT(Entry, "\nInjectKexts: skipping, FSInject already injected them\n");
      DBG_PAUSE(Entry, 5);

      return;
    }

    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    deviceTreeP = bootArgs2->deviceTreeP;
    deviceTreeLength = bootArgs2->deviceTreeLength;

    Status = InjectKexts(deviceTreeP, &deviceTreeLength, Entry);

    if (!EFI_ERROR(Status)) {
      KernelBooterExtensionsPatch(Entry);
    }
  }

  return;

NoKernelData:

  DBG_RT(Entry, "==> ERROR: in KernelAndKextPatcherInit\n");
  DBG_PAUSE(Entry, 5);
}
