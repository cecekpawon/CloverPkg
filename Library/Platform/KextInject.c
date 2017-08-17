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

  switch (FHP->magic) {
    case FAT_MAGIC:
      Nfat = FHP->nfat_arch;
      break;

    case FAT_CIGAM:
      Nfat = SwapBytes32 (FHP->nfat_arch);
      Swapped = 1;
      //already thin
      break;

    case THIN_X64:
      if (ArchCpuType == CPU_TYPE_X86_64) {
        return EFI_SUCCESS;
      }

    default:
      DBG ("Thinning fails\n");
      return EFI_NOT_FOUND;
      //break;
  }

  for (; Nfat > 0; Nfat--, FAP++) {
    FAPcputype  = Swapped ? SwapBytes32 (FAP->cputype) : FAP->cputype;
    FAPOffset   = Swapped ? SwapBytes32 (FAP->offset)  : FAP->offset;
    FAPSize     = Swapped ? SwapBytes32 (FAP->size)    : FAP->size;

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

VOID
FixLineEnding (
  UINT8   *Buffer,
  UINT32  Size
) {
  UINT8   Search[]  = { 0x0D, 0x0A }, // "CRLF"
          Replace[] = { 0x0A, 0x20 }; // "LF "

  SearchAndReplace (Buffer, Size, &Search[0], ARRAY_SIZE (Search), &Replace[0], 0xFF, -1);
}

EFI_STATUS
EFIAPI
LoadKext (
  IN      EFI_FILE           *RootDir,
  IN      CHAR16             *FileName,
  IN      UINT8              *InfoDictBuffer,
  IN OUT  DeviceTreeBuffer   *Kext
) {
  EFI_STATUS            Status, Ret = EFI_NOT_FOUND;
  UINT8                 *ExecutableFatBuffer = NULL, *ExecutableBuffer = NULL;
  UINTN                 InfoDictBufferLength = 0, ExecutableBufferLength = 0, BundlePathBufferLength = 0;
  CHAR8                 *BundlePathBuffer = NULL;
  CHAR16                TempName[AVALUE_MAX_SIZE];
  TagPtr                Dict = NULL, Prop = NULL;
  BOOLEAN               NoContents = FALSE;
  BooterKextFileInfo    *InfoAddr = NULL;

  UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s\\%s", FileName, L"Contents\\Info.plist");

  if (InfoDictBuffer) {
    InfoDictBufferLength = AsciiStrLen ((CHAR8 *)InfoDictBuffer);
  } else {
    Status = LoadFile (RootDir, TempName, &InfoDictBuffer, &InfoDictBufferLength);
    if (EFI_ERROR (Status)) {
      // try to find a planar kext, without Contents
      UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s\\%s", FileName, L"Info.plist");

      Status = LoadFile (RootDir, TempName, &InfoDictBuffer, &InfoDictBufferLength);
      if (EFI_ERROR (Status)) {
        DBG ("Failed to load extra kext (Info.plist not found): %s\n", FileName);
        goto Finish;
      }

      NoContents = TRUE;
    }
  }

  FixLineEnding (InfoDictBuffer, (UINT32)InfoDictBufferLength);

  if (EFI_ERROR (ParseXML ((CHAR8 *)InfoDictBuffer, 0, &Dict))) {
    DBG ("Failed to load extra kext (failed to parse Info.plist): %s\n", FileName);
    goto Finish;
  }

  Prop = GetProperty (Dict, kPropCFBundleExecutable);
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    UnicodeSPrint (TempName, SVALUE_MAX_SIZE, L"%s%s\\%a", FileName, NoContents ? L"" : L"\\Contents\\MacOS", Prop->string);

    Status = LoadFile (RootDir, TempName, &ExecutableFatBuffer, &ExecutableBufferLength);
    if (EFI_ERROR (Status)) {
      DBG ("Failed to load extra kext (Executable not found): %s\n", FileName);
      goto Finish;
    }

    ExecutableBuffer = ExecutableFatBuffer;
    if (EFI_ERROR (ThinFatFile (&ExecutableBuffer, &ExecutableBufferLength, CPU_TYPE_X86_64))) {
      FreePool (ExecutableBuffer);
      DBG ("Thinning failed: %s\n", FileName);
      goto Finish;
    }
  } /*else {
    DBG ("Failed to read '%a' prop\n", kPropCFBundleExecutable);
  }*/

  BundlePathBufferLength = StrLen (FileName) + 1;
  BundlePathBuffer = AllocateZeroPool (BundlePathBufferLength);
  UnicodeStrToAsciiStrS (FileName, BundlePathBuffer, BundlePathBufferLength);

  Kext->length = (UINT32)(sizeof (BooterKextFileInfo) + InfoDictBufferLength + ExecutableBufferLength + BundlePathBufferLength);

  InfoAddr = (BooterKextFileInfo *)AllocatePool (Kext->length);
  InfoAddr->infoDictPhysAddr    = sizeof (BooterKextFileInfo);
  InfoAddr->infoDictLength      = (UINT32)InfoDictBufferLength;
  InfoAddr->executablePhysAddr  = (UINT32)(InfoAddr->infoDictPhysAddr + InfoAddr->infoDictLength);
  InfoAddr->executableLength    = (UINT32)ExecutableBufferLength;
  InfoAddr->bundlePathPhysAddr  = (UINT32)(InfoAddr->executablePhysAddr + InfoAddr->executableLength);
  InfoAddr->bundlePathLength    = (UINT32)BundlePathBufferLength;

  Kext->paddr = (UINT32)(UINTN)InfoAddr;

  CopyMem ((CHAR8 *)InfoAddr + InfoAddr->infoDictPhysAddr, InfoDictBuffer, InfoDictBufferLength);
  if (ExecutableBuffer != NULL) {
    CopyMem ((CHAR8 *)InfoAddr + InfoAddr->executablePhysAddr, ExecutableBuffer, ExecutableBufferLength);
  }
  CopyMem ((CHAR8 *)InfoAddr + InfoAddr->bundlePathPhysAddr, BundlePathBuffer, BundlePathBufferLength);

  Ret = EFI_SUCCESS;

  Finish:

  if (InfoDictBuffer != NULL) {
    FreePool (InfoDictBuffer);
  }

  if (ExecutableFatBuffer != NULL) {
    FreePool (ExecutableFatBuffer);
  }

  if (BundlePathBuffer != NULL) {
    FreePool (BundlePathBuffer);
  }

  return Ret;
}

EFI_STATUS
EFIAPI
AddKext (
  IN EFI_FILE   *RootDir,
  IN CHAR16     *FileName,
  IN UINT8      *InfoDictBuffer
) {
  EFI_STATUS    Status;
  KEXT_ENTRY    *KextEntry;

  KextEntry = AllocatePool (sizeof (KEXT_ENTRY));
  KextEntry->Signature = KEXT_SIGNATURE;

  Status = LoadKext (RootDir, FileName, InfoDictBuffer, &KextEntry->Kext);
  if (EFI_ERROR (Status)) {
    FreePool (KextEntry);
  } else {
    InsertTailList (&gKextList, &KextEntry->Link);
  }

  return Status;
}

VOID
LoadIOPersonalitiesInjector (
  IN LOADER_ENTRY   *Entry
) {
  if (
    gSettings.KextPatchesAllowed &&
    (Entry->KernelAndKextPatches->IOPersonalitiesInjector != NULL) &&
    Entry->KernelAndKextPatches->NrIOPersonalitiesInjector
  ) {
    UINTN    i = 0;

    MsgLog ("Load IOPersonalitiesInjector:\n");

    for (; i < Entry->KernelAndKextPatches->NrIOPersonalitiesInjector; ++i) {
      BOOLEAN   NeedBuildVersion = (
                  (Entry->OSBuildVersion != NULL) &&
                  (Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].MatchBuild != NULL)
                );

      MsgLog (" - [%02d]: %a | [MatchOS: %a | MatchBuild: %a]",
        i,
        Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Label,
        Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].MatchOS
          ? Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].MatchOS
          : "All",
        NeedBuildVersion
          ? Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].MatchBuild
          : "All"
      );

      if (NeedBuildVersion) {
        Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled = !IsPatchEnabled (
          Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].MatchBuild, Entry->OSBuildVersion);

        MsgLog (" | Allowed: %a\n", Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled ? "No" : "Yes");

        //if (!Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled) {
          continue; // If user give MatchOS, should we ignore MatchOS / keep reading 'em?
        //}
      }

      Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled = !IsPatchEnabled (
        Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].MatchOS, Entry->OSVersion);

      MsgLog (" | Allowed: %a\n", Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled ? "No" : "Yes");

      if (!Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled) {
        AddKext (
          gSelfVolume->RootDir,
          // Is path really matter here?
          //PoolPrint (L"%s\\%a%a.kext", OSX_PATH_SLE, BOOTER_IOPERSONALITIES_PREFIX, Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Name),
          PoolPrint (L"%a%a.kext", BOOTER_IOPERSONALITIES_PREFIX, Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Name),
          (UINT8 *)Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].IOKitPersonalities
        );
      }
    }
  }
}

VOID
GetKextSummaries () {
  LIST_ENTRY    *Link;
  KEXT_ENTRY    *KextEntry;

  if (!IsListEmpty (&gKextList)) {
    for (Link = gKextList.ForwardLink; Link != &gKextList; Link = Link->ForwardLink) {
      KextEntry = CR (Link, KEXT_ENTRY, Link, KEXT_SIGNATURE);
      gKextSize += RoundPage (KextEntry->Kext.length);
      gKextCount++;
    }
  }
}

VOID
RecursiveLoadKexts (
  IN EFI_FILE   *RootDir,
  IN CHAR16     *SrcDir,
  IN BOOLEAN    PlugIn
) {
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  CHAR16            FileName[AVALUE_MAX_SIZE], PlugIns[AVALUE_MAX_SIZE], *Indent = L"         ";
  UINTN             i = 0;

  if ((RootDir == NULL) || (SrcDir == NULL)) {
    return;
  }

  // look through contents of the directory
  DirIterOpen (RootDir, SrcDir, &DirIter);
  while (DirIterNext (&DirIter, 1, L"*.kext", &DirEntry)) {
    if ((DirEntry->FileName[0] == '.') || (StriStr (DirEntry->FileName, L".kext") == NULL)) {
      continue; // skip this
    }

    if (!i) {
      MsgLog ("%sLoad kexts (%s):\n", PlugIn ? Indent : L"", SrcDir);
    }

    UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\%s", SrcDir, DirEntry->FileName);
    MsgLog ("%s - [%02d]: %s\n", PlugIn ? Indent : L"", i++, DirEntry->FileName);
    AddKext (RootDir, FileName, NULL);

    if (!PlugIn) {
      UnicodeSPrint (PlugIns, ARRAY_SIZE (PlugIns), L"%s\\%s", FileName, L"Contents\\PlugIns");
      RecursiveLoadKexts (RootDir, PlugIns, TRUE);
    }
  }

  DirIterClose (&DirIter);
}

EFI_STATUS
LoadKexts (
  IN LOADER_ENTRY   *Entry
) {
  //EFI_STATUS      Status;
  UINTN             Index = 0, InjectKextsDirCount = ARRAY_SIZE (gInjectKextsDir);

  if ((Entry == 0) || BIT_ISUNSET (Entry->Flags, OSFLAG_WITHKEXTS)) {
    return EFI_NOT_STARTED;
  }

  // Force kexts to load
  if (
    (Entry->KernelAndKextPatches != NULL) &&
    (Entry->KernelAndKextPatches->NrForceKexts > 0) &&
    (Entry->KernelAndKextPatches->ForceKexts != NULL)
  ) {
    CHAR16  PlugIns[AVALUE_MAX_SIZE];

    MsgLog ("Force kext: %d requested\n", Entry->KernelAndKextPatches->NrForceKexts);

    for (; Index < Entry->KernelAndKextPatches->NrForceKexts; ++Index) {
      //DBG ("  Force kext: %s\n", Entry->KernelAndKextPatches->ForceKexts[Index]);
      if (Entry->Volume && Entry->Volume->RootDir) {
        // Check if the entry is a directory
        if (StriStr (Entry->KernelAndKextPatches->ForceKexts[Index], L".kext") == NULL) {
          RecursiveLoadKexts (Entry->Volume->RootDir, Entry->KernelAndKextPatches->ForceKexts[Index], FALSE);
        } else {
          AddKext (Entry->Volume->RootDir, Entry->KernelAndKextPatches->ForceKexts[Index], NULL);
          UnicodeSPrint (PlugIns, ARRAY_SIZE (PlugIns), L"%s\\%s", Entry->KernelAndKextPatches->ForceKexts[Index], L"Contents\\PlugIns");
          RecursiveLoadKexts (Entry->Volume->RootDir, PlugIns, TRUE);
        }
      }
    }
  }

  for (Index = 0; Index < InjectKextsDirCount; Index++) {
    if (gInjectKextsDir[Index] != NULL) {
      RecursiveLoadKexts (gSelfVolume->RootDir, gInjectKextsDir[Index], FALSE);
    }
  }

  LoadIOPersonalitiesInjector (Entry);

  GetKextSummaries ();

  // reserve space in the device tree
  if (gKextCount > 0) {
    UINTN   MMExtraSize, ExtraSize;
    VOID    *MMExtra, *Extra;

    MMExtraSize = gKextCount * (sizeof (DeviceTreeNodeProperty) + sizeof (DeviceTreeBuffer));
    MMExtra = AllocateZeroPool (MMExtraSize - sizeof (DeviceTreeNodeProperty));
    /*Status =  */LogDataHub (&gEfiMiscSubClassGuid, L"DTKextsInfo", MMExtra, (UINT32)(MMExtraSize - sizeof (DeviceTreeNodeProperty)));
    ExtraSize = gKextSize;
    Extra = AllocateZeroPool (ExtraSize - sizeof (DeviceTreeNodeProperty) + EFI_PAGE_SIZE);
    /*Status =  */LogDataHub (&gEfiMiscSubClassGuid, L"DTKextsExtra", Extra, (UINT32)(ExtraSize - sizeof (DeviceTreeNodeProperty) + EFI_PAGE_SIZE));
    //DBG ("KextCount: %d\n", gKextCount);
    //DBG ("MMExtraSize: %d\n", MMExtraSize);
    //DBG ("ExtraSize: %d\n", ExtraSize);
    //DBG ("Offset: %d\n", ExtraSize - sizeof (DeviceTreeNodeProperty) + EFI_PAGE_SIZE);
  }

  return EFI_SUCCESS;
}

////////////////////
// OnExitBootServices
////////////////////

#if 0
BOOLEAN
IsIOPersonalitiesAllowed (
  IN LOADER_ENTRY   *Entry,
  IN CHAR8          *KextName
) {
  BOOLEAN   Ret = TRUE;
  CHAR8     *IOPersonalitiesName = AsciiStrStr (KextName, BOOTER_IOPERSONALITIES_PREFIX), Hold;
  UINTN     i = 0, Len = AsciiStrLen (BOOTER_IOPERSONALITIES_PREFIX);

  if (IOPersonalitiesName != NULL) {
    IOPersonalitiesName += Len;
    Len = IOPersonalitiesName - KextName;
    IOPersonalitiesName = AsciiStrStr (IOPersonalitiesName, ".kext");

    if (IOPersonalitiesName != NULL) {
      Ret = FALSE;

      if (gSettings.KextPatchesAllowed) {
        for (; (i < Entry->KernelAndKextPatches->NrIOPersonalitiesInjector) && !Ret; ++i) {
          if (Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Disabled) {
            continue;
          }

          Len = (IOPersonalitiesName - KextName) - Len;
          Hold = *IOPersonalitiesName;
          *IOPersonalitiesName = '\0';
          IOPersonalitiesName -= Len;

          if (AsciiStrCmp (IOPersonalitiesName, Entry->KernelAndKextPatches->IOPersonalitiesInjector[i].Name) == 0) {
            Ret = TRUE;
          }

          *(IOPersonalitiesName + Len) = Hold;
        }
      }
    }
  }

  return Ret;
}
#endif

EFI_STATUS
InjectKexts (
  IN UINT32         deviceTreeP,
  IN UINT32         *deviceTreeLength,
  IN LOADER_ENTRY   *Entry
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

  DBG ("InjectKexts: ");

  if (!gKextCount) {
    DBG ("no kexts to inject ...\n");
    return EFI_NOT_FOUND;
  }

  DBG ("%d kexts ...\n", gKextCount);

  DTInit (gDTEntry);

  if (DTLookupEntry (NULL, "/chosen/memory-map", &MemmapEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc (MemmapEntry, Iter) == kSuccess) {
      while (DTIterateProperties (Iter, &Ptr) == kSuccess) {
        Prop = Iter->currentProperty;
        DrvPtr = (UINT8 *)Prop;

        if (AsciiStrnCmp (Prop->name, BOOTER_KEXT_PREFIX, AsciiStrLen (BOOTER_KEXT_PREFIX)) == 0) {
          break;
        }
      }
    }
  }

  if (DTLookupEntry (NULL, "/efi/platform", &PlatformEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc (PlatformEntry, Iter) == kSuccess) {
      while (DTIterateProperties (Iter, &Ptr) == kSuccess) {
        Prop = Iter->currentProperty;
        if (AsciiStrCmp (Prop->name, "DTKextsInfo") == 0) {
          InfoPtr = (UINT8 *)Prop;
        }

        if (AsciiStrCmp (Prop->name, "DTKextsExtra") == 0) {
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
    DBG ("Invalid device tree for kext injection\n");
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
    Index = 0;
    for (Link = gKextList.ForwardLink; Link != &gKextList; Link = Link->ForwardLink) {
      //BOOLEAN   AllowedToLoad = TRUE;

      KextEntry = CR (Link, KEXT_ENTRY, Link, KEXT_SIGNATURE);

      CopyMem ((VOID *)KextBase, (VOID *)(UINTN)KextEntry->Kext.paddr, KextEntry->Kext.length);
      Drvinfo = (BooterKextFileInfo *)KextBase;
      Drvinfo->infoDictPhysAddr += (UINT32)KextBase;
      Drvinfo->executablePhysAddr += (UINT32)KextBase;
      Drvinfo->bundlePathPhysAddr += (UINT32)KextBase;

      MemmapEntry->nProperties++;
      Prop = ((DeviceTreeNodeProperty *)DrvPtr);
      Prop->length = sizeof (DeviceTreeBuffer);
      MM = (DeviceTreeBuffer *) (((UINT8 *)Prop) + sizeof (DeviceTreeNodeProperty));
      MM->paddr = (UINT32)KextBase;
      MM->length = KextEntry->Kext.length;

      //AllowedToLoad = IsIOPersonalitiesAllowed (Entry, (CHAR8 *)(UINTN)Drvinfo->bundlePathPhysAddr);

      //AsciiSPrint (Prop->name, 31, (AllowedToLoad ? (BOOTER_KEXT_PREFIX "%x") : (BOOTER_KEXT_NOLOAD_PREFIX "%x")), KextBase);
      AsciiSPrint (Prop->name, 31, BOOTER_KEXT_PREFIX "%x", KextBase);

      DrvPtr += sizeof (DeviceTreeNodeProperty) + sizeof (DeviceTreeBuffer);
      KextBase = RoundPage (KextBase + KextEntry->Kext.length);

      DBG (" - [%02d]: %a", Index++, (CHAR8 *)(UINTN)Drvinfo->bundlePathPhysAddr);

      if (
        gSettings.KextPatchesAllowed &&
        Entry->KernelAndKextPatches->NrKexts &&
        KernelAndKextPatcherInit (Entry)
      ) {
        INT32       i, IsBundle;
        CHAR8       SavedValue, *InfoPlist = (CHAR8 *)(UINTN)Drvinfo->infoDictPhysAddr,
                    KextBundleIdentifier[AVALUE_MAX_SIZE];
#ifdef LAZY_PARSE_KEXT_PLIST
        TagPtr      KextsDict, DictPointer;
#else
        CHAR8       KextBundleVersion[AVALUE_MAX_SIZE];
#endif

        SavedValue = InfoPlist[Drvinfo->infoDictLength];
        InfoPlist[Drvinfo->infoDictLength] = '\0';

#ifdef LAZY_PARSE_KEXT_PLIST
        if (EFI_ERROR (ParseXML (InfoPlist, 0, &KextsDict))) {
          DBG ("  - Error reading plist.\n");
          continue;
        }

        DictPointer = GetProperty (KextsDict, kPropCFBundleIdentifier);
        if ((DictPointer != NULL) && (DictPointer->type == kTagTypeString)) {
          AsciiStrCpyS (KextBundleIdentifier, ARRAY_SIZE (KextBundleIdentifier), DictPointer->string);
        }

        DictPointer = GetProperty (KextsDict, kPropCFBundleVersion);
        if ((DictPointer != NULL) && (DictPointer->type == kTagTypeString)) {
          //AsciiStrCpyS (KextBundleVersion, ARRAY_SIZE (KextBundleVersion), DictPointer->string);
          DBG (" (%a)", DictPointer->string);
        }
#else
        ExtractKextPropString (KextBundleIdentifier, ARRAY_SIZE (KextBundleIdentifier), PropCFBundleIdentifierKey, InfoPlist);
        ExtractKextPropString (KextBundleVersion, ARRAY_SIZE (KextBundleVersion), PropCFBundleVersionKey, InfoPlist);

        if (AsciiStrLen (KextBundleVersion)) {
          DBG (" (%a)", KextBundleVersion);
        }
#endif

        DBG ("\n");

        for (i = 0; i < Entry->KernelAndKextPatches->NrKexts; i++) {
          if (
            !Entry->KernelAndKextPatches->KextPatches[i].Disabled &&
            !Entry->KernelAndKextPatches->KextPatches[i].Patched &&
            IsPatchNameMatch (KextBundleIdentifier, Entry->KernelAndKextPatches->KextPatches[i].Name, InfoPlist, &IsBundle)
          ) {
            AnyKextPatch (
              KextBundleIdentifier,
              (UINT8 *)(UINTN)Drvinfo->executablePhysAddr,
              Drvinfo->executableLength,
              InfoPlist,
              Drvinfo->infoDictLength,
              &Entry->KernelAndKextPatches->KextPatches[i]
            );

            if (IsBundle) {
              Entry->KernelAndKextPatches->KextPatches[i].Patched = TRUE;
            }
          }
        }

        CheckForFakeSMC (InfoPlist, Entry);

        InfoPlist[Drvinfo->infoDictLength] = SavedValue;
      }
    }
  }

  return EFI_SUCCESS;
}
