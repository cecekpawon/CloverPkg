/*
 * Original idea of patching kernel by Evan Lojewsky, 2009
 *
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 * Correction and improvements by Clover team
 */

#include "kernel_patcher.h"

#define KERNEL_DEBUG 0

#if KERNEL_DEBUG
#define DBG(...)    AsciiPrint(__VA_ARGS__);
#else
#define DBG(...)
#endif

// runtime debug
#define DBG_RT(entry, ...)  \
  if ( \
    (entry != NULL) && \
    (entry->KernelAndKextPatches != NULL) && \
    entry->KernelAndKextPatches->KPDebug \
  ) { AsciiPrint(__VA_ARGS__); }


EFI_PHYSICAL_ADDRESS      KernelRelocBase = 0;
//BootArgs1                 *bootArgs1 = NULL;
BootArgs2                 *bootArgs2 = NULL;
CHAR8                     *dtRoot = NULL;
VOID                      *KernelData = NULL;
UINT32                    KernelSlide = 0;
BOOLEAN                   isKernelcache = FALSE;
BOOLEAN                   is64BitKernel = FALSE;
//BOOLEAN                   SSSE3;

BOOLEAN                   PatcherInited = FALSE;

// notes:
// - 64bit segCmd64->vmaddr is 0xffffff80xxxxxxxx and we are taking
//   only lower 32bit part into PrelinkTextAddr
// - PrelinkTextAddr is segCmd64->vmaddr + KernelRelocBase
UINT32                    PrelinkTextLoadCmdAddr = 0;
UINT32                    PrelinkTextAddr = 0;
UINT32                    PrelinkTextSize = 0;

// notes:
// - 64bit sect->addr is 0xffffff80xxxxxxxx and we are taking
//   only lower 32bit part into PrelinkInfoAddr
// - PrelinkInfoAddr is sect->addr + KernelRelocBase
UINT32                    PrelinkInfoLoadCmdAddr = 0;
UINT32                    PrelinkInfoAddr = 0;
UINT32                    PrelinkInfoSize = 0;


VOID SetKernelRelocBase() {
  UINTN   DataSize = sizeof(KernelRelocBase);

  KernelRelocBase = 0;
  // OsxAptioFixDrv will set this
  gRT->GetVariable(L"OsxAptioFixDrv-RelocBase", &gEfiAppleBootGuid, NULL, &DataSize, &KernelRelocBase);
  DeleteNvramVariable(L"OsxAptioFixDrv-RelocBase", &gEfiAppleBootGuid); // clean up the temporary variable
  // KernelRelocBase is now either read or 0

  return;
}

//Slice - FakeCPUID substitution, (c)2014

//procedure location
//STATIC UINT8 StrCpuid1[] = {
//  0xb8, 0x01, 0x00, 0x00, 0x00, 0x31, 0xdb, 0x89, 0xd9, 0x89, 0xda, 0x0f, 0xa2};

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


BOOLEAN PatchCPUID (
  UINT8           *bytes,
  UINT8           *Location,
  INT32           LenLoc,
  UINT8           *Search4,
  UINT8           *Search10,
  UINT8           *ReplaceModel,
  UINT8           *ReplaceExt,
  INT32           Len,
  LOADER_ENTRY    *Entry
) {
  INT32     patchLocation = 0, patchLocation1 = 0, Adr = 0, Num;
  BOOLEAN   Patched = FALSE;
  UINT8     FakeModel = (Entry->KernelAndKextPatches->FakeCPUID >> 4) & 0x0f,
            FakeExt = (Entry->KernelAndKextPatches->FakeCPUID >> 0x10) & 0x0f;

  for (Num = 0; Num < 2; Num++) {
    Adr = FindBin(&bytes[Adr], 0x800000 - Adr, Location, LenLoc);
    if (Adr < 0) {
      break;
    }

    DBG_RT(Entry, "found location at %x\n", Adr);
    patchLocation = FindBin(&bytes[Adr], 0x100, Search4, Len);

    if (patchLocation > 0 && patchLocation < 70) {
      //found
      DBG_RT(Entry, "found Model location at %x\n", Adr + patchLocation);
      CopyMem(&bytes[Adr + patchLocation], ReplaceModel, Len);
      bytes[Adr + patchLocation + 1] = FakeModel;
      patchLocation1 = FindBin(&bytes[Adr], 0x100, Search10, Len);

      if (patchLocation1 > 0 && patchLocation1 < 100) {
        DBG_RT(Entry, "found ExtModel location at %x\n", Adr + patchLocation1);
        CopyMem(&bytes[Adr + patchLocation1], ReplaceExt, Len);
        bytes[Adr + patchLocation1 + 1] = FakeExt;
      }

      Patched = TRUE;
    }
  }

  return Patched;
}

VOID
KernelCPUIDPatch (
  UINT8         *kernelData,
  LOADER_ENTRY  *Entry
) {
  //Lion patterns
  DBG_RT(Entry, "CPUID: try Lion patch...\n");
  if (PatchCPUID(kernelData, &StrMsr8b[0], sizeof(StrMsr8b), &SearchModel107[0],
                 &SearchExt107[0], &ReplaceModel107[0], &ReplaceModel107[0],
                 sizeof(SearchModel107), Entry)) {
    DBG_RT(Entry, "...done!\n");
    return;
  }

  //Mavericks
  DBG_RT(Entry, "CPUID: try Mavericks patch...\n");
  if (PatchCPUID(kernelData, &StrMsr8b[0], sizeof(StrMsr8b), &SearchModel109[0],
                 &SearchExt109[0], &ReplaceModel109[0], &ReplaceExt109[0],
                 sizeof(SearchModel109), Entry)) {
    DBG_RT(Entry, "...done!\n");
    return;
  }

  //Yosemite
  DBG_RT(Entry, "CPUID: try Yosemite patch...\n");
  if (PatchCPUID(kernelData, &StrMsr8b[0], sizeof(StrMsr8b), &SearchModel101[0],
                 &SearchExt101[0], &ReplaceModel107[0], &ReplaceModel107[0],
                 sizeof(SearchModel107), Entry)) {
    DBG_RT(Entry, "...done!\n");
    return;
  }
}


// Power management patch for kernel 13.0
STATIC UINT8 KernelPatchPmSrc[] = {
  0x55, 0x48, 0x89, 0xe5, 0x41, 0x89, 0xd0, 0x85,
  0xf6, 0x74, 0x6c, 0x48, 0x83, 0xc7, 0x28, 0x90,
  0x8b, 0x05, 0x5e, 0x30, 0x5e, 0x00, 0x85, 0x47,
  0xdc, 0x74, 0x54, 0x8b, 0x4f, 0xd8, 0x45, 0x85,
  0xc0, 0x74, 0x08, 0x44, 0x39, 0xc1, 0x44, 0x89,
  0xc1, 0x75, 0x44, 0x0f, 0x32, 0x89, 0xc0, 0x48,
  0xc1, 0xe2, 0x20, 0x48, 0x09, 0xc2, 0x48, 0x89,
  0x57, 0xf8, 0x48, 0x8b, 0x47, 0xe8, 0x48, 0x85,
  0xc0, 0x74, 0x06, 0x48, 0xf7, 0xd0, 0x48, 0x21,
  0xc2, 0x48, 0x0b, 0x57, 0xf0, 0x49, 0x89, 0xd1,
  0x49, 0xc1, 0xe9, 0x20, 0x89, 0xd0, 0x8b, 0x4f,
  0xd8, 0x4c, 0x89, 0xca, 0x0f, 0x30, 0x8b, 0x4f,
  0xd8, 0x0f, 0x32, 0x89, 0xc0, 0x48, 0xc1, 0xe2,
  0x20, 0x48, 0x09, 0xc2, 0x48, 0x89, 0x17, 0x48,
  0x83, 0xc7, 0x30, 0xff, 0xce, 0x75, 0x99, 0x5d,
  0xc3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
};

STATIC UINT8 KernelPatchPmRepl[] = {
  0x55, 0x48, 0x89, 0xe5, 0x41, 0x89, 0xd0, 0x85,
  0xf6, 0x74, 0x73, 0x48, 0x83, 0xc7, 0x28, 0x90,
  0x8b, 0x05, 0x5e, 0x30, 0x5e, 0x00, 0x85, 0x47,
  0xdc, 0x74, 0x5b, 0x8b, 0x4f, 0xd8, 0x45, 0x85,
  0xc0, 0x74, 0x08, 0x44, 0x39, 0xc1, 0x44, 0x89,
  0xc1, 0x75, 0x4b, 0x0f, 0x32, 0x89, 0xc0, 0x48,
  0xc1, 0xe2, 0x20, 0x48, 0x09, 0xc2, 0x48, 0x89,
  0x57, 0xf8, 0x48, 0x8b, 0x47, 0xe8, 0x48, 0x85,
  0xc0, 0x74, 0x06, 0x48, 0xf7, 0xd0, 0x48, 0x21,
  0xc2, 0x48, 0x0b, 0x57, 0xf0, 0x49, 0x89, 0xd1,
  0x49, 0xc1, 0xe9, 0x20, 0x89, 0xd0, 0x8b, 0x4f,
  0xd8, 0x4c, 0x89, 0xca, 0x66, 0x81, 0xf9, 0xe2,
  0x00, 0x74, 0x02, 0x0f, 0x30, 0x8b, 0x4f, 0xd8,
  0x0f, 0x32, 0x89, 0xc0, 0x48, 0xc1, 0xe2, 0x20,
  0x48, 0x09, 0xc2, 0x48, 0x89, 0x17, 0x48, 0x83,
  0xc7, 0x30, 0xff, 0xce, 0x75, 0x92, 0x5d, 0xc3
};

// Power management patch for kernel 12.5
STATIC UINT8 KernelPatchPmSrc2[] = {
  0x55, 0x48, 0x89, 0xe5, 0x41, 0x89, 0xd0, 0x85,
  0xf6, 0x74, 0x69, 0x48, 0x83, 0xc7, 0x28, 0x90,
  0x8b, 0x05, 0xfe, 0xce, 0x5f, 0x00, 0x85, 0x47,
  0xdc, 0x74, 0x51, 0x8b, 0x4f, 0xd8, 0x45, 0x85,
  0xc0, 0x74, 0x05, 0x44, 0x39, 0xc1, 0x75, 0x44,
  0x0f, 0x32, 0x89, 0xc0, 0x48, 0xc1, 0xe2, 0x20,
  0x48, 0x09, 0xc2, 0x48, 0x89, 0x57, 0xf8, 0x48,
  0x8b, 0x47, 0xe8, 0x48, 0x85, 0xc0, 0x74, 0x06,
  0x48, 0xf7, 0xd0, 0x48, 0x21, 0xc2, 0x48, 0x0b,
  0x57, 0xf0, 0x49, 0x89, 0xd1, 0x49, 0xc1, 0xe9,
  0x20, 0x89, 0xd0, 0x8b, 0x4f, 0xd8, 0x4c, 0x89,
  0xca, 0x0f, 0x30, 0x8b, 0x4f, 0xd8, 0x0f, 0x32,
  0x89, 0xc0, 0x48, 0xc1, 0xe2, 0x20, 0x48, 0x09,
  0xc2, 0x48, 0x89, 0x17, 0x48, 0x83, 0xc7, 0x30,
  0xff, 0xce, 0x75, 0x9c, 0x5d, 0xc3, 0x90, 0x90,
  0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
};

STATIC UINT8 KernelPatchPmRepl2[] = {
  0x55, 0x48, 0x89, 0xe5, 0x41, 0x89, 0xd0, 0x85,
  0xf6, 0x74, 0x70, 0x48, 0x83, 0xc7, 0x28, 0x90,
  0x8b, 0x05, 0xfe, 0xce, 0x5f, 0x00, 0x85, 0x47,
  0xdc, 0x74, 0x58, 0x8b, 0x4f, 0xd8, 0x45, 0x85,
  0xc0, 0x74, 0x05, 0x44, 0x39, 0xc1, 0x75, 0x4b,
  0x0f, 0x32, 0x89, 0xc0, 0x48, 0xc1, 0xe2, 0x20,
  0x48, 0x09, 0xc2, 0x48, 0x89, 0x57, 0xf8, 0x48,
  0x8b, 0x47, 0xe8, 0x48, 0x85, 0xc0, 0x74, 0x06,
  0x48, 0xf7, 0xd0, 0x48, 0x21, 0xc2, 0x48, 0x0b,
  0x57, 0xf0, 0x49, 0x89, 0xd1, 0x49, 0xc1, 0xe9,
  0x20, 0x89, 0xd0, 0x8b, 0x4f, 0xd8, 0x4c, 0x89,
  0xca, 0x66, 0x81, 0xf9, 0xe2, 0x00, 0x74, 0x02,
  0x0f, 0x30, 0x8b, 0x4f, 0xd8, 0x0f, 0x32, 0x89,
  0xc0, 0x48, 0xc1, 0xe2, 0x20, 0x48, 0x09, 0xc2,
  0x48, 0x89, 0x17, 0x48, 0x83, 0xc7, 0x30, 0xff,
  0xce, 0x75, 0x95, 0x5d, 0xc3, 0x90, 0x90, 0x90
};

#define KERNEL_PATCH_SIGNATURE     0x85d08941e5894855ULL
//#define KERNEL_YOS_PATCH_SIGNATURE 0x56415741e5894855ULL

BOOLEAN
KernelPatchPm (
  VOID    *kernelData
) {
  UINT8   *Ptr = (UINT8 *)kernelData,
          *End = Ptr + 0x1000000;

  if (Ptr == NULL) {
    return FALSE;
  }

  // Credits to RehabMan for the kernel patch information
  DBG("Patching kernel power management...\n");
  while (Ptr < End) {
    if (KERNEL_PATCH_SIGNATURE == (*((UINT64 *)Ptr))) {
      // Bytes 19,20 of KernelPm patch for kernel 13.x change between kernel versions, so we skip them in search&replace
      if (
        (CompareMem(Ptr + sizeof(UINT64),   KernelPatchPmSrc + sizeof(UINT64),   18*sizeof(UINT8) - sizeof(UINT64)) == 0) &&
        (CompareMem(Ptr + 20*sizeof(UINT8), KernelPatchPmSrc + 20*sizeof(UINT8), sizeof(KernelPatchPmSrc) - 20*sizeof(UINT8)) == 0)
      ) {
        // Don't copy more than the source here!
        CopyMem(Ptr, KernelPatchPmRepl, 18*sizeof(UINT8));
        CopyMem(Ptr + 20*sizeof(UINT8), KernelPatchPmRepl + 20*sizeof(UINT8), sizeof(KernelPatchPmSrc) - 20*sizeof(UINT8));
        DBG("Kernel power management patch region 1 found and patched\n");
        return TRUE;
      } else if (
        CompareMem(Ptr + sizeof(UINT64), KernelPatchPmSrc2 + sizeof(UINT64), sizeof(KernelPatchPmSrc2) - sizeof(UINT64)) == 0
      ) {
        // Don't copy more than the source here!
        CopyMem(Ptr, KernelPatchPmRepl2, sizeof(KernelPatchPmSrc2));
        DBG("Kernel power management patch region 2 found and patched\n");
        return TRUE;
      }
    } else if (0x00000002000000E2ULL == (*((UINT64 *)Ptr))) {
      //rehabman: for 10.10 (data portion)
      (*((UINT64 *)Ptr)) = 0x0000000000000000ULL;
      DBG("Kernel power management patch 10.10(data1) found and patched\n");
      //return TRUE;
    } else if (0x0000004C000000E2ULL == (*((UINT64 *)Ptr))) {
      (*((UINT64 *)Ptr)) = 0x0000000000000000ULL;
      DBG("Kernel power management patch 10.10(data2) found and patched\n");
      //return TRUE;
    } else if (0x00000190000000E2ULL == (*((UINT64 *)Ptr))) {
      (*((UINT64 *)Ptr)) = 0x0000000000000000ULL;
      DBG("Kernel power management patch 10.10(data3) found and patched\n");
      return TRUE;
    } else if (0x00001390000000E2ULL == (*((UINT64 *)Ptr))) {
      // rehabman: change for 10.11.1 beta 15B38b
      (*((UINT64 *)Ptr)) = 0x0000000000000000ULL;
      DBG("Kernel power management patch 10.11.1(beta 15B38b)(data3) found and patched\n");
      return TRUE;
    } else if (0x00003390000000E2ULL == (*((UINT64 *)Ptr))) {
      // sherlocks: change for 10.12 DP1
      (*((UINT64 *)Ptr)) = 0x0000000000000000ULL;
      DBG("Kernel power management patch 10.12 DP1 found and patched\n");
      return TRUE;
    }

    Ptr += 16;
  }

  DBG("Kernel power management patch region not found!\n");

  return FALSE;
}

VOID
Get_PreLink() {
  UINT32    ncmds, cmdsize, binaryIndex;
  UINTN     cnt;
  UINT8     *binary = (UINT8*)KernelData;
  struct    load_command        *loadCommand;
  //struct    segment_command     *segCmd;
  struct    segment_command_64  *segCmd64;

  binaryIndex = sizeof(struct mach_header_64);

  ncmds = MACH_GET_NCMDS(binary);

  for (cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct load_command *)(binary + binaryIndex);
    cmdsize = loadCommand->cmdsize;

    switch (loadCommand->cmd) {
      case LC_SEGMENT_64:
        segCmd64 = (struct segment_command_64 *)loadCommand;
        //DBG("segCmd64->segname = %a\n",segCmd64->segname);
        //DBG("segCmd64->vmaddr = 0x%08x\n",segCmd64->vmaddr)
        //DBG("segCmd64->vmsize = 0x%08x\n",segCmd64->vmsize);

        if (AsciiStrCmp(segCmd64->segname, kPrelinkTextSegment) == 0) {
          DBG("Found PRELINK_TEXT, 64bit\n");

          if (segCmd64->vmsize > 0) {
            // 64bit segCmd64->vmaddr is 0xffffff80xxxxxxxx
            // PrelinkTextAddr = xxxxxxxx + KernelRelocBase
            PrelinkTextAddr = (UINT32)(segCmd64->vmaddr ? segCmd64->vmaddr + KernelRelocBase : 0);
            PrelinkTextSize = (UINT32)segCmd64->vmsize;
            PrelinkTextLoadCmdAddr = (UINT32)(UINTN)segCmd64;
          }

          DBG("at %p: vmaddr = 0x%lx, vmsize = 0x%lx\n", segCmd64, segCmd64->vmaddr, segCmd64->vmsize);
          DBG("PrelinkTextLoadCmdAddr = 0x%x, PrelinkTextAddr = 0x%x, PrelinkTextSize = 0x%x\n",
              PrelinkTextLoadCmdAddr, PrelinkTextAddr, PrelinkTextSize);

          //DBG("cmd = 0x%08x\n",segCmd64->cmd);
          //DBG("cmdsize = 0x%08x\n",segCmd64->cmdsize);
          //DBG("vmaddr = 0x%08x\n",segCmd64->vmaddr);
          //DBG("vmsize = 0x%08x\n",segCmd64->vmsize);
          //DBG("fileoff = 0x%08x\n",segCmd64->fileoff);
          //DBG("filesize = 0x%08x\n",segCmd64->filesize);
          //DBG("maxprot = 0x%08x\n",segCmd64->maxprot);
          //DBG("initprot = 0x%08x\n",segCmd64->initprot);
          //DBG("nsects = 0x%08x\n",segCmd64->nsects);
          //DBG("flags = 0x%08x\n",segCmd64->flags);
        }

        if (AsciiStrCmp(segCmd64->segname, kPrelinkInfoSegment) == 0) {
          UINT32    sectionIndex;
          struct    section_64 *sect;

          DBG("Found PRELINK_INFO, 64bit\n");
          //DBG("cmd = 0x%08x\n",segCmd64->cmd);
          //DBG("cmdsize = 0x%08x\n",segCmd64->cmdsize);
          DBG("vmaddr = 0x%08x\n",segCmd64->vmaddr);
          DBG("vmsize = 0x%08x\n",segCmd64->vmsize);
          //DBG("fileoff = 0x%08x\n",segCmd64->fileoff);
          //DBG("filesize = 0x%08x\n",segCmd64->filesize);
          //DBG("maxprot = 0x%08x\n",segCmd64->maxprot);
          //DBG("initprot = 0x%08x\n",segCmd64->initprot);
          //DBG("nsects = 0x%08x\n",segCmd64->nsects);
          //DBG("flags = 0x%08x\n",segCmd64->flags);

          sectionIndex = sizeof(struct segment_command_64);

          while(sectionIndex < segCmd64->cmdsize) {
            sect = (struct section_64 *)((UINT8*)segCmd64 + sectionIndex);
            sectionIndex += sizeof(struct section_64);

            if(AsciiStrCmp(sect->sectname, kPrelinkInfoSection) == 0 && AsciiStrCmp(sect->segname, kPrelinkInfoSegment) == 0) {
              if (sect->size > 0) {
                // 64bit sect->addr is 0xffffff80xxxxxxxx
                // PrelinkInfoAddr = xxxxxxxx + KernelRelocBase
                PrelinkInfoLoadCmdAddr = (UINT32)(UINTN)sect;
                PrelinkInfoAddr = (UINT32)(sect->addr ? sect->addr + KernelRelocBase : 0);
                PrelinkInfoSize = (UINT32)sect->size;
              }

              DBG("__info found at %p: addr = 0x%lx, size = 0x%lx\n", sect, sect->addr, sect->size);
              DBG("PrelinkInfoLoadCmdAddr = 0x%x, PrelinkInfoAddr = 0x%x, PrelinkInfoSize = 0x%x\n",
                  PrelinkInfoLoadCmdAddr, PrelinkInfoAddr, PrelinkInfoSize);
            }
          }
        }
        break;

      default:
        break;
    }
    binaryIndex += cmdsize;
  }

  //gBS->Stall(20*1000000);
  //return;
}

VOID
FindBootArgs (
  IN LOADER_ENTRY   *Entry
) {
  UINT8   *ptr, archMode = sizeof(UINTN) * 8;

  // start searching from 0x200000.
  ptr = (UINT8*)(UINTN)0x200000;

  while(TRUE) {
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
      KernelSlide = bootArgs2->kslide;

      DBG_RT(Entry, "Found bootArgs2 at 0x%08x, DevTree at %p\n", ptr, dtRoot);
      //DBG("bootArgs2->kaddr = 0x%08x and bootArgs2->ksize =  0x%08x\n", bootArgs2->kaddr, bootArgs2->ksize);
      //DBG("bootArgs2->efiMode = 0x%02x\n", bootArgs2->efiMode);
      DBG_RT(Entry, "bootArgs2->CommandLine = %a\n", bootArgs2->CommandLine);
      DBG_RT(Entry, "bootArgs2->flags = 0x%x\n", bootArgs2->flags);
      DBG_RT(Entry, "bootArgs2->kslide = 0x%x\n", bootArgs2->kslide);
      DBG_RT(Entry, "bootArgs2->bootMemStart = 0x%x\n", bootArgs2->bootMemStart);
      gBS->Stall(2000000);

      // disable other pointer
      //bootArgs1 = NULL;
      break;
    }

    ptr += 0x1000;
  }
}

BOOLEAN
KernelUserPatch (
  IN  UINT8         *UKernelData,
      LOADER_ENTRY  *Entry
) {
  INTN    Num, i = 0, y = 0;

  for (; i < Entry->KernelAndKextPatches->NrKernels; ++i) {
    DBG_RT(Entry, "Patch[%d]: %a\n", i, Entry->KernelAndKextPatches->KernelPatches[i].Label);
    if (Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
      DBG_RT(Entry, "==> is not allowed for booted OS %a\n", Entry->OSVersion);
      continue;
    }

    /*
        Num = SearchAndCount (
          UKernelData,
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
      UKernelData,
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

  return (y != 0);
}

BOOLEAN
KernelAndKextPatcherInit (
  IN LOADER_ENTRY   *Entry
) {
  if (PatcherInited) {
    goto finish;
  }

  PatcherInited = TRUE;

  // KernelRelocBase will normally be 0
  // but if OsxAptioFixDrv is used, then it will be > 0
  SetKernelRelocBase();

  DBG("KernelRelocBase = %lx\n", KernelRelocBase);

  // Find bootArgs - we need then for proper detection
  // of kernel Mach-O header
  FindBootArgs(Entry);

  if (bootArgs2 == NULL) {
    DBG("BootArgs not found - skipping patches!\n");
    goto finish;
  }

  // Find kernel Mach-O header:
  // for ML: bootArgs2->kslide + 0x00200000
  // for older versions: just 0x200000
  // for AptioFix booting - it's always at KernelRelocBase + 0x200000
  KernelData = (VOID*)(UINTN)(KernelSlide + KernelRelocBase + 0x00200000);

  // check that it is Mach-O header and detect architecture
  if ((MACH_GET_MAGIC(KernelData) == MH_MAGIC_64) || (MACH_GET_MAGIC(KernelData) == MH_CIGAM_64)) {
    DBG_RT(Entry, "Found 64 bit kernel at 0x%p\n", KernelData);
    is64BitKernel = TRUE;
  } else {
    // not valid Mach-O header - exiting
    DBG_RT(Entry, "64Bit Kernel not found at 0x%p - skipping patches!", KernelData);
    KernelData = NULL;
    goto finish;
  }

  // find __PRELINK_TEXT and __PRELINK_INFO
  Get_PreLink();

  isKernelcache = PrelinkTextSize > 0 && PrelinkInfoSize > 0;
  DBG_RT(Entry, "isKernelcache: %s\n", isKernelcache ? L"Yes" : L"No");

finish:
  return (is64BitKernel && (KernelData != NULL));
}

VOID
KernelAndKextsPatcherStart (
  IN LOADER_ENTRY   *Entry
) {
  BOOLEAN KextPatchesNeeded, patchedOk;

  // we will call KernelAndKextPatcherInit() only if needed
  if ((Entry == NULL) || (Entry->KernelAndKextPatches == NULL)) {
    return;
  }

  KextPatchesNeeded = (
    Entry->KernelAndKextPatches->KPAsusAICPUPM ||
    //Entry->KernelAndKextPatches->KPAppleRTC ||
    (Entry->KernelAndKextPatches->KPATIConnectorsPatch != NULL) ||
    ((Entry->KernelAndKextPatches->NrKexts > 0) && (Entry->KernelAndKextPatches->KextPatches != NULL))
  );

  DBG_RT(Entry, "\nKernelToPatch: ");

  if (
    gSettings.KernelPatchesAllowed &&
    (Entry->KernelAndKextPatches->KernelPatches != NULL) &&
    Entry->KernelAndKextPatches->NrKernels
  ) {
    DBG_RT(Entry, "Enabled: ");
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    patchedOk = KernelUserPatch(KernelData, Entry);
    DBG_RT(Entry, patchedOk ? " OK\n" : " FAILED!\n");
  } else {
    DBG_RT(Entry, "Disabled\n");
  }

  //other method for KernelCPU patch is FakeCPUID
  DBG_RT(Entry, "\nFakeCPUID patch: ");

  if (Entry->KernelAndKextPatches->FakeCPUID) {
    DBG_RT(Entry, "Enabled: 0x%06x\n", Entry->KernelAndKextPatches->FakeCPUID);

    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    KernelCPUIDPatch((UINT8*)KernelData, Entry);
  } else {
    DBG_RT(Entry, "Disabled\n");
  }

  // CPU power management patch for haswell with locked msr
  DBG_RT(Entry, "\nKernelPm patch: ");

  if (Entry->KernelAndKextPatches->KPKernelPm) {
    DBG_RT(Entry, "Enabled: ");
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    patchedOk = KernelPatchPm(KernelData);
    DBG_RT(Entry, patchedOk ? " OK\n" : " FAILED!\n");
  } else {
    DBG_RT(Entry, "Disabled\n");
  }

  if (Entry->KernelAndKextPatches->KPDebug) {
    gBS->Stall(2000000);
  }

  //
  // Kext patches
  //

  DBG_RT(Entry, "\nKextPatches Needed: %c, Allowed: %c ... ",
         (KextPatchesNeeded ? L'Y' : L'n'),
         (gSettings.KextPatchesAllowed ? L'Y' : L'n')
         );

  if (KextPatchesNeeded && gSettings.KextPatchesAllowed) {
    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    DBG_RT(Entry, "\nKext patching STARTED\n");
    KextPatcherStart(Entry);
    DBG_RT(Entry, "\nKext patching ENDED\n");
  } else {
    DBG_RT(Entry, "Disabled\n");
  }

  if (Entry->KernelAndKextPatches->KPDebug) {
    DBG_RT(Entry, "Pausing 10 secs ...\n\n");
    gBS->Stall(10000000);
  }

  //
  // Kext add
  //

  if ((Entry != 0) && OSFLAG_ISSET(Entry->Flags, OSFLAG_WITHKEXTS)) {
    UINT32        deviceTreeP, deviceTreeLength;
    EFI_STATUS    Status;
    UINTN         DataSize;

    // check if FSInject already injected kexts
    DataSize = 0;
    Status = gRT->GetVariable (L"FSInject.KextsInjected", &gEfiGlobalVariableGuid, NULL, &DataSize, NULL);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      // var exists - just exit
      if (Entry->KernelAndKextPatches->KPDebug) {
        DBG_RT(Entry, "\nInjectKexts: skipping, FSInject already injected them\n");
        gBS->Stall(500000);
      }
      return;
    }

    if (!KernelAndKextPatcherInit(Entry)) {
      goto NoKernelData;
    }

    if (bootArgs2 != NULL) {
      deviceTreeP = bootArgs2->deviceTreeP;
      deviceTreeLength = bootArgs2->deviceTreeLength;
    } else return;

    Status = InjectKexts(deviceTreeP, &deviceTreeLength, Entry);

    if (!EFI_ERROR(Status)) {
      KernelBooterExtensionsPatch(KernelData, Entry);
    }
  }

  return;

NoKernelData:
  if (/*(KernelData == NULL) && */Entry->KernelAndKextPatches->KPDebug) {
    DBG_RT(Entry, "==> ERROR: Kernel not found\n");
    gBS->Stall(5000000);
  }
}
