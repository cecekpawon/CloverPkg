#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_KEXT_INJECT
#define DEBUG_KEXT_INJECT 0
#endif
#else
#ifdef DEBUG_KEXT_INJECT
#undef DEBUG_KEXT_INJECT
#endif
#define DEBUG_KEXT_INJECT DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_KEXT_INJECT, __VA_ARGS__)

// runtime debug
#define DBG_ON(Entry) \
  ((Entry != NULL) && (Entry->KernelAndKextPatches != NULL) && \
  ((DEBUG_KEXT_INJECT != 0) || OSFLAG_ISSET (Entry->Flags, OSFLAG_DBGPATCHES) || gSettings.DebugKP))
  /*Entry->KernelAndKextPatches->KPDebug && \*/
#define DBG_RT(Entry, ...) \
  if (DBG_ON (Entry)) AsciiPrint (__VA_ARGS__)
#define DBG_PAUSE(Entry, S) \
  if (DBG_ON (Entry)) gBS->Stall (S * 1000000)

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
  IN OUT  UINT8         **Binary,
  IN OUT  UINTN         *Length,
  IN      cpu_type_t    ArchCpuType
) {
  UINT32        Nfat, Swapped = 0, Size = 0, FAPOffset, FAPSize;
  FAT_HEADER    *FHP = (FAT_HEADER *)*Binary;
  FAT_ARCH      *FAP = (FAT_ARCH *)(*Binary + sizeof (FAT_HEADER));
  cpu_type_t    FAPcputype;

  if (FHP->magic == FAT_MAGIC) {
    Nfat = FHP->nfat_arch;
  } else if (FHP->magic == FAT_CIGAM) {
    Nfat = SwapBytes32 (FHP->nfat_arch);
    Swapped = 1;
    //already thin
  } else if (FHP->magic == THIN_X64){
    if (ArchCpuType == CPU_TYPE_X86_64) {
      return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
  } else {
    DBG ("Thinning fails\n");
    return EFI_NOT_FOUND;
  }

  for (; Nfat > 0; Nfat--, FAP++) {
    if (Swapped) {
      FAPcputype = SwapBytes32 (FAP->cputype);
      FAPOffset = SwapBytes32 (FAP->offset);
      FAPSize = SwapBytes32 (FAP->size);
    } else {
      FAPcputype = FAP->cputype;
      FAPOffset = FAP->offset;
      FAPSize = FAP->size;
    }

    if (FAPcputype == ArchCpuType) {
      *Binary = (*Binary + FAPOffset);
      Size = FAPSize;
      break;
    }
  }

  if (Length != 0) {
    *Length = Size;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LoadKext (
  IN      LOADER_ENTRY       *Entry,
  IN      EFI_FILE           *RootDir,
  IN      CHAR16             *FileName,
  IN OUT  DeviceTreeBuffer   *Kext
) {
  EFI_STATUS            Status;
  UINT8                 *InfoDictBuffer = NULL, *ExecutableFatBuffer = NULL, *ExecutableBuffer = NULL;
  UINTN                 InfoDictBufferLength = 0, ExecutableBufferLength = 0, BundlePathBufferLength = 0;
  CHAR8                 *BundlePathBuffer = NULL;
  CHAR16                TempName[AVALUE_MAX_SIZE], Executable[AVALUE_MAX_SIZE];
  TagPtr                Dict = NULL, Prop = NULL;
  BOOLEAN               NoContents = FALSE;
  BooterKextFileInfo    *InfoAddr = NULL;

  UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s\\%s", FileName, L"Contents\\Info.plist");

  Status = LoadFile (RootDir, TempName, &InfoDictBuffer, &InfoDictBufferLength);
  if (EFI_ERROR (Status)) {
    //try to find a planar kext, without Contents
    UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s\\%s", FileName, L"Info.plist");

    Status = LoadFile (RootDir, TempName, &InfoDictBuffer, &InfoDictBufferLength);
    if (EFI_ERROR (Status)) {
      DBG ("Failed to load extra kext (Info.plist not found): %s\n", FileName);
      return EFI_NOT_FOUND;
    }

    NoContents = TRUE;
  }

  if (EFI_ERROR (ParseXML ((CHAR8 *)InfoDictBuffer, &Dict, 0))) {
    FreePool (InfoDictBuffer);
    DBG ("Failed to load extra kext (failed to parse Info.plist): %s\n", FileName);
    return EFI_NOT_FOUND;
  }

  Prop = GetProperty (Dict, kPropCFBundleExecutable);
  if (Prop != 0) {
    AsciiStrToUnicodeStrS (Prop->string, Executable, ARRAY_SIZE (Executable));
    if (NoContents) {
      UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s\\%s", FileName, Executable);
    } else {
      UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s\\%s\\%s", FileName, L"Contents\\MacOS", Executable);
    }

    Status = LoadFile (RootDir, TempName, &ExecutableFatBuffer, &ExecutableBufferLength);
    if (EFI_ERROR (Status)) {
      FreePool (InfoDictBuffer);
      DBG ("Failed to load extra kext (Executable not found): %s\n", FileName);
      return EFI_NOT_FOUND;
    }

    ExecutableBuffer = ExecutableFatBuffer;
    if (EFI_ERROR (ThinFatFile (&ExecutableBuffer, &ExecutableBufferLength, CPU_TYPE_X86_64))) {
      FreePool (InfoDictBuffer);
      FreePool (ExecutableBuffer);
      DBG ("Thinning failed: %s\n", FileName);
      return EFI_NOT_FOUND;
    }
  } else {
    DBG ("Failed to read '%a' prop\n", kPropCFBundleExecutable);
  }

  BundlePathBufferLength = StrLen (FileName) + 1;
  BundlePathBuffer = AllocateZeroPool (BundlePathBufferLength);
  UnicodeStrToAsciiStrS (FileName, BundlePathBuffer, BundlePathBufferLength);

  Kext->length = (UINT32)(sizeof (BooterKextFileInfo) + InfoDictBufferLength + ExecutableBufferLength + BundlePathBufferLength);

  InfoAddr = (BooterKextFileInfo *)AllocatePool (Kext->length);
  InfoAddr->infoDictPhysAddr    = sizeof (BooterKextFileInfo);
  InfoAddr->infoDictLength      = (UINT32)InfoDictBufferLength;
  InfoAddr->executablePhysAddr  = (UINT32)(sizeof (BooterKextFileInfo) + InfoDictBufferLength);
  InfoAddr->executableLength    = (UINT32)ExecutableBufferLength;
  InfoAddr->bundlePathPhysAddr  = (UINT32)(sizeof (BooterKextFileInfo) + InfoDictBufferLength + ExecutableBufferLength);
  InfoAddr->bundlePathLength    = (UINT32)BundlePathBufferLength;

  Kext->paddr = (UINT32)(UINTN)InfoAddr; // Note that we cannot free InfoAddr because of this

  CopyMem ((CHAR8 *)InfoAddr + sizeof (BooterKextFileInfo), InfoDictBuffer, InfoDictBufferLength);
  CopyMem ((CHAR8 *)InfoAddr + sizeof (BooterKextFileInfo) + InfoDictBufferLength, ExecutableBuffer, ExecutableBufferLength);
  CopyMem ((CHAR8 *)InfoAddr + sizeof (BooterKextFileInfo) + InfoDictBufferLength + ExecutableBufferLength, BundlePathBuffer, BundlePathBufferLength);

  FreePool (InfoDictBuffer);
  FreePool (ExecutableFatBuffer);
  FreePool (BundlePathBuffer);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AddKext (
  IN LOADER_ENTRY   *Entry,
  IN EFI_FILE       *RootDir,
  IN CHAR16         *FileName
) {
  EFI_STATUS    Status;
  KEXT_ENTRY    *KextEntry;

  KextEntry = AllocatePool (sizeof (KEXT_ENTRY));
  KextEntry->Signature = KEXT_SIGNATURE;

  Status = LoadKext (Entry, RootDir, FileName, &KextEntry->kext);
  if (EFI_ERROR (Status)) {
    FreePool (KextEntry);
  } else {
    InsertTailList (&gKextList, &KextEntry->Link);
  }

  return Status;
}

UINT32
GetListCount (
  LIST_ENTRY  CONST *List
) {
  LIST_ENTRY    *Link;
  UINT32        Count = 0;

  if (!IsListEmpty (List)) {
    for (Link = List->ForwardLink; Link != List; Link = Link->ForwardLink) {
      Count++;
    }
  }

  return Count;
}

UINT32
GetKextCount () {
  return (UINT32)GetListCount (&gKextList);
}

UINT32
GetKextsSize () {
  LIST_ENTRY    *Link;
  KEXT_ENTRY    *KextEntry;
  UINT32        kextsSize = 0;

  if (!IsListEmpty (&gKextList)) {
    for (Link = gKextList.ForwardLink; Link != &gKextList; Link = Link->ForwardLink) {
      KextEntry = CR (Link, KEXT_ENTRY, Link, KEXT_SIGNATURE);
      kextsSize += RoundPage (KextEntry->kext.length);
    }
  }

  return kextsSize;
}

VOID
RecursiveLoadKexts (
  IN LOADER_ENTRY   *Entry,
  IN EFI_FILE       *RootDir,
  IN CHAR16         *SrcDir,
  IN BOOLEAN        PlugIn/**/
) {
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  CHAR16            FileName[AVALUE_MAX_SIZE], PlugIns[AVALUE_MAX_SIZE];
  UINTN             i = 0;

  if ((Entry == NULL) || (RootDir == NULL) || (SrcDir == NULL)) {
    return;
  }

  // look through contents of the directory
  DirIterOpen (RootDir, SrcDir, &DirIter);
  while (DirIterNext (&DirIter, 1, L"*.kext", &DirEntry)) {
    if ((DirEntry->FileName[0] == '.') || (StriStr (DirEntry->FileName, L".kext") == NULL)) {
      continue;   // skip this
    }

    if (!i) {
      MsgLog ("Inject kexts (%s):\n", SrcDir);
    }

    UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\%s", SrcDir, DirEntry->FileName);
    MsgLog (" - [%02d]: %s\n", i++, DirEntry->FileName);
    AddKext (Entry, RootDir, FileName);

    if (!PlugIn) {
      UnicodeSPrint (PlugIns, ARRAY_SIZE (PlugIns), L"%s\\%s", FileName, L"Contents\\PlugIns");
      RecursiveLoadKexts (Entry, RootDir, PlugIns, TRUE);
    }
  }

  DirIterClose (&DirIter);
}

EFI_STATUS
LoadKexts (
  IN LOADER_ENTRY   *Entry
) {
  //EFI_STATUS      Status;

  if ((Entry == 0) || OSFLAG_ISUNSET (Entry->Flags, OSFLAG_WITHKEXTS)) {
    return EFI_NOT_STARTED;
  }

  // Force kexts to load
  if (
    (Entry->KernelAndKextPatches != NULL) &&
    (Entry->KernelAndKextPatches->NrForceKexts > 0) &&
    (Entry->KernelAndKextPatches->ForceKexts != NULL)
  ) {
    CHAR16  PlugIns[AVALUE_MAX_SIZE];
    UINTN   i = 0;

    MsgLog ("Force kext: %d requested\n", Entry->KernelAndKextPatches->NrForceKexts);

    for (; i < Entry->KernelAndKextPatches->NrForceKexts; ++i) {
      //DBG ("  Force kext: %s\n", Entry->KernelAndKextPatches->ForceKexts[i]);
      if (Entry->Volume && Entry->Volume->RootDir) {
        // Check if the entry is a directory
        if (StriStr (Entry->KernelAndKextPatches->ForceKexts[i], L".kext") == NULL) {
          RecursiveLoadKexts (Entry, Entry->Volume->RootDir, Entry->KernelAndKextPatches->ForceKexts[i], FALSE);
        } else {
          AddKext (Entry, Entry->Volume->RootDir, Entry->KernelAndKextPatches->ForceKexts[i]);
          UnicodeSPrint (PlugIns, ARRAY_SIZE (PlugIns), L"%s\\%s", Entry->KernelAndKextPatches->ForceKexts[i], L"Contents\\PlugIns");
          RecursiveLoadKexts (Entry, Entry->Volume->RootDir, PlugIns, TRUE);
        }
      }
    }
  }

  RecursiveLoadKexts (Entry, SelfVolume->RootDir, GetOtherKextsDir (), FALSE);
  RecursiveLoadKexts (Entry, SelfVolume->RootDir, GetOSVersionKextsDir (Entry->OSVersion), FALSE);

  gKextCount = GetKextCount ();
  gKextSize = GetKextsSize ();

  // reserve space in the device tree
  if (gKextCount > 0) {
    UINTN   MMExtraSize, ExtraSize;
    VOID    *MMExtra, *Extra;

    MMExtraSize = gKextCount * (sizeof (DeviceTreeNodeProperty) + sizeof (DeviceTreeBuffer));
    MMExtra = AllocateZeroPool (MMExtraSize - sizeof (DeviceTreeNodeProperty));
    /*Status =  */LogDataHub (&gEfiMiscSubClassGuid, L"mm_extra", MMExtra, (UINT32)(MMExtraSize - sizeof (DeviceTreeNodeProperty)));
    ExtraSize = gKextSize;
    Extra = AllocateZeroPool (ExtraSize - sizeof (DeviceTreeNodeProperty) + EFI_PAGE_SIZE);
    /*Status =  */LogDataHub (&gEfiMiscSubClassGuid, L"extra", Extra, (UINT32)(ExtraSize - sizeof (DeviceTreeNodeProperty) + EFI_PAGE_SIZE));
    //MsgLog ("count: %d    \n", gKextCount);
    //MsgLog ("MMExtraSize: %d    \n", MMExtraSize);
    //MsgLog ("ExtraSize: %d    \n", ExtraSize);
    //MsgLog ("offset: %d      \n", ExtraSize - sizeof (DeviceTreeNodeProperty) + EFI_PAGE_SIZE);
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
          UINT8                     *gDTEntry = (UINT8 *)(UINTN) deviceTreeP, *InfoPtr = 0, *ExtraPtr = 0, *DrvPtr = 0;
          UINTN                     DtLength = (UINTN)*deviceTreeLength, Offset = 0, KextBase = 0, Index;
          DTEntry                   PlatformEntry, MemmapEntry;
          CHAR8                     *Ptr;
  struct  OpaqueDTPropertyIterator  OPropIter;
          DTPropertyIterator        Iter = &OPropIter;
          DeviceTreeNodeProperty    *Prop = NULL;
          LIST_ENTRY                *Link;
          KEXT_ENTRY                *KextEntry;
          DeviceTreeBuffer          *MM;
          BooterKextFileInfo        *Drvinfo;

  DBG_RT (Entry, "\nInjectKexts: ");

  if (gKextCount == 0) {
    DBG_RT (Entry, "no kexts to inject ...\n");
    DBG_PAUSE (Entry, 5);

    return EFI_NOT_FOUND;
  }

  DBG_RT (Entry, "%d kexts ...\n", gKextCount);

  DTInit (gDTEntry);

  if (DTLookupEntry (NULL,"/chosen/memory-map", &MemmapEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc (MemmapEntry, Iter) == kSuccess) {
      while (DTIterateProperties (Iter, &Ptr) == kSuccess) {
        Prop = Iter->currentProperty;
        DrvPtr = (UINT8 *)Prop;

        if (
          (AsciiStrnCmp (Prop->name, "Driver-", 7) == 0) ||
          (AsciiStrnCmp (Prop->name, "DriversPackage-", 15) == 0)
        ) {
          break;
        }
      }
    }
  }

  if (DTLookupEntry (NULL,"/efi/platform", &PlatformEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc (PlatformEntry, Iter) == kSuccess) {
      while (DTIterateProperties (Iter, &Ptr) == kSuccess) {
        Prop = Iter->currentProperty;
        if (AsciiStrCmp (Prop->name, "mm_extra") == 0) {
          InfoPtr = (UINT8 *)Prop;
        }

        if (AsciiStrCmp (Prop->name, "extra") == 0) {
          ExtraPtr = (UINT8 *)Prop;
        }
      }
    }
  }

  if (
    (DrvPtr == 0) ||
    (InfoPtr == 0) ||
    (ExtraPtr == 0) ||
    (DrvPtr > InfoPtr) ||
    (DrvPtr > ExtraPtr) ||
    (InfoPtr > ExtraPtr)
  ) {
    DBG_RT (Entry, "\nInvalid device tree for kext injection\n");
    DBG_PAUSE (Entry, 5);

    return EFI_INVALID_PARAMETER;
  }

  // make space for memory map entries
  PlatformEntry->nProperties -= 2;
  Offset = sizeof (DeviceTreeNodeProperty) + ((DeviceTreeNodeProperty *)InfoPtr)->length;
  CopyMem (DrvPtr + Offset, DrvPtr, InfoPtr - DrvPtr);

  // make space behind device tree
  // PlatformEntry->nProperties--;
  Offset = sizeof (DeviceTreeNodeProperty) + ((DeviceTreeNodeProperty *)ExtraPtr)->length;
  CopyMem (ExtraPtr, ExtraPtr + Offset, DtLength - (UINTN)(ExtraPtr - gDTEntry) - Offset);
  *deviceTreeLength -= (UINT32)Offset;

  KextBase = RoundPage (gDTEntry + *deviceTreeLength);
  if (!IsListEmpty (&gKextList)) {
    Index = 1;
    for (Link = gKextList.ForwardLink; Link != &gKextList; Link = Link->ForwardLink) {
      KextEntry = CR (Link, KEXT_ENTRY, Link, KEXT_SIGNATURE);

      CopyMem ((VOID *)KextBase, (VOID *)(UINTN)KextEntry->kext.paddr, KextEntry->kext.length);
      Drvinfo = (BooterKextFileInfo *)KextBase;
      Drvinfo->infoDictPhysAddr += (UINT32)KextBase;
      Drvinfo->executablePhysAddr += (UINT32)KextBase;
      Drvinfo->bundlePathPhysAddr += (UINT32)KextBase;

      MemmapEntry->nProperties++;
      Prop = ((DeviceTreeNodeProperty *)DrvPtr);
      Prop->length = sizeof (DeviceTreeBuffer);
      MM = (DeviceTreeBuffer *) (((UINT8 *)Prop) + sizeof (DeviceTreeNodeProperty));
      MM->paddr = (UINT32)KextBase;
      MM->length = KextEntry->kext.length;
      AsciiSPrint (Prop->name, 31, "Driver-%x", KextBase);

      DrvPtr += sizeof (DeviceTreeNodeProperty) + sizeof (DeviceTreeBuffer);
      KextBase = RoundPage (KextBase + KextEntry->kext.length);

      DBG_RT (Entry, " %d - %a\n", Index, (CHAR8 *)(UINTN)Drvinfo->bundlePathPhysAddr);

      if (
        gSettings.KextPatchesAllowed &&
        Entry->KernelAndKextPatches->NrKexts &&
        KernelAndKextPatcherInit (Entry)
      ) {
        INT32       i, isBundle;
        CHAR8       SavedValue, *InfoPlist = (CHAR8 *)(UINTN)Drvinfo->infoDictPhysAddr,
                    gKextBundleIdentifier[256];
#ifdef LAZY_PARSE_KEXT_PLIST
        TagPtr      KextsDict, DictPointer;
#endif

        SavedValue = InfoPlist[Drvinfo->infoDictLength];
        InfoPlist[Drvinfo->infoDictLength] = '\0';

#ifdef LAZY_PARSE_KEXT_PLIST
        if (EFI_ERROR (ParseXML (InfoPlist, &KextsDict, 0))) {
          continue;
        }

        DictPointer = GetProperty (KextsDict, kPropCFBundleIdentifier);
        if ((DictPointer != NULL) && DictPointer->string) {
          AsciiStrCpyS (gKextBundleIdentifier, ARRAY_SIZE (gKextBundleIdentifier), DictPointer->string);
        }
#else
        ExtractKextBundleIdentifier (gKextBundleIdentifier, ARRAY_SIZE (gKextBundleIdentifier), InfoPlist);
#endif

        for (i = 0; i < Entry->KernelAndKextPatches->NrKexts; i++) {
          if (
            !Entry->KernelAndKextPatches->KextPatches[i].Patched &&
            IsPatchNameMatch (gKextBundleIdentifier, Entry->KernelAndKextPatches->KextPatches[i].Name, InfoPlist, &isBundle)
          ) {
            //DBG_RT (Entry, "Kext: %a\n", gKextBundleIdentifier);

            AnyKextPatch (
              (UINT8 *)(UINTN)Drvinfo->executablePhysAddr,
              Drvinfo->executableLength,
              InfoPlist,
              Drvinfo->infoDictLength,
              &Entry->KernelAndKextPatches->KextPatches[i],
              Entry
            );

            if (isBundle) {
              Entry->KernelAndKextPatches->KextPatches[i].Patched = TRUE;
            }
          }
        }

        InfoPlist[Drvinfo->infoDictLength] = SavedValue;
        FreePool (gKextBundleIdentifier);
      }

      Index++;
    }
  }

  DBG_RT (Entry, "Done.\n");
  DBG_PAUSE (Entry, 2);

  return EFI_SUCCESS;
}
