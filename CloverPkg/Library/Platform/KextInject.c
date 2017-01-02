#include <Library/Platform/Platform.h>

//#define KEXT_INJECT_DEBUG 0

//#if KEXT_INJECT_DEBUG
//#define DBG(...)  AsciiPrint(__VA_ARGS__);
//#else
//#define DBG(...)
//#endif

// runtime debug
#define DBG_ON(entry) \
  ((entry != NULL) && (entry->KernelAndKextPatches != NULL) && \
  (OSFLAG_ISSET(gSettings.FlagsBits, OSFLAG_DBGPATCHES) || gSettings.DebugKP))
  /*entry->KernelAndKextPatches->KPDebug && \*/
#define DBG_RT(entry, ...) \
  if (DBG_ON(entry)) AsciiPrint(__VA_ARGS__)
#define DBG_PAUSE(entry, s) \
  if (DBG_ON(entry)) gBS->Stall(s * 1000000)

////////////////////
// globals
////////////////////
LIST_ENTRY    gKextList = INITIALIZE_LIST_HEAD_VARIABLE (gKextList);
UINT32        gKextCount = 0, gKextSize = 0;

////////////////////
// before booting
////////////////////
EFI_STATUS
EFIAPI
ThinFatFile (
  IN OUT UINT8    **binary,
  IN OUT UINTN    *length,
  IN cpu_type_t   archCpuType
) {
  UINT32        nfat, swapped = 0, size = 0, fapoffset, fapsize;
  FAT_HEADER    *fhp = (FAT_HEADER *)*binary;
  FAT_ARCH      *fap = (FAT_ARCH *)(*binary + sizeof(FAT_HEADER));
  cpu_type_t    fapcputype;

  if (fhp->magic == FAT_MAGIC) {
    nfat = fhp->nfat_arch;
  } else if (fhp->magic == FAT_CIGAM) {
    nfat = SwapBytes32(fhp->nfat_arch);
    swapped = 1;
    //already thin
  } else if (fhp->magic == THIN_X64){
    if (archCpuType == CPU_TYPE_X86_64) {
      return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
  } else {
    MsgLog("Thinning fails\n");
    return EFI_NOT_FOUND;
  }

  for (; nfat > 0; nfat--, fap++) {
    if (swapped) {
      fapcputype = SwapBytes32(fap->cputype);
      fapoffset = SwapBytes32(fap->offset);
      fapsize = SwapBytes32(fap->size);
    } else {
      fapcputype = fap->cputype;
      fapoffset = fap->offset;
      fapsize = fap->size;
    }

    if (fapcputype == archCpuType) {
      *binary = (*binary + fapoffset);
      size = fapsize;
      break;
    }
  }

  if (length != 0) {
    *length = size;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LoadKext (
  IN LOADER_ENTRY           *Entry,
  IN EFI_FILE               *RootDir,
  IN CHAR16                 *FileName,
  IN cpu_type_t             archCpuType,
  IN OUT _DeviceTreeBuffer  *kext
) {
  EFI_STATUS            Status;
  UINT8                 *infoDictBuffer = NULL, *executableFatBuffer = NULL, *executableBuffer = NULL;
  UINTN                 infoDictBufferLength = 0, executableBufferLength = 0, bundlePathBufferLength = 0;
  CHAR8                 *bundlePathBuffer = NULL;
  CHAR16                TempName[256], Executable[256];
  TagPtr                dict = NULL, prop = NULL;
  BOOLEAN               NoContents = FALSE;
  _BooterKextFileInfo   *infoAddr = NULL;

  UnicodeSPrint(TempName, 512, L"%s\\%s", FileName, L"Contents\\Info.plist");

  Status = egLoadFile(RootDir, TempName, &infoDictBuffer, &infoDictBufferLength);
  if (EFI_ERROR(Status)) {
    //try to find a planar kext, without Contents
    UnicodeSPrint(TempName, 512, L"%s\\%s", FileName, L"Info.plist");

    Status = egLoadFile(RootDir, TempName, &infoDictBuffer, &infoDictBufferLength);
    if (EFI_ERROR(Status)) {
      MsgLog("Failed to load extra kext (Info.plist not found): %s\n", FileName);
      return EFI_NOT_FOUND;
    }

    NoContents = TRUE;
  }

  if (EFI_ERROR(ParseXML((CHAR8*)infoDictBuffer, &dict, 0))) {
    FreePool(infoDictBuffer);
    MsgLog("Failed to load extra kext (failed to parse Info.plist): %s\n", FileName);
    return EFI_NOT_FOUND;
  }

  prop = GetProperty(dict, kPropCFBundleExecutable);
  if (prop != 0) {
    AsciiStrToUnicodeStr(prop->string, Executable);
    if (NoContents) {
      UnicodeSPrint(TempName, 512, L"%s\\%s", FileName, Executable);
    } else {
      UnicodeSPrint(TempName, 512, L"%s\\%s\\%s", FileName, L"Contents\\MacOS",Executable);
    }

    Status = egLoadFile(RootDir, TempName, &executableFatBuffer, &executableBufferLength);
    if (EFI_ERROR(Status)) {
      FreePool(infoDictBuffer);
      MsgLog("Failed to load extra kext (executable not found): %s\n", FileName);
      return EFI_NOT_FOUND;
    }

    executableBuffer = executableFatBuffer;
    if (ThinFatFile(&executableBuffer, &executableBufferLength, archCpuType)) {
      FreePool(infoDictBuffer);
      FreePool(executableBuffer);
      MsgLog("Thinning failed: %s\n", FileName);
      return EFI_NOT_FOUND;
    }
  } else {
    MsgLog("Failed to read '%a' prop\n", kPropCFBundleExecutable);
  }

  bundlePathBufferLength = StrLen(FileName) + 1;
  bundlePathBuffer = AllocateZeroPool(bundlePathBufferLength);
  UnicodeStrToAsciiStr(FileName, bundlePathBuffer);

  kext->length = (UINT32)(sizeof(_BooterKextFileInfo) + infoDictBufferLength + executableBufferLength + bundlePathBufferLength);

  infoAddr = (_BooterKextFileInfo *)AllocatePool(kext->length);
  infoAddr->infoDictPhysAddr    = sizeof(_BooterKextFileInfo);
  infoAddr->infoDictLength      = (UINT32)infoDictBufferLength;
  infoAddr->executablePhysAddr  = (UINT32)(sizeof(_BooterKextFileInfo) + infoDictBufferLength);
  infoAddr->executableLength    = (UINT32)executableBufferLength;
  infoAddr->bundlePathPhysAddr  = (UINT32)(sizeof(_BooterKextFileInfo) + infoDictBufferLength + executableBufferLength);
  infoAddr->bundlePathLength    = (UINT32)bundlePathBufferLength;

  kext->paddr = (UINT32)(UINTN)infoAddr; // Note that we cannot free infoAddr because of this

  CopyMem((CHAR8 *)infoAddr + sizeof(_BooterKextFileInfo), infoDictBuffer, infoDictBufferLength);
  CopyMem((CHAR8 *)infoAddr + sizeof(_BooterKextFileInfo) + infoDictBufferLength, executableBuffer, executableBufferLength);
  CopyMem((CHAR8 *)infoAddr + sizeof(_BooterKextFileInfo) + infoDictBufferLength + executableBufferLength, bundlePathBuffer, bundlePathBufferLength);

  FreePool(infoDictBuffer);
  FreePool(executableFatBuffer);
  FreePool(bundlePathBuffer);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AddKext (
  IN LOADER_ENTRY   *Entry,
  IN EFI_FILE       *RootDir,
  IN CHAR16         *FileName,
  IN cpu_type_t     archCpuType
) {
  EFI_STATUS  Status;
  KEXT_ENTRY  *KextEntry;

  KextEntry = AllocatePool (sizeof(KEXT_ENTRY));
  KextEntry->Signature = KEXT_SIGNATURE;

  Status = LoadKext(Entry, RootDir, FileName, archCpuType, &KextEntry->kext);
  if (EFI_ERROR(Status)) {
    FreePool(KextEntry);
  } else {
    InsertTailList (&gKextList, &KextEntry->Link);
  }

  return Status;
}

UINT32
GetListCount (
  LIST_ENTRY  const *List
) {
  LIST_ENTRY    *Link;
  UINT32        Count=0;

  if (!IsListEmpty(List)) {
    for (Link = List->ForwardLink; Link != List; Link = Link->ForwardLink) {
      Count++;
    }
  }

  return Count;
}

UINT32
GetKextCount() {
  return (UINT32)GetListCount(&gKextList);
}

UINT32
GetKextsSize () {
  LIST_ENTRY    *Link;
  KEXT_ENTRY    *KextEntry;
  UINT32        kextsSize=0;

  if (!IsListEmpty(&gKextList)) {
    for (Link = gKextList.ForwardLink; Link != &gKextList; Link = Link->ForwardLink) {
      KextEntry = CR(Link, KEXT_ENTRY, Link, KEXT_SIGNATURE);
      kextsSize += RoundPage(KextEntry->kext.length);
    }
  }

  return kextsSize;
}

VOID
LoadPlugInKexts (
  IN LOADER_ENTRY   *Entry,
  IN EFI_FILE       *RootDir,
  IN CHAR16         *DirName,
  IN cpu_type_t     archCpuType,
  IN BOOLEAN        Force
) {
  REFIT_DIR_ITER          PlugInIter;
  EFI_FILE_INFO           *PlugInFile;
  CHAR16                  FileName[256];

  if ((Entry == NULL) || (RootDir == NULL) || (DirName == NULL)) {
    return;
  }

  DirIterOpen(RootDir, DirName, &PlugInIter);
  while (DirIterNext(&PlugInIter, 1, L"*.kext", &PlugInFile)) {
    if (PlugInFile->FileName[0] == '.' || StriStr(PlugInFile->FileName, L".kext") == NULL) {
      continue;   // skip this
    }

    UnicodeSPrint(FileName, 512, L"%s\\%s", DirName, PlugInFile->FileName);
    MsgLog("    %s PlugIn kext: %s\n", Force ? L"Force" : L"Extra", FileName);
    AddKext(Entry, RootDir, FileName, archCpuType);
  }

  DirIterClose(&PlugInIter);
}

EFI_STATUS
LoadKexts (
  IN LOADER_ENTRY   *Entry
) {
  //EFI_STATUS      Status;
  CHAR16            *SrcDir = NULL, FileName[256], PlugIns[256];
  REFIT_DIR_ITER    KextIter, PlugInIter;
  EFI_FILE_INFO     *KextFile, *PlugInFile;
  cpu_type_t        archCpuType=CPU_TYPE_X86_64;
  UINTN             mm_extra_size, extra_size, i = 0;
  VOID              *mm_extra, *extra;

  if ((Entry == 0) || OSFLAG_ISUNSET(Entry->Flags, OSFLAG_WITHKEXTS)) {
    return EFI_NOT_STARTED;
  }

  // Force kexts to load
  if (
    (Entry->KernelAndKextPatches != NULL) &&
    (Entry->KernelAndKextPatches->NrForceKexts > 0) &&
    (Entry->KernelAndKextPatches->ForceKexts != NULL)
  ) {
    for (; i < Entry->KernelAndKextPatches->NrForceKexts; ++i) {
      MsgLog("  Force kext: %s\n", Entry->KernelAndKextPatches->ForceKexts[i]);
      if (Entry->Volume && Entry->Volume->RootDir) {
        // Check if the entry is a directory
        if (StriStr(Entry->KernelAndKextPatches->ForceKexts[i], L".kext") == NULL) {
          DirIterOpen(Entry->Volume->RootDir, Entry->KernelAndKextPatches->ForceKexts[i], &PlugInIter);
          while (DirIterNext(&PlugInIter, 1, L"*.kext", &PlugInFile)) {
            if ((PlugInFile->FileName[0] == '.') || (StrStr(PlugInFile->FileName, L".kext") == NULL)) {
              continue;   // skip this
            }

            UnicodeSPrint(FileName, 512, L"%s\\%s", Entry->KernelAndKextPatches->ForceKexts[i], PlugInFile->FileName);
            MsgLog("  Force kext: %s\n", FileName);
            AddKext(Entry, Entry->Volume->RootDir, FileName, archCpuType);
            UnicodeSPrint(PlugIns, 512, L"%s\\%s", FileName, L"Contents\\PlugIns");
            LoadPlugInKexts(Entry, Entry->Volume->RootDir, PlugIns, archCpuType, TRUE);
          }
          DirIterClose(&PlugInIter);
        } else {
          AddKext(Entry, Entry->Volume->RootDir, Entry->KernelAndKextPatches->ForceKexts[i], archCpuType);

          UnicodeSPrint(PlugIns, 512, L"%s\\%s", Entry->KernelAndKextPatches->ForceKexts[i], L"Contents\\PlugIns");
          LoadPlugInKexts(Entry, Entry->Volume->RootDir, PlugIns, archCpuType, TRUE);
        }
      }
    }
  }

  //  Volume = Entry->Volume;
  SrcDir = GetOtherKextsDir();
  if (SrcDir != NULL) {
    // look through contents of the directory
    i = 0;
    DirIterOpen(SelfVolume->RootDir, SrcDir, &KextIter);
    while (DirIterNext(&KextIter, 1, L"*.kext", &KextFile)) {
      if ((KextFile->FileName[0] == '.') || (StriStr(KextFile->FileName, L".kext") == NULL)) {
        continue;   // skip this
      }

      if (!i) {
        MsgLog("Inject kexts (%s):\n", SrcDir);
      }

      i++;

      UnicodeSPrint(FileName, 512, L"%s\\%s", SrcDir, KextFile->FileName);
      MsgLog(" - [%02d]: %s\n", i, KextFile->FileName);
      AddKext(Entry, SelfVolume->RootDir, FileName, archCpuType);

      UnicodeSPrint(PlugIns, 512, L"%s\\%s", FileName, L"Contents\\PlugIns");
      LoadPlugInKexts(Entry, SelfVolume->RootDir, PlugIns, archCpuType, FALSE);
    }

    DirIterClose(&KextIter);
  }

  SrcDir = GetOSVersionKextsDir(Entry->OSVersion);
  if (SrcDir != NULL) {
    // look through contents of the directory
    i = 0;
    DirIterOpen(SelfVolume->RootDir, SrcDir, &KextIter);
    while (DirIterNext(&KextIter, 1, L"*.kext", &KextFile)) {
      if ((KextFile->FileName[0] == '.') || (StriStr(KextFile->FileName, L".kext") == NULL)) {
        continue;   // skip this
      }

      if (!i) {
        MsgLog("Inject kexts (%s):\n", SrcDir);
      }

      i++;

      UnicodeSPrint(FileName, 512, L"%s\\%s", SrcDir, KextFile->FileName);
      MsgLog(" - [%02d]: %s\n", i, KextFile->FileName);
      AddKext(Entry, SelfVolume->RootDir, FileName, archCpuType);

      UnicodeSPrint(PlugIns, 512, L"%s\\%s", FileName, L"Contents\\PlugIns");
      LoadPlugInKexts(Entry, SelfVolume->RootDir, PlugIns, archCpuType, FALSE);
    }

    DirIterClose(&KextIter);
  }

  gKextCount = GetKextCount();
  gKextSize = GetKextsSize();

  // reserve space in the device tree
  if (gKextCount > 0) {
    mm_extra_size = gKextCount * (sizeof(DeviceTreeNodeProperty) + sizeof(_DeviceTreeBuffer));
    mm_extra = AllocateZeroPool(mm_extra_size - sizeof(DeviceTreeNodeProperty));
    /*Status =  */LogDataHub(&gEfiMiscSubClassGuid, L"mm_extra", mm_extra, (UINT32)(mm_extra_size - sizeof(DeviceTreeNodeProperty)));
    extra_size = gKextSize;
    extra = AllocateZeroPool(extra_size - sizeof(DeviceTreeNodeProperty) + EFI_PAGE_SIZE);
    /*Status =  */LogDataHub(&gEfiMiscSubClassGuid, L"extra", extra, (UINT32)(extra_size - sizeof(DeviceTreeNodeProperty) + EFI_PAGE_SIZE));
    //MsgLog("count: %d    \n", gKextCount);
    //MsgLog("mm_extra_size: %d    \n", mm_extra_size);
    //MsgLog("extra_size: %d    \n", extra_size);
    //MsgLog("offset: %d      \n", extra_size - sizeof(DeviceTreeNodeProperty) + EFI_PAGE_SIZE);
  }

  return EFI_SUCCESS;
}

////////////////////
// OnExitBootServices
////////////////////
EFI_STATUS
InjectKexts (
  IN UINT32       deviceTreeP,
  IN UINT32       *deviceTreeLength,
  LOADER_ENTRY    *Entry
) {
  UINT8                             *dtEntry = (UINT8*)(UINTN) deviceTreeP, *infoPtr = 0, *extraPtr = 0, *drvPtr = 0;
  UINTN                             dtLength = (UINTN) *deviceTreeLength, offset = 0, KextBase = 0, Index;
  DTEntry                           platformEntry, memmapEntry;
  CHAR8                             *ptr;
  struct OpaqueDTPropertyIterator   OPropIter;
  DTPropertyIterator                iter = &OPropIter;
  DeviceTreeNodeProperty            *prop = NULL;
  LIST_ENTRY                        *Link;
  KEXT_ENTRY                        *KextEntry;
  _DeviceTreeBuffer                 *mm;
  _BooterKextFileInfo               *drvinfo;

  DBG_RT(Entry, "\nInjectKexts: ");

  if (gKextCount == 0) {
    DBG_RT(Entry, "no kexts to inject ...\n");
    DBG_PAUSE(Entry, 5);

    return EFI_NOT_FOUND;
  }

  DBG_RT(Entry, "%d kexts ...\n", gKextCount);

  DTInit(dtEntry);
  if (DTLookupEntry(NULL,"/chosen/memory-map",&memmapEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc(memmapEntry,iter) == kSuccess) {
      while (DTIterateProperties(iter,&ptr) == kSuccess) {
        prop = iter->currentProperty;
        drvPtr = (UINT8*) prop;

        if (
          (AsciiStrnCmp(prop->name, "Driver-", 7) == 0) ||
          (AsciiStrnCmp(prop->name, "DriversPackage-", 15) == 0)
        ) {
          break;
        }
      }
    }
  }

  if (DTLookupEntry(NULL,"/efi/platform",&platformEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc(platformEntry,iter) == kSuccess) {
      while (DTIterateProperties(iter,&ptr) == kSuccess) {
        prop = iter->currentProperty;
        if (AsciiStrCmp(prop->name,"mm_extra") == 0) {
          infoPtr = (UINT8*) prop;
        }

        if (AsciiStrCmp(prop->name,"extra") == 0) {
          extraPtr = (UINT8*) prop;
        }
      }
    }
  }

  if (
    (drvPtr == 0) ||
    (infoPtr == 0) ||
    (extraPtr == 0) ||
    (drvPtr > infoPtr) ||
    (drvPtr > extraPtr) ||
    (infoPtr > extraPtr)
  ) {
    DBG_RT(Entry, "\nInvalid device tree for kext injection\n");
    DBG_PAUSE(Entry, 5);

    return EFI_INVALID_PARAMETER;
  }

  // make space for memory map entries
  platformEntry->nProperties -= 2;
  offset = sizeof(DeviceTreeNodeProperty) + ((DeviceTreeNodeProperty*) infoPtr)->length;
  CopyMem(drvPtr+offset, drvPtr, infoPtr-drvPtr);

  // make space behind device tree
  // platformEntry->nProperties--;
  offset = sizeof(DeviceTreeNodeProperty)+((DeviceTreeNodeProperty*) extraPtr)->length;
  CopyMem(extraPtr, extraPtr+offset, dtLength-(UINTN)(extraPtr-dtEntry)-offset);
  *deviceTreeLength -= (UINT32)offset;

  KextBase = RoundPage(dtEntry + *deviceTreeLength);
  if (!IsListEmpty(&gKextList)) {
    Index = 1;
    for (Link = gKextList.ForwardLink; Link != &gKextList; Link = Link->ForwardLink) {
      KextEntry = CR(Link, KEXT_ENTRY, Link, KEXT_SIGNATURE);

      CopyMem((VOID*) KextBase, (VOID*)(UINTN) KextEntry->kext.paddr, KextEntry->kext.length);
      drvinfo = (_BooterKextFileInfo*) KextBase;
      drvinfo->infoDictPhysAddr += (UINT32) KextBase;
      drvinfo->executablePhysAddr += (UINT32) KextBase;
      drvinfo->bundlePathPhysAddr += (UINT32) KextBase;

      memmapEntry->nProperties++;
      prop = ((DeviceTreeNodeProperty*) drvPtr);
      prop->length = sizeof(_DeviceTreeBuffer);
      mm = (_DeviceTreeBuffer*) (((UINT8*)prop) + sizeof(DeviceTreeNodeProperty));
      mm->paddr = (UINT32)KextBase;
      mm->length = KextEntry->kext.length;
      AsciiSPrint(prop->name, 31, "Driver-%x", KextBase);

      drvPtr += sizeof(DeviceTreeNodeProperty) + sizeof(_DeviceTreeBuffer);
      KextBase = RoundPage(KextBase + KextEntry->kext.length);

      DBG_RT(Entry, " %d - %a\n", Index, (CHAR8 *)(UINTN)drvinfo->bundlePathPhysAddr);

#if 0
      if (
        gSettings.KextPatchesAllowed &&
        Entry->KernelAndKextPatches->NrKexts &&
        KernelAndKextPatcherInit(Entry)
      ) {
        INT32   i;
        CHAR8   SavedValue, *InfoPlist = (CHAR8*)(UINTN)drvinfo->infoDictPhysAddr,
                *gKextBundleIdentifier = ExtractKextBundleIdentifier(InfoPlist);

        SavedValue = InfoPlist[drvinfo->infoDictLength];
        InfoPlist[drvinfo->infoDictLength] = '\0';

        for (i = 0; i < Entry->KernelAndKextPatches->NrKexts; i++) {
          if (
            isPatchNameMatch(gKextBundleIdentifier, Entry->KernelAndKextPatches->KextPatches[i].Name, InfoPlist)
          ) {
            DBG_RT(Entry, "Kext: %a\n", gKextBundleIdentifier);
            AnyKextPatch (
              (UINT8*)(UINTN)drvinfo->executablePhysAddr,
              drvinfo->executableLength,
              InfoPlist,
              drvinfo->infoDictLength,
              i,
              Entry
            );
          }
        }

        InfoPlist[drvinfo->infoDictLength] = SavedValue;
        FreePool(gKextBundleIdentifier);
      }
#endif

      Index++;
    }
  }

  DBG_RT(Entry, "Done.\n");
  DBG_PAUSE(Entry, 5);

  return EFI_SUCCESS;
}
