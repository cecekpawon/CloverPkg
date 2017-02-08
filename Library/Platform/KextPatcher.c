/*
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 */

#include <Library/Platform/KernelPatcher.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_KEXT_PATCH
#define DEBUG_KEXT_PATCH 0
#endif
#else
#ifdef DEBUG_KEXT_PATCH
#undef DEBUG_KEXT_PATCH
#endif
#define DEBUG_KEXT_PATCH DEBUG_ALL
#endif

// runtime debug
#define DBG_ON(Entry) \
  ((Entry != NULL) && (Entry->KernelAndKextPatches != NULL) && \
  ((DEBUG_KEXT_PATCH != 0) || OSFLAG_ISSET (Entry->Flags, OSFLAG_DBGPATCHES) || gSettings.DebugKP))
  /*Entry->KernelAndKextPatches->KPDebug && \*/
#define DBG_RT(Entry, ...) \
  if (DBG_ON (Entry)) AsciiPrint (__VA_ARGS__)
#define DBG_PAUSE(Entry, S) \
  if (DBG_ON (Entry)) gBS->Stall (S * 1000000)

#ifdef LAZY_PARSE_KEXT_PLIST
#define PRELINKKEXTLIST_SIGNATURE SIGNATURE_32 ('C','E','X','T')

typedef struct {
  UINT32            Signature;
  LIST_ENTRY        Link;
  CHAR8             *BundleIdentifier;
  INTN              Address;
  INTN              Size;
  UINTN             Offset;
  UINTN             Taglen;
} PRELINKKEXTLIST;

LIST_ENTRY gPrelinkKextList = INITIALIZE_LIST_HEAD_VARIABLE (gPrelinkKextList)/*,
           gLoadedKextList = INITIALIZE_LIST_HEAD_VARIABLE (gLoadedKextList)*/;
#endif

//
// Searches Source for Search pattern of size SearchSize
// and returns the number of occurences.
//
UINTN
SearchAndCount (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize
) {
  UINTN     NumFounds = 0;
  UINT8     *End = Source + SourceSize;

  while (Source < End) {
    if (CompareMem (Source, Search, SearchSize) == 0) {
      NumFounds++;
      Source += SearchSize;
    } else {
      Source++;
    }
  }

  return NumFounds;
}

//
// Searches Source for Search pattern of size SearchSize
// and replaces it with Replace up to MaxReplaces times.
// If MaxReplaces <= 0, then there is no restriction on number of replaces.
// Replace should have the same size as Search.
// Returns number of replaces done.
//
UINTN
SearchAndReplace (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize,
  UINT8     *Replace,
  INTN      MaxReplaces
) {
  UINTN     NumReplaces = 0;
  BOOLEAN   NoReplacesRestriction = MaxReplaces <= 0;
  UINT8     *End = Source + SourceSize;

  if (!Source || !Search || !Replace || !SearchSize) {
    return 0;
  }

  while ((Source < End) && (NoReplacesRestriction || (MaxReplaces > 0))) {
    if (CompareMem (Source, Search, SearchSize) == 0) {
      CopyMem (Source, Replace, SearchSize);
      NumReplaces++;
      MaxReplaces--;
      Source += SearchSize;
    } else {
      Source++;
    }
  }

  return NumReplaces;
}

UINTN
SearchAndReplaceTxt (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize,
  UINT8     *Replace,
  INTN      MaxReplaces
) {
  BOOLEAN   NoReplacesRestriction = (MaxReplaces <= 0);
  UINTN     NumReplaces = 0, Skip;
  UINT8     *End = Source + SourceSize, *SearchEnd = Search + SearchSize, *Pos = NULL, *FirstMatch = Source;

  if (!Source || !Search || !Replace || !SearchSize) {
    return 0;
  }

  while (
    ((Source + SearchSize) <= End) &&
    (NoReplacesRestriction || (MaxReplaces > 0))
  ) { // num replaces
    while (*Source != '\0') {  //comparison
      Pos = Search;
      FirstMatch = Source;
      Skip = 0;

      while ((*Source != '\0') && (Pos != SearchEnd)) {
        if (*Source <= 0x20) { //skip invisibles in sources
          Source++;
          Skip++;
          continue;
        }

        if (*Source != *Pos) {
          break;
        }

        //AsciiPrint ("%c", *Source);
        Source++;
        Pos++;
      }

      if (Pos == SearchEnd) { // pattern found
        Pos = FirstMatch;
        break;
      } else {
        Pos = NULL;
      }

      Source = FirstMatch + 1;
      /*
      if (Pos != Search) {
        AsciiPrint ("\n");
      }
      */
    }

    if (!Pos) {
      break;
    }

    CopyMem (Pos, Replace, SearchSize);
    SetMem (Pos + SearchSize, Skip, 0x20); //fill skip places with spaces
    NumReplaces++;
    MaxReplaces--;
    Source = FirstMatch + SearchSize + Skip;
  }

  return NumReplaces;
}

BOOLEAN
IsPatchNameMatch (
  CHAR8   *BundleIdentifier,
  CHAR8   *Name,
  CHAR8   *InfoPlist,
  INT32   *IsBundle
) {
  *IsBundle = (CountOccurrences (Name, '.') < 2) ? 0 : 1;
  return
    (InfoPlist != NULL) && !IsBundle // Full BundleIdentifier: com.apple.driver.AppleHDA
      ? (AsciiStrStr (InfoPlist, Name) != NULL)
      : (AsciiStrCmp (BundleIdentifier, Name) == 0);
}

////////////////////////////////////
//
// ATIConnectors patch
//
// bcc9's patch: http://www.insanelymac.com/forum/index.php?showtopic=249642
//

// inited or not?
BOOLEAN   ATIConnectorsPatchInited = FALSE;

// ATIConnectorsController's bundle IDs for
// 0: ATI version - Lion, SnowLeo 10.6.7 2011 MBP
// 1: AMD version - ML
CHAR8   ATIKextBundleId[4][64];

//
// Inits patcher: prepares ATIKextBundleIds.
//
VOID
ATIConnectorsPatchInit (
  LOADER_ENTRY    *Entry
) {
  //
  // prepare bundle ids
  //

  // Lion, SnowLeo 10.6.7 2011 MBP
  AsciiSPrint (
    ATIKextBundleId[0],
    sizeof (ATIKextBundleId[0]),
    "com.apple.kext.ATI%sController",
    Entry->KernelAndKextPatches->KPATIConnectorsController
  );

  // ML
  AsciiSPrint (
    ATIKextBundleId[1],
    sizeof (ATIKextBundleId[1]),
    "com.apple.kext.AMD%sController",
    Entry->KernelAndKextPatches->KPATIConnectorsController
  );

  AsciiSPrint (
    ATIKextBundleId[2],
    sizeof (ATIKextBundleId[2]),
    "com.apple.kext.ATIFramebuffer"
  );

  AsciiSPrint (
    ATIKextBundleId[3],
    sizeof (ATIKextBundleId[3]),
    "com.apple.kext.AMDFramebuffer"
  );

  ATIConnectorsPatchInited = TRUE;

  //DBG_RT (Entry, "Bundle1: %a\n", ATIKextBundleId[0]);
  //DBG_RT (Entry, "Bundle2: %a\n", ATIKextBundleId[1]);
  //DBG_RT (Entry, "Bundle3: %a\n", ATIKextBundleId[2]);
  //DBG_RT (Entry, "Bundle4: %a\n", ATIKextBundleId[3]);
  //DBG_PAUSE (Entry, 5);
}

//
// Registers kexts that need force-load during WithKexts boot.
//
VOID
ATIConnectorsPatchRegisterKexts (
  FSINJECTION_PROTOCOL  *FSInject,
  FSI_STRING_LIST       *ForceLoadKexts,
  LOADER_ENTRY          *Entry
) {
  CHAR16  *AtiForceLoadKexts[] = {
            L"\\IOGraphicsFamily.kext\\Info.plist",
            L"\\ATISupport.kext\\Contents\\Info.plist",
            L"\\AMDSupport.kext\\Contents\\Info.plist",
            L"\\AppleGraphicsControl.kext\\Info.plist",
            L"\\AppleGraphicsControl.kext\\Contents\\PlugIns\\AppleGraphicsDeviceControl.kext\\Info.plist"
            /*,
            // SnowLeo
            L"\\ATIFramebuffer.kext\\Contents\\Info.plist",
            L"\\AMDFramebuffer.kext\\Contents\\Info.plist"
            */
          };
  UINTN   i = 0, AtiForceLoadKextsCount = ARRAY_SIZE (AtiForceLoadKexts);

  // for future?
  FSInject->AddStringToList (
              ForceLoadKexts,
              PoolPrint (L"\\AMD%sController.kext\\Contents\\Info.plist", Entry->KernelAndKextPatches->KPATIConnectorsController)
            );

  // Lion, ML, SnowLeo 10.6.7 2011 MBP
  FSInject->AddStringToList (
              ForceLoadKexts,
              PoolPrint (L"\\ATI%sController.kext\\Contents\\Info.plist", Entry->KernelAndKextPatches->KPATIConnectorsController)
            );

  // dependencies
  while (i < AtiForceLoadKextsCount) {
    FSInject->AddStringToList (ForceLoadKexts, AtiForceLoadKexts[i++]);
  }
}

//
// Patch function.
//
VOID
ATIConnectorsPatch (
  UINT8         *Driver,
  UINT32        DriverSize,/*
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,*/
  LOADER_ENTRY  *Entry
) {
  UINTN   Num = 0;

  DBG_RT (Entry, "\nATIConnectorsPatch: driverAddr = %x, driverSize = %x\nController = %s\n",
         Driver, DriverSize, Entry->KernelAndKextPatches->KPATIConnectorsController);

  // number of occurences od Data should be 1
  //Num = SearchAndCount (
  //        Driver,
  //        DriverSize,
  //        Entry->KernelAndKextPatches->KPATIConnectorsData,
  //        Entry->KernelAndKextPatches->KPATIConnectorsDataLen
  //      );

  //if (Num > 1) {
  //  // error message - shoud always be printed
  //  DBG_RT (Entry, "==> KPATIConnectorsData found %d times - skip patching!\n", Num);
  //  //DBG_PAUSE (Entry, 5);
  //  return;
  //}

  // patch
  Num = SearchAndReplace (
          Driver,
          DriverSize,
          Entry->KernelAndKextPatches->KPATIConnectorsData,
          Entry->KernelAndKextPatches->KPATIConnectorsDataLen,
          Entry->KernelAndKextPatches->KPATIConnectorsPatch,
          1
        );

  if (Num > 0) {
    DBG_RT (Entry, "==> patched %d times!\n", Num);
  } else {
    DBG_RT (Entry, "==> NOT patched!\n");
  }

  //DBG_PAUSE (Entry, 5);
}

VOID
GetText (
  UINT8           *binary,
  OUT UINT32      *Addr,
  OUT UINT32      *Size,
  OUT UINT32      *Off,
  LOADER_ENTRY    *Entry
) {
  struct  load_command        *loadCommand;
  struct  segment_command_64  *segCmd64;
  struct  section_64          *sect64;
          UINT32              ncmds, cmdsize, binaryIndex, sectionIndex;
          UINTN               cnt;

  if (MACH_GET_MAGIC (binary) != MH_MAGIC_64) {
    return;
  }

  binaryIndex = sizeof (struct mach_header_64);

  ncmds = MACH_GET_NCMDS (binary);

  for (cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct load_command *)(binary + binaryIndex);

    if (loadCommand->cmd != LC_SEGMENT_64) {
      continue;
    }

    cmdsize = loadCommand->cmdsize;
    segCmd64 = (struct segment_command_64 *)loadCommand;
    sectionIndex = sizeof (struct segment_command_64);

    while (sectionIndex < segCmd64->cmdsize) {
      sect64 = (struct section_64 *)((UINT8 *)segCmd64 + sectionIndex);
      sectionIndex += sizeof (struct section_64);

      if (
        (sect64->size > 0) &&
        (AsciiStrCmp (sect64->segname, kTextSegment) == 0) &&
        (AsciiStrCmp (sect64->sectname, kTextTextSection) == 0)
      ) {
        *Addr = (UINT32)sect64->addr;
        *Size = (UINT32)sect64->size;
        *Off = sect64->offset;
        //DBG_RT (Entry, "%a, %a address 0x%x\n", kTextSegment, kTextTextSection, Off);
        //DBG_PAUSE (Entry, 10);
        break;
      }
    }

    binaryIndex += cmdsize;
  }

  return;
}

// TODO: to swap this method, hardcodedless

////////////////////////////////////
//
// AsusAICPUPM patch
//
// fLaked's SpeedStepper patch for Asus (and some other) boards:
// http://www.insanelymac.com/forum/index.php?showtopic=258611
//
// Credits: Samantha/RevoGirl/DHP
// http://www.insanelymac.com/forum/topic/253642-dsdt-for-asus-p8p67-m-pro/page__st__200#entry1681099
// Rehabman corrections 2014
//
UINT8   MovlE2ToEcx[] = { 0xB9, 0xE2, 0x00, 0x00, 0x00 };
UINT8   MovE2ToCx[]   = { 0x66, 0xB9, 0xE2, 0x00 };
UINT8   Wrmsr[]       = { 0x0F, 0x30 };

VOID
AsusAICPUPMPatch (
  UINT8           *Driver,
  UINT32          DriverSize,/*
  CHAR8           *InfoPlist,
  UINT32          InfoPlistSize,*/
  LOADER_ENTRY    *Entry
) {
  UINTN   Index1 = 0, Index2 = 0, Count = 0;
  UINT32  addr, size, off;

  GetText (Driver, &addr, &size, &off, Entry);

  DBG_RT (Entry, "\nAsusAICPUPMPatch: driverAddr = %x, driverSize = %x\n", Driver, DriverSize);

  if (off && size) {
    Index1 = off;
    DriverSize = (off + size);
  }

  // TODO: we should scan only __text __TEXT
  // DONE: https://github.com/cecekpawon/Clover/commit/6e5a49ab2125889f666cc810f267552ba0627f74#diff-749d163a7ade3d836e727afe79424a70L5
  for (; Index1 < DriverSize; Index1++) {
    // search for MovlE2ToEcx
    if (CompareMem (Driver + Index1, MovlE2ToEcx, sizeof (MovlE2ToEcx)) == 0) {
      // search for wrmsr in next few bytes
      for (Index2 = Index1 + sizeof (MovlE2ToEcx); Index2 < Index1 + sizeof (MovlE2ToEcx) + 32; Index2++) {
        if ((Driver[Index2] == Wrmsr[0]) && (Driver[Index2 + 1] == Wrmsr[1])) {
          // found it - patch it with nops
          Count++;
          Driver[Index2] = 0x90;
          Driver[Index2 + 1] = 0x90;
          DBG_RT (Entry, " %d. patched at 0x%x\n", Count, Index2);
          break;
        } else if (
          ((Driver[Index2] == 0xC9) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0x5D) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0xB9) && (Driver[Index2 + 3] == 0) && (Driver[Index2 + 4] == 0)) ||
          ((Driver[Index2] == 0x66) && (Driver[Index2 + 1] == 0xB9) && (Driver[Index2 + 3] == 0))
        ) {
          // a leave/ret will cancel the search
          // so will an intervening "mov[l] $xx, [e]cx"
          break;
        }
      }
    } else if (CompareMem (Driver + Index1, MovE2ToCx, sizeof (MovE2ToCx)) == 0) {
      // search for wrmsr in next few bytes
      for (Index2 = Index1 + sizeof (MovE2ToCx); Index2 < Index1 + sizeof (MovE2ToCx) + 32; Index2++) {
        if ((Driver[Index2] == Wrmsr[0]) && (Driver[Index2 + 1] == Wrmsr[1])) {
          // found it - patch it with nops
          Count++;
          Driver[Index2] = 0x90;
          Driver[Index2 + 1] = 0x90;
          DBG_RT (Entry, " %d. patched at 0x%x\n", Count, Index2);
          break;
        } else if (
          ((Driver[Index2] == 0xC9) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0x5D) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0xB9) && (Driver[Index2 + 3] == 0) && (Driver[Index2 + 4] == 0)) ||
          ((Driver[Index2] == 0x66) && (Driver[Index2 + 1] == 0xB9) && (Driver[Index2 + 3] == 0))
        ) {
          // a leave/ret will cancel the search
          // so will an intervening "mov[l] $xx, [e]cx"
          break;
        }
      }
    }
  }

  DBG_RT (Entry, "= %d patches\n", Count);
  //DBG_PAUSE (Entry, 5);
}

////////////////////////////////////
//
// Place other kext patches here
//

// ...

////////////////////////////////////
//
// Generic kext patch functions
//
//
VOID
AnyKextPatch (
  UINT8         *Driver,
  UINT32        DriverSize,
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,
  KEXT_PATCH    *KextPatches,
  LOADER_ENTRY  *Entry
) {
  UINTN   Num = 0;
  //INTN    Ind;

  DBG_RT (Entry, "\nAnyKextPatch: driverAddr = %x, driverSize = %x\nAnyKext = %a\n",
         Driver, DriverSize, KextPatches->Label);

  if (KextPatches->Disabled) {
    DBG_RT (Entry, "==> DISABLED!\n");
    return;
  }

  if (KextPatches->IsPlistPatch) { // Info plist patch
    DBG_RT (Entry, "Info.plist patch\n");

    /*
      DBG_RT (Entry, "Info.plist data : '");

      for (Ind = 0; Ind < KextPatches->DataLen; Ind++) {
        DBG_RT (Entry, "%c", KextPatches->Data[Ind]);
      }

      DBG_RT (Entry, "' ->\n");
      DBG_RT (Entry, "Info.plist patch: '");

      for (Ind = 0; Ind < KextPatches->DataLen; Ind++) {
        DBG_RT (Entry, "%c", KextPatches->Patch[Ind]);
      }

      DBG_RT (Entry, "' \n");
    */

    Num = SearchAndReplaceTxt (
            (UINT8 *)InfoPlist,
            InfoPlistSize,
            KextPatches->Data,
            KextPatches->DataLen,
            KextPatches->Patch,
            -1
          );
  } else { // kext binary patch
    UINT32    addr, size, off;

    GetText (Driver, &addr, &size, &off, Entry);

    DBG_RT (Entry, "Binary patch\n");

    if (off && size) {
      Driver += off;
      DriverSize = size;
    }

    Num = SearchAndReplace (
            Driver,
            DriverSize,
            KextPatches->Data,
            KextPatches->DataLen,
            KextPatches->Patch,
            -1
          );
  }

  if (Num > 0) {
    DBG_RT (Entry, "==> patched %d times!\n", Num);
  } else {
    DBG_RT (Entry, "==> NOT patched!\n");
  }

  DBG_PAUSE (Entry, 1);
}

//
// Called from SetFSInjection (), before boot.efi is started,
// to allow patchers to prepare FSInject to force load needed kexts.
//
VOID
KextPatcherRegisterKexts (
  FSINJECTION_PROTOCOL    *FSInject,
  FSI_STRING_LIST         *ForceLoadKexts,
  LOADER_ENTRY            *Entry
) {
  INTN i;

  if (Entry->KernelAndKextPatches->KPATIConnectorsController != NULL) {
    ATIConnectorsPatchRegisterKexts (FSInject, ForceLoadKexts, Entry);
  }

  // Not sure about this. NrKexts / NrForceKexts, what purposes?
  for (i = 0; i < Entry->KernelAndKextPatches->/* NrKexts */NrForceKexts; i++) {
    FSInject->AddStringToList (
                ForceLoadKexts,
                //PoolPrint (
                //  L"\\%a.kext\\Contents\\Info.plist",
                //    Entry->KernelAndKextPatches->KextPatches[i].Filename
                //      ? Entry->KernelAndKextPatches->KextPatches[i].Filename
                //      : Entry->KernelAndKextPatches->KextPatches[i].Name
                //)
                PoolPrint (L"\\%a\\Contents\\Info.plist", Entry->KernelAndKextPatches->ForceKexts[i])
              );
  }
}

//
// PatchKext is called for every kext from prelinked kernel (kernelcache) or from DevTree (booting with drivers).
// Add kext detection code here and call kext specific patch function.
//
VOID
PatchKext (
  UINT8         *Driver,
  UINT32        DriverSize,
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,
  CHAR8         *BundleIdentifier,
  LOADER_ENTRY  *Entry
) {
  INT32  isBundle;

  //
  // ATIConnectors
  //

  if (
    (Entry->KernelAndKextPatches->KPATIConnectorsController != NULL) &&
    (
      IsPatchNameMatch (BundleIdentifier, ATIKextBundleId[0], NULL, &isBundle) ||
      IsPatchNameMatch (BundleIdentifier, ATIKextBundleId[1], NULL, &isBundle) ||
      IsPatchNameMatch (BundleIdentifier, ATIKextBundleId[2], NULL, &isBundle) ||
      IsPatchNameMatch (BundleIdentifier, ATIKextBundleId[3], NULL, &isBundle)
    )
  ) {
    if (!ATIConnectorsPatchInited) {
      ATIConnectorsPatchInit (Entry);
    }

    ATIConnectorsPatch (Driver, DriverSize, Entry);

    FreePool (Entry->KernelAndKextPatches->KPATIConnectorsController);
    Entry->KernelAndKextPatches->KPATIConnectorsController = NULL;

    if (Entry->KernelAndKextPatches->KPATIConnectorsData != NULL) {
      FreePool (Entry->KernelAndKextPatches->KPATIConnectorsData);
    }

    if (Entry->KernelAndKextPatches->KPATIConnectorsPatch != NULL) {
      FreePool (Entry->KernelAndKextPatches->KPATIConnectorsPatch);
    }

  //
  // AsusAICPUPM
  //

  } else if (
    Entry->KernelAndKextPatches->KPAsusAICPUPM &&
    IsPatchNameMatch (BundleIdentifier, NULL, "com.apple.driver.AppleIntelCPUPowerManagement", &isBundle)
  ) {
    AsusAICPUPMPatch (Driver, DriverSize, Entry);
    Entry->KernelAndKextPatches->KPAsusAICPUPM = FALSE;

  //
  // Others
  //

  } else {
    INT32   i;

    for (i = 0; i < Entry->KernelAndKextPatches->NrKexts; i++) {
      if (
        Entry->KernelAndKextPatches->KextPatches[i].Patched || // avoid redundant: if unique / isBundle
        Entry->KernelAndKextPatches->KextPatches[i].Disabled ||
        !IsPatchNameMatch (BundleIdentifier, Entry->KernelAndKextPatches->KextPatches[i].Name, InfoPlist, &isBundle)
      ) {
        continue;
      }

      AnyKextPatch (Driver, DriverSize, InfoPlist, InfoPlistSize, &Entry->KernelAndKextPatches->KextPatches[i], Entry);
      if (isBundle) {
        Entry->KernelAndKextPatches->KextPatches[i].Patched = TRUE;
      }
    }
  }
}

#ifdef LAZY_PARSE_KEXT_PLIST

EFI_STATUS
ParsePrelinkKexts (
  LOADER_ENTRY    *Entry,
  CHAR8           *WholePlist
) {
  TagPtr        KextsDict, DictPointer;
  EFI_STATUS    Status = ParseXML (WholePlist, &KextsDict, 0);

  //DBG_RT (Entry, "Parse %a: %r\n", kPrelinkInfoDictionaryKey, Status);
  //DBG_PAUSE (Entry, 5);

  if (EFI_ERROR (Status)) {
    KextsDict = NULL;
  } else {
    DictPointer = GetProperty (KextsDict, kPrelinkInfoDictionaryKey);
    if ((DictPointer != NULL) && (DictPointer->type == kTagTypeArray)) {
      INTN    Count = DictPointer->size, i = 0,
              iPrelinkExecutableSourceKey, iPrelinkExecutableSourceKeySize, //KextAddr
              iPrelinkExecutableSizeKey, iPrelinkExecutableSizeKeySize; //KextSize
      TagPtr  Dict, Prop;
      CHAR8   *sPrelinkExecutableSourceKey, *sPrelinkExecutableSizeKey;

      while (i < Count) {
        Status = GetElement (DictPointer, i++, Count, &Dict);

        if (
          EFI_ERROR (Status) ||
          (Dict == NULL) ||
          !Dict->offset ||
          !Dict->taglen
        ) {
          //DBG_RT (Entry, " %r parsing / empty element\n", Status);
          //DBG_PAUSE (Entry, 1);
          continue;
        }

        Prop = GetProperty (Dict, kPrelinkExecutableSourceKey);
        if (
          (Prop == NULL) ||
          (
            (Prop->ref != -1) &&
            EFI_ERROR (
              GetRefInteger (
                KextsDict,
                Prop->ref,
                &sPrelinkExecutableSourceKey,
                &iPrelinkExecutableSourceKey,
                &iPrelinkExecutableSourceKeySize
              )
            )
          )
        ) {
          continue;
        } else {
          iPrelinkExecutableSourceKey = Prop->integer;
        }

        Prop = GetProperty (Dict, kPrelinkExecutableSizeKey);
        if (
          (Prop == NULL) ||
          (
            (Prop->ref != -1) &&
            EFI_ERROR (
              GetRefInteger (
                KextsDict,
                Prop->ref,
                &sPrelinkExecutableSizeKey,
                &iPrelinkExecutableSizeKey,
                &iPrelinkExecutableSizeKeySize
              )
            )
          )
        ) {
          continue;
        } else {
          iPrelinkExecutableSizeKey = Prop->integer;
        }

        Prop = GetProperty (Dict, kPropCFBundleIdentifier);
        if (
          (Prop != NULL) && Prop->string &&
          iPrelinkExecutableSourceKey &&
          iPrelinkExecutableSizeKey
        ) {
          // To speed up process sure we can apply all patches here immediately.
          // By saving all kexts data into list could be useful for other purposes, I hope.
          PRELINKKEXTLIST   *nKext = AllocateZeroPool (sizeof (PRELINKKEXTLIST));

          if (nKext) {
            nKext->Signature = PRELINKKEXTLIST_SIGNATURE;
            nKext->BundleIdentifier = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
            nKext->Address = iPrelinkExecutableSourceKey;
            nKext->Size = iPrelinkExecutableSizeKey;
            nKext->Offset = Dict->offset;
            nKext->Taglen = Dict->taglen;
            InsertTailList (&gPrelinkKextList, (LIST_ENTRY *)(((UINT8 *)nKext) + OFFSET_OF (PRELINKKEXTLIST, Link)));
          }
        }
      }

      //DBG_RT (Entry, "Count %d\n", Count);
    } else {
      DBG_RT (Entry, "NO kPrelinkInfoDictionaryKey\n");
      DBG_PAUSE (Entry, 5);
    }
  }

  //DBG_RT (Entry, "%a: done\n", __FUNCTION__);
  //DBG_PAUSE (Entry, 5);

  return IsListEmpty (&gPrelinkKextList) ? EFI_UNSUPPORTED : EFI_SUCCESS;
}

//
// Iterates over kexts in kernelcache
// and calls PatchKext () for each.
//
// PrelinkInfo section contains following plist, without spaces:
// <dict>
//   <key>_PrelinkInfoDictionary</key>
//   <array>
//     <!-- start of kext Info.plist -->
//     <dict>
//       <key>CFBundleName</key>
//       <string>MAC Framework Pseudoextension</string>
//       <key>_PrelinkExecutableLoadAddr</key>
//       <integer size="64">0xffffff7f8072f000</integer>
//       <!-- Kext size -->
//       <key>_PrelinkExecutableSize</key>
//       <integer size="64">0x3d0</integer>
//       <!-- Kext address -->
//       <key>_PrelinkExecutableSourceAddr</key>
//       <integer size="64">0xffffff80009a3000</integer>
//       ...
//     </dict>
//     <!-- start of next kext Info.plist -->
//     <dict>
//       ...
//     </dict>
//       ...
VOID
PatchPrelinkedKexts (
  LOADER_ENTRY    *Entry
) {
  CHAR8   *WholePlist = (CHAR8 *)(UINTN)KernelInfo->PrelinkInfoAddr;

  if (!EFI_ERROR (ParsePrelinkKexts (Entry, WholePlist))) {
    LIST_ENTRY    *Link;

    for (Link = gPrelinkKextList.ForwardLink; Link != &gPrelinkKextList; Link = Link->ForwardLink) {
      PRELINKKEXTLIST   *sKext = CR (Link, PRELINKKEXTLIST, Link, PRELINKKEXTLIST_SIGNATURE);
      CHAR8             *InfoPlist = AllocateCopyPool (sKext->Taglen, WholePlist + sKext->Offset);

      InfoPlist[sKext->Taglen] = '\0';

      // patch it
      PatchKext (
        (UINT8 *)(UINTN) (
            // get kext address from _PrelinkExecutableSourceAddr
            // truncate to 32 bit to get physical addr
            (UINT32)sKext->Address +
            // KextAddr is always relative to 0x200000
            // and if KernelSlide is != 0 then KextAddr must be adjusted
            KernelInfo->Slide +
            // and adjust for AptioFixDrv's KernelRelocBase
            (UINT32)KernelInfo->RelocBase
          ),
        (UINT32)sKext->Size,
        WholePlist + sKext->Offset,
        (UINT32)sKext->Taglen,
        sKext->BundleIdentifier,
        Entry
      );

      FreePool (InfoPlist);
    }
  }
}

#else

/** Extracts kext BundleIdentifier from given Plist into gKextBundleIdentifier */

#define PropCFBundleIdentifierKey "<key>" kPropCFBundleIdentifier "</key>"

VOID
ExtractKextBundleIdentifier (
  OUT CHAR8   *Res,
  IN  INTN    Len,
  IN  CHAR8   *Plist
) {
  CHAR8     *Tag, *BIStart, *BIEnd;
  INTN      DictLevel = 0;

  Res[0] = '\0';

  // start with first <dict>
  Tag = AsciiStrStr (Plist, "<dict>");
  if (Tag == NULL) {
    return;
  }

  Tag += 6;
  DictLevel++;

  while (*Tag != '\0') {
    if (AsciiStrnCmp (Tag, "<dict>", 6) == 0) {
      // opening dict
      DictLevel++;
      Tag += 6;
    } else if (AsciiStrnCmp (Tag, "</dict>", 7) == 0) {
      // closing dict
      DictLevel--;
      Tag += 7;
    } else if ((DictLevel == 1) && (AsciiStrnCmp (Tag, PropCFBundleIdentifierKey, 29) == 0)) {
      // BundleIdentifier is next <string>...</string>
      BIStart = AsciiStrStr (Tag + 29, "<string>");
      if (BIStart != NULL) {
        BIStart += 8; // skip "<string>"
        BIEnd = AsciiStrStr (BIStart, "</string>");
        if ((BIEnd != NULL) && ((BIEnd - BIStart + 1) < Len)) {
          CopyMem (Res, BIStart, BIEnd - BIStart);
          Res[BIEnd - BIStart] = '\0';
          break;
        }
      }
      Tag++;
    } else {
      Tag++;
    }

    // advance to next tag
    while ((*Tag != '<') && (*Tag != '\0')) {
      Tag++;
    }
  }
}

//
// Returns parsed hex integer key.
// Plist - kext pist
// Key - key to find
// WholePlist - _PrelinkInfoDictionary, used to find referenced values
//
// Searches for Key in Plist and it's value:
// a) <integer ID="26" size="64">0x2b000</integer>
//    returns 0x2b000
// b) <integer IDREF="26"/>
//    searches for <integer ID="26"... from WholePlist
//    and returns value from that referenced field
//
// Whole function is here since we should avoid ParseXML () and it's
// memory allocations during ExitBootServices (). And it seems that
// ParseXML () does not support IDREF.
// This func is hard to read and debug and probably not reliable,
// but it seems it works.
//
UINT64
GetPlistHexValue (
  CHAR8     *Plist,
  CHAR8     *Key,
  CHAR8     *WholePlist
) {
  UINT64    NumValue = 0;
  CHAR8     *Value, *IntTag, *IDStart, *IDEnd, Buffer[48];
  UINTN     IDLen, BufferLen = ARRAY_SIZE (Buffer);

  // search for Key
  Value = AsciiStrStr (Plist, Key);
  if (Value == NULL) {
    return 0;
  }

  // search for <integer
  IntTag = AsciiStrStr (Value, "<integer");
  if (IntTag == NULL) {
    return 0;
  }

  // find <integer end
  Value = AsciiStrStr (IntTag, ">");
  if (Value == NULL) {
    return 0;
  }

  if (Value[-1] != '/') {
    // normal case: value is here
    NumValue = AsciiStrHexToUint64 (Value + 1);
    return NumValue;
  }

  // it might be a reference: IDREF="173"/>
  Value = AsciiStrStr (IntTag, "<integer IDREF=\"");
  if (Value != IntTag) {
    return 0;
  }

  // compose <integer ID="xxx" in the Buffer
  IDStart = AsciiStrStr (IntTag, "\"") + 1;
  IDEnd = AsciiStrStr (IDStart, "\"");
  IDLen = IDEnd - IDStart;

  if (IDLen > 8) {
    return 0;
  }

  AsciiStrCpyS (Buffer, BufferLen, "<integer ID=\"");
  AsciiStrnCatS (Buffer, BufferLen, IDStart, IDLen);
  AsciiStrCatS (Buffer, BufferLen, "\"");

  // and search whole plist for ID
  IntTag = AsciiStrStr (WholePlist, Buffer);
  if (IntTag == NULL) {
    return 0;
  }

  Value = AsciiStrStr (IntTag, ">");
  if (Value == NULL) {
    return 0;
  }

  if (Value[-1] == '/') {
    return 0;
  }

  // we should have value now
  NumValue = AsciiStrHexToUint64 (Value + 1);

  return NumValue;
}

VOID
PatchPrelinkedKexts (
  LOADER_ENTRY    *Entry
) {
  CHAR8     *WholePlist, *DictPtr, *InfoPlistStart = NULL,
            *InfoPlistEnd = NULL, SavedValue;
  INTN      DictLevel = 0;
  UINT32    KextAddr, KextSize;

  WholePlist = (CHAR8 *)(UINTN)KernelInfo->PrelinkInfoAddr;

  //
  // Detect FakeSMC and if present then
  // disable kext injection InjectKexts ().
  // There is some bug in the folowing code that
  // searches for individual kexts in prelink info
  // and FakeSMC is not found on my SnowLeo although
  // it is present in kernelcache.
  // But searching through the whole prelink info
  // works and that's the reason why it is here.
  //
  //CheckForFakeSMC (WholePlist, Entry);

  DictPtr = WholePlist;

  while ((DictPtr = AsciiStrStr (DictPtr, "dict>")) != NULL) {
    if (DictPtr[-1] == '<') {
      // opening dict
      DictLevel++;
      if (DictLevel == 2) {
        // kext start
        InfoPlistStart = DictPtr - 1;
      }

    } else if ((DictPtr[-2] == '<') && (DictPtr[-1] == '/')) {
      // closing dict
      if ((DictLevel == 2) && (InfoPlistStart != NULL)) {
        CHAR8   gKextBundleIdentifier[256];

        // kext end
        InfoPlistEnd = DictPtr + 5 /* "dict>" */;

        // terminate Info.plist with 0
        SavedValue = *InfoPlistEnd;
        *InfoPlistEnd = '\0';

        // get kext address from _PrelinkExecutableSourceAddr
        // truncate to 32 bit to get physical addr
        KextAddr = (UINT32)GetPlistHexValue (InfoPlistStart, kPrelinkExecutableSourceKey, WholePlist);
        // KextAddr is always relative to 0x200000
        // and if KernelSlide is != 0 then KextAddr must be adjusted
        KextAddr += KernelInfo->Slide;
        // and adjust for AptioFixDrv's KernelRelocBase
        KextAddr += (UINT32)KernelInfo->RelocBase;

        KextSize = (UINT32)GetPlistHexValue (InfoPlistStart, kPrelinkExecutableSizeKey, WholePlist);

        ExtractKextBundleIdentifier (gKextBundleIdentifier, ARRAY_SIZE(gKextBundleIdentifier), InfoPlistStart);

        // patch it
        PatchKext (
          (UINT8 *)(UINTN)KextAddr,
          KextSize,
          InfoPlistStart,
          (UINT32)(InfoPlistEnd - InfoPlistStart),
          gKextBundleIdentifier,
          Entry
        );

        // return saved char
        *InfoPlistEnd = SavedValue;
      }

      DictLevel--;
    }

    DictPtr += 5;
  }

  DBG_RT (Entry, "%a: done\n", __FUNCTION__);
  DBG_PAUSE (Entry, 1);
}

#endif

//
// Iterates over kexts loaded by booter
// and calls PatchKext () for each.
//
VOID
PatchLoadedKexts (
  LOADER_ENTRY    *Entry
) {
          CHAR8                             *PropName, SavedValue, *InfoPlist;
          DTEntry                           MMEntry;
          BooterKextFileInfo                *KextFileInfo;
          DeviceTreeBuffer                  *PropEntry;
  struct  OpaqueDTPropertyIterator          OPropIter;
          DTPropertyIterator                PropIter = &OPropIter;
          //UINTN                           DbgCount = 0;

  //DBG_RT (Entry, "\nPatchLoadedKexts ... dtRoot = %p\n", dtRoot);

  if (!dtRoot) {
    return;
  }

  DTInit (dtRoot);

  if (
    (DTLookupEntry (NULL,"/chosen/memory-map", &MMEntry) == kSuccess) &&
    (DTCreatePropertyIteratorNoAlloc (MMEntry, PropIter) == kSuccess)
  ) {
    while (DTIterateProperties (PropIter, &PropName) == kSuccess) {
      //DBG_RT (Entry, "Prop: %a\n", PropName);
      if (AsciiStrStr (PropName,"Driver-")) {
#ifdef LAZY_PARSE_KEXT_PLIST
        TagPtr    KextsDict, Dict;
#else
        CHAR8   gKextBundleIdentifier[256];
#endif

        // PropEntry DeviceTreeBuffer is the value of Driver-XXXXXX property
        PropEntry = (DeviceTreeBuffer *)(((UINT8 *)PropIter->currentProperty) + sizeof (DeviceTreeNodeProperty));
        //if (DbgCount < 3) {
        //  DBG_RT (Entry, "%a: paddr = %x, length = %x\n", PropName, PropEntry->paddr, PropEntry->length);
        //}

        // PropEntry->paddr points to BooterKextFileInfo
        KextFileInfo = (BooterKextFileInfo *)(UINTN)PropEntry->paddr;

        // Info.plist should be terminated with 0, but will also do it just in case
        InfoPlist = (CHAR8 *)(UINTN)KextFileInfo->infoDictPhysAddr;
        SavedValue = InfoPlist[KextFileInfo->infoDictLength];
        InfoPlist[KextFileInfo->infoDictLength] = '\0';

#ifdef LAZY_PARSE_KEXT_PLIST
        // TODO: Store into list first & process all later?
        if (!EFI_ERROR (ParseXML (InfoPlist, &KextsDict, 0))) {
          Dict = GetProperty (KextsDict, kPropCFBundleIdentifier);
          if ((Dict != NULL) && Dict->string) {
            PatchKext (
              (UINT8 *)(UINTN)KextFileInfo->executablePhysAddr,
              KextFileInfo->executableLength,
              InfoPlist,
              KextFileInfo->infoDictLength,
              Dict->string,
              Entry
            );
          }
        }
#else
        ExtractKextBundleIdentifier (gKextBundleIdentifier, ARRAY_SIZE(gKextBundleIdentifier), InfoPlist);

        PatchKext (
          (UINT8 *)(UINTN)KextFileInfo->executablePhysAddr,
          KextFileInfo->executableLength,
          InfoPlist,
          KextFileInfo->infoDictLength,
          gKextBundleIdentifier,
          Entry
        );
#endif

        // Check for FakeSMC here
        //CheckForFakeSMC (InfoPlist, Entry);

        InfoPlist[KextFileInfo->infoDictLength] = SavedValue;
        //DbgCount++;
      }

      //if (AsciiStrStr (PropName,"DriversPackage-") != 0)
      //{
      //    DBG_RT (Entry, "Found %a\n", PropName);
      //    break;
      //}
    }
  }
}

//
// Entry for all kext patches.
// Will iterate through kext in prelinked kernel (kernelcache)
// or DevTree (drivers boot) and do patches.
//
VOID
KextPatcherStart (
  LOADER_ENTRY    *Entry
) {
  DBG_RT (Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (KernelInfo->isCache) {
    DBG_RT (Entry, "Patching kernelcache ...\n");
    //DBG_PAUSE (Entry, 1);

    PatchPrelinkedKexts (Entry);
  } else {
    DBG_RT (Entry, "Patching loaded kexts ...\n");
    //DBG_PAUSE (Entry, 1);

    PatchLoadedKexts (Entry);
  }

  DBG_RT (Entry, "%a: End\n", __FUNCTION__);
}
