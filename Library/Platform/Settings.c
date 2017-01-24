/*
 Slice 2012
 */

#include <Library/Platform/Platform.h>
#include <Library/Platform/KernelPatcher.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_SETTING
#define DEBUG_SETTING -1
#endif
#else
#ifdef DEBUG_SETTING
#undef DEBUG_SETTING
#endif
#define DEBUG_SETTING DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_SETTING, __VA_ARGS__)

S_FILES                           *aConfigs = NULL, *aThemes = NULL;
ACPI_PATCHED_AML                  *ACPIPatchedAML = NULL;
UINTN                             ACPIDropTablesNum = 0, ACPIPatchedAMLNum = 0;

SETTINGS_DATA                     gSettings;
LANGUAGES                         gLanguage = english;
GFX_PROPERTIES                    gGraphics[4]; //no more then 4 graphics cards
SLOT_DEVICE                       SlotDevices[16]; //assume DEV_XXX, Arpt=6

EFI_EDID_DISCOVERED_PROTOCOL      *EdidDiscovered;
UINT8                             *gEDID = NULL;

UINTN                             NGFX = 0; // number of GFX

UINTN                             nLanCards;        // number of LAN cards
UINT16                            gLanVendor[4];    // their vendors
UINT8                             *gLanMmio[4];     // their MMIO regions
UINT8                             gLanMac[4][6];    // their MAC addresses
UINTN                             nLanPaths;        // number of LAN pathes
BOOLEAN                           GetLegacyLanAddress;

UINTN                             gEvent;

extern MEM_STRUCTURE              gRAM;
extern BOOLEAN                    NeedPMfix;

GUI_ANIME                         *GuiAnime = NULL;

// global configuration with default values
REFIT_CONFIG   DefaultConfig = {
  FALSE,              // BOOLEAN      TextOnly;
  -1,                 // INTN         Timeout;
  0,                  // UINTN        DisableFlags;
  0,                  // UINTN        HideBadges;
  0,                  // UINTN        HideUIFlags;
  TRUE,               // BOOLEAN      Quiet;
  FALSE,              // BOOLEAN      DebugLog;
  FALSE,              // BOOLEAN      FastBoot;
  FALSE,              // BOOLEAN      NeverHibernate;
  FONT_GRAY,          // FONT_TYPE    Font; //Welcome should be white
  9,                  // INTN         CharWidth;
  //13,               //INTN          CharHeight;
  6,                  //INTN          CharRows;
  16,                 //INTN          CharCols;
  0xA0A0A055,         // UINTN        SelectionColor;
  0xFFFFFFFF,         // UINTN        SelectionTextColor;
  0x000000FF,         // UINTN        TextColor;
  FALSE,              // BOOLEAN      FontEmbedded;
  NULL,               // CHAR16       *FontFileName;
  NULL,               // CHAR16       *Theme;
  NULL,               // CHAR16       *BannerFileName;
  NULL,               // CHAR16       *SelectionSmallFileName;
  NULL,               // CHAR16       *SelectionBigFileName;
  NULL,               // CHAR16       *SelectionIndicatorName;
  NULL,               // CHAR16       *DefaultSelection;
  NULL,               // CHAR16       *ScreenResolution;
  NULL,               // CHAR16       *BackgroundName;
  None,               // SCALING      BackgroundScale;
  0,                  // INT32        BackgroundSharp;
  FALSE,              // BOOLEAN      BackgroundDark;
  //FALSE,            // BOOLEAN      CustomIcons;
  FALSE,              // BOOLEAN      SelectionOnTop;
  FALSE,              // BOOLEAN      BootCampStyle;
  0xFFFF,             // INTN         BadgeOffsetX;
  0xFFFF,             // INTN         BadgeOffsetY;
  8,                  // INTN         BadgeScale;
  0xFFFF,             // INTN         ThemeDesignWidth;
  0xFFFF,             // INTN         ThemeDesignHeight;
  0xFFFF,             // UINT32       BannerPosX;
  0xFFFF,             // UINT32       BannerPosY;
  0,                  // INTN         BannerEdgeHorizontal;
  0,                  // INTN         BannerEdgeVertical;
  0,                  // INTN         BannerNudgeX;
  0,                  // INTN         BannerNudgeY;
  FALSE,              // BOOLEAN      NonSelectedGrey;
  128,                // INTN         MainEntriesSize;
  8,                  // INTN         TileXSpace;
  24,                 // INTN         TileYSpace;
  //FALSE,            // BOOLEAN      Proportional;
  FALSE,              // BOOLEAN      NoEarlyProgress;
  0,                  // INTN         PruneScrollRows;
  ICON_FORMAT_DEF,    // INTN         IconFormat;
  144,                // INTN         row0TileSize
  64,                 // INTN         row1TileSize
  64,                 // INTN         LayoutBannerOffset
  0,                  // INTN         LayoutButtonOffset
  0,                  // INTN         LayoutTextOffset
  0,                  // INTN         LayoutAnimMoveForMenuX
  16,                 // INTN         ScrollWidth
  20,                 // INTN         ScrollButtonsHeight
  5,                  // INTN         ScrollBarDecorationsHeight
  7,                  // INTN         ScrollScrollDecorationsHeight
};

REFIT_CONFIG   GlobalConfig;

DEVICES_BIT_K   ADEVICES[] = {
  { "ATI",        DEV_ATI },
  { "NVidia",     DEV_NVIDIA },
  { "IntelGFX",   DEV_INTEL },
  { "HDA",        DEV_HDA },
  { "HDMI",       DEV_HDMI },
  { "SATA",       DEV_SATA },
  { "LAN",        DEV_LAN },
  { "WIFI",       DEV_WIFI },
  { "USB",        DEV_USB },
  { "LPC",        DEV_LPC },
  { "SmBUS",      DEV_SMBUS },
  { "Firewire",   DEV_FIREWIRE },
  { "IDE",        DEV_IDE },
};

INTN    OptDevicesBitNum = ARRAY_SIZE (ADEVICES);

OPT_MENU_BIT_K OPT_MENU_DSDTBIT[] = {
  { "Add MCHC",       "AddMCHC_0008",         FIX_MCHC },
  { "Add PNLF",       "AddPNLF_1000000",      FIX_PNLF },
  { "Add HDMI",       "AddHDMI_8000000",      FIX_HDMI },
  { "Fix Display",    "FixDisplay_0100",      FIX_DISPLAY },
  { "Fix LAN",        "FixLAN_2000",          FIX_LAN },
  { "Fix Airport",    "FixAirport_4000",      FIX_WIFI },
  { "Fix Sound",      "FixHDA_8000",          FIX_HDA },
  { "Fix IMEI",       "AddIMEI_80000",        FIX_IMEI },
  { "Fix IntelGFX",   "FIX_INTELGFX_100000",  FIX_INTELGFX },
};

INTN    OptMenuDSDTBitNum = ARRAY_SIZE (OPT_MENU_DSDTBIT);

CHAR16    *SystemPlists[] = {
  L"\\System\\Library\\CoreServices\\SystemVersion.plist", // OS X Regular
  L"\\System\\Library\\CoreServices\\ServerVersion.plist", // OS X Server
  NULL
};

VOID
GetDefaultConfig () {
  CopyMem (&GlobalConfig, &DefaultConfig, sizeof (REFIT_CONFIG));
}

UINT32
GetCrc32 (
  UINT8   *Buffer,
  UINTN   Size
) {
  UINTN     i, Len;
  UINT32    x, *Fake;

  Fake = (UINT32 *)Buffer;

  if (Fake == NULL) {
    DBG ("Buffer=NULL\n");
    return 0;
  }

  x = 0;
  Len = Size >> 2;

  for (i = 0; i < Len; i++) {
    x += Fake[i];
  }

  return x;
}

BOOLEAN
IsPropertyTrue (
  TagPtr  Prop
) {
  return (
    (Prop != NULL) &&
    (
      (Prop->type == kTagTypeTrue) ||
      (
        (Prop->type == kTagTypeString) && Prop->string &&
        ((Prop->string[0] == 'y') || (Prop->string[0] == 'Y'))
      )
    )
  );
}

INTN
GetPropertyInteger (
  TagPtr  Prop,
  INTN    Default
) {
  if (Prop == NULL) {
    return Default;
  }

  if (Prop->type == kTagTypeInteger) {
    return Prop->integer; //(INTN)Prop->string;
  } else if ((Prop->type == kTagTypeString) && Prop->string) {
    if ((Prop->string[1] == 'x') || (Prop->string[1] == 'X')) {
      return (INTN)AsciiStrHexToUintn (Prop->string);
    }

    if (Prop->string[0] == '-') {
      return -(INTN)AsciiStrDecimalToUintn (Prop->string + 1);
    }

    return (INTN)AsciiStrDecimalToUintn (Prop->string);
  }

  return Default;
}

//
// returns binary setting in a new allocated buffer and data length in dataLen.
// data can be specified in <data></data> base64 encoded
// or in <string></string> hex encoded
//
VOID *
StringDataToHex (
  IN   CHAR8    *Val,
  OUT  UINTN    *DataLen
) {
  UINT8     *Data = NULL;
  UINT32    Len;

  Len = (UINT32)AsciiStrLen (Val) >> 1; // number of hex digits
  Data = AllocateZeroPool (Len); // 2 chars per byte, one more byte for odd number
  Len  = Hex2Bin (Val, Data, Len);
  *DataLen = Len;

  return Data;
}

VOID *
GetDataSetting (
  IN   TagPtr   Dict,
  IN   CHAR8    *PropName,
  OUT  UINTN    *DataLen
) {
  TagPtr    Prop;
  UINT8     *Data = NULL;
  UINTN     Len;

  Prop = GetProperty (Dict, PropName);
  if (Prop != NULL) {
    //if (Prop->data != NULL/* && Prop->size > 0 */) { //rehabman: allow zero length data
    if (Prop->data && Prop->size) { //rehabman: allow zero length data
      // data property
      Data = AllocateZeroPool (Prop->size);
      CopyMem (Data, Prop->data, Prop->size);

      if (DataLen != NULL) {
        *DataLen = Prop->size;
      }
      /*
        DBG ("Data: %p, Len: %d = ", Data, Prop->size);
        for (i = 0; i < Prop->size; i++) {
          DBG ("%02x ", Data[i]);
        }
        DBG ("\n");
      */
    } else {
      // assume data in hex encoded string property
      Data = StringDataToHex (Prop->string, &Len);
      *DataLen = Len;

      /*
        DBG ("Data (str): %p, Len: %d = ", data, len);
        for (i = 0; i < Len; i++) {
          DBG ("%02x ", data[i]);
        }
        DBG ("\n");
      */
    }
  }

  return Data;
}

MatchOSes *
GetStrArraySeparatedByChar (
  CHAR8   *str,
  CHAR8   sep
) {
  struct    MatchOSes   *mo;
  INTN      len = 0, i = 0, inc = 1, newLen = 0;
  CHAR8     doubleSep[2];

  mo = AllocatePool (sizeof (struct MatchOSes));
  if (!mo) {
    return NULL;
  }

  mo->count = CountOccurrences ( str, sep ) + 1;

  len = (INTN)AsciiStrLen (str);
  doubleSep[0] = sep; doubleSep[1] = sep;

  if (
    !len ||
    (AsciiStrStr (str, doubleSep) != NULL) ||
    (str[0] == sep) ||
    (str[len -1] == sep)
  ) {
    mo->count = 0;
    mo->array[0] = NULL;
    return mo;
  }

  if (mo->count > 1) {
    INTN    *indexes = (INTN *)AllocatePool (mo->count + 1);

    for (i = 0; i < len; ++i) {
      CHAR8 c = str[i];

      if (c == sep) {
        indexes[inc]=i;
        inc++;
      }
    }

    // manually add first index
    indexes[0] = 0;
    // manually add last index
    indexes[mo->count] = len;

    for (i = 0; i < mo->count; ++i) {
      INTN    startLocation, endLocation;

      mo->array[i] = 0;

      if (i == 0) {
        startLocation = indexes[0];
        endLocation = indexes[1] - 1;
      } else if (i == mo->count - 1) { // never reach the end of the array
        startLocation = indexes[i] + 1;
        endLocation = len;
      } else {
        startLocation = indexes[i] + 1;
        endLocation = indexes[i + 1] - 1;
      }

      //DBG ("start %d, end %d\n", startLocation, endLocation);

      newLen = (endLocation - startLocation) + 2;

      mo->array[i] = AllocateCopyPool (newLen, str + startLocation);
      mo->array[i][newLen - 1] = '\0';
    }

    FreePool (indexes);
  } else {
    //DBG ("str contains only one component and it is our string %s!\n", str);
    mo->array[0] = AllocateCopyPool (AsciiStrLen (str)+1, str);
  }

  return mo;
}

VOID
DeallocMatchOSes (
  struct MatchOSes    *s
) {
  INTN    i;

  if (!s) {
    return;
  }

  for (i = 0; i < s->count; i++) {
    if (s->array[i]) {
      FreePool (s->array[i]);
    }
  }

  FreePool (s);
}

BOOLEAN
IsOSValid (
  CHAR8   *MatchOS,
  CHAR8   *CurrOS
) {
  /* example for valid matches are:
    10.7, only 10.7 (10.7.1 will be skipped)
    10.10.2 only 10.10.2 (10.10.1 or 10.10.5 will be skipped)
    10.10.x (or 10.10.X), in this case is valid for all minor version of 10.10 (10.10.(0-9))
  */

  BOOLEAN     ret = FALSE, Match, MatchX;
  struct      MatchOSes    *osToc, *currOStoc;

  if (!MatchOS || !CurrOS) {
    return TRUE; //undefined matched corresponds to old behavior
  }

  osToc = GetStrArraySeparatedByChar (MatchOS, '.');
  currOStoc = GetStrArraySeparatedByChar (CurrOS,  '.');

  Match = (
      (AsciiStrCmp (osToc->array[0], currOStoc->array[0]) == 0) &&
      (AsciiStrCmp (osToc->array[1], currOStoc->array[1]) == 0)
    );

  if (osToc->count == 2) {
    if (Match) {
      ret = TRUE;
    }
  } else if (osToc->count == 3) {
    MatchX = (
        Match &&
        ((AsciiStrCmp (osToc->array[2], "x") == 0) || (AsciiStrCmp (osToc->array[2], "X") == 0))
      );

    if (currOStoc->count == 3) {
      if (
        Match &&
        (AsciiStrCmp (osToc->array[2], currOStoc->array[2]) == 0)
      ) {
        ret = TRUE;
      } else if (MatchX) {
        ret = TRUE;
      }
    } else if (currOStoc->count == 2) {
      if (Match) {
        ret = TRUE;
      } else if (MatchX) {
        ret = TRUE;
      }
    }
  }

  DeallocMatchOSes (osToc);
  DeallocMatchOSes (currOStoc);

  return ret;
}

BOOLEAN
IsPatchEnabled (
  CHAR8   *MatchOSEntry,
  CHAR8   *CurrOS
) {
  INTN      i;
  BOOLEAN   ret = FALSE;
  struct    MatchOSes  *mos;

  if (!MatchOSEntry || !CurrOS) {
    return TRUE; //undefined matched corresponds to old behavior
  }

  mos = GetStrArraySeparatedByChar (MatchOSEntry, ',');
  if (!mos) {
    return TRUE; //memory fails -> anyway the patch enabled
  }

  for (i = 0; i < mos->count; ++i) {
    // dot represent MatchOS
    if (
      ((AsciiStrStr (mos->array[i], ".") != NULL) && IsOSValid (mos->array[i], CurrOS)) || // MatchOS
      (AsciiStrCmp (mos->array[i], CurrOS) == 0) // MatchBuild
      //(AsciiStrStr (mos->array[i], CurrOS) != NULL) // MatchBuild : saverals MatchBuild by commas
    ) {
      //DBG ("\nthis patch will activated for OS %s!\n", mos->array[i]);
      ret =  TRUE;
      break;
    }
  }

  DeallocMatchOSes (mos);

  return ret;
}
// End of MatchOS

UINT8
CheckVolumeType (
  UINT8     VolumeType,
  TagPtr    Prop
) {
  UINT8   VolumeTypeTmp = VolumeType;

  if (AsciiStriCmp (Prop->string, "Internal") == 0) {
    VolumeTypeTmp |= VOLTYPE_INTERNAL;
  } else if (AsciiStriCmp (Prop->string, "External") == 0) {
    VolumeTypeTmp |= VOLTYPE_EXTERNAL;
  } else if (AsciiStriCmp (Prop->string, "Optical") == 0) {
    VolumeTypeTmp |= VOLTYPE_OPTICAL;
  } else if (AsciiStriCmp (Prop->string, "FireWire") == 0) {
    VolumeTypeTmp |= VOLTYPE_FIREWIRE;
  }
  return VolumeTypeTmp;
}

UINT8
GetVolumeType (
  TagPtr    DictPointer
) {
  TagPtr    Prop, Prop2;
  UINT8     VolumeType = 0;

  Prop = GetProperty (DictPointer, "VolumeType");
  if (Prop != NULL) {
    if (Prop->type == kTagTypeString) {
      VolumeType = CheckVolumeType (0, Prop);
    } else if (Prop->type == kTagTypeArray) {
      INTN   i, Count = GetTagCount (Prop);

      if (Count > 0) {
        Prop2 = NULL;

        for (i = 0; i < Count; i++) {
          if (EFI_ERROR (GetElement (Prop, i, &Prop2))) {
            continue;
          }

          if (Prop2 == NULL) {
            break;
          }

          if ((Prop2->type != kTagTypeString) || (Prop2->string == NULL)) {
            continue;
          }

          VolumeType = CheckVolumeType (VolumeType, Prop2);
        }
      }
    }
  }

  return VolumeType;
}

STATIC
BOOLEAN
AddCustomEntry (
  IN CUSTOM_LOADER_ENTRY    *Entry
) {
  if (Entry == NULL) {
    return FALSE;
  }

  if (gSettings.CustomEntries) {
    CUSTOM_LOADER_ENTRY *Entries = gSettings.CustomEntries;

    while (Entries->Next != NULL) {
      Entries = Entries->Next;
    }

    Entries->Next = Entry;
  } else {
    gSettings.CustomEntries = Entry;
  }

  return TRUE;
}

STATIC
BOOLEAN
AddCustomToolEntry (
  IN CUSTOM_TOOL_ENTRY    *Entry
) {
  if (Entry == NULL) {
    return FALSE;
  }

  if (gSettings.CustomTool) {
    CUSTOM_TOOL_ENTRY   *Entries = gSettings.CustomTool;

    while (Entries->Next != NULL) {
      Entries = Entries->Next;
    }

    Entries->Next = Entry;
  } else {
    gSettings.CustomTool = Entry;
  }

  return TRUE;
}

STATIC
BOOLEAN
AddCustomSubEntry (
  IN OUT CUSTOM_LOADER_ENTRY   *Entry,
  IN     CUSTOM_LOADER_ENTRY   *SubEntry
) {
  if ((Entry == NULL) || (SubEntry == NULL)) {
    return FALSE;
  }

  if (Entry->SubEntries != NULL) {
    CUSTOM_LOADER_ENTRY   *Entries = Entry->SubEntries;

    while (Entries->Next != NULL) {
      Entries = Entries->Next;
    }

    Entries->Next = SubEntry;
  } else {
    Entry->SubEntries = SubEntry;
  }

  return TRUE;
}

BOOLEAN
CopyKernelAndKextPatches (
  IN OUT KERNEL_AND_KEXT_PATCHES   *Dst,
  IN     KERNEL_AND_KEXT_PATCHES   *Src
) {
  INTN  i;

  if ((Dst == NULL) || (Src == NULL)) {
    return FALSE;
  }

  Dst->KPDebug       = Src->KPDebug;
  //Dst->KPKernelCpu   = Src->KPKernelCpu;
  //Dst->KPLapicPanic  = Src->KPLapicPanic;
  Dst->KPAsusAICPUPM = Src->KPAsusAICPUPM;
  //Dst->KPAppleRTC    = Src->KPAppleRTC;
  Dst->KPKernelPm    = Src->KPKernelPm;
  Dst->FakeCPUID     = Src->FakeCPUID;

  if (gSettings.HasGraphics->Ati) {
    if (Src->KPATIConnectorsController != NULL) {
      Dst->KPATIConnectorsController = EfiStrDuplicate (Src->KPATIConnectorsController);
    }

    if (
      (Src->KPATIConnectorsDataLen > 0) &&
      (Src->KPATIConnectorsData != NULL) &&
      (Src->KPATIConnectorsPatch != NULL)
    ) {
      Dst->KPATIConnectorsDataLen = Src->KPATIConnectorsDataLen;
      Dst->KPATIConnectorsData    = AllocateCopyPool (Src->KPATIConnectorsDataLen, Src->KPATIConnectorsData);
      Dst->KPATIConnectorsPatch   = AllocateCopyPool (Src->KPATIConnectorsDataLen, Src->KPATIConnectorsPatch);
    }
  }

  if ((Src->NrForceKexts > 0) && (Src->ForceKexts != NULL)) {
    Dst->ForceKexts = AllocatePool (Src->NrForceKexts * sizeof (CHAR16 *));

    for (i = 0; i < Src->NrForceKexts; i++) {
      Dst->ForceKexts[Dst->NrForceKexts++] = EfiStrDuplicate (Src->ForceKexts[i]);
    }
  }

  if ((Src->NrKexts > 0) && (Src->KextPatches != NULL)) {
    Dst->KextPatches = AllocatePool (Src->NrKexts * sizeof (KEXT_PATCH));

    for (i = 0; i < Src->NrKexts; i++) {
      if (
        (Src->KextPatches[i].DataLen <= 0) ||
        (Src->KextPatches[i].Data == NULL) ||
        (Src->KextPatches[i].Patch == NULL)
      ) {
        continue;
      }

      if (Src->KextPatches[i].Name) {
        Dst->KextPatches[Dst->NrKexts].Name         = (CHAR8 *)AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].Name), Src->KextPatches[i].Name);
      }

      if (Src->KextPatches[i].Label) {
        Dst->KextPatches[Dst->NrKexts].Label        = (CHAR8 *)AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].Label), Src->KextPatches[i].Label);
      }

      if (Src->KextPatches[i].Filename) {
        Dst->KextPatches[Dst->NrKexts].Filename     = (CHAR8 *)AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].Filename), Src->KextPatches[i].Filename);
      }

      Dst->KextPatches[Dst->NrKexts].Patched        = FALSE;
      Dst->KextPatches[Dst->NrKexts].Disabled       = Src->KextPatches[i].Disabled;
      Dst->KextPatches[Dst->NrKexts].IsPlistPatch   = Src->KextPatches[i].IsPlistPatch;
      Dst->KextPatches[Dst->NrKexts].DataLen        = Src->KextPatches[i].DataLen;
      Dst->KextPatches[Dst->NrKexts].Data           = AllocateCopyPool (Src->KextPatches[i].DataLen, Src->KextPatches[i].Data);
      Dst->KextPatches[Dst->NrKexts].Patch          = AllocateCopyPool (Src->KextPatches[i].DataLen, Src->KextPatches[i].Patch);
      Dst->KextPatches[Dst->NrKexts].MatchOS        = AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].MatchOS), Src->KextPatches[i].MatchOS);
      Dst->KextPatches[Dst->NrKexts].MatchBuild     = AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].MatchBuild), Src->KextPatches[i].MatchBuild);
      ++(Dst->NrKexts);
    }
  }

  if ((Src->NrKernels > 0) && (Src->KernelPatches != NULL)) {
    Dst->KernelPatches = AllocatePool (Src->NrKernels * sizeof (KERNEL_PATCH));

    for (i = 0; i < Src->NrKernels; i++) {
      if (
        (Src->KernelPatches[i].DataLen <= 0) ||
        (Src->KernelPatches[i].Data == NULL) ||
        (Src->KernelPatches[i].Patch == NULL)
      ) {
        continue;
      }

      if (Src->KernelPatches[i].Label) {
        Dst->KernelPatches[Dst->NrKernels].Label    = (CHAR8 *)AllocateCopyPool (AsciiStrSize (Src->KernelPatches[i].Label), Src->KernelPatches[i].Label);
      }

      Dst->KernelPatches[Dst->NrKernels].Disabled   = Src->KernelPatches[i].Disabled;
      Dst->KernelPatches[Dst->NrKernels].DataLen    = Src->KernelPatches[i].DataLen;
      Dst->KernelPatches[Dst->NrKernels].Data       = AllocateCopyPool (Src->KernelPatches[i].DataLen, Src->KernelPatches[i].Data);
      Dst->KernelPatches[Dst->NrKernels].Patch      = AllocateCopyPool (Src->KernelPatches[i].DataLen, Src->KernelPatches[i].Patch);
      Dst->KernelPatches[Dst->NrKernels].Count      = Src->KernelPatches[i].Count;
      Dst->KernelPatches[Dst->NrKernels].MatchOS    = AllocateCopyPool (AsciiStrSize (Src->KernelPatches[i].MatchOS), Src->KernelPatches[i].MatchOS);
      Dst->KernelPatches[Dst->NrKernels].MatchBuild = AllocateCopyPool (AsciiStrSize (Src->KernelPatches[i].MatchBuild), Src->KernelPatches[i].MatchBuild);
      ++(Dst->NrKernels);
    }
  }

  return TRUE;
}

STATIC
CUSTOM_LOADER_ENTRY *
DuplicateCustomEntry (
  IN CUSTOM_LOADER_ENTRY  *Entry
) {
  CUSTOM_LOADER_ENTRY   *DuplicateEntry;

  if (Entry == NULL) {
    return NULL;
  }

  DuplicateEntry = (CUSTOM_LOADER_ENTRY *)AllocateZeroPool (sizeof (CUSTOM_LOADER_ENTRY));
  if (DuplicateEntry != NULL) {
    if (Entry->Volume != NULL) {
      DuplicateEntry->Volume         = EfiStrDuplicate (Entry->Volume);
    }

    if (Entry->Path != NULL) {
      DuplicateEntry->Path           = EfiStrDuplicate (Entry->Path);
    }

    if (Entry->Options != NULL) {
      DuplicateEntry->Options        = EfiStrDuplicate (Entry->Options);
    }

    if (Entry->FullTitle != NULL) {
      DuplicateEntry->FullTitle      = EfiStrDuplicate (Entry->FullTitle);
    }

    if (Entry->Title != NULL) {
      DuplicateEntry->Title          = EfiStrDuplicate (Entry->Title);
    }

    if (Entry->ImagePath != NULL) {
      DuplicateEntry->ImagePath      = EfiStrDuplicate (Entry->ImagePath);
    }

    if (Entry->DriveImagePath) {
      DuplicateEntry->DriveImagePath = EfiStrDuplicate (Entry->DriveImagePath);
    }

    DuplicateEntry->Image         = Entry->Image;
    DuplicateEntry->DriveImage    = Entry->DriveImage;
    DuplicateEntry->Hotkey        = Entry->Hotkey;
    DuplicateEntry->Flags         = Entry->Flags;
    DuplicateEntry->Type          = Entry->Type;
    DuplicateEntry->VolumeType    = Entry->VolumeType;
    DuplicateEntry->KernelScan    = Entry->KernelScan;
    //DuplicateEntry->CustomBoot  = Entry->CustomBoot;
    //DuplicateEntry->CustomLogo  = Entry->CustomLogo;

    CopyKernelAndKextPatches (
      (KERNEL_AND_KEXT_PATCHES *)(((UINTN)DuplicateEntry) + OFFSET_OF (CUSTOM_LOADER_ENTRY, KernelAndKextPatches)),
      (KERNEL_AND_KEXT_PATCHES *)(((UINTN)Entry) + OFFSET_OF (CUSTOM_LOADER_ENTRY, KernelAndKextPatches))
    );
  }

  return DuplicateEntry;
}

STATIC
BOOLEAN
FillinKextPatches (
  IN OUT KERNEL_AND_KEXT_PATCHES    *Patches,
  TagPtr                            DictPointer
) {
  TagPtr  Prop;

  if ((Patches == NULL) || (DictPointer == NULL)) {
    return FALSE;
  }

  if (NeedPMfix) {
    Patches->KPKernelPm = TRUE;
    Patches->KPAsusAICPUPM = TRUE;
  }

  gSettings.DebugKP = Patches->KPDebug = IsPropertyTrue (GetProperty (DictPointer, "Debug"));

  gSettings.FlagsBits = Patches->KPDebug
    ? OSFLAG_SET (gSettings.FlagsBits, OSFLAG_DBGPATCHES)
    : OSFLAG_UNSET (gSettings.FlagsBits, OSFLAG_DBGPATCHES);

  //Patches->KPKernelCpu = IsPropertyTrue (GetProperty (DictPointer, "KernelCpu"));

  Prop = GetProperty (DictPointer, "FakeCPUID");
  if (Prop != NULL) {
    Patches->FakeCPUID = (UINT32)GetPropertyInteger (Prop, 0);
    DBG ("FakeCPUID: %x\n", Patches->FakeCPUID);
  }

  Patches->KPAsusAICPUPM = IsPropertyTrue (GetProperty (DictPointer, "AsusAICPUPM"));

  Patches->KPKernelPm = IsPropertyTrue (GetProperty (DictPointer, "KernelPm"));

  //Patches->KPLapicPanic = IsPropertyTrue (GetProperty (DictPointer, "KernelLapic"));

  //Patches->KPHaswellE = IsPropertyTrue (GetProperty (DictPointer, "KernelHaswellE"));
  if (gSettings.HasGraphics->Ati) {
    Prop = GetProperty (DictPointer, "ATIConnectorsController");
    if (Prop != NULL) {
      UINTN   len = 0, i=0;

      // ATIConnectors patch
      len = (AsciiStrnLenS (Prop->string, 255) * sizeof (CHAR16) + 2);
      Patches->KPATIConnectorsController = AllocateZeroPool (len);
      AsciiStrToUnicodeStrS (Prop->string, Patches->KPATIConnectorsController, len);

      Patches->KPATIConnectorsData = GetDataSetting (DictPointer, "ATIConnectorsData", &len);
      Patches->KPATIConnectorsDataLen = len;
      Patches->KPATIConnectorsPatch = GetDataSetting (DictPointer, "ATIConnectorsPatch", &i);

      if (
        (Patches->KPATIConnectorsData == NULL) ||
        (Patches->KPATIConnectorsPatch == NULL) ||
        (Patches->KPATIConnectorsDataLen == 0) ||
        (Patches->KPATIConnectorsDataLen != i)
      ) {
        // invalid params - no patching
        DBG ("ATIConnectors patch: invalid parameters!\n");
        goto FreeAtiPatches;
      } else {
        goto PatchesNext;
      }
    }
  }

  FreeAtiPatches:

  if (Patches->KPATIConnectorsController != NULL) {
    FreePool (Patches->KPATIConnectorsController);
  }

  if (Patches->KPATIConnectorsData != NULL) {
    FreePool (Patches->KPATIConnectorsData);
  }

  if (Patches->KPATIConnectorsPatch != NULL) {
    FreePool (Patches->KPATIConnectorsPatch);
  }

  Patches->KPATIConnectorsController = NULL;
  Patches->KPATIConnectorsData       = NULL;
  Patches->KPATIConnectorsPatch      = NULL;
  Patches->KPATIConnectorsDataLen    = 0;

  PatchesNext:

  Prop = GetProperty (DictPointer, "ForceKextsToLoad");
  if (Prop != NULL) {
    INTN   i, Count = GetTagCount (Prop);

    if (Count > 0) {
      TagPtr    Prop2 = NULL;
      CHAR16    **newForceKexts = AllocateZeroPool ((Patches->NrForceKexts + Count) * sizeof (CHAR16 *));

      if (Patches->ForceKexts != NULL) {
        CopyMem (newForceKexts, Patches->ForceKexts, (Patches->NrForceKexts * sizeof (CHAR16 *)));
        FreePool (Patches->ForceKexts);
      }

      Patches->ForceKexts = newForceKexts;
      MsgLog ("ForceKextsToLoad: %d requested\n", Count);

      for (i = 0; i < Count; i++) {
        EFI_STATUS Status = GetElement (Prop, i, &Prop2);

        DBG (" - [%02d]:", i);

        if (EFI_ERROR (Status) || (Prop2 == NULL) ) {
          DBG (" %r parsing / empty element\n", Status);
          continue;
        }

        if (Prop2->string != NULL) {
          if (*(Prop2->string) == '\\') {
            ++Prop2->string;
          }

          if (AsciiStrnLenS (Prop2->string, 255) > 0) {
            UINTN   Len = (AsciiStrnLenS (Prop2->string, 255) * sizeof (CHAR16) + 2);

            Patches->ForceKexts[Patches->NrForceKexts] = AllocateZeroPool (Len);
            AsciiStrToUnicodeStrS (Prop2->string, Patches->ForceKexts[Patches->NrForceKexts], Len);
            DBG (" %s\n", Patches->NrForceKexts, Patches->ForceKexts[Patches->NrForceKexts]);
            ++Patches->NrForceKexts;
          } else {
            DBG (" skip\n");
          }
        }
      }
    }
  }

  Prop = GetProperty (DictPointer, "KextsToPatch");
  if (Prop != NULL) {
    INTN   i, Count = GetTagCount (Prop);
    //delete old and create new
    if (Patches->KextPatches) {
      Patches->NrKexts = 0;
      FreePool (Patches->KextPatches);
    }

    if (Count > 0) {
      TagPtr        Prop2 = NULL, Dict = NULL;
      KEXT_PATCH    *newPatches = AllocateZeroPool (Count * sizeof (KEXT_PATCH));

      Patches->KextPatches = newPatches;
      MsgLog ("KextsToPatch: %d requested\n", Count);

      for (i = 0; i < Count; i++) {
        CHAR8     *KextPatchesName, *KextPatchesLabel;
        UINTN     FindLen = 0, ReplaceLen = 0;
        UINT8     *TmpData, *TmpPatch;

        EFI_STATUS Status = GetElement (Prop, i, &Prop2);

        DBG (" - [%02d]:", i);

        if (EFI_ERROR (Status) || (Prop2 == NULL)) {
          DBG (" %r parsing / empty element\n", Status);
          continue;
        }

        Dict = GetProperty (Prop2, "Name");
        if (Dict == NULL) {
          DBG (" patch without Name, skip\n");
          continue;
        }

        KextPatchesName = AllocateCopyPool (255, Dict->string);
        KextPatchesLabel = AllocateCopyPool (255, KextPatchesName);

        Dict = GetProperty (Prop2, "Comment");
        if (Dict != NULL) {
          AsciiStrCatS (KextPatchesLabel, 255, " (");
          AsciiStrCatS (KextPatchesLabel, 255, Dict->string);
          AsciiStrCatS (KextPatchesLabel, 255, ")");

        } else {
          AsciiStrCatS (KextPatchesLabel, 255, " (NoLabel)");
        }

        DBG (" %a", KextPatchesLabel);

        Dict = GetProperty (Prop2, "Disabled");
        if (IsPropertyTrue (Dict)) {
          DBG (" | patch disabled, skip\n");
          continue;
        }

        TmpData    = GetDataSetting (Prop2, "Find", &FindLen);
        TmpPatch   = GetDataSetting (Prop2, "Replace", &ReplaceLen);

        if (!FindLen || !ReplaceLen || (FindLen != ReplaceLen)) {
          DBG (" - invalid Find/Replace data, skip\n");
          continue;
        }

        Patches->KextPatches[Patches->NrKexts].Data       = AllocateCopyPool (FindLen, TmpData);
        Patches->KextPatches[Patches->NrKexts].DataLen    = FindLen;
        Patches->KextPatches[Patches->NrKexts].Patch      = AllocateCopyPool (FindLen, TmpPatch);
        Patches->KextPatches[Patches->NrKexts].MatchOS    = NULL;
        Patches->KextPatches[Patches->NrKexts].MatchBuild = NULL;
        Patches->KextPatches[Patches->NrKexts].Filename   = NULL;
        Patches->KextPatches[Patches->NrKexts].Disabled   = FALSE;
        Patches->KextPatches[Patches->NrKexts].Patched    = FALSE;
        Patches->KextPatches[Patches->NrKexts].Name       = AllocateCopyPool (AsciiStrnLenS (KextPatchesName, 255) + 1, KextPatchesName);
        Patches->KextPatches[Patches->NrKexts].Label      = AllocateCopyPool (AsciiStrnLenS (KextPatchesLabel, 255) + 1, KextPatchesLabel);

        FreePool (TmpData);
        FreePool (TmpPatch);
        FreePool (KextPatchesName);
        FreePool (KextPatchesLabel);

        // check enable/disabled patch (OS based) by Micky1979
        Dict = GetProperty (Prop2, "MatchOS");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          Patches->KextPatches[Patches->NrKexts].MatchOS = AllocateCopyPool (AsciiStrnLenS (Dict->string, 255) + 1, Dict->string);
          DBG (" | MatchOS: %a", Patches->KextPatches[Patches->NrKexts].MatchOS);
        }

        Dict = GetProperty (Prop2, "MatchBuild");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          Patches->KextPatches[Patches->NrKexts].MatchBuild = AllocateCopyPool (AsciiStrnLenS (Dict->string, 255) + 1, Dict->string);
          DBG (" | MatchBuild: %a", Patches->KextPatches[Patches->NrKexts].MatchBuild);
        }

        // check if this is Info.plist patch or kext binary patch
        Patches->KextPatches[Patches->NrKexts].IsPlistPatch = IsPropertyTrue (GetProperty (Prop2, "InfoPlistPatch"));

        DBG (" | %a | len: %d\n",
          Patches->KextPatches[Patches->NrKexts].IsPlistPatch ? "PlistPatch" : "BinPatch",
          Patches->KextPatches[Patches->NrKexts].DataLen
        );

        Patches->NrKexts++; //must be out of DBG because it may be empty compiled
      }
    }
  }

  Prop = GetProperty (DictPointer, "KernelToPatch");
  if (Prop != NULL) {
    INTN    i, Count = GetTagCount (Prop);

    //delete old and create new
    if (Patches->KernelPatches) {
      Patches->NrKernels = 0;
      FreePool (Patches->KernelPatches);
    }

    if (Count > 0) {
      TagPtr          Prop2 = NULL, Dict = NULL;
      KERNEL_PATCH    *newPatches = AllocateZeroPool (Count * sizeof (KERNEL_PATCH));

      Patches->KernelPatches = newPatches;
      MsgLog ("KernelToPatch: %d requested\n", Count);

      for (i = 0; i < Count; i++) {
        CHAR8         *KernelPatchesLabel;
        UINTN         FindLen = 0, ReplaceLen = 0;
        UINT8         *TmpData, *TmpPatch;
        EFI_STATUS    Status = GetElement (Prop, i, &Prop2);

        DBG (" - [%02d]:", i);

        if (EFI_ERROR (Status) || (Prop2 == NULL)) {
          DBG (" %r parsing / empty element\n", Status);
          continue;
        }

        Dict = GetProperty (Prop2, "Comment");
        if (Dict != NULL) {
          KernelPatchesLabel = AllocateCopyPool (AsciiStrSize (Dict->string), Dict->string);
        } else {
          KernelPatchesLabel = AllocateCopyPool (8, "NoLabel");
        }

        DBG (" %a", KernelPatchesLabel);

        if (IsPropertyTrue (GetProperty (Prop2, "Disabled"))) {
          DBG (" | patch disabled, skip\n");
          continue;
        }

        TmpData    = GetDataSetting (Prop2, "Find", &FindLen);
        TmpPatch   = GetDataSetting (Prop2, "Replace", &ReplaceLen);

        if (!FindLen || !ReplaceLen || (FindLen != ReplaceLen)) {
          DBG (" | invalid Find/Replace data, skip\n");
          continue;
        }

        Patches->KernelPatches[Patches->NrKernels].Data       = AllocateCopyPool (FindLen, TmpData);
        Patches->KernelPatches[Patches->NrKernels].DataLen    = FindLen;
        Patches->KernelPatches[Patches->NrKernels].Patch      = AllocateCopyPool (FindLen, TmpPatch);
        Patches->KernelPatches[Patches->NrKernels].Count      = 0;
        Patches->KernelPatches[Patches->NrKernels].MatchOS    = NULL;
        Patches->KernelPatches[Patches->NrKernels].MatchBuild = NULL;
        Patches->KernelPatches[Patches->NrKernels].Disabled   = FALSE;
        Patches->KernelPatches[Patches->NrKernels].Label      = AllocateCopyPool (AsciiStrSize (KernelPatchesLabel), KernelPatchesLabel);

        Dict = GetProperty (Prop2, "Count");
        if (Dict != NULL) {
          Patches->KernelPatches[Patches->NrKernels].Count = GetPropertyInteger (Dict, 0);
        }

        FreePool (TmpData);
        FreePool (TmpPatch);
        FreePool (KernelPatchesLabel);

        // check enable/disabled patch (OS based) by Micky1979
        Dict = GetProperty (Prop2, "MatchOS");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          Patches->KernelPatches[Patches->NrKernels].MatchOS = AllocateCopyPool (AsciiStrSize (Dict->string), Dict->string);
          DBG (" | MatchOS: %a", Patches->KernelPatches[Patches->NrKernels].MatchOS);
        }

        Dict = GetProperty (Prop2, "MatchBuild");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          Patches->KernelPatches[Patches->NrKernels].MatchBuild = AllocateCopyPool (AsciiStrSize (Dict->string), Dict->string);
          DBG (" | MatchBuild: %a", Patches->KernelPatches[Patches->NrKernels].MatchBuild);
        }

        DBG (" | len: %d\n", Patches->KernelPatches[Patches->NrKernels].DataLen);

        Patches->NrKernels++;
      }
    }
  }

  return TRUE;
}

STATIC
BOOLEAN
FillinCustomEntry (
  IN OUT CUSTOM_LOADER_ENTRY    *Entry,
  TagPtr                        DictPointer,
  IN BOOLEAN                    SubEntry
) {
  TagPtr    Prop;

  if ((Entry == NULL) || (DictPointer == NULL)) {
    return FALSE;
  }

  if (IsPropertyTrue (GetProperty (DictPointer, "Disabled"))) {
    return FALSE;
  }

  Prop = GetProperty (DictPointer, "Volume");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (Entry->Volume) {
      FreePool (Entry->Volume);
    }

    Entry->Volume = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Path");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (Entry->Path) {
      FreePool (Entry->Path);
    }

    Entry->Path = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Settings");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (Entry->Settings) {
      FreePool (Entry->Settings);
    }

    Entry->Settings = PoolPrint (L"%a", Prop->string);
  }

  Entry->CommonSettings = IsPropertyTrue (GetProperty (DictPointer, "CommonSettings"));

  Prop = GetProperty (DictPointer, "AddArguments");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (Entry->Options != NULL) {
      CHAR16 *OldOptions = Entry->Options;
      Entry->Options = PoolPrint (L"%s %a", OldOptions, Prop->string);
      FreePool (OldOptions);
    } else {
      Entry->Options = PoolPrint (L"%a", Prop->string);
    }
  } else {
    Prop = GetProperty (DictPointer, "Arguments");
    if (Prop != NULL && (Prop->type == kTagTypeString)) {
      if (Entry->Options != NULL) {
        FreePool (Entry->Options);
      }

      Entry->Options = PoolPrint (L"%a", Prop->string);
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NODEFAULTARGS);
    }
  }

  Prop = GetProperty (DictPointer, "Title");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (Entry->FullTitle != NULL) {
      FreePool (Entry->FullTitle);
      Entry->FullTitle = NULL;
    }

    if (Entry->Title != NULL) {
      FreePool (Entry->Title);
    }

    Entry->Title = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "FullTitle");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (Entry->FullTitle) {
      FreePool (Entry->FullTitle);
    }

    Entry->FullTitle = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Image");
  if (Prop != NULL) {
    if (Entry->ImagePath) {
      FreePool (Entry->ImagePath);
      Entry->ImagePath = NULL;
    }

    if (Entry->Image) {
      FreeImage (Entry->Image);
      Entry->Image = NULL;
    }

    if (Prop->type == kTagTypeString) {
      Entry->ImagePath = PoolPrint (L"%a", Prop->string);
    }
  } else {
    Prop = GetProperty (DictPointer, "ImageData");
    if (Prop != NULL) {
      if (Entry->Image) {
        FreeImage (Entry->Image);
        Entry->Image = NULL;
      }

      if (Entry->ImagePath) {
        FreePool (Entry->ImagePath);
        Entry->ImagePath = NULL;
      }

      if (Prop->type == kTagTypeString) {
        UINT32  len = (UINT32)(AsciiStrLen (Prop->string) >> 1);

        if (len > 0) {
          UINT8   *data  = (UINT8 *)AllocateZeroPool (len);

          if (data) {
            Entry->Image = DecodePNG (data, Hex2Bin (Prop->string, data, len), 0, TRUE);
          }
        }
      } else if (Prop->type == kTagTypeData) {
        Entry->Image = DecodePNG (Prop->data, Prop->size, 0, TRUE);
      }
    }
  }

  Prop = GetProperty (DictPointer, "DriveImage");
  if (Prop != NULL) {
    if (Entry->DriveImagePath != NULL) {
      FreePool (Entry->DriveImagePath);
      Entry->DriveImagePath = NULL;
    }

    if (Entry->DriveImage != NULL) {
      FreeImage (Entry->DriveImage);
      Entry->DriveImage     = NULL;
    }

    if (Prop->type == kTagTypeString) {
      Entry->DriveImagePath = PoolPrint (L"%a", Prop->string);
    }
  } else {
    Prop = GetProperty (DictPointer, "DriveImageData");
    if (Prop != NULL) {
      if (Entry->DriveImage) {
        FreeImage (Entry->DriveImage);
        Entry->Image = NULL;
      }

      if (Entry->DriveImagePath != NULL) {
        FreePool (Entry->DriveImagePath);
        Entry->DriveImagePath = NULL;
      }

      if (Prop->type == kTagTypeString) {
        UINT32  len = (UINT32)(AsciiStrLen (Prop->string) >> 1);

        if (len > 0) {
          UINT8   *data = (UINT8 *)AllocateZeroPool (len);

          if (data) {
            Entry->DriveImage = DecodePNG (data, Hex2Bin (Prop->string, data, len), 0, TRUE);
          }
        }
      } else if (Prop->type == kTagTypeData) {
        Entry->DriveImage = DecodePNG (Prop->data, Prop->size, 0, TRUE);
      }
    }
  }

  Prop = GetProperty (DictPointer, "Hotkey");
  if ((Prop != NULL) && (Prop->type == kTagTypeString) && Prop->string) {
    Entry->Hotkey = *(Prop->string);
  }

  // Hidden Property, Values:
  // - No (show the entry)
  // - Yes (hide the entry but can be show with F3)
  // - Always (always hide the entry)
  Prop = GetProperty (DictPointer, "Hidden");
  if (Prop != NULL) {
    if (
      (Prop->type == kTagTypeString) &&
      (AsciiStriCmp (Prop->string, "Always") == 0)
    ) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_DISABLED);
    } else if (IsPropertyTrue (Prop)) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_HIDDEN);
    } else {
      Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_HIDDEN);
    }
  }

  Prop = GetProperty (DictPointer, "Type");
  if (Prop != NULL && (Prop->type == kTagTypeString)) {
    if (AsciiStriCmp (Prop->string, "OSX") == 0) {
      Entry->Type = OSTYPE_OSX;
    } else if (AsciiStriCmp (Prop->string, "OSXInstaller") == 0) {
      Entry->Type = OSTYPE_OSX_INSTALLER;
    } else if (AsciiStriCmp (Prop->string, "OSXRecovery") == 0) {
      Entry->Type = OSTYPE_RECOVERY;
    } else if (AsciiStriCmp (Prop->string, "Windows") == 0) {
      Entry->Type = OSTYPE_WINEFI;
    } else if (AsciiStriCmp (Prop->string, "Linux") == 0) {
      Entry->Type = OSTYPE_LIN;
    } else if (AsciiStriCmp (Prop->string, "LinuxKernel") == 0) {
      Entry->Type = OSTYPE_LINEFI;
    } else {
      DBG ("** Warning: unknown custom entry Type '%a'\n", Prop->string);
      Entry->Type = OSTYPE_OTHER;
    }
  } else {
    if ((Entry->Type == 0) && Entry->Path) {
      // Try to set Entry->type from Entry->Path
      Entry->Type = GetOSTypeFromPath (Entry->Path);
    }
  }

  Entry->VolumeType = GetVolumeType (DictPointer);

  if ((Entry->Options == NULL) && OSTYPE_IS_WINDOWS (Entry->Type)) {
    Entry->Options = L"-s -h";
  }

  if (Entry->Title == NULL) {
    if (OSTYPE_IS_OSX_RECOVERY (Entry->Type)) {
      Entry->Title = PoolPrint (L"Recovery");
    } else if (OSTYPE_IS_OSX_INSTALLER (Entry->Type)) {
      Entry->Title = PoolPrint (L"Install OSX");
    }
  }

  if ((Entry->Image == NULL) && (Entry->ImagePath == NULL)) {
    if (OSTYPE_IS_OSX_RECOVERY (Entry->Type)) {
      Entry->ImagePath = L"mac";
    }
  }

  if ((Entry->DriveImage == NULL) && (Entry->DriveImagePath == NULL)) {
    if (OSTYPE_IS_OSX_RECOVERY (Entry->Type)) {
      Entry->DriveImagePath = L"recovery";
    }
  }

  // OS Specific flags
  if (OSTYPE_IS_OSX_GLOB (Entry->Type)) {
    // InjectKexts default values
    Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_WITHKEXTS);

    Prop = GetProperty (DictPointer, "InjectKexts");
    if (Prop != NULL) {
      if (IsPropertyTrue (Prop)) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_WITHKEXTS);
      } else if (
          (Prop->type == kTagTypeString) &&
          (AsciiStriCmp (Prop->string, "Detect") == 0)
        ) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_WITHKEXTS);
      } else {
        DBG ("** Warning: unknown custom entry InjectKexts value '%a'\n", Prop->string);
      }
    } else {
      // Use global settings
      if (gSettings.WithKexts) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_WITHKEXTS);
      }
    }

    // NoCaches default value
    Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_NOCACHES);

    if (IsPropertyTrue (GetProperty (DictPointer, "NoCaches")) || gSettings.NoCaches) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NOCACHES);
    }

    // KernelAndKextPatches
    if (!SubEntry) { // CopyKernelAndKextPatches already in: DuplicateCustomEntry if SubEntry == TRUE
      //DBG ("Copying global patch settings\n");
      CopyKernelAndKextPatches (
        (KERNEL_AND_KEXT_PATCHES *)(((UINTN)Entry) + OFFSET_OF (CUSTOM_LOADER_ENTRY, KernelAndKextPatches)),
        (KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF (SETTINGS_DATA, KernelAndKextPatches))
      );
    }
  }

  // Sub entries
  Prop = GetProperty (DictPointer, "SubEntries");
  if (Prop != NULL) {
    if (Prop->type == kTagTypeFalse) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NODEFAULTMENU);
    } else if (Prop->type != kTagTypeTrue) {
      CUSTOM_LOADER_ENTRY   *CustomSubEntry;
      INTN                  i, Count = GetTagCount (Prop);
      TagPtr                Dict = NULL;

      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NODEFAULTMENU);

      if (Count > 0) {
        for (i = 0; i < Count; i++) {
          if (EFI_ERROR (GetElement (Prop, i, &Dict))) {
            continue;
          }

          if (Dict == NULL) {
            break;
          }

          // Allocate a sub entry
          CustomSubEntry = DuplicateCustomEntry (Entry);
          if (CustomSubEntry) {
            if (!FillinCustomEntry (CustomSubEntry, Dict, TRUE) || !AddCustomSubEntry (Entry, CustomSubEntry)) {
              if (CustomSubEntry) {
                FreePool (CustomSubEntry);
              }
            }
          }
        }
      }
    }
  }

  return TRUE;
}

STATIC
BOOLEAN
FillinCustomTool (
  IN OUT    CUSTOM_TOOL_ENTRY *Entry,
  TagPtr    DictPointer
) {
  TagPtr  Prop;

  if ((Entry == NULL) || (DictPointer == NULL)) {
    return FALSE;
  }

  if (IsPropertyTrue (GetProperty (DictPointer, "Disabled"))) {
    return FALSE;
  }

  Prop = GetProperty (DictPointer, "Volume");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    if (Entry->Volume) {
      FreePool (Entry->Volume);
    }
    Entry->Volume = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Path");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    if (Entry->Path != NULL) {
      FreePool (Entry->Path);
    }

    Entry->Path = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Arguments");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    if (Entry->Options != NULL) {
      FreePool (Entry->Options);
    } else {
      Entry->Options = PoolPrint (L"%a", Prop->string);
    }
  }

  Prop = GetProperty (DictPointer, "FullTitle");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    if (Entry->FullTitle != NULL) {
      FreePool (Entry->FullTitle);
    }

    Entry->FullTitle = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Title");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    if (Entry->Title != NULL) {
      FreePool (Entry->Title);
    }
    Entry->Title = PoolPrint (L"%a", Prop->string);
  }

  Prop = GetProperty (DictPointer, "Image");
  if (Prop != NULL) {
    if (Entry->ImagePath != NULL) {
      FreePool (Entry->ImagePath);
      Entry->ImagePath = NULL;
    }

    if (Entry->Image != NULL) {
      FreeImage (Entry->Image);
      Entry->Image = NULL;
    }

    if (Prop->type == kTagTypeString) {
      Entry->ImagePath = PoolPrint (L"%a", Prop->string);
    }
  } else {
    Prop = GetProperty (DictPointer, "ImageData");
    if (Prop != NULL) {
      if (Entry->Image != NULL) {
        FreeImage (Entry->Image);
        Entry->Image = NULL;
      }

      if (Prop->type == kTagTypeString) {
        UINT32 Len = (UINT32)(AsciiStrLen (Prop->string) >> 1);
        if (Len > 0) {
          UINT8   *data = (UINT8 *)AllocateZeroPool (Len);

          if (data != NULL) {
            Entry->Image = DecodePNG (data, Hex2Bin (Prop->string, data, Len), 0, TRUE);
          }
        }
      } else if (Prop->type == kTagTypeData) {
        Entry->Image = DecodePNG (Prop->data, Prop->size, 0, TRUE);
      }
    }
  }

  Prop = GetProperty (DictPointer, "Hotkey");
  if ((Prop != NULL) && (Prop->type == kTagTypeString) && Prop->string) {
    Entry->Hotkey = *(Prop->string);
  }
  // Hidden Property, Values:
  // - No (show the entry)
  // - Yes (hide the entry but can be show with F3)
  // - Always (always hide the entry)
  Prop = GetProperty (DictPointer, "Hidden");
  if (Prop != NULL) {
    if (
      (Prop->type == kTagTypeString) &&
      (AsciiStriCmp (Prop->string, "Always") == 0)
    ) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_DISABLED);
    } else if (IsPropertyTrue (Prop)) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_HIDDEN);
    } else {
      Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_HIDDEN);
    }
  }

/*
  Entry->VolumeType = GetVolumeType (DictPointer);
*/
  return TRUE;
}

VOID
GetListOfACPI () {
  //EFI_STATUS          Status;
  REFIT_DIR_ITER      DirIter;
  EFI_FILE_INFO       *DirEntry;
  UINTN               i = 0, y = 0/*, Size = 0 */;
  CHAR8               *Ptr = NULL;
  CHAR16              *AcpiPath = PoolPrint (DIR_ACPI_PATCHED, OEMPath);

  DbgHeader ("GetListOfACPI");

  DirIterOpen (SelfRootDir, AcpiPath, &DirIter);

  while (DirIterNext (&DirIter, 2, L"*.aml", &DirEntry)) {
    if (DirEntry->FileName[0] == L'.') {
      continue;
    }

    if (StriStr (DirEntry->FileName, L"DSDT") != NULL) {
      continue;
    }

    MsgLog ("- [%02d]: %s", i++, DirEntry->FileName);

    //Status = LoadFile (SelfRootDir, PoolPrint (L"%s\\%s", AcpiPath, DirEntry->FileName), (UINT8 **)&Ptr, &Size);

    //if (EFI_ERROR (Status) || (Ptr == NULL) || (Size == 0)) {
    if (0) {
      MsgLog (" - bad, skip");
    } else {
      BOOLEAN   ACPIDisabled = FALSE;
      ACPI_PATCHED_AML    *ACPIPatchedAMLTmp = AllocateZeroPool (sizeof (ACPI_PATCHED_AML));

      ACPIPatchedAMLTmp->FileName = PoolPrint (DirEntry->FileName);

      for (y = 0; y < gSettings.DisabledAMLCount; y++) {
        if (StriCmp (ACPIPatchedAMLTmp->FileName, gSettings.DisabledAML[y]) == 0) {
          ACPIDisabled = TRUE;
          break;
        }
      }

      ACPIPatchedAMLTmp->MenuItem.BValue = ACPIDisabled;
      ACPIPatchedAMLTmp->Next = ACPIPatchedAML;
      ACPIPatchedAML = ACPIPatchedAMLTmp;
      ACPIPatchedAMLNum++;

      MsgLog (" - added");
    }

    MsgLog ("\n");

    FreePool (Ptr);
  }

  if (ACPIPatchedAMLNum) {
    ACPI_PATCHED_AML   *aTmp = ACPIPatchedAML;

    ACPIPatchedAML = 0;

    while (aTmp) {
      ACPI_PATCHED_AML   *next = aTmp->Next;

      aTmp->Next = ACPIPatchedAML;
      ACPIPatchedAML = aTmp;
      aTmp = next;
    }
  }

  DirIterClose (&DirIter);
  FreePool (AcpiPath);
}

VOID
GetListOfConfigs () {
  EFI_STATUS        Status;
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  CHAR8             *Ptr = NULL;
  UINTN             i = 0, y = 0, Size = 0;
  BOOLEAN           Found = FALSE;

  DbgHeader ("GetListOfConfigs");

  DirIterOpen (SelfRootDir, DIR_CLOVER, &DirIter);

  OldChosenConfig = 0;

  while (DirIterNext (&DirIter, 2, L"*.plist", &DirEntry)) {
    if (DirEntry->FileName[0] == L'.') {
      continue;
    }

    MsgLog ("- [%02d]: %s", i++, DirEntry->FileName);

    Status = LoadFile (SelfRootDir, PoolPrint (L"%s\\%s", DIR_CLOVER, DirEntry->FileName), (UINT8 **)&Ptr, &Size);

    if (EFI_ERROR (Status) || (Ptr == NULL) || (Size == 0)) {
      MsgLog (" - bad, skip");
    } else {
      S_FILES   *aTmp = AllocateZeroPool (sizeof (S_FILES));
      CHAR16    *TmpCfg = EfiStrDuplicate (DirEntry->FileName);

      TmpCfg = ReplaceExtension (DirEntry->FileName, L"");

      aTmp->Index = y;
      Found = TRUE;

      if (StrniCmp (TmpCfg, CONFIG_FILENAME, StrLen (CONFIG_FILENAME)) == 0) {
        OldChosenConfig = y;
      }

      aTmp->FileName = PoolPrint (TmpCfg);
      aTmp->Next = aConfigs;
      aConfigs = aTmp;

      FreePool (TmpCfg);
      MsgLog (" - added");

      y++;
    }

    MsgLog ("\n");

    FreePool (Ptr);
    Size = 0;
  }

  if (Found) {
    S_FILES   *aTmp = aConfigs;

    aConfigs = 0;

    while (aTmp) {
      S_FILES   *next = aTmp->Next;

      aTmp->Next = aConfigs;
      aConfigs = aTmp;
      aTmp = next;
    }
  }

  DirIterClose (&DirIter);
}

TagPtr
LoadTheme (
  CHAR16    *TestTheme
) {
  EFI_STATUS    Status    = EFI_UNSUPPORTED;
  TagPtr        ThemeDict = NULL;
  CHAR8         *ThemePtr = NULL;
  UINTN         Size      = 0;

  if (TestTheme != NULL) {
    if (ThemePath != NULL) {
      FreePool (ThemePath);
    }

    ThemePath = PoolPrint (L"%s\\%s", DIR_THEMES, TestTheme);
    if (ThemePath != NULL) {
      if (ThemeDir != NULL) {
        ThemeDir->Close (ThemeDir);
        ThemeDir = NULL;
      }

      Status = SelfRootDir->Open (SelfRootDir, &ThemeDir, ThemePath, EFI_FILE_MODE_READ, 0);
      if (!EFI_ERROR (Status)) {
        Status = LoadFile (ThemeDir, PoolPrint (L"%s.plist", CONFIG_THEME_FILENAME), (UINT8 **)&ThemePtr, &Size);
        if (!EFI_ERROR (Status) && (ThemePtr != NULL) && (Size != 0)) {
          Status = ParseXML (ThemePtr, &ThemeDict, 0);

          if (EFI_ERROR (Status)) {
            ThemeDict = NULL;
            DBG ("Theme: '%s' (%s) %s parsed\n", TestTheme, ThemePath, (ThemeDict == NULL) ? L"NOT" : L"");
          }
        }

        if (ThemePtr != NULL) {
          FreePool (ThemePtr);
        }
      }
    }
  }

  return ThemeDict;
}

CHAR16 *
RandomTheme (
  INTN    Index
) {
  S_FILES    *aTmp = aThemes;

  while (aTmp) {
    if (aTmp->Index == Index) {
      return aTmp->FileName;
    }

    aTmp = aTmp->Next;
  }

  return CONFIG_THEME_EMBEDDED;
}

VOID
SetThemeIndex () {
  S_FILES    *aTmp = aThemes;

  while (aTmp) {
    if (StriCmp (GlobalConfig.Theme, aTmp->FileName) == 0) {
      OldChosenTheme = aTmp->Index;
      break;
    }

    aTmp = aTmp->Next;
  }
}

VOID
FreeTheme () {
  FreeSelections ();
  FreeBanner ();
  FreeButtons ();
  FreeBuiltinIcons ();
  FreeAnims ();
  FreeScrollBar ();

  if (GlobalConfig.BackgroundName != NULL) {
    FreePool (GlobalConfig.BackgroundName);
    GlobalConfig.BackgroundName = NULL;
  }

  if (GlobalConfig.BannerFileName != NULL) {
    FreePool (GlobalConfig.BannerFileName);
    GlobalConfig.BannerFileName = NULL;
  }

  if (GlobalConfig.SelectionSmallFileName != NULL) {
    FreePool (GlobalConfig.SelectionSmallFileName);
    GlobalConfig.SelectionSmallFileName = NULL;
  }

  if (GlobalConfig.SelectionBigFileName != NULL) {
    FreePool (GlobalConfig.SelectionBigFileName);
    GlobalConfig.SelectionBigFileName   = NULL;
  }

  if (GlobalConfig.SelectionIndicatorName != NULL) {
    FreePool (GlobalConfig.SelectionIndicatorName);
    GlobalConfig.SelectionIndicatorName = NULL;
  }

  if (GlobalConfig.FontFileName != NULL) {
    FreePool (GlobalConfig.FontFileName);
    GlobalConfig.FontFileName = NULL;
  }

  if (BigBack != NULL) {
    FreeImage (BigBack);
    BigBack = NULL;
  }

  if (BackgroundImage != NULL) {
    FreeImage (BackgroundImage);
    BackgroundImage = NULL;
  }

  if (FontImage != NULL) {
    FreeImage (FontImage);
    FontImage = NULL;
  }

  if (FontImageHover != NULL) {
    FreeImage (FontImageHover);
    FontImageHover = NULL;
  }
}

STATIC
EFI_STATUS
GetThemeTagSettings (
  TagPtr    DictPointer
) {
  TagPtr    Dict, Dict2, Dict3;
  BOOLEAN   Embedded = (DictPointer == NULL);

  FreeTheme ();

  //fill default to have an ability change theme
  GlobalConfig.BackgroundDark                   = DefaultConfig.BackgroundDark;
  GlobalConfig.BackgroundScale                  = Embedded ? DefaultConfig.BackgroundScale : Crop;
  GlobalConfig.BackgroundSharp                  = DefaultConfig.BackgroundSharp;
  GlobalConfig.BadgeOffsetX                     = DefaultConfig.BadgeOffsetX;
  GlobalConfig.BadgeOffsetY                     = DefaultConfig.BadgeOffsetY;
  GlobalConfig.BadgeScale                       = DefaultConfig.BadgeScale;
  GlobalConfig.BannerEdgeHorizontal             = Embedded ? DefaultConfig.BannerEdgeHorizontal : SCREEN_EDGE_LEFT;
  GlobalConfig.BannerEdgeVertical               = Embedded ? DefaultConfig.BannerEdgeVertical : SCREEN_EDGE_TOP;
  GlobalConfig.BannerNudgeX                     = DefaultConfig.BannerNudgeX;
  GlobalConfig.BannerNudgeY                     = DefaultConfig.BannerNudgeY;
  GlobalConfig.BannerPosX                       = DefaultConfig.BannerPosX;
  GlobalConfig.BannerPosY                       = DefaultConfig.BannerPosY;
  GlobalConfig.BootCampStyle                    = DefaultConfig.BootCampStyle;
  GlobalConfig.CharWidth                        = DefaultConfig.CharWidth;
  //GlobalConfig.CharHeight                       = DefaultConfig.CharHeight;
  GlobalConfig.CharRows                         = DefaultConfig.CharRows;
  GlobalConfig.CharCols                         = DefaultConfig.CharCols;
  GlobalConfig.FontEmbedded                     = DefaultConfig.FontEmbedded;
  GlobalConfig.Font                             = Embedded ? FONT_ALFA : FONT_LOAD;
  GlobalConfig.HideBadges                       = DefaultConfig.HideBadges;
  GlobalConfig.HideUIFlags                      = DefaultConfig.HideUIFlags;
  GlobalConfig.DisableFlags                     = DefaultConfig.DisableFlags;
  GlobalConfig.IconFormat                       = DefaultConfig.IconFormat;
  GlobalConfig.MainEntriesSize                  = DefaultConfig.MainEntriesSize;
  GlobalConfig.PruneScrollRows                  = DefaultConfig.PruneScrollRows;
  GlobalConfig.SelectionColor                   = DefaultConfig.SelectionColor;
  GlobalConfig.SelectionOnTop                   = DefaultConfig.SelectionOnTop;
  GlobalConfig.SelectionTextColor               = DefaultConfig.SelectionTextColor;
  GlobalConfig.TextColor                        = DefaultConfig.TextColor;
  GlobalConfig.ThemeDesignHeight                = DefaultConfig.ThemeDesignHeight;
  GlobalConfig.ThemeDesignWidth                 = DefaultConfig.ThemeDesignWidth;
  GlobalConfig.TileXSpace                       = DefaultConfig.TileXSpace;
  GlobalConfig.TileYSpace                       = DefaultConfig.TileYSpace;

  GlobalConfig.row0TileSize                     = DefaultConfig.row0TileSize;
  GlobalConfig.row1TileSize                     = DefaultConfig.row1TileSize;
  GlobalConfig.LayoutBannerOffset               = DefaultConfig.LayoutBannerOffset;
  GlobalConfig.LayoutButtonOffset               = DefaultConfig.LayoutButtonOffset;
  GlobalConfig.LayoutTextOffset                 = DefaultConfig.LayoutTextOffset;
  GlobalConfig.LayoutAnimMoveForMenuX           = DefaultConfig.LayoutAnimMoveForMenuX;

  GlobalConfig.ScrollWidth                      = DefaultConfig.ScrollWidth;
  GlobalConfig.ScrollButtonsHeight              = DefaultConfig.ScrollButtonsHeight;
  GlobalConfig.ScrollBarDecorationsHeight       = DefaultConfig.ScrollBarDecorationsHeight;
  GlobalConfig.ScrollScrollDecorationsHeight    = DefaultConfig.ScrollScrollDecorationsHeight;

  if (Embedded) {
    return EFI_SUCCESS;
  }

  GlobalConfig.BootCampStyle = IsPropertyTrue (GetProperty (DictPointer, "BootCampStyle"));

  Dict = GetProperty (DictPointer, "Background");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Type");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString) && Dict2->string) {
      if ((Dict2->string[0] == 'S') || (Dict2->string[0] == 's')) {
        GlobalConfig.BackgroundScale = Scale;
      } else if ((Dict2->string[0] == 'T') || (Dict2->string[0] == 't')) {
        GlobalConfig.BackgroundScale = Tile;
      }
    }

    Dict2 = GetProperty (Dict, "Path");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString) && Dict2->string) {
      GlobalConfig.BackgroundName = PoolPrint (L"%a", Dict2->string);
    }

    Dict2 = GetProperty (Dict, "Sharp");
    GlobalConfig.BackgroundSharp  = (INT32)GetPropertyInteger (Dict2, DefaultConfig.BackgroundSharp);

    GlobalConfig.BackgroundDark   = IsPropertyTrue (GetProperty (Dict, "Dark"));
  }

  Dict = GetProperty (DictPointer, "Banner");
  if (Dict != NULL) {
    // retain for legacy themes.
    if ((Dict->type == kTagTypeString) && Dict->string) {
      GlobalConfig.BannerFileName = PoolPrint (L"%a", Dict->string);
    } else {
      // for new placement settings
      Dict2 = GetProperty (Dict, "Path");
      if (Dict2 != NULL) {
        if ((Dict2->type == kTagTypeString) && Dict2->string) {
          GlobalConfig.BannerFileName = PoolPrint (L"%a", Dict2->string);
        }
      }

      Dict2 = GetProperty (Dict, "ScreenEdgeX");
      if (Dict2 != NULL && (Dict2->type == kTagTypeString) && Dict2->string) {
        if (AsciiStrCmp (Dict2->string, "left") == 0) {
          GlobalConfig.BannerEdgeHorizontal = SCREEN_EDGE_LEFT;
        } else if (AsciiStrCmp (Dict2->string, "right") == 0) {
          GlobalConfig.BannerEdgeHorizontal = SCREEN_EDGE_RIGHT;
        }
      }

      Dict2 = GetProperty (Dict, "ScreenEdgeY");
      if (Dict2 != NULL && (Dict2->type == kTagTypeString) && Dict2->string) {
        if (AsciiStrCmp (Dict2->string, "top") == 0) {
          GlobalConfig.BannerEdgeVertical = SCREEN_EDGE_TOP;
        } else if (AsciiStrCmp (Dict2->string, "bottom") == 0) {
          GlobalConfig.BannerEdgeVertical = SCREEN_EDGE_BOTTOM;
        }
      }

      Dict2 = GetProperty (Dict, "DistanceFromScreenEdgeX%");
      GlobalConfig.BannerPosX   = (INT32)GetPropertyInteger (Dict2, DefaultConfig.BannerPosX);

      Dict2 = GetProperty (Dict, "DistanceFromScreenEdgeY%");
      GlobalConfig.BannerPosY   = (INT32)GetPropertyInteger (Dict2, DefaultConfig.BannerPosY);

      Dict2 = GetProperty (Dict, "NudgeX");
      GlobalConfig.BannerNudgeX = GetPropertyInteger (Dict2, 0);

      Dict2 = GetProperty (Dict, "NudgeY");
      GlobalConfig.BannerNudgeY = GetPropertyInteger (Dict2, 0);
    }
  }

  Dict = GetProperty (DictPointer, "Badges");
  if (Dict != NULL) {
    if (IsPropertyTrue (GetProperty (Dict, "Swap"))) {
      GlobalConfig.HideBadges |= HDBADGES_SWAP;
      //DBG ("OS main and drive as badge\n");
    }

    if (IsPropertyTrue (GetProperty (Dict, "Show"))) {
      GlobalConfig.HideBadges |= HDBADGES_SHOW;
    }

    if (IsPropertyTrue (GetProperty (Dict, "Inline"))) {
      GlobalConfig.HideBadges |= HDBADGES_INLINE;
    }

    // blackosx added X and Y position for badge offset.
    Dict2 = GetProperty (Dict, "OffsetX");
    GlobalConfig.BadgeOffsetX = (INT32)GetPropertyInteger (Dict2, DefaultConfig.BadgeOffsetX);

    Dict2 = GetProperty (Dict, "OffsetY");
    GlobalConfig.BadgeOffsetY = (INT32)GetPropertyInteger (Dict2, DefaultConfig.BadgeOffsetY);

    Dict2 = GetProperty (Dict, "Scale");
    GlobalConfig.BadgeScale = GetPropertyInteger (Dict2, DefaultConfig.BadgeScale);
  }

  Dict = GetProperty (DictPointer, "Origination");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "DesignWidth");
    GlobalConfig.ThemeDesignWidth = GetPropertyInteger (Dict2, DefaultConfig.ThemeDesignWidth);

    Dict2 = GetProperty (Dict, "DesignHeight");
    GlobalConfig.ThemeDesignHeight = GetPropertyInteger (Dict2, DefaultConfig.ThemeDesignHeight);
  }

  Dict = GetProperty (DictPointer, "Layout");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "BannerOffset");
    GlobalConfig.LayoutBannerOffset = GetPropertyInteger (Dict2, DefaultConfig.LayoutBannerOffset);

    Dict2 = GetProperty (Dict, "ButtonOffset");
    GlobalConfig.LayoutButtonOffset = GetPropertyInteger (Dict2, DefaultConfig.LayoutButtonOffset);

    Dict2 = GetProperty (Dict, "TextOffset");
    GlobalConfig.LayoutTextOffset = GetPropertyInteger (Dict2, DefaultConfig.LayoutTextOffset);

    Dict2 = GetProperty (Dict, "AnimAdjustForMenuX");
    GlobalConfig.LayoutAnimMoveForMenuX = GetPropertyInteger (Dict2, DefaultConfig.LayoutAnimMoveForMenuX);

    // GlobalConfig.MainEntriesSize
    Dict2 = GetProperty (Dict, "MainEntriesSize");
    GlobalConfig.MainEntriesSize = GetPropertyInteger (Dict2, DefaultConfig.MainEntriesSize);

    Dict2 = GetProperty (Dict, "TileXSpace");
    GlobalConfig.TileXSpace = GetPropertyInteger (Dict2, DefaultConfig.TileXSpace);

    Dict2 = GetProperty (Dict, "TileYSpace");
    GlobalConfig.TileYSpace = GetPropertyInteger (Dict2, DefaultConfig.TileYSpace);

    Dict2 = GetProperty (Dict, "SelectionBigWidth");
    GlobalConfig.row0TileSize = GetPropertyInteger (Dict2, DefaultConfig.row0TileSize);

    Dict2 = GetProperty (Dict, "SelectionSmallWidth");
    GlobalConfig.row1TileSize = GetPropertyInteger (Dict2, DefaultConfig.row1TileSize);

    Dict2 = GetProperty (Dict, "PruneScrollRows");
    GlobalConfig.PruneScrollRows = GetPropertyInteger (Dict2, DefaultConfig.PruneScrollRows);
  }

  Dict = GetProperty (DictPointer, "Components");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Banner");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BANNER;
    }

    Dict2 = GetProperty (Dict, "Functions");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_FUNCS;
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_FUNCS)) {
      Dict2 = GetProperty (Dict, "Help");
      if (Dict2 && Dict2->type == kTagTypeFalse) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HELP;
      }

      Dict2 = GetProperty (Dict, "Reset");
      if (Dict2 && Dict2->type == kTagTypeFalse) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_RESET;
      }

      Dict2 = GetProperty (Dict, "Shutdown");
      if (Dict2 && Dict2->type == kTagTypeFalse) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SHUTDOWN;
      }

      Dict2 = GetProperty (Dict, "Exit");
      if (Dict2 && Dict2->type == kTagTypeFalse) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_EXIT;
      }
    }

    Dict2 = GetProperty (Dict, "Tools");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.DisableFlags |= HIDEUI_FLAG_TOOLS;
    }

    Dict2 = GetProperty (Dict, "Label");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_LABEL;
    }

    Dict2 = GetProperty (Dict, "RevisionText");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_REVISION_TEXT;
    }

    Dict2 = GetProperty (Dict, "HelpText");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HELP_TEXT;
    }

    Dict2 = GetProperty (Dict, "MenuTitle");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_MENU_TITLE;
    }

    Dict2 = GetProperty (Dict, "MenuTitleImage");
    if (Dict2 && Dict2->type == kTagTypeFalse) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_MENU_TITLE_IMAGE;
    }
  }

  Dict = GetProperty (DictPointer, "Selection");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "TextColor");
    GlobalConfig.SelectionTextColor = (UINTN)GetPropertyInteger (Dict2, DefaultConfig.SelectionTextColor);

    Dict2 = GetProperty (Dict, "Color");
    GlobalConfig.SelectionColor = (UINTN)GetPropertyInteger (Dict2, DefaultConfig.SelectionColor);

    Dict2 = GetProperty (Dict, "Small");
    if ((Dict2->type == kTagTypeString) && Dict2->string) {
      GlobalConfig.SelectionSmallFileName = PoolPrint (L"%a", Dict2->string);
    }

    Dict2 = GetProperty (Dict, "Big");
    if ((Dict2->type == kTagTypeString) && Dict2->string) {
      GlobalConfig.SelectionBigFileName = PoolPrint (L"%a", Dict2->string);
    }

    Dict2 = GetProperty (Dict, "Indicator");
    if ((Dict2->type == kTagTypeString) && Dict2->string) {
      GlobalConfig.SelectionIndicatorName = PoolPrint (L"%a", Dict2->string);
    }

    GlobalConfig.SelectionOnTop = IsPropertyTrue (GetProperty (Dict, "OnTop"));

    GlobalConfig.NonSelectedGrey = IsPropertyTrue (GetProperty (Dict, "ChangeNonSelectedGrey"));
  }

  Dict = GetProperty (DictPointer, "Scroll");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Width");
    GlobalConfig.ScrollWidth = GetPropertyInteger (Dict2, DefaultConfig.ScrollWidth);

    Dict2 = GetProperty (Dict, "Height");
    GlobalConfig.ScrollButtonsHeight = GetPropertyInteger (Dict2, DefaultConfig.ScrollButtonsHeight);

    Dict2 = GetProperty (Dict, "BarHeight");
    GlobalConfig.ScrollBarDecorationsHeight = GetPropertyInteger (Dict2, DefaultConfig.ScrollBarDecorationsHeight);

    Dict2 = GetProperty (Dict, "ScrollHeight");
    GlobalConfig.ScrollScrollDecorationsHeight = GetPropertyInteger (Dict2, DefaultConfig.ScrollScrollDecorationsHeight);
  }

  Dict = GetProperty (DictPointer, "Font");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Type");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString) && Dict2->string) {
      if ((Dict2->string[0] == 'A') || (Dict2->string[0] == 'B')) {
        GlobalConfig.Font = FONT_ALFA;
      } else if ((Dict2->string[0] == 'G') || (Dict2->string[0] == 'W')) {
        GlobalConfig.Font = FONT_GRAY;
      } else if ((Dict2->string[0] == 'L') || (Dict2->string[0] == 'l')) {
        GlobalConfig.Font = FONT_LOAD;
      }
    }

    Dict2 = GetProperty (Dict, "Path");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString) && Dict2->string) {
      GlobalConfig.FontFileName = PoolPrint (L"%a", Dict2->string);
      GlobalConfig.Font = FONT_LOAD;
    }

    Dict2 = GetProperty (Dict, "TextColor");
    GlobalConfig.TextColor = (UINTN)GetPropertyInteger (Dict2, DefaultConfig.TextColor);

    Dict2 = GetProperty (Dict, "CharWidth");
    GlobalConfig.CharWidth = GetPropertyInteger (Dict2, DefaultConfig.CharWidth);
    //if (GlobalConfig.CharWidth & 1) {
    //  MsgLog ("Warning! Character width %d should be even!\n", GlobalConfig.CharWidth);
    //  GlobalConfig.CharWidth++;
    //}

    //Dict2 = GetProperty (Dict, "CharHeight");
    //GlobalConfig.CharHeight = GetPropertyInteger (Dict2, DefaultConfig.CharHeight);

    Dict2 = GetProperty (Dict, "CharRows");
    GlobalConfig.CharRows = GetPropertyInteger (Dict2, 0);

    //GlobalConfig.Proportional = IsPropertyTrue (GetProperty (Dict, "Proportional"));
  }

  Dict = GetProperty (DictPointer, "Anime");
  if (Dict != NULL) {
    INTN   i, Count = GetTagCount (Dict);

    for (i = 0; i < Count; i++) {
      GUI_ANIME   *Anime;

      if (EFI_ERROR (GetElement (Dict, i, &Dict3))) {
        continue;
      }

      if (Dict3 == NULL) {
        break;
      }

      Anime = AllocateZeroPool (sizeof (GUI_ANIME));
      if (Anime == NULL) {
        break;
      }

      Dict2 = GetProperty (Dict3, "ID");
      Anime->ID = (UINTN)GetPropertyInteger (Dict2, 1); //default=main screen

      Dict2 = GetProperty (Dict3, "Path");
      if (Dict2 != NULL && (Dict2->type == kTagTypeString) && Dict2->string) {
        Anime->Path = PoolPrint (L"%a", Dict2->string);
      }

      Dict2 = GetProperty (Dict3, "Frames");
      Anime->Frames = (UINTN)GetPropertyInteger (Dict2, Anime->Frames);

      Dict2 = GetProperty (Dict3, "FrameTime");
      Anime->FrameTime = (UINTN)GetPropertyInteger (Dict2, Anime->FrameTime);

      Dict2 = GetProperty (Dict3, "ScreenEdgeX");
      if (Dict2 != NULL && (Dict2->type == kTagTypeString) && Dict2->string) {
        if (AsciiStrCmp (Dict2->string, "left") == 0) {
          Anime->ScreenEdgeHorizontal = SCREEN_EDGE_LEFT;
        } else if (AsciiStrCmp (Dict2->string, "right") == 0) {
          Anime->ScreenEdgeHorizontal = SCREEN_EDGE_RIGHT;
        }
      }

      Dict2 = GetProperty (Dict3, "ScreenEdgeY");
      if (Dict2 != NULL && (Dict2->type == kTagTypeString) && Dict2->string) {
        if (AsciiStrCmp (Dict2->string, "top") == 0) {
          Anime->ScreenEdgeVertical = SCREEN_EDGE_TOP;
        } else if (AsciiStrCmp (Dict2->string, "bottom") == 0) {
          Anime->ScreenEdgeVertical = SCREEN_EDGE_BOTTOM;
        }
      }

      //default values are centre

      Dict2 = GetProperty (Dict3, "DistanceFromScreenEdgeX%");
      Anime->FilmX = (INT32)GetPropertyInteger (Dict2, INITVALUE);

      Dict2 = GetProperty (Dict3, "DistanceFromScreenEdgeY%");
      Anime->FilmY = (INT32)GetPropertyInteger (Dict2, INITVALUE);

      Dict2 = GetProperty (Dict3, "NudgeX");
      Anime->NudgeX = GetPropertyInteger (Dict2, INITVALUE);

      Dict2 = GetProperty (Dict3, "NudgeY");
      Anime->NudgeY = GetPropertyInteger (Dict2, INITVALUE);
      Anime->Once = IsPropertyTrue (GetProperty (Dict3, "Once"));

      // Add the anime to the list
      if ((Anime->ID == 0) || (Anime->Path == NULL)) {
        FreePool (Anime);
      } else if (GuiAnime != NULL) { //second anime or further
        if (GuiAnime->ID == Anime->ID) { //why the same anime here?
          Anime->Next = GuiAnime->Next;
          FreeAnime (GuiAnime); //free double
        } else {
          GUI_ANIME   *Ptr = GuiAnime;

          while (Ptr->Next) {
            if (Ptr->Next->ID == Anime->ID) { //delete double from list
              GUI_ANIME *Next = Ptr->Next;
              Ptr->Next = Next->Next;
              FreeAnime (Next);
              break;
            }
            Ptr = Ptr->Next;
          }
          Anime->Next = GuiAnime;
        }

        GuiAnime = Anime;
      } else {
        GuiAnime = Anime; //first anime
      }
    }
  }

  // set file defaults in case they were not set
  Dict = GetProperty (DictPointer, "Icon");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Format");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString) && Dict2->string) {
      if (AsciiStriCmp (Dict2->string, "ICNS") == 0) {
        GlobalConfig.IconFormat = ICON_FORMAT_ICNS;
        //IconFormat = PoolPrint (L"%s", L"icns");
      } else if (AsciiStriCmp (Dict2->string, "PNG") == 0) {
        GlobalConfig.IconFormat = ICON_FORMAT_PNG;
        //IconFormat = PoolPrint (L"%s", L"png");
      }/* else if (AsciiStriCmp (Dict2->string, "BMP") == 0) {
        GlobalConfig.IconFormat = ICON_FORMAT_BMP;
        IconFormat = PoolPrint (L"%s", L"bmp");
      } else {
        GlobalConfig.IconFormat = ICON_FORMAT_DEF;
      }*/
    }
  }

  if (GlobalConfig.BackgroundName == NULL) {
    GlobalConfig.BackgroundName = GetIconsExt (L"background", L"png");
  }

  if (GlobalConfig.BannerFileName == NULL) {
    GlobalConfig.BannerFileName = GetIconsExt (L"logo", L"png");
  }

  if (GlobalConfig.SelectionSmallFileName == NULL) {
    GlobalConfig.SelectionSmallFileName = GetIconsExt (L"selection_small", L"png");
  }

  if (GlobalConfig.SelectionBigFileName == NULL) {
    GlobalConfig.SelectionBigFileName = GetIconsExt (L"selection_big", L"png");
  }

  if (GlobalConfig.SelectionIndicatorName == NULL) {
    GlobalConfig.SelectionIndicatorName = GetIconsExt (L"selection_indicator", L"png");
  }

  if (GlobalConfig.FontFileName == NULL) {
    GlobalConfig.FontFileName = GetIconsExt (L"font", L"png");
  }

  if (
    (GlobalConfig.TextColor == DefaultConfig.TextColor) ||
    (GlobalConfig.SelectionTextColor == DefaultConfig.SelectionTextColor)
  ) {
    GlobalConfig.Font = FONT_RAW;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
InitTheme (
  BOOLEAN     UseThemeDefinedInNVRam,
  EFI_TIME    *Time
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  UINTN         Size = 0, Rnd;
  TagPtr        ThemeDict = NULL;
  CHAR16        *TestTheme = NULL, *RndTheme;
  CHAR8         *NvramTheme = NULL;

  DbgHeader ("InitTheme");

  if (GlobalConfig.TextOnly) {
    if (GlobalConfig.Theme) {
      FreePool (GlobalConfig.Theme);
      GlobalConfig.Theme = NULL;
    }

    GlobalConfig.BootCampStyle = FALSE;
    goto PrepareFont;
  }

  Rnd = ((Time != NULL) && (OldChosenTheme != 0)) ? Time->Second % OldChosenTheme : 0;

  RndTheme = RandomTheme (Rnd);

  if (!aThemes || StriCmp (GlobalConfig.Theme, CONFIG_THEME_EMBEDDED) == 0) {
    goto finish;
  } else {
#if 0
    // Try special theme first
    if (Time != NULL) {
      if ((Time->Month == 12) && ((Time->Day >= 25) && (Time->Day <= 31))) {
        TestTheme = PoolPrint (CONFIG_THEME_CHRISTMAS);
      } else if ((Time->Month == 1) && ((Time->Day >= 1) && (Time->Day <= 3))) {
        TestTheme = PoolPrint (CONFIG_THEME_NEWYEAR);
      }

      if (TestTheme != NULL) {
        ThemeDict = LoadTheme (TestTheme);
        if (ThemeDict != NULL) {
          //DBG ("special theme %s found and %s parsed\n", TestTheme, PoolPrint (L"%s.plist", CONFIG_THEME_FILENAME));
          if (GlobalConfig.Theme) {
            FreePool (GlobalConfig.Theme);
          }

          GlobalConfig.Theme = PoolPrint (TestTheme);
        } /*else { // special theme not loaded
          DBG ("special theme %s not found, skipping\n", TestTheme, PoolPrint (L"%s.plist", CONFIG_THEME_FILENAME));
        }*/

        FreePool (TestTheme);
      }
    }
#endif

    // Try theme from nvram
    if ((ThemeDict == NULL) && UseThemeDefinedInNVRam) {
      NvramTheme = GetNvramVariable (L"Clover.Theme", &gEfiAppleBootGuid, NULL, &Size);
      if (NvramTheme != NULL) {
        TestTheme = PoolPrint (L"%a", NvramTheme);
        if (StriCmp (TestTheme, CONFIG_THEME_EMBEDDED) == 0) {
          goto finish;
        }

        if (StriCmp (TestTheme, CONFIG_THEME_RANDOM) == 0) {
          ThemeDict = LoadTheme (RndTheme);
          goto finish;
        }

        ThemeDict = LoadTheme (TestTheme);
        if (ThemeDict != NULL) {
          DBG ("Theme %s defined in NVRAM found and %s parsed\n", TestTheme, PoolPrint (L"%s.plist", CONFIG_THEME_FILENAME));
          if (GlobalConfig.Theme != NULL) {
            FreePool (GlobalConfig.Theme);
          }

          GlobalConfig.Theme = PoolPrint (TestTheme);
        } else { // theme from nvram not loaded
          if (GlobalConfig.Theme != NULL) {
            DBG ("Theme %s chosen from nvram is absent, using theme defined in config: %s\n", TestTheme, GlobalConfig.Theme);
          } else {
            DBG ("Theme %s chosen from nvram is absent, get first theme\n", TestTheme);
          }
        }

        FreePool (NvramTheme);
      }
    }

    // Try to get theme from settings
    if (ThemeDict == NULL) {
      if (GlobalConfig.Theme == NULL) {
        if (Time != NULL) {
          DBG ("No default theme, get random theme %s\n", RndTheme);
        } else {
          DBG ("No default theme, get first theme %s\n", RandomTheme (0));
        }
      } else {
        if (StriCmp (GlobalConfig.Theme, CONFIG_THEME_EMBEDDED) == 0) {
          goto finish;
        } else if (StriCmp (GlobalConfig.Theme, CONFIG_THEME_RANDOM) == 0) {
          ThemeDict = LoadTheme (RndTheme);
        } else {
          ThemeDict = LoadTheme (GlobalConfig.Theme);

          if (ThemeDict == NULL) {
            DBG ("GlobalConfig: %s not found, get random theme %s\n", PoolPrint (L"%s.plist", CONFIG_THEME_FILENAME), RndTheme);
            FreePool (GlobalConfig.Theme);
            GlobalConfig.Theme = NULL;
          }
        }
      }
    }

    // Try to get a theme
    if (ThemeDict == NULL) {
      ThemeDict = LoadTheme (RndTheme);
      if (ThemeDict != NULL) {
        GlobalConfig.Theme = PoolPrint (RndTheme);
      }
    }
  }

  finish:

  if (TestTheme!= NULL) {
    FreePool (TestTheme);
  }

  if (!ThemeDict) {  // No theme could be loaded, use embedded
    //DBG ("Using theme: embedded\n");
    GlobalConfig.Theme = NULL;
    if (ThemePath != NULL) {
      FreePool (ThemePath);
      ThemePath = NULL;
    }

    if (ThemeDir != NULL) {
      ThemeDir->Close (ThemeDir);
      ThemeDir = NULL;
    }

    GlobalConfig.Theme = PoolPrint (CONFIG_THEME_EMBEDDED);
    GetThemeTagSettings (NULL);
  } else { // theme loaded successfully
    // read theme settings
    TagPtr    DictPointer = GetProperty (ThemeDict, "Theme");

    if (DictPointer != NULL) {
      Status = GetThemeTagSettings (DictPointer);
      if (EFI_ERROR (Status)) {
        DBG ("Config theme error: %r\n", Status);
      }
    }

    FreeTag (ThemeDict);
  }

  SetThemeIndex ();

  PrepareFont:

  PrepareFont ();
  return Status;
}

VOID
GetListOfThemes () {
  EFI_STATUS        Status = EFI_NOT_FOUND;
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  CHAR16            *TestPath;
  EFI_FILE          *TestDir = NULL;
  CHAR8             *Ptr = NULL;
  UINTN             i = 0, y = 0, Size = 0;
  BOOLEAN           Found = FALSE;

  DbgHeader ("GetListOfThemes");

  DirIterOpen (SelfRootDir, DIR_THEMES, &DirIter);

  while (DirIterNext (&DirIter, 1, NULL, &DirEntry)) {
    if (
      (DirEntry->FileName[0] == '.') ||
      (StriCmp (DirEntry->FileName, CONFIG_THEME_EMBEDDED) == 0) ||
      (StriCmp (DirEntry->FileName, CONFIG_THEME_RANDOM) == 0)
    ) {
      continue;
    }

    MsgLog ("- [%02d]: %s", i++, DirEntry->FileName);

    TestPath = PoolPrint (L"%s\\%s", DIR_THEMES, DirEntry->FileName);
    if (TestPath != NULL) {
      Status = SelfRootDir->Open (SelfRootDir, &TestDir, TestPath, EFI_FILE_MODE_READ, 0);
      if (!EFI_ERROR (Status)) {
        Status = LoadFile (TestDir, PoolPrint (L"%s.plist", CONFIG_THEME_FILENAME), (UINT8 **)&Ptr, &Size);
        if (EFI_ERROR (Status) || (Ptr == NULL) || (Size == 0)) {
          DBG (" - bad, skip");
        } else {
          S_FILES   *aTmp = AllocateZeroPool (sizeof (S_FILES));

          aTmp->Index = OldChosenTheme = y;
          Found = TRUE;

          aTmp->FileName = PoolPrint (DirEntry->FileName);
          aTmp->Next = aThemes;
          aThemes = aTmp;

          MsgLog (" - added");

          y++;
        }
      }

      FreePool (TestPath);
    }

    MsgLog ("\n");
  }

  if (Found) {
    S_FILES   *aTmp, *Embedded = AllocateZeroPool (sizeof (S_FILES));

    Embedded->Index = OldChosenTheme = y;

    Embedded->FileName = PoolPrint (CONFIG_THEME_EMBEDDED);
    Embedded->Next = aThemes;
    aThemes = Embedded;

    aTmp = aThemes;
    aThemes = 0;

    while (aTmp) {
      S_FILES   *next = aTmp->Next;

      aTmp->Next = aThemes;
      aThemes = aTmp;
      aTmp = next;
    }
  }

  DirIterClose (&DirIter);
}

EFI_STATUS
GetEarlyUserSettings (
  IN EFI_FILE     *RootDir,
  TagPtr          CfgDict
) {
  EFI_STATUS    Status = EFI_SUCCESS;
  TagPtr        Dict, Dict2, DictPointer, Prop;

  gSettings.KextPatchesAllowed = TRUE;
  gSettings.KernelPatchesAllowed = TRUE;

  Dict = CfgDict;
  if (Dict != NULL) {
    //DBG ("Loading early settings\n");
    DbgHeader ("GetEarlyUserSettings");

    DictPointer = GetProperty (Dict, "Boot");
    if (DictPointer != NULL) {
      Prop = GetProperty (DictPointer, "Arguments");
      if ((Prop != NULL) && (Prop->type == kTagTypeString) && (Prop->string != NULL)) {
        AsciiStrnCpyS (gSettings.BootArgs, 255, Prop->string, AsciiStrLen (Prop->string));
      }

      Prop = GetProperty (DictPointer, "Timeout");
      if (Prop != NULL) {
        GlobalConfig.Timeout = (INT32)GetPropertyInteger (Prop, DefaultConfig.Timeout);
        DBG ("Timeout set to %d\n", GlobalConfig.Timeout);
      }

      GlobalConfig.FastBoot = IsPropertyTrue (GetProperty (DictPointer, "Fast"));

      GlobalConfig.NoEarlyProgress = IsPropertyTrue (GetProperty (DictPointer, "NoEarlyProgress"));

      // defaults if "DefaultVolume" is not present or is empty
      gSettings.LastBootedVolume = FALSE;
      //gSettings.DefaultVolume    = NULL;

      Prop = GetProperty (DictPointer, "DefaultVolume");
      if (Prop != NULL) {
        UINTN   Size = AsciiStrSize (Prop->string);

        if (Size > 0) {
          if (gSettings.DefaultVolume != NULL) { //override value from Boot Option
            FreePool (gSettings.DefaultVolume);
            gSettings.DefaultVolume = NULL;
          }

          // check for special value for remembering boot volume
          if (AsciiStriCmp (Prop->string, "LastBootedVolume") == 0) {
            gSettings.LastBootedVolume = TRUE;
          } else {
            UINTN   Len = (Size * sizeof (CHAR16));

            gSettings.DefaultVolume = AllocateZeroPool (Len);
            AsciiStrToUnicodeStrS (Prop->string, gSettings.DefaultVolume, Len);
          }
        }
      }

      Prop = GetProperty (DictPointer, "DefaultLoader");
      if (Prop != NULL) {
        UINTN   Len = (AsciiStrSize (Prop->string) * sizeof (CHAR16));

        gSettings.DefaultLoader = AllocateZeroPool (Len);
        AsciiStrToUnicodeStrS (Prop->string, gSettings.DefaultLoader, Len);
      }

      GlobalConfig.DebugLog = IsPropertyTrue (GetProperty (DictPointer, "Debug"));

      GlobalConfig.NeverHibernate = IsPropertyTrue (GetProperty (DictPointer, "NeverHibernate"));

      // XMP memory profiles
      Prop = GetProperty (DictPointer, "XMPDetection");
      if (Prop != NULL) {
        gSettings.XMPDetection = 0;

        if (Prop->type == kTagTypeFalse) {
          gSettings.XMPDetection = -1;
        } else if (Prop->type == kTagTypeString) {
          if (
            (Prop->string[0] == 'n') ||
            (Prop->string[0] == 'N') ||
            (Prop->string[0] == '-')
          ) {
            gSettings.XMPDetection = -1;
          } else {
            gSettings.XMPDetection = (INT8)AsciiStrDecimalToUintn (Prop->string);
          }
        } else if (Prop->type == kTagTypeInteger) {
          gSettings.XMPDetection   = (INT8)(UINTN)Prop->integer;//Prop->string;
        }

        // Check that the setting value is sane
        if ((gSettings.XMPDetection < -1) || (gSettings.XMPDetection > 2)) {
          gSettings.XMPDetection   = -1;
        }
      }
    }

    //*** SYSTEM ***

    DictPointer = GetProperty (Dict, "SystemParameters");
    if (DictPointer != NULL) {
      // Inject kexts
      gSettings.WithKexts = IsPropertyTrue (GetProperty (DictPointer, "InjectKexts"));

      // No caches
      gSettings.NoCaches = IsPropertyTrue (GetProperty (DictPointer, "NoCaches"));
    }

    // KernelAndKextPatches
    DictPointer = GetProperty (Dict, "KernelAndKextPatches");
    if (DictPointer != NULL) {
      FillinKextPatches (
        (KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF (SETTINGS_DATA, KernelAndKextPatches)),
        DictPointer
      );
    }

    DictPointer = GetProperty (Dict, "GUI");
    if (DictPointer != NULL) {
      GlobalConfig.TextOnly = IsPropertyTrue (GetProperty (DictPointer, "TextOnly"));

      if (!GlobalConfig.TextOnly) {
        Prop = GetProperty (DictPointer, "Theme");
        if (Prop != NULL) {
          if ((Prop->type == kTagTypeString) && Prop->string) {
            GlobalConfig.Theme = PoolPrint (L"%a", Prop->string);
            MsgLog ("Default theme: %s\n", GlobalConfig.Theme);
            SetThemeIndex ();
          }
        }
      }

      Prop = GetProperty (DictPointer, "ScreenResolution");
      if (Prop != NULL) {
        if ((Prop->type == kTagTypeString) && Prop->string) {
          GlobalConfig.ScreenResolution = PoolPrint (L"%a", Prop->string);
        }
      }

      // hide by name/uuid
      Prop = GetProperty (DictPointer, "Hide");
      if (Prop != NULL) {
        INTN   i, Count = GetTagCount (Prop);

        if (Count > 0) {
          gSettings.HVCount = 0;
          gSettings.HVHideStrings = AllocateZeroPool (Count * sizeof (CHAR16 *));

          if (gSettings.HVHideStrings) {
            for (i = 0; i < Count; i++) {
              if (EFI_ERROR (GetElement (Prop, i, &Dict2))) {
                continue;
              }

              if (Dict2 == NULL) {
                break;
              }

              if ((Dict2->type == kTagTypeString) && Dict2->string) {
                gSettings.HVHideStrings[gSettings.HVCount] = PoolPrint (L"%a", Dict2->string);

                if (gSettings.HVHideStrings[gSettings.HVCount]) {
                  DBG ("Hiding entries with string %s\n", gSettings.HVHideStrings[gSettings.HVCount]);
                  gSettings.HVCount++;
                }
              }
            }
          }
        }
      }

      gSettings.LinuxScan = TRUE;
      gSettings.AndroidScan = TRUE;
      gSettings.DisableEntryScan = FALSE;
      gSettings.DisableToolScan  = FALSE;

      // Disable loader scan
      Prop = GetProperty (DictPointer, "Scan");
      if (Prop != NULL) {
        if (Prop->type == kTagTypeDict) {
          gSettings.DisableEntryScan = !IsPropertyTrue (GetProperty (Prop, "Entries"));
          gSettings.DisableToolScan = !IsPropertyTrue (GetProperty (Prop, "Tool"));
          gSettings.LinuxScan = IsPropertyTrue (GetProperty (Prop, "Linux"));
          gSettings.AndroidScan = IsPropertyTrue (GetProperty (Prop, "Android"));
        } else if (!IsPropertyTrue (Prop)) {
          gSettings.DisableEntryScan = TRUE;
          gSettings.DisableToolScan  = TRUE;
        }
      }

      // Custom entries
      Dict2 = GetProperty (DictPointer, "Custom");
      if (Dict2 != NULL) {
        Prop = GetProperty (Dict2, "Entries");
        if (Prop != NULL) {
          CUSTOM_LOADER_ENTRY   *Entry;
          INTN                  i, Count = GetTagCount (Prop);
          TagPtr                Dict3;

          if (Count > 0) {
            for (i = 0; i < Count; i++) {
              if (EFI_ERROR (GetElement (Prop, i, &Dict3))) {
                continue;
              }

              if (Dict3 == NULL) {
                break;
              }

              // Allocate an entry
              Entry = (CUSTOM_LOADER_ENTRY *)AllocateZeroPool (sizeof (CUSTOM_LOADER_ENTRY));
              // Fill it in
              if ((Entry != NULL) && (!FillinCustomEntry (Entry, Dict3, FALSE) || !AddCustomEntry (Entry))) {
                FreePool (Entry);
              }
            }
          }
        }

        Prop = GetProperty (Dict2, "Tool");
        if (Prop != NULL) {
          CUSTOM_TOOL_ENTRY   *Entry;
          INTN                i, Count = GetTagCount (Prop);
          TagPtr              Dict3;

          if (Count > 0) {
            for (i = 0; i < Count; i++) {
              if (EFI_ERROR (GetElement (Prop, i, &Dict3))) {
                continue;
              }

              if (Dict3 == NULL) {
                break;
              }

              // Allocate an entry
              Entry = (CUSTOM_TOOL_ENTRY *)AllocateZeroPool (sizeof (CUSTOM_TOOL_ENTRY));
              if (Entry) {
                // Fill it in
                if (!FillinCustomTool (Entry, Dict3) || !AddCustomToolEntry (Entry)) {
                  FreePool (Entry);
                }
              }
            }
          }
        }
      }
    }

    DictPointer = GetProperty (Dict, "Graphics");
    if (DictPointer != NULL) {
      gSettings.NvidiaSingle = IsPropertyTrue (GetProperty (DictPointer, "NvidiaSingle"));

      //InjectEDID
      gSettings.InjectEDID = IsPropertyTrue (GetProperty (DictPointer, "InjectEDID"));

      Prop = GetProperty (DictPointer, "CustomEDID");
      if (Prop != NULL) {
        UINTN   j = 128;

        gSettings.CustomEDID = GetDataSetting (DictPointer, "CustomEDID", &j);

        if ((j % 128) != 0) {
          DBG ("CustomEDID has wrong length=%d\n", j);
        } else {
          DBG ("CustomEDID ok\n");
          InitializeEdidOverride ();
        }
      }
    }

    DictPointer = GetProperty (Dict, "DisableDrivers");
    if (DictPointer != NULL) {
      INTN   i, Count = GetTagCount (DictPointer);
      if (Count > 0) {
        gSettings.BlackListCount = 0;
        gSettings.BlackList = AllocateZeroPool (Count * sizeof (CHAR16 *));

        for (i = 0; i < Count; i++) {
          if (
              !EFI_ERROR (GetElement (DictPointer, i, &Prop)) &&
              (Prop != NULL) &&
              (Prop->type == kTagTypeString)
          ) {
            gSettings.BlackList[gSettings.BlackListCount++] = PoolPrint (L"%a", Prop->string);
          }
        }
      }
    }

    DictPointer = GetProperty (Dict, "RtVariables");
    if (DictPointer != NULL) {
      Prop = GetProperty (DictPointer, "ROM");
      if (Prop != NULL) {
        if (
          (AsciiStriCmp (Prop->string, "UseMacAddr0") == 0) ||
          (AsciiStriCmp (Prop->string, "UseMacAddr1") == 0)
        ) {
          GetLegacyLanAddress = TRUE;
        }
      }
    }

  }

  return Status;
}

VOID
ParseSMBIOSSettings (
  TagPtr    DictPointer
) {
  CHAR16    UStr[64];
  TagPtr    Prop;
  BOOLEAN   Default = FALSE;

  Prop = GetProperty (DictPointer, "ProductName");
  if (Prop != NULL) {
    MACHINE_TYPES   Model;

    AsciiStrCpyS (gSettings.ProductName, ARRAY_SIZE (gSettings.ProductName), Prop->string);
    // let's fill all other fields based on this ProductName
    // to serve as default
    Model = GetModelFromString (gSettings.ProductName);
    if (Model != MaxMachineType) {
      SetDMISettingsForModel (Model, FALSE);
      Default = TRUE;
    } else {
      //if new model then fill at least as MacPro3,1, except custom ProductName
      // something else?
      SetDMISettingsForModel (MacPro31, FALSE);
    }
  }

  Prop = GetProperty (DictPointer, "BiosVendor");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.VendorName, ARRAY_SIZE (gSettings.VendorName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BiosVersion");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.RomVersion, ARRAY_SIZE (gSettings.RomVersion), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BiosReleaseDate");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.ReleaseDate, ARRAY_SIZE (gSettings.ReleaseDate), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Manufacturer");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.ManufactureName, ARRAY_SIZE (gSettings.ManufactureName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Version");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.VersionNr, ARRAY_SIZE (gSettings.VersionNr), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Family");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.FamilyName, ARRAY_SIZE (gSettings.FamilyName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "SerialNumber");
  if (Prop != NULL) {
    ZeroMem (gSettings.SerialNr, ARRAY_SIZE (gSettings.SerialNr));
    AsciiStrCpyS (gSettings.SerialNr, ARRAY_SIZE (gSettings.SerialNr), Prop->string);
  }

  Prop = GetProperty (DictPointer, "SmUUID");
  if (Prop != NULL) {
    if (IsValidGuidAsciiString (Prop->string)) {
      AsciiStrToUnicodeStrS (Prop->string, (CHAR16 *)&UStr[0], 64);
      StrToGuidLE ((CHAR16 *)&UStr[0], &gSettings.SmUUID);
      gSettings.SmUUIDConfig = TRUE;
    } else {
      DBG ("Error: invalid SmUUID '%a' - should be in the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\n", Prop->string);
    }
  }

  Prop = GetProperty (DictPointer, "BoardManufacturer");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.BoardManufactureName, ARRAY_SIZE (gSettings.BoardManufactureName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BoardSerialNumber");
  if ((Prop != NULL) && (AsciiStrLen (Prop->string) > 0)) {
    AsciiStrCpyS (gSettings.BoardSerialNumber, ARRAY_SIZE (gSettings.BoardSerialNumber), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Board-ID");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.BoardNumber, ARRAY_SIZE (gSettings.BoardNumber), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BoardVersion");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.BoardVersion, ARRAY_SIZE (gSettings.BoardVersion), Prop->string);
  } else if (!Default) {
    AsciiStrCpyS (gSettings.BoardVersion, ARRAY_SIZE (gSettings.BoardVersion), gSettings.ProductName);
  }

  Prop = GetProperty (DictPointer, "BoardType");
  gSettings.BoardType = (UINT8)GetPropertyInteger (Prop, gSettings.BoardType);

  gSettings.Mobile = IsPropertyTrue (GetProperty (DictPointer, "Mobile"));
  if (!gSettings.Mobile && !Default) {
    gSettings.Mobile = (AsciiStrStr (gSettings.ProductName, "MacBook") != NULL);
  }

  Prop = GetProperty (DictPointer, "LocationInChassis");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.LocationInChassis, ARRAY_SIZE (gSettings.LocationInChassis), Prop->string);
  }

  Prop = GetProperty (DictPointer, "ChassisManufacturer");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.ChassisManufacturer, ARRAY_SIZE (gSettings.ChassisManufacturer), Prop->string);
  }

  Prop = GetProperty (DictPointer, "ChassisAssetTag");
  if (Prop != NULL) {
    AsciiStrCpyS (gSettings.ChassisAssetTag, ARRAY_SIZE (gSettings.ChassisAssetTag), Prop->string);
  }

  Prop = GetProperty (DictPointer, "ChassisType");
  if (Prop != NULL) {
    gSettings.ChassisType = (UINT8)GetPropertyInteger (Prop, gSettings.ChassisType);
    DBG ("ChassisType: 0x%x\n", gSettings.ChassisType);
  }

  //gFwFeatures = 0xC0001403 - by default
  Prop = GetProperty (DictPointer, "FirmwareFeatures");
  gFwFeatures = (UINT32)GetPropertyInteger (Prop, gFwFeatures);
}

//
//  Attempt to parse ROM string with "%" char
//

CHAR8 *
GetDataROM (
  IN CHAR8   *Str
) {
  CHAR8     *Tmp = NULL;
  INTN      i = 0, y = 0, x = 0;
  BOOLEAN   Found = FALSE;

  for (i = 0; Str[i]; i++) {
    if (Str[i] == '%') {
      Found = TRUE;
      x = 0;
      continue;
    }

    if (!y) {
      Tmp = AllocateZeroPool (64);
    }

    if (Found) {
      if (x < 2) {
        Tmp[y++] = Str[i];
        x++;
        continue;
      } else {
        Found = FALSE;
      }
    }

    AsciiStrCatS (Tmp, 64, Bytes2HexStr ((UINT8 *)Str + i, 1));
    y += 2;
  }

  return Tmp;
}

VOID
GetDefaultSettings () {
  MACHINE_TYPES   Model;
  //UINT64         msr = 0;

  //DbgHeader ("GetDefaultSettings");
  //DBG ("GetDefaultSettings\n");

  //gLanguage         = english;
  Model             = GetDefaultModel ();
  gSettings.CpuType = GetAdvancedCpuType ();

  SetDMISettingsForModel (Model, TRUE);

  //default values will be overritten by config.plist
  //use explicitly settings TRUE or FALSE (Yes or No)

  gSettings.InjectIntel          = (gGraphics[0].Vendor == Intel) || (gGraphics[1].Vendor == Intel);

  gSettings.InjectATI            = (((gGraphics[0].Vendor == Ati) && ((gGraphics[0].DeviceID & 0xF000) != 0x6000)) ||
                                    ((gGraphics[1].Vendor == Ati) && ((gGraphics[1].DeviceID & 0xF000) != 0x6000)));

  gSettings.InjectNVidia         = (((gGraphics[0].Vendor == Nvidia) && (gGraphics[0].Family < 0xE0)) ||
                                    ((gGraphics[1].Vendor == Nvidia) && (gGraphics[1].Family < 0xE0)));

  gSettings.GraphicsInjector     = gSettings.InjectATI || gSettings.InjectNVidia;
  //gSettings.CustomEDID           = NULL; //no sense to assign 0 as the structure is zeroed
  gSettings.DualLink             = 1;
  //gSettings.HDAInjection         = TRUE;
  gSettings.HDALayoutId          = 0;
  //gSettings.USBInjection         = TRUE; // enabled by default to have the same behavior as before

  StrCpyS (gSettings.DsdtName, ARRAY_SIZE (gSettings.DsdtName), L"DSDT.aml");

  gSettings.BacklightLevel       = 0xFFFF; //0x0503; -- the value from MBA52
  gSettings.BacklightLevelConfig = FALSE;
  gSettings.TrustSMBIOS          = TRUE;

  gSettings.InjectSystemID       = TRUE;
  gSettings.SmUUIDConfig         = FALSE;
  //gSettings.DefaultBackgroundColor = 0x80000000; //the value to delete the variable

  gSettings.RtROM                = NULL;
  gSettings.RtROMLen             = 0;
  gSettings.CsrActiveConfig      = 0xFFFF;
  gSettings.BooterConfig         = 0xFFFF;

  if (gCPUStructure.Model >= CPU_MODEL_IVY_BRIDGE) {
    gSettings.GeneratePStates    = TRUE;
    gSettings.GenerateCStates    = TRUE;
    //gSettings.EnableISS          = FALSE;
    //gSettings.EnableC2           = TRUE;
    //gSettings.EnableC6           = TRUE;
    gSettings.PluginType         = 1;

    if (gCPUStructure.Model == CPU_MODEL_IVY_BRIDGE) {
      gSettings.MinMultiplier    = 7;
    }

    //  gSettings.DoubleFirstState   = FALSE;
    gSettings.DropSSDT           = TRUE;
    gSettings.C3Latency          = 0x00FA;
  }

  gSettings.Turbo                = gCPUStructure.Turbo;
}

EFI_STATUS
GetUserSettings (
  IN  EFI_FILE    *RootDir,
  TagPtr          CfgDict
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  TagPtr        Dict, Dict2, Prop, Prop2, Prop3, DictPointer;

  Dict = CfgDict;
  if (Dict != NULL) {
    //DBG ("Loading main settings\n");
    DbgHeader ("GetUserSettings");

    // Boot settings.
    // Discussion. Why Arguments is here? It should be SystemParameters property!
    // we will read them again because of change in GUI menu. It is not only EarlySettings
    //
    DictPointer = GetProperty (Dict, "Boot");
    if (DictPointer != NULL) {
      Prop = GetProperty (DictPointer, "Arguments");
      //if (Prop != NULL && (Prop->type == kTagTypeString) && Prop->string != NULL) {
      if (
        (Prop != NULL) &&
        (Prop->type == kTagTypeString) &&
        (Prop->string != NULL) &&
        (AsciiStrStr (gSettings.BootArgs, Prop->string) == NULL)
      ) {
        UINTN   Len = ARRAY_SIZE (gSettings.BootArgs);

        AsciiStrnCpyS (gSettings.BootArgs, Len, Prop->string, Len - 1);
        //gBootArgsChanged = TRUE;
        //gBootChanged = TRUE;
      }
    }

    //Graphics

    DictPointer = GetProperty (Dict, "Graphics");
    if (DictPointer != NULL) {
      INTN    i;

      Dict2 = GetProperty (DictPointer, "Inject");
      if (Dict2 != NULL) {
        if (IsPropertyTrue (Dict2)) {
          gSettings.GraphicsInjector = TRUE;
          gSettings.InjectIntel      = TRUE;
          gSettings.InjectATI        = TRUE;
          gSettings.InjectNVidia     = TRUE;
        } else if (Dict2->type == kTagTypeDict) {
          gSettings.InjectIntel = IsPropertyTrue (GetProperty (Dict2, "Intel"));
          gSettings.InjectATI = IsPropertyTrue (GetProperty (Dict2, "ATI"));
          gSettings.InjectNVidia = IsPropertyTrue (GetProperty (Dict2, "NVidia"));
        } else {
          gSettings.GraphicsInjector = FALSE;
          gSettings.InjectIntel      = FALSE;
          gSettings.InjectATI        = FALSE;
          gSettings.InjectNVidia     = FALSE;
        }
      }

      Prop = GetProperty (DictPointer, "VRAM");
      gSettings.VRAM = LShiftU64 ((UINTN)GetPropertyInteger (Prop, (INTN)gSettings.VRAM), 20); //Mb -> bytes

      gSettings.LoadVBios = IsPropertyTrue (GetProperty (DictPointer, "LoadVBios"));

      for (i = 0; i < (INTN)NGFX; i++) {
        gGraphics[i].LoadVBios = gSettings.LoadVBios; //default
      }

      Prop = GetProperty (DictPointer, "VideoPorts");
      gSettings.VideoPorts = (UINT16)GetPropertyInteger (Prop, gSettings.VideoPorts);

      Prop = GetProperty (DictPointer, "BootDisplay");
      gSettings.BootDisplay = (INT8)GetPropertyInteger (Prop, -1);

      Prop = GetProperty (DictPointer, "FBName");
      if (Prop != NULL) {
        AsciiStrToUnicodeStrS (Prop->string, gSettings.FBName, ARRAY_SIZE (gSettings.FBName));
      }

      Prop = GetProperty (DictPointer, "NVCAP");
      if (Prop != NULL) {
        Hex2Bin (Prop->string, (UINT8 *)&gSettings.NVCAP[0], 20);
        DBG ("Read NVCAP: ");

        for (i = 0; i<20; i++) {
          DBG ("%02x", gSettings.NVCAP[i]);
        }

        DBG ("\n");
        //thus confirmed this procedure is working
      }

      Prop = GetProperty (DictPointer, "display-cfg");
      if (Prop != NULL) {
        Hex2Bin (Prop->string, (UINT8 *)&gSettings.Dcfg[0], 8);
      }

      Prop = GetProperty (DictPointer, "DualLink");
      gSettings.DualLink = (UINT32)GetPropertyInteger (Prop, gSettings.DualLink);

      gSettings.InjectEDID = IsPropertyTrue (GetProperty (DictPointer, "InjectEDID"));

      Prop = GetProperty (DictPointer, "CustomEDID");
      if (Prop != NULL) {
        UINTN j = 128;
        gSettings.CustomEDID = GetDataSetting (DictPointer, "CustomEDID", &j);
      }

      gSettings.NvidiaSingle = IsPropertyTrue (GetProperty (DictPointer, "NvidiaSingle"));

      Prop = GetProperty (DictPointer, "ig-platform-id");
      gSettings.IgPlatform = (UINT32)GetPropertyInteger (Prop, gSettings.IgPlatform);

      Prop = GetProperty (DictPointer, "snb-platform-id");
      gSettings.IgPlatform = (UINT32)GetPropertyInteger (Prop, gSettings.IgPlatform);

      FillCardList (DictPointer); //#@ Getcardslist
    }

    DictPointer = GetProperty (Dict, "Devices");
    if (DictPointer != NULL) {
      gSettings.IntelBacklight = IsPropertyTrue (GetProperty (DictPointer, "SetIntelBacklight"));
      gSettings.StringInjector = IsPropertyTrue (GetProperty (DictPointer, "Inject"));

      if (gSettings.StringInjector) {
        Prop = GetProperty (DictPointer, "Properties");
        if (Prop != NULL) {
          EFI_PHYSICAL_ADDRESS    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS; //0xFE000000;
          UINTN                   strlength   = AsciiStrLen (Prop->string);

          cDeviceProperties = AllocateZeroPool (strlength + 1);
          AsciiStrCpyS (cDeviceProperties, strlength + 1, Prop->string);
          //-------
          Status = gBS->AllocatePages (
                           AllocateMaxAddress,
                           EfiACPIReclaimMemory,
                           EFI_SIZE_TO_PAGES (strlength) + 1,
                           &BufferPtr
                         );

          if (!EFI_ERROR (Status)) {
            cProperties = (UINT8 *)(UINTN)BufferPtr;
            cPropSize   = (UINT32)(strlength >> 1);
            cPropSize   = Hex2Bin (cDeviceProperties, cProperties, cPropSize);
            DBG ("Injected EFIString of length %d\n", cPropSize);
          }
          //---------
        }
      }

      gSettings.NoDefaultProperties = IsPropertyTrue (GetProperty (DictPointer, "NoDefaultProperties"));

      Prop = GetProperty (DictPointer, "Arbitrary");
      if (Prop != NULL) {
        INTN            Index, Count = GetTagCount (Prop);
        DEV_PROPERTY    *DevProp;

        if (Count > 0) {
          DBG ("Add %d devices:\n", Count);
          for (Index = 0; Index < Count; Index++) {
            UINTN         DeviceAddr = 0U;
            EFI_STATUS    Status = GetElement (Prop, Index, &Prop2);

            DBG (" - [%02d]:", Index);

             if (EFI_ERROR (Status) || (Prop2 == NULL) ) {
              DBG (" %r parsing / empty element\n", Status);
              continue;
            }

            Dict2 = GetProperty (Prop2, "Comment");
            if (Dict2 != NULL) {
              DBG (" (%a)", Index, Dict2->string);
            }

            Dict2 = GetProperty (Prop2, "PciAddr");
            if (Dict2 != NULL) {
              INTN      Bus, Dev, Func;
              CHAR8     *Str = Dict2->string;

              if (Str[2] != ':') {
                DBG (" wrong PciAddr string: %a\n", Str);
                continue;
              }

              Bus   = HexStrToUint8 (Str);
              Dev   = HexStrToUint8 (&Str[3]);
              Func  = HexStrToUint8 (&Str[6]);
              DeviceAddr = PCIADDR (Bus, Dev, Func);

              DBG (" %02x:%02x.%02x\n", Bus, Dev, Func);
            }

            Dict2 = GetProperty (Prop2, "CustomProperties");
            if (Dict2 != NULL) {
              TagPtr      Dict3;
              INTN        PropIndex, PropCount = GetTagCount (Dict2);

              for (PropIndex = 0; PropIndex < PropCount; PropIndex++) {
                UINTN     Size = 0;

                if (!EFI_ERROR (GetElement (Dict2, PropIndex, &Dict3))) {
                  DevProp = gSettings.AddProperties;
                  gSettings.AddProperties = AllocateZeroPool (sizeof (DEV_PROPERTY));
                  gSettings.AddProperties->Next = DevProp;

                  gSettings.AddProperties->Device = (UINT32)DeviceAddr;

                  Prop3 = GetProperty (Dict3, "Key");
                  if (Prop3 && (Prop3->type == kTagTypeString) && Prop3->string) {
                    gSettings.AddProperties->Key = AllocateCopyPool (AsciiStrSize (Prop3->string), Prop3->string);
                  }

                  Prop3 = GetProperty (Dict3, "Value");
                  if (Prop3 && (Prop3->type == kTagTypeString) && Prop3->string) {
                    //first suppose it is Ascii string
                    gSettings.AddProperties->Value = AllocateCopyPool (AsciiStrSize (Prop3->string), Prop3->string);
                    gSettings.AddProperties->ValueLen = AsciiStrLen (Prop3->string) + 1;
                  } else if (Prop3 && (Prop3->type == kTagTypeInteger)) {
                    gSettings.AddProperties->Value = AllocatePool (4);
                    CopyMem (gSettings.AddProperties->Value, &(Prop3->integer), 4);
                    gSettings.AddProperties->ValueLen = 4;
                  } else {
                    //else  data
                    gSettings.AddProperties->Value = GetDataSetting (Dict3, "Value", &Size);
                    gSettings.AddProperties->ValueLen = Size;
                  }
                }
                // gSettings.NrAddProperties++;
              }   //for () device properties
            }
          } //for () devices
        }

        gSettings.NrAddProperties = 0xFFFE;
      } else { //can't use AddProperties with CustomProperties
        Prop = GetProperty (DictPointer, "AddProperties");
        if (Prop != NULL) {
          INTN    i, Count = GetTagCount (Prop), Index = 0;  //begin from 0 if second enter

          if (Count > 0) {
            DBG ("Add %d properties:\n", Count);
            gSettings.AddProperties = AllocateZeroPool (Count * sizeof (DEV_PROPERTY));

            for (i = 0; i < Count; i++) {
              UINTN         Size = 0;
              EFI_STATUS    Status = GetElement (Prop, i, &Dict2);

              DBG (" - [%02d]:", i);

              if (EFI_ERROR (Status) || (Dict2 == NULL)) {
                DBG (" %r parsing / empty element\n", Status);
                continue;
              }

              Prop2 = GetProperty (Dict2, "Device");
              if (Prop2 && (Prop2->type == kTagTypeString) && Prop2->string) {
                BOOLEAN       Found = FALSE;

                for (i = 0; i < OptDevicesBitNum; ++i) {
                  if (AsciiStriCmp (Prop2->string, ADEVICES[i].Title) == 0) {
                    gSettings.AddProperties[Index].Device = ADEVICES[i].Bit;
                    //DBG (" %a\n", ADEVICES[i].Title);
                    Found = TRUE;
                    break;
                  }
                }

                if (Found) {
                  DBG (" %a", Prop2->string);
                } else {
                  DBG (" unknown device, skip\n");
                  continue;
                }
              }

              //DBG (" %a", Prop2->string);

              Prop2 = GetProperty (Dict2, "Key");
              if (Prop2 && (Prop2->type == kTagTypeString) && Prop2->string) {
                gSettings.AddProperties[Index].Key = AllocateCopyPool (AsciiStrSize (Prop2->string), Prop2->string);
              }

              Prop2 = GetProperty (Dict2, "Value");
              if (Prop2 && (Prop2->type == kTagTypeString) && Prop2->string) {
                //first suppose it is Ascii string
                gSettings.AddProperties[Index].Value = AllocateCopyPool (AsciiStrSize (Prop2->string), Prop2->string);
                gSettings.AddProperties[Index].ValueLen = AsciiStrLen (Prop2->string) + 1;
              } else if (Prop2 && (Prop2->type == kTagTypeInteger)) {
                gSettings.AddProperties[Index].Value = AllocatePool (4);
                CopyMem (gSettings.AddProperties[Index].Value, &(Prop2->integer), 4);
                gSettings.AddProperties[Index].ValueLen = 4;
              } else {
                //else  data
                gSettings.AddProperties[Index].Value = GetDataSetting (Dict2, "Value", &Size);
                gSettings.AddProperties[Index].ValueLen = Size;
              }

              DBG (", len: %d\n", gSettings.AddProperties[Index].ValueLen);

              ++Index;
            }

            gSettings.NrAddProperties = Index;
          }
        }
      }

      Prop = GetProperty (DictPointer, "FakeID");
      if (Prop != NULL) {
        Prop2 = GetProperty (Prop, "ATI");
        if (Prop2 && (Prop2->type == kTagTypeString)) {
          gSettings.FakeATI  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
        }

        Prop2 = GetProperty (Prop, "NVidia");
        if (Prop2 && (Prop2->type == kTagTypeString)) {
          gSettings.FakeNVidia  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
        }

        Prop2 = GetProperty (Prop, "IntelGFX");
        if (Prop2 && (Prop2->type == kTagTypeString)) {
          gSettings.FakeIntel  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
        }

        Prop2 = GetProperty (Prop, "LAN");
        if (Prop2 && (Prop2->type == kTagTypeString)) {
          gSettings.FakeLAN  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
        }

        Prop2 = GetProperty (Prop, "WIFI");
        if (Prop2 && (Prop2->type == kTagTypeString)) {
          gSettings.FakeWIFI  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
        }

        Prop2 = GetProperty (Prop, "IMEI");
        if (Prop2 && (Prop2->type == kTagTypeString)) {
          gSettings.FakeIMEI  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
        }
      }

      gSettings.UseIntelHDMI = IsPropertyTrue (GetProperty (DictPointer, "UseIntelHDMI"));

      gSettings.HDALayoutId = 0;

      Prop2 = GetProperty (DictPointer, "Audio");
      if (Prop2 != NULL) {
        // HDA
        Prop = GetProperty (Prop2, "Inject");
        if (Prop != NULL) {
          if (Prop->type == kTagTypeInteger) {
            gSettings.HDALayoutId = (INT32)(UINTN)Prop->integer; //must be signed
          } else if (Prop->type == kTagTypeString) {
            if ((Prop->string[0] == '0') && ((Prop->string[1] == 'x') || (Prop->string[1] == 'X'))) {
              // assume it's a hex layout id
              gSettings.HDALayoutId = (INT32)AsciiStrHexToUintn (Prop->string);
            } else {
              // assume it's a decimal layout id
              gSettings.HDALayoutId = (INT32)AsciiStrDecimalToUintn (Prop->string);
            }
          }
        }
      }
    }

    //*** ACPI ***//

    DictPointer = GetProperty (Dict, "ACPI");
    if (DictPointer) {
      Prop = GetProperty (DictPointer, "DropTables");
      if (Prop) {
        INTN      i, Count = GetTagCount (Prop);
        BOOLEAN   Dropped;

        if (Count > 0) {
          MsgLog ("Dropping %d tables:\n", Count);

          for (i = 0; i < Count; i++) {
            UINT32        Signature = 0, TabLength = 0;
            UINT64        TableId = 0;
            EFI_STATUS    Status = GetElement (Prop, i, &Dict2);

            MsgLog (" - [%02d]:", i);

            if (EFI_ERROR (Status) || (Dict2 == NULL)) {
              MsgLog (" %r parsing / empty element\n", Status);
              continue;
            }

            // Get the table signatures to drop
            Prop2 = GetProperty (Dict2, "Signature");
            if (Prop2 && (Prop2->type == kTagTypeString) && Prop2->string) {
              CHAR8     s1 = 0, s2 = 0, s3 = 0, s4 = 0, *str = Prop2->string;

              MsgLog (" signature=\"");

              if (str) {
                if (*str) {
                  s1 = *str++;
                  MsgLog ("%c", s1);
                }
                if (*str) {
                  s2 = *str++;
                  MsgLog ("%c", s2);
                }
                if (*str) {
                  s3 = *str++;
                  MsgLog ("%c", s3);
                }
                if (*str) {
                  s4 = *str++;
                  MsgLog ("%c", s4);
                }
              }

              Signature = SIGNATURE_32 (s1, s2, s3, s4);
              DBG ("\" (%8.8X)", Signature);
            }
            // Get the table ids to drop
            Prop2 = GetProperty (Dict2, "TableId");
            if (Prop2 != NULL) {
              UINTN     IdIndex = 0;
              CHAR8     Id[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, *Str = Prop2->string;

              MsgLog (" table-id=\"");

              if (Str) {
                while (*Str && (IdIndex < 8)) {
                  MsgLog ("%c", *Str);
                  Id[IdIndex++] = *Str++;
                }
              }

              CopyMem (&TableId, (CHAR8 *)&Id[0], 8);
              MsgLog ("\" (%16.16lX)", TableId);
            }

            // Get the table len to drop
            Prop2 = GetProperty (Dict2, "Length");
            if (Prop2 != NULL) {
              TabLength = (UINT32)GetPropertyInteger (Prop2, 0);
              MsgLog (" length=%d (0x%x)", TabLength);
            }

            MsgLog ("\n");

            //set to drop
            if (gSettings.ACPIDropTables) {
              ACPI_DROP_TABLE   *DropTable = gSettings.ACPIDropTables;
              DBG ("         - set table: %08x, %16lx to drop:", Signature, TableId);
              Dropped = FALSE;

              while (DropTable) {
                if (
                    (
                      (Signature == DropTable->Signature) &&
                      (!TableId || (DropTable->TableId == TableId)) &&
                      (!TabLength || (DropTable->Length == TabLength))
                    ) ||
                    (!Signature && (DropTable->TableId == TableId))
                ) {
                  DropTable->MenuItem.BValue = TRUE;
                  gSettings.DropSSDT = FALSE; //if one item=true then dropAll=false by default
                  //DBG (" true");
                  Dropped = TRUE;
                }

                DropTable = DropTable->Next;
              }

              DBG (" %a\n", Dropped ? "yes" : "no");
            }
          }
        }
      }

      Dict2 = GetProperty (DictPointer, "DSDT");
      if (Dict2) {
        //gSettings.DsdtName by default is "DSDT.aml", but name "BIOS" will mean autopatch
        Prop = GetProperty (Dict2, "Name");
        if (Prop != NULL) {
          AsciiStrToUnicodeStrS (Prop->string, gSettings.DsdtName, ARRAY_SIZE (gSettings.DsdtName));
        }

        gSettings.DebugDSDT = IsPropertyTrue (GetProperty (Dict2, "Debug"));

        Prop = GetProperty (Dict2, "FixMask");
        gSettings.FixDsdt = (UINT32)GetPropertyInteger (Prop, gSettings.FixDsdt);

        Prop = GetProperty (Dict2, "Fixes");
        if (Prop != NULL) {
          //DBG ("Fixes will override DSDT fix mask %08x!\n", gSettings.FixDsdt);

          if (Prop->type == kTagTypeDict) {
            INTN    i;

            gSettings.FixDsdt = 0;

            for (i = 0; i < OptMenuDSDTBitNum; ++i) {
              if (IsPropertyTrue (GetProperty (Prop, OPT_MENU_DSDTBIT[i].OptLabel))) {
                gSettings.FixDsdt |= OPT_MENU_DSDTBIT[i].Bit;
              }
            }
          }

        }
        DBG (" - final DSDT Fix mask=%08x\n", gSettings.FixDsdt);

        Prop = GetProperty (Dict2, "Patches");
        if (Prop != NULL) {
          INTN   i, Count = GetTagCount (Prop);

          if (Count > 0) {
            gSettings.PatchDsdtNum     = (UINT32)Count;
            gSettings.PatchDsdtFind    = AllocateZeroPool (Count * sizeof (UINT8 *));
            gSettings.PatchDsdtReplace = AllocateZeroPool (Count * sizeof (UINT8 *));
            gSettings.LenToFind        = AllocateZeroPool (Count * sizeof (UINT32));
            gSettings.LenToReplace     = AllocateZeroPool (Count * sizeof (UINT32));

            MsgLog ("PatchesDSDT: %d requested\n", Count);

            for (i = 0; i < Count; i++) {
              UINTN   Size = 0;

              MsgLog (" - [%02d]:", i);

              Status     = GetElement (Prop, i, &Prop2);

              if (EFI_ERROR (Status) || (Prop2 == NULL)) {
                MsgLog (" %r parsing / empty element\n", Status);
                continue;
              }

              Prop3 = GetProperty (Prop2, "Comment");
              if ((Prop3 != NULL) && (Prop3->type == kTagTypeString) && Prop3->string) {
                MsgLog (" (%a)", Prop3->string);
              }

              if (IsPropertyTrue (GetProperty (Prop2, "Disabled"))) {
                MsgLog (" patch disabled, skip\n");
                continue;
              }

              //DBG (" DSDT bin patch #%d ", i);
              gSettings.PatchDsdtFind[i]    = GetDataSetting (Prop2, "Find",    &Size);
              MsgLog (" lenToFind: %d", Size);
              gSettings.LenToFind[i]        = (UINT32)Size;
              gSettings.PatchDsdtReplace[i] = GetDataSetting (Prop2, "Replace", &Size);
              MsgLog (", lenToReplace: %d\n", Size);
              gSettings.LenToReplace[i]     = (UINT32)Size;
            }
          } //if count > 0
        } //if prop PatchesDSDT

        gSettings.ReuseFFFF = IsPropertyTrue (GetProperty (Dict2, "ReuseFFFF"));

        gSettings.DropOEM_DSM = 0xFFFF;

        Prop   = GetProperty (Dict2, "DropOEM_DSM");

        if (Prop != NULL) {
          if (Prop->type == kTagTypeInteger) {
            gSettings.DropOEM_DSM = (UINT16)(UINTN)Prop->integer;
          } else if (Prop->type == kTagTypeDict) {
            INTN    i;

            gSettings.DropOEM_DSM = 0;

            for (i = 0; i < OptDevicesBitNum; ++i) {
              if (IsPropertyTrue (GetProperty (Prop, ADEVICES[i].Title))) {
                gSettings.DropOEM_DSM |= ADEVICES[i].Bit;
              }
            }
          } else if (!IsPropertyTrue (Prop)) {
            gSettings.DropOEM_DSM = 0;
          }
        }
      }

      Dict2 = GetProperty (DictPointer, "SSDT");
      if (Dict2) {
        Prop2 = GetProperty (Dict2, "Generate");
        if (Prop2 != NULL) {
          if (IsPropertyTrue (Prop2)) {
            gSettings.GeneratePStates = TRUE;
            gSettings.GenerateCStates = TRUE;
          } else if (Prop2->type == kTagTypeDict) {
            gSettings.GeneratePStates = IsPropertyTrue (GetProperty (Prop2, "PStates"));
            gSettings.GenerateCStates = IsPropertyTrue (GetProperty (Prop2, "CStates"));
          } else if (!IsPropertyTrue (Prop2)) {
            gSettings.GeneratePStates = FALSE;
            gSettings.GenerateCStates = FALSE;
          }
        }

        gSettings.DropSSDT  = IsPropertyTrue (GetProperty (Dict2, "DropOem"));

        gSettings.EnableC7 = IsPropertyTrue (GetProperty (Dict2, "EnableC7"));
        gSettings.EnableC6 = IsPropertyTrue (GetProperty (Dict2, "EnableC6"));
        gSettings.EnableC4 = IsPropertyTrue (GetProperty (Dict2, "EnableC4"));
        gSettings.EnableC2 = IsPropertyTrue (GetProperty (Dict2, "EnableC2"));

        gSettings.C3Latency = IsPropertyTrue (GetProperty (Dict2, "C3Latency"));

        gSettings.DoubleFirstState = IsPropertyTrue (GetProperty (Dict2, "DoubleFirstState"));

        Prop = GetProperty (Dict2, "MinMultiplier");
        if (Prop != NULL) {
          gSettings.MinMultiplier = (UINT8)GetPropertyInteger (Prop, gSettings.MinMultiplier);
          DBG ("MinMultiplier: %d\n", gSettings.MinMultiplier);
        }

        Prop = GetProperty (Dict2, "MaxMultiplier");
        if (Prop != NULL) {
          gSettings.MaxMultiplier = (UINT8)GetPropertyInteger (Prop, gSettings.MaxMultiplier);
          DBG ("MaxMultiplier: %d\n", gSettings.MaxMultiplier);
        }

        Prop = GetProperty (Dict2, "PluginType");
        if (Prop != NULL) {
          gSettings.PluginType = (UINT8)GetPropertyInteger (Prop, gSettings.PluginType);
          MsgLog ("PluginType: %d\n", gSettings.PluginType);
        }
      }

      gSettings.DropMCFG = IsPropertyTrue (GetProperty (DictPointer, "DropMCFG"));

      gSettings.smartUPS   = IsPropertyTrue (GetProperty (DictPointer, "smartUPS"));

      Prop = GetProperty (DictPointer, "SortedOrder");
      if (Prop) {
        INTN   i, Count = GetTagCount (Prop);

        Prop2 = NULL;

        if (Count > 0) {
          gSettings.SortedACPICount = 0;
          gSettings.SortedACPI = AllocateZeroPool (Count * sizeof (CHAR16 *));

          for (i = 0; i < Count; i++) {
            if (
              !EFI_ERROR (GetElement (Prop, i, &Prop2)) &&
              (Prop2 != NULL) &&
              (Prop2->type == kTagTypeString)
            ) {
              gSettings.SortedACPI[gSettings.SortedACPICount++] = PoolPrint (L"%a", Prop2->string);
            }
          }
        }
      }

      Prop = GetProperty (DictPointer, "DisabledAML");
      if (Prop) {
        INTN   i, Count = GetTagCount (Prop);

        Prop2 = NULL;

        if (Count > 0) {
          gSettings.DisabledAMLCount = 0;
          gSettings.DisabledAML = AllocateZeroPool (Count * sizeof (CHAR16 *));

          if (gSettings.DisabledAML) {
            for (i = 0; i < Count; i++) {
              if (
                !EFI_ERROR (GetElement (Prop, i, &Prop2)) &&
                (Prop2 != NULL) &&
                (Prop2->type == kTagTypeString)
              ) {
                gSettings.DisabledAML[gSettings.DisabledAMLCount++] = PoolPrint (L"%a", Prop2->string);
              }
            }
          }
        }
      }
    }

    //*** SMBIOS ***//

    DictPointer = GetProperty (Dict, "SMBIOS");
    if (DictPointer != NULL) {
      ParseSMBIOSSettings (DictPointer);

      gSettings.TrustSMBIOS = IsPropertyTrue (GetProperty (DictPointer, "Trust"));

      // Inject memory tables into SMBIOS
      Prop = GetProperty (DictPointer, "Memory");
      if (Prop != NULL){
        // Get memory table count
        Prop2   = GetProperty (Prop, "SlotCount");
        gRAM.UserInUse = (UINT8)GetPropertyInteger (Prop2, 0);

        // Get memory channels
        Prop2 = GetProperty (Prop, "Channels");
        gRAM.UserChannels = (UINT8)GetPropertyInteger (Prop2, 0);

        // Get memory tables
        Prop2 = GetProperty (Prop, "Modules");
        if (Prop2 != NULL) {
          INTN    i, Count = GetTagCount (Prop2);

          Prop3 = NULL;

          for (i = 0; i < Count; i++) {
            UINT8           Slot = MAX_RAM_SLOTS;
            RAM_SLOT_INFO   *SlotPtr;

            if (EFI_ERROR (GetElement (Prop2, i, &Prop3))) {
              continue;
            }

            if (Prop3 == NULL) {
              break;
            }

            // Get memory slot
            Dict2 = GetProperty (Prop3, "Slot");
            if (Dict2 == NULL) {
              continue;
            }

            if ((Dict2->type == kTagTypeString) && Dict2->string) {
              Slot = (UINT8)AsciiStrDecimalToUintn (Dict2->string);
            } else if (Dict2->type == kTagTypeInteger) {
              Slot = (UINT8)(UINTN)Dict2->integer;
            } else {
              continue;
            }

            if (Slot >= MAX_RAM_SLOTS) {
              continue;
            }

            SlotPtr = &gRAM.User[Slot];

            // Get memory size
            Dict2 = GetProperty (Prop3, "Size");
            SlotPtr->ModuleSize = (UINT32)GetPropertyInteger (Dict2, SlotPtr->ModuleSize);

            // Get memory frequency
            Dict2 = GetProperty (Prop3, "Frequency");
            SlotPtr->Frequency = (UINT32)GetPropertyInteger (Dict2, SlotPtr->Frequency);

            // Get memory vendor
            Dict2 = GetProperty (Prop3, "Vendor");
            if (Dict2 && (Dict2->type == kTagTypeString) && (Dict2->string != NULL)) {
              SlotPtr->Vendor = Dict2->string;
            }

            // Get memory part number
            Dict2 = GetProperty (Prop3, "Part");
            if (Dict2 && (Dict2->type == kTagTypeString) && (Dict2->string != NULL)) {
              SlotPtr->PartNo = Dict2->string;
            }

            // Get memory serial number
            Dict2 = GetProperty (Prop3, "Serial");
            if (Dict2 && (Dict2->type == kTagTypeString) && (Dict2->string != NULL)) {
              SlotPtr->SerialNo = Dict2->string;
            }

            // Get memory type
            SlotPtr->Type = MemoryTypeDdr3;
            Dict2 = GetProperty (Prop3, "Type");
            if (Dict2 && Dict2->type == kTagTypeString && Dict2->string != NULL) {
              if (AsciiStriCmp (Dict2->string, "DDR2") == 0) {
                SlotPtr->Type = MemoryTypeDdr2;
              } else if (AsciiStriCmp (Dict2->string, "DDR3") == 0) {
                SlotPtr->Type = MemoryTypeDdr3;
              } else if (AsciiStriCmp (Dict2->string, "DDR4") == 0) {
                SlotPtr->Type = MemoryTypeDdr4;
              } else if (AsciiStriCmp (Dict2->string, "DDR") == 0) {
                SlotPtr->Type = MemoryTypeDdr;
              }
            }

            SlotPtr->InUse = (SlotPtr->ModuleSize > 0);
            if (SlotPtr->InUse) {
              if (gRAM.UserInUse <= Slot) {
                gRAM.UserInUse = Slot + 1;
              }
            }
          }

          if (gRAM.UserInUse > 0) {
            gSettings.InjectMemoryTables = TRUE;
          }
        }
      }

      Prop = GetProperty (DictPointer, "Slots");
      if (Prop != NULL) {
        INTN    DeviceN, Index, Count = GetTagCount (Prop);

        Prop3 = NULL;

        for (Index = 0; Index < Count; ++Index) {
          if (EFI_ERROR (GetElement (Prop, Index, &Prop3))) {
            continue;
          }

          if (Prop3 == NULL) {
            break;
          }

          if (!Index) {
            DBG ("Slots->Device:");
          }

          Prop2 = GetProperty (Prop3, "Device");
          DeviceN = -1;
          if (Prop2 && (Prop2->type == kTagTypeString) && Prop2->string) {
            if (AsciiStriCmp (Prop2->string,        "ATI") == 0) {
              DeviceN = 0;
            } else if (AsciiStriCmp (Prop2->string, "NVidia") == 0) {
              DeviceN = 1;
            } else if (AsciiStriCmp (Prop2->string, "IntelGFX") == 0) {
              DeviceN = 2;
            } else if (AsciiStriCmp (Prop2->string, "LAN") == 0) {
              DeviceN = 5;
            } else if (AsciiStriCmp (Prop2->string, "WIFI") == 0) {
              DeviceN = 6;
            } else if (AsciiStriCmp (Prop2->string, "Firewire") == 0) {
              DeviceN = 12;
            } else if (AsciiStriCmp (Prop2->string, "HDMI") == 0) {
              DeviceN = 4;
            } else if (AsciiStriCmp (Prop2->string, "USB") == 0) {
              DeviceN = 11;
            } else if (AsciiStriCmp (Prop2->string, "NVME") == 0) {
              DeviceN = 13;
            } else {
              DBG (" - add properties to unknown device %a, ignored\n", Prop2->string);
              continue;
            }
          } else {
            DBG (" - no device  property for slot\n");
            continue;
          }

          if (DeviceN >= 0) {
            SLOT_DEVICE   *SlotDevice = &SlotDevices[DeviceN];

            Prop2 = GetProperty (Prop3, "ID");
            SlotDevice->SlotID = (UINT8)GetPropertyInteger (Prop2, DeviceN);
            SlotDevice->SlotType = SlotTypePci;

            Prop2 = GetProperty (Prop3, "Type");
            if (Prop2 != NULL) {
              switch ((UINT8)GetPropertyInteger (Prop2, 0)) {
                case 0:
                  SlotDevice->SlotType = SlotTypePci;
                  break;

                case 1:
                  SlotDevice->SlotType = SlotTypePciExpressX1;
                  break;

                case 2:
                  SlotDevice->SlotType = SlotTypePciExpressX2;
                  break;

                case 4:
                  SlotDevice->SlotType = SlotTypePciExpressX4;
                  break;

                case 8:
                  SlotDevice->SlotType = SlotTypePciExpressX8;
                  break;

                case 16:
                  SlotDevice->SlotType = SlotTypePciExpressX16;
                  break;

                default:
                  SlotDevice->SlotType = SlotTypePciExpress;
                  break;
              }
            }

            Prop2 = GetProperty (Prop3, "Name");
            if (Prop2 && (Prop2->type == kTagTypeString) && Prop2->string) {
              AsciiSPrint (SlotDevice->SlotName, 31, "%a", Prop2->string);
            } else {
              AsciiSPrint (SlotDevice->SlotName, 31, "PCI Slot %d", DeviceN);
            }

            DBG (" - %a\n", SlotDevice->SlotName);
          }
        }
      }

      Prop = GetProperty (DictPointer, "PlatformFeature");
      gSettings.PlatformFeature = (UINT64)GetPropertyInteger (Prop, 0xFFFF);
    }

    //CPU

    DictPointer = GetProperty (Dict, "CPU");
    if (DictPointer != NULL) {
      Prop = GetProperty (DictPointer, "QPI");
      if (Prop != NULL) {
        gSettings.QPI = (UINT16)GetPropertyInteger (Prop, (INTN)gCPUStructure.ProcessorInterconnectSpeed);
        if (gSettings.QPI == 0) { //this is not default, this is zero!
          gSettings.QPI = 0xFFFF;
          DBG ("QPI: 0 disable table132\n");
        } else {
          DBG ("QPI: %dMHz\n", gSettings.QPI);
        }
      }

      Prop = GetProperty (DictPointer, "FrequencyMHz");
      if (Prop != NULL) {
        gSettings.CpuFreqMHz = (UINT32)GetPropertyInteger (Prop, gSettings.CpuFreqMHz);
        DBG ("CpuFreq: %dMHz\n", gSettings.CpuFreqMHz);
      }

      Prop = GetProperty (DictPointer, "Type");
      gSettings.CpuType = GetAdvancedCpuType ();
      if (Prop != NULL) {
        gSettings.CpuType = (UINT16)GetPropertyInteger (Prop, gSettings.CpuType);
        DBG ("CpuType: %x\n", gSettings.CpuType);
      }

      gSettings.QEMU = IsPropertyTrue (GetProperty (DictPointer, "QEMU"));

      //Prop = GetProperty (DictPointer, "UseARTFrequency");
      gSettings.UseARTFreq = IsPropertyTrue (GetProperty (DictPointer, "UseARTFrequency"));

      gSettings.UserBusSpeed = FALSE;
      Prop = GetProperty (DictPointer, "BusSpeedkHz");
      if (Prop != NULL) {
        gSettings.BusSpeed = (UINT32)GetPropertyInteger (Prop, gSettings.BusSpeed);
        DBG ("BusSpeed: %dkHz\n", gSettings.BusSpeed);
        gSettings.UserBusSpeed = TRUE;
      }

      gSettings.EnableC7 = IsPropertyTrue (GetProperty (DictPointer, "C7"));
      gSettings.EnableC6 = IsPropertyTrue (GetProperty (DictPointer, "C6"));
      gSettings.EnableC4 = IsPropertyTrue (GetProperty (DictPointer, "C4"));
      gSettings.EnableC2 = IsPropertyTrue (GetProperty (DictPointer, "C2"));

      //Usually it is 0x03e9, but if you want Turbo, you may set 0x00FA
      Prop = GetProperty (DictPointer, "Latency");
      gSettings.C3Latency = (UINT16)GetPropertyInteger (Prop, gSettings.C3Latency);

      if (IsPropertyTrue (GetProperty (DictPointer, "TurboDisable"))) {
        UINT64  msr = AsmReadMsr64 (MSR_IA32_MISC_ENABLE);

        gSettings.Turbo = 0;
        msr &= ~(1ULL<<38);
        AsmWriteMsr64 (MSR_IA32_MISC_ENABLE, msr);
      }
    }

    // RtVariables
    DictPointer = GetProperty (Dict, "RtVariables");
    if (DictPointer != NULL) {
      // ROM: <data>bin data</data> or <string>base 64 encoded bin data</string>
      Prop = GetProperty (DictPointer, "ROM");
      if (Prop != NULL) {
        if (AsciiStriCmp (Prop->string, "UseMacAddr0") == 0) {
          gSettings.RtROM = &gLanMac[0][0];
          gSettings.RtROMLen = 6;
        } else if (AsciiStriCmp (Prop->string, "UseMacAddr1") == 0) {
          gSettings.RtROM = &gLanMac[1][0];
          gSettings.RtROMLen = 6;
        } else {
          UINTN   ROMLength = 0;

          if (AsciiStrStr (Prop->string, "%") != NULL) {
            gSettings.RtROM = StringDataToHex (GetDataROM (Prop->string), &ROMLength);
          } else {
            gSettings.RtROM = GetDataSetting (DictPointer, "ROM", &ROMLength);
          }
          gSettings.RtROMLen = ROMLength;
        }

        if ((gSettings.RtROM == NULL) || (gSettings.RtROMLen == 0)) {
          gSettings.RtROM = NULL;
          gSettings.RtROMLen = 0;
        }
      }

      // MLB: <string>some value</string>
      Prop = GetProperty (DictPointer, "MLB");
      if ((Prop != NULL) && (AsciiStrLen (Prop->string) > 0)) {
        gSettings.RtMLB = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
      }

      // CsrActiveConfig
      Prop = GetProperty (DictPointer, "CsrActiveConfig");
      gSettings.CsrActiveConfig = (UINT32)GetPropertyInteger (Prop, 0x67); //the value 0xFFFF means not set

      //BooterConfig
      Prop = GetProperty (DictPointer, "BooterConfig");
      gSettings.BooterConfig = (UINT16)GetPropertyInteger (Prop, 0xFFFF); //the value 0xFFFF means not set
    }

    if (gSettings.RtROM == NULL) {
      gSettings.RtROM = (UINT8 *)&gSettings.SmUUID.Data4[2];
      gSettings.RtROMLen = 6;
    }

    if (gSettings.RtMLB == NULL) {
      gSettings.RtMLB = &gSettings.BoardSerialNumber[0];
    }

    // if CustomUUID and InjectSystemID are not specified
    // then use InjectSystemID=TRUE and SMBIOS UUID
    // to get Chameleon's default behaviour (to make user's life easier)
    CopyMem ((VOID *)&gUuid, (VOID *)&gSettings.SmUUID, sizeof (EFI_GUID));
    //gSettings.InjectSystemID = TRUE;

    // SystemParameters again - values that can depend on previous params
    DictPointer = GetProperty (Dict, "SystemParameters");
    if (DictPointer != NULL) {
      //BacklightLevel
      Prop = GetProperty (DictPointer, "BacklightLevel");
      if (Prop != NULL) {
        gSettings.BacklightLevel = (UINT16)GetPropertyInteger (Prop, gSettings.BacklightLevel);
        gSettings.BacklightLevelConfig = TRUE;
      }

      Prop = GetProperty (DictPointer, "CustomUUID");
      if (Prop != NULL) {

        if (IsValidGuidAsciiString (Prop->string)) {
          AsciiStrToUnicodeStrS (Prop->string, gSettings.CustomUuid, ARRAY_SIZE (gSettings.CustomUuid));
          Status = StrToGuidLE (gSettings.CustomUuid, &gUuid);

          if (!EFI_ERROR (Status)) {
            // if CustomUUID specified, then default for InjectSystemID=FALSE
            // to stay compatibile with previous Clover behaviour
            gSettings.InjectSystemID = FALSE;
          }
        }

        if (gSettings.InjectSystemID) {
          DBG ("Error: invalid CustomUUID '%a' - should be in the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\n", Prop->string);
        }
      }
      //else gUuid value from SMBIOS

      if (gSettings.InjectSystemID) {
        gSettings.InjectSystemID = IsPropertyTrue (GetProperty (DictPointer, "InjectSystemID"));
      }
    }

    // KernelAndKextPatches
    if (gBootChanged) {
      DictPointer = GetProperty (Dict, "KernelAndKextPatches");
      if (DictPointer != NULL) {
        FillinKextPatches ((KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF (SETTINGS_DATA, KernelAndKextPatches)), DictPointer);
      }
    }

    if (gThemeChanged /* && GlobalConfig.Theme */) {
      gTextOnly = FALSE;
      DictPointer = GetProperty (Dict, "GUI");
      if (DictPointer != NULL) {
        gTextOnly = IsPropertyTrue (GetProperty (DictPointer, "TextOnly"));

        if (!gTextOnly) {
          Prop = GetProperty (DictPointer, "Theme");
          if ((Prop != NULL) && (Prop->type == kTagTypeString) && Prop->string) {
            FreePool (GlobalConfig.Theme);
            GlobalConfig.Theme = PoolPrint (L"%a", Prop->string);
            DBG ("Theme from new config: %s\n", GlobalConfig.Theme);
          }
        }
      }
    }

    SaveSettings ();
  }

  //DBG ("config.plist read and return %r\n", Status);
  return EFI_SUCCESS;
}

EFI_STATUS
LoadUserSettings (
  IN EFI_FILE   *RootDir,
  IN CHAR16     *ConfName,
  TagPtr        *Dict
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  UINTN         Size = 0;
  CHAR8         *gConfigPtr = NULL;
  CHAR16        *ConfigPlistPath, *ConfigOemPath;

  //DbgHeader ("LoadUserSettings");

  // load config
  if ((ConfName == NULL) || (Dict == NULL)) {
    //DBG ("Can't load plist in LoadUserSettings: NULL\n");
    return EFI_NOT_FOUND;
  }

  ConfigPlistPath = PoolPrint (L"%s\\%s.plist", DIR_CLOVER, ConfName);
  ConfigOemPath   = PoolPrint (L"%s\\%s.plist", OEMPath, ConfName);

  if (FileExists (SelfRootDir, ConfigOemPath)) {
    Status = LoadFile (SelfRootDir, ConfigOemPath, (UINT8 **)&gConfigPtr, &Size);
    DBG ("Load plist: '%s' ... %r\n", ConfigOemPath, Status);
  }

  if (EFI_ERROR (Status)) {
    if ((RootDir != NULL) && FileExists (RootDir, ConfigPlistPath)) {
      Status = LoadFile (RootDir, ConfigPlistPath, (UINT8 **)&gConfigPtr, &Size);
      DBG ("Load plist: '%s' from RootDir ... %r\n", ConfigPlistPath, Status);
    }

    if (EFI_ERROR (Status)) {
      Status = LoadFile (SelfRootDir, ConfigPlistPath, (UINT8 **)&gConfigPtr, &Size);
      DBG ("Load plist: '%s' from SelfRootDir ... %r\n", ConfigPlistPath, Status);
    }
    //DBG ("Using %s.plist at path: %s", ConfName, ConfigPlistPath);
  }

  if (!EFI_ERROR (Status) && (gConfigPtr != NULL)) {
    Status = ParseXML (gConfigPtr, Dict, (UINT32)Size);

    DBG ("Parsing plist: ... %r\n", Status);
  }

  return Status;
}

CHAR8 *
GetOSVersion (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  CHAR8         *OSVersion  = NULL, *PlistBuffer = NULL;
  UINTN         PlistLen, i = 0;
  TagPtr        Dict = NULL,  Prop = NULL;
  BOOLEAN       OSINSTALLER_VER = FALSE;

  if (!Entry || !Entry->Volume  || !Entry->LoaderType || !OSTYPE_IS_OSX_GLOB (Entry->LoaderType)) {
    return OSVersion;
  }

  if (OSTYPE_IS_OSX (Entry->LoaderType)) {
    // Detect exact version for Mac OS X Regular/Server
    while ((SystemPlists[i] != NULL) && !FileExists (Entry->Volume->RootDir, SystemPlists[i])) {
      i++;
    }

    if (SystemPlists[i] != NULL) { // found OSX System
      Status = LoadFile (Entry->Volume->RootDir, SystemPlists[i], (UINT8 **)&PlistBuffer, &PlistLen);

      if (!EFI_ERROR (Status) && (PlistBuffer != NULL) && !EFI_ERROR (ParseXML (PlistBuffer, &Dict, 0))) {
        Prop = GetProperty (Dict, "ProductVersion");
        if ((Prop != NULL) && (Prop->string != NULL) && (Prop->string[0] != '\0')) {
          OSVersion = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
        }

        Prop = GetProperty (Dict, "ProductBuildVersion");
        if ((Prop != NULL) && (Prop->string != NULL) && (Prop->string[0] != '\0')) {
          Entry->BuildVersion = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
        }
      }
    }
  }

  if (OSTYPE_IS_OSX_INSTALLER (Entry->LoaderType)) {
    // Detect exact version for 2nd stage Installer (thanks to dmazar for this idea)
    // This should work for most installer cases. Rest cases will be read from boot.efi before booting.

    /*
      CHAR16 *IABootFilesSystemVersion = L"\\.IABootFilesSystemVersion.plist";
      if (FileExists (Entry->Volume->RootDir, IABootFilesSystemVersion)) {
        Status = LoadFile (Entry->Volume->RootDir, IABootFilesSystemVersion, (UINT8 **)&PlistBuffer, &PlistLen);
        if (!EFI_ERROR (Status) && PlistBuffer != NULL && !EFI_ERROR (ParseXML (PlistBuffer, &Dict, 0))) {
          Prop = GetProperty (Dict, "ProductVersion");
          if (Prop != NULL && Prop->string != NULL && Prop->string[0] != '\0') {
            OSVersion = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
            OSINSTALLER_VER = TRUE;
          }
          Prop = GetProperty (Dict, "ProductBuildVersion");
          if (Prop != NULL && Prop->string != NULL && Prop->string[0] != '\0') {
            Entry->BuildVersion = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
          }
        }
      }
    */

    if (!OSINSTALLER_VER) {
      CHAR16 *InstallerPlist = L"\\.IABootFiles\\com.apple.Boot.plist";

      if (FileExists (Entry->Volume->RootDir, InstallerPlist)) {
        Status = LoadFile (Entry->Volume->RootDir, InstallerPlist, (UINT8 **)&PlistBuffer, &PlistLen);
        if (!EFI_ERROR (Status) && PlistBuffer != NULL && !EFI_ERROR (ParseXML (PlistBuffer, &Dict, 0))) {
          Prop = GetProperty (Dict, "Kernel Flags");
          if (Prop != NULL && Prop->string != NULL && Prop->string[0] != '\0') {
            if (AsciiStrStr (Prop->string, "Sierra") || AsciiStrStr (Prop->string, "10.12")) {
              OSVersion = AllocateCopyPool (6, "10.12");
            } else if (AsciiStrStr (Prop->string, "Capitan") || AsciiStrStr (Prop->string, "10.11")) {
              OSVersion = AllocateCopyPool (6, "10.11");
            } else if (AsciiStrStr (Prop->string, "Yosemite") || AsciiStrStr (Prop->string, "10.10")) {
              OSVersion = AllocateCopyPool (6, "10.10");
            } else if (AsciiStrStr (Prop->string, "Mavericks") || AsciiStrStr (Prop->string, "10.9")) {
              OSVersion = AllocateCopyPool (5, "10.9");
            } else if (AsciiStrStr (Prop->string, "Mountain") || AsciiStrStr (Prop->string, "10.8")) {
              OSVersion = AllocateCopyPool (5, "10.8");
            } else if (AsciiStrStr (Prop->string, "Lion") || AsciiStrStr (Prop->string, "10.7")) {
              OSVersion = AllocateCopyPool (5, "10.7");
            }
          }
        }
      }
    }
  }

  if (OSTYPE_IS_OSX_RECOVERY (Entry->LoaderType)) {
    // Detect exact version for OS X Recovery
    CHAR16    *RecoveryPlist = L"\\com.apple.recovery.boot\\SystemVersion.plist";

    if (FileExists (Entry->Volume->RootDir, RecoveryPlist)) {
      Status = LoadFile (Entry->Volume->RootDir, RecoveryPlist, (UINT8 **)&PlistBuffer, &PlistLen);
      if (!EFI_ERROR (Status) && (PlistBuffer != NULL) && !EFI_ERROR (ParseXML (PlistBuffer, &Dict, 0))) {
        Prop = GetProperty (Dict, "ProductVersion");
        if (Prop != NULL && Prop->string != NULL && Prop->string[0] != '\0') {
          OSVersion = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
        }

        Prop = GetProperty (Dict, "ProductBuildVersion");
        if ((Prop != NULL) && (Prop->string != NULL) && (Prop->string[0] != '\0')) {
          Entry->BuildVersion = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
        }
      }
    }
  }

  if (PlistBuffer != NULL) {
    FreePool (PlistBuffer);
  }

  return OSVersion;
}

CHAR16 *
GetOSIconName (
  IN CHAR8    *OSVersion
) {
  CHAR16  *OSIconName;

  if (OSVersion == NULL) {
    OSIconName = L"mac";
  } else if (AsciiStrStr (OSVersion, "10.12") != NULL) {
    // Sierra
    OSIconName = L"sierra,mac";
  } else if (AsciiStrStr (OSVersion, "10.11") != NULL) {
    // El Capitan
    OSIconName = L"cap,mac";
  } else if (AsciiStrStr (OSVersion, "10.10") != NULL) {
    // Yosemite
    OSIconName = L"yos,mac";
  } else if (AsciiStrStr (OSVersion, "10.9") != NULL) {
    // Mavericks
    OSIconName = L"mav,mac";
  } else if (AsciiStrStr (OSVersion, "10.8") != NULL) {
    // Mountain Lion
    OSIconName = L"cougar,mac";
  } else if (AsciiStrStr (OSVersion, "10.7") != NULL) {
    // Lion
    OSIconName = L"lion,mac";
  } else {
    OSIconName = L"mac";
  }

  return OSIconName;
}

//Get the UUID of the AppleRaid or CoreStorage volume from the boot helper partition
EFI_STATUS
GetRootUUID (
  IN  REFIT_VOLUME    *Volume
) {
  EFI_STATUS    Status;
  CHAR8         *PlistBuffer;
  CHAR16        Uuid[40], *SystemPlistR, *SystemPlistP, *SystemPlistS;
  UINTN         PlistLen;
  TagPtr        Dict, Prop;
  BOOLEAN       HasRock, HasPaper, HasScissors;

  Status = EFI_NOT_FOUND;
  if (Volume == NULL) {
    return Status;
  }

  SystemPlistR = L"\\com.apple.boot.R\\Library\\Preferences\\SystemConfiguration\\com.apple.Boot.plist";
  HasRock      = FileExists (Volume->RootDir,     SystemPlistR);

  SystemPlistP = L"\\com.apple.boot.P\\Library\\Preferences\\SystemConfiguration\\com.apple.Boot.plist";
  HasPaper     = FileExists (Volume->RootDir,    SystemPlistP);

  SystemPlistS = L"\\com.apple.boot.S\\Library\\Preferences\\SystemConfiguration\\com.apple.Boot.plist";
  HasScissors  = FileExists (Volume->RootDir, SystemPlistS);

  PlistBuffer = NULL;
  // Playing Rock, Paper, Scissors to chose which settings to load.
  if (HasRock && HasPaper && HasScissors) {
    // Rock wins when all three are around
    Status = LoadFile (Volume->RootDir, SystemPlistR, (UINT8 **)&PlistBuffer, &PlistLen);
  } else if (HasRock && HasPaper) {
    // Paper beats rock
    Status = LoadFile (Volume->RootDir, SystemPlistP, (UINT8 **)&PlistBuffer, &PlistLen);
  } else if (HasRock && HasScissors) {
    // Rock beats scissors
    Status = LoadFile (Volume->RootDir, SystemPlistR, (UINT8 **)&PlistBuffer, &PlistLen);
  } else if (HasPaper && HasScissors) {
    // Scissors beat paper
    Status = LoadFile (Volume->RootDir, SystemPlistS, (UINT8 **)&PlistBuffer, &PlistLen);
  } else if (HasPaper) {
    // No match
    Status = LoadFile (Volume->RootDir, SystemPlistP, (UINT8 **)&PlistBuffer, &PlistLen);
  } else if (HasScissors) {
    // No match
    Status = LoadFile (Volume->RootDir, SystemPlistS, (UINT8 **)&PlistBuffer, &PlistLen);
  } else {
    // Rock wins by default
    Status = LoadFile (Volume->RootDir, SystemPlistR, (UINT8 **)&PlistBuffer, &PlistLen);
  }

  if (!EFI_ERROR (Status)) {
    Dict = NULL;
    if (EFI_ERROR (ParseXML (PlistBuffer, &Dict, 0))) {
      FreePool (PlistBuffer);
      return EFI_NOT_FOUND;
    }

    Prop = GetProperty (Dict, "Root UUID");
    if (Prop != NULL) {
      AsciiStrToUnicodeStrS (Prop->string, Uuid, ARRAY_SIZE (Uuid));
      Status = StrToGuidLE (Uuid, &Volume->RootUUID);
    }

    FreePool (PlistBuffer);
  }

  return Status;
}

VOID
GetDevices () {
  EFI_STATUS              Status;
  UINTN                   Index, Segment = 0, Bus = 0, Device = 0, Function = 0, HandleCount  = 0;
  EFI_HANDLE              *HandleArray = NULL;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  PCI_TYPE00              Pci;
  UINT32                  Bar0;
  SLOT_DEVICE             *SlotDevice;

  NGFX = 0;
  //Arpt.Valid = FALSE; //global variables initialized by 0 - c-language

  DbgHeader ("GetDevices");

  gSettings.HasGraphics->Intel = FALSE;
  gSettings.HasGraphics->Nvidia = FALSE;
  gSettings.HasGraphics->Ati = FALSE;

  // Scan PCI handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleArray
                );

  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < HandleCount; ++Index) {
      Status = gBS->HandleProtocol (HandleArray[Index], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
      if (!EFI_ERROR (Status)) {
        // Read PCI BUS
        PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        Status = PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              sizeof (Pci) / sizeof (UINT32),
                              &Pci
                            );

        MsgLog ("PCI (%02x|%02x:%02x.%02x) : %04x %04x class=%02x%02x%02x\n",
          Segment,
          Bus,
          Device,
          Function,
          Pci.Hdr.VendorId,
          Pci.Hdr.DeviceId,
          Pci.Hdr.ClassCode[2],
          Pci.Hdr.ClassCode[1],
          Pci.Hdr.ClassCode[0]
        );

        // GFX

        if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_DISPLAY) &&
          (
            (Pci.Hdr.ClassCode[1] == (PCI_CLASS_DISPLAY_VGA)) ||
            (Pci.Hdr.ClassCode[1] == (PCI_CLASS_DISPLAY_OTHER))
          ) &&
          (NGFX < 4)
        ) {
          GFX_PROPERTIES      *gfx = &gGraphics[NGFX];

          gfx->DeviceID            = Pci.Hdr.DeviceId;
          gfx->Segment             = Segment;
          gfx->Bus                 = Bus;
          gfx->Device              = Device;
          gfx->Function            = Function;
          gfx->Handle              = HandleArray[Index];

          MsgLog (" - GFX");

          switch (Pci.Hdr.VendorId) {
            case 0x1002:
              //info        = NULL;
              gfx->Vendor = Ati;

              //do {
              //  info      = &radeon_cards[i];
              //  if (info->device_id == Pci.Hdr.DeviceId) {
              //    break;
              //  }
              //} while (radeon_cards[i++].device_id != 0);

              //AsciiSPrint (gfx->Model,  64, "%a", info->model_name);
              //AsciiSPrint (gfx->Config, 64, "%a", card_configs[info->cfg_name].name);
              //gfx->Ports                  = card_configs[info->cfg_name].ports;
              GetAtiModel (&gfx[0], Pci.Hdr.DeviceId);
              //DBG (" (ATI/AMD) %a\n", gfx->Model);
              MsgLog (" (ATI/AMD)\n");

              SlotDevice                    = &SlotDevices[0];
              SlotDevice->SegmentGroupNum   = (UINT16)Segment;
              SlotDevice->BusNum            = (UINT8)Bus;
              SlotDevice->DevFuncNum        = (UINT8)((Device << 3) | (Function & 0x07));
              SlotDevice->Valid             = TRUE;
              AsciiSPrint (SlotDevice->SlotName, 31, "PCI Slot 0");
              SlotDevice->SlotID            = 1;
              SlotDevice->SlotType          = SlotTypePciExpressX16;

              gSettings.HasGraphics->Ati    = TRUE;
              break;

            case 0x8086:
              gfx->Vendor                 = Intel;
              gfx->Ports                  = 1;
              //AsciiSPrint (gfx->Model, 64, "%a", get_gma_model (Pci.Hdr.DeviceId));
              //DBG (" (Intel) %a\n", gfx->Model);
              MsgLog (" (Intel)\n");
              /*
                SlotDevice                  = &SlotDevices[2];
                SlotDevice->SegmentGroupNum = (UINT16)Segment;
                SlotDevice->BusNum          = (UINT8)Bus;
                SlotDevice->DevFuncNum      = (UINT8)((Device << 4) | (Function & 0x0F));
                SlotDevice->Valid           = TRUE;
                AsciiSPrint (SlotDevice->SlotName, 31, "PCI Slot 0");
                SlotDevice->SlotID          = 0;
                SlotDevice->SlotType        = SlotTypePciExpressX16;
              */

              gSettings.HasGraphics->Intel  = TRUE;
              break;

            case 0x10de:
              gfx->Vendor = Nvidia;
              Bar0        = Pci.Device.Bar[0];
              gfx->Mmio   = (UINT8 *)(UINTN)(Bar0 & ~0x0f);
              //DBG ("BAR: 0x%p\n", Mmio);
              // get card type
              gfx->Family = (REG32 (gfx->Mmio, 0) >> 20) & 0x1ff;

              //AsciiSPrint (
              //  gfx->Model,
              //  64,
              //  "%a",
              //  get_nvidia_model (
              //    ((Pci.Hdr.VendorId << 16) | Pci.Hdr.DeviceId),
              //    ((Pci.Device.SubsystemVendorID << 16) | Pci.Device.SubsystemID),
              //    NULL //NULL: get from generic lists
              //  )
              //);

              //DBG (" (Nvidia) %a\n", gfx->Model);
              MsgLog (" (Nvidia)\n");
              gfx->Ports                    = 0;

              SlotDevice                    = &SlotDevices[1];
              SlotDevice->SegmentGroupNum   = (UINT16)Segment;
              SlotDevice->BusNum            = (UINT8)Bus;
              SlotDevice->DevFuncNum        = (UINT8)((Device << 3) | (Function & 0x07));
              SlotDevice->Valid             = TRUE;
              AsciiSPrint (SlotDevice->SlotName, 31, "PCI Slot 0");
              SlotDevice->SlotID            = 1;
              SlotDevice->SlotType          = SlotTypePciExpressX16;

              gSettings.HasGraphics->Nvidia = TRUE;
              break;

            default:
              gfx->Vendor = Unknown;
              AsciiSPrint (gfx->Model, 64, "pci%x,%x", Pci.Hdr.VendorId, Pci.Hdr.DeviceId);
              gfx->Ports  = 1;
              //DBG (" (Unknown) %a\n", gfx->Model);
              MsgLog (" (Unknown)\n");
              break;
          }

          NGFX++;
        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_OTHER)
        ) {
          SlotDevice                  = &SlotDevices[6];
          SlotDevice->SegmentGroupNum = (UINT16)Segment;
          SlotDevice->BusNum          = (UINT8)Bus;
          SlotDevice->DevFuncNum      = (UINT8)((Device << 3) | (Function & 0x07));
          SlotDevice->Valid           = TRUE;
          AsciiSPrint (SlotDevice->SlotName, 31, "Airport");
          SlotDevice->SlotID          = 0;
          SlotDevice->SlotType        = SlotTypePciExpressX1;

          MsgLog (" - WIFI\n");
        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_ETHERNET)
        ) {
          SlotDevice                  = &SlotDevices[5];
          SlotDevice->SegmentGroupNum = (UINT16)Segment;
          SlotDevice->BusNum          = (UINT8)Bus;
          SlotDevice->DevFuncNum      = (UINT8)((Device << 3) | (Function & 0x07));
          SlotDevice->Valid           = TRUE;
          AsciiSPrint (SlotDevice->SlotName, 31, "Ethernet");
          SlotDevice->SlotID          = 2;
          SlotDevice->SlotType        = SlotTypePciExpressX1;
          gLanVendor[nLanCards]       = Pci.Hdr.VendorId;
          Bar0                        = Pci.Device.Bar[0];
          gLanMmio[nLanCards++]       = (UINT8 *)(UINTN)(Bar0 & ~0x0f);

          MsgLog (" - LAN\n");

          if (nLanCards >= 4) {
            DBG (" - [!] too many LAN card in the system (upto 4 limit exceeded), overriding the last one\n");
            nLanCards = 3; // last one will be rewritten
          }
        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_SERIAL) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_SERIAL_FIREWIRE)
        ) {
          SlotDevice = &SlotDevices[12];
          SlotDevice->SegmentGroupNum = (UINT16)Segment;
          SlotDevice->BusNum          = (UINT8)Bus;
          SlotDevice->DevFuncNum      = (UINT8)((Device << 3) | (Function & 0x07));
          SlotDevice->Valid           = TRUE;
          AsciiSPrint (SlotDevice->SlotName, 31, "Firewire");
          SlotDevice->SlotID          = 3;
          SlotDevice->SlotType        = SlotTypePciExpressX4;

          DBG (" - SERIAL\n");
        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_MEDIA) &&
          (
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_HDA) ||
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_AUDIO)
          )
        ) {
          MsgLog (" - AUDIO");
          if (IsHDMIAudio (HandleArray[Index])) {
            MsgLog (" (HDMI)");
          //if ((Pci.Hdr.VendorId == 0x1002) || (Pci.Hdr.VendorId == 0x10DE)){
            SlotDevice = &SlotDevices[4];
            SlotDevice->SegmentGroupNum = (UINT16)Segment;
            SlotDevice->BusNum          = (UINT8)Bus;
            SlotDevice->DevFuncNum      = (UINT8)((Device << 3) | (Function & 0x07));
            SlotDevice->Valid           = TRUE;
            AsciiSPrint (SlotDevice->SlotName, 31, "HDMI port");
            SlotDevice->SlotID          = 5;
            SlotDevice->SlotType        = SlotTypePciExpressX4;
          }

          MsgLog ("\n");
        }
      }
    }
  }
}

VOID
SetDevices (
  LOADER_ENTRY    *Entry
) {
  EFI_STATUS              Status;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  PCI_TYPE00              Pci;
  EFI_HANDLE              *HandleBuffer;
  pci_dt_t                PCIdevice;
  UINTN                   HandleCount, i, Segment, Bus, Device, Function;
  BOOLEAN                 StringDirty = FALSE, TmpDirty = FALSE;

  DbgHeader ("SetDevices");

  MsgLog ("NoDefaultProperties: %a\n", gSettings.NoDefaultProperties ? "Yes" : "No");

  devices_number = 1; //should initialize for reentering GUI
  // Scan PCI handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );

  if (!EFI_ERROR (Status)) {
    for (i = 0; i < HandleCount; i++) {
      Status = gBS->HandleProtocol (HandleBuffer[i], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
      if (!EFI_ERROR (Status)) {
        // Read PCI BUS
        Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, 0, sizeof (Pci) / sizeof (UINT32), &Pci);
        if (EFI_ERROR (Status)) {
          continue;
        }

        Status                               = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        PCIdevice.DeviceHandle               = HandleBuffer[i];
        PCIdevice.dev.addr                   = (UINT32)PCIADDR (Bus, Device, Function);
        PCIdevice.vendor_id                  = Pci.Hdr.VendorId;
        PCIdevice.device_id                  = Pci.Hdr.DeviceId;
        PCIdevice.revision                   = Pci.Hdr.RevisionID;
        PCIdevice.subclass                   = Pci.Hdr.ClassCode[0];
        PCIdevice.class_id                   = *((UINT16 *)(Pci.Hdr.ClassCode+1));
        PCIdevice.subsys_id.subsys.vendor_id = Pci.Device.SubsystemVendorID;
        PCIdevice.subsys_id.subsys.device_id = Pci.Device.SubsystemID;

        if (gSettings.NrAddProperties == 0xFFFE) {
          DEV_PROPERTY      *Prop = gSettings.AddProperties;
          DevPropDevice     *device = NULL;
          BOOLEAN           Once = TRUE;

          MsgLog ("Inject CustomProperties: \n");

          if (!string) {
            string = DevpropCreateString ();
          }

          while (Prop) {
            if (Prop->Device != PCIdevice.dev.addr) {
              Prop = Prop->Next;
              continue;
            }

            if (Once) {
              device = DevpropAddDevicePci (string, &PCIdevice);
              Once = FALSE;
            }

            DevpropAddValue (device, Prop->Key, (UINT8 *)Prop->Value, Prop->ValueLen);
            StringDirty = TRUE;
            Prop = Prop->Next;
          }

          MsgLog (" for device %02x:%02x.%02x injected, continue\n", Bus, Device, Function);
          continue;
        }

        // GFX
        if (
          /* gSettings.GraphicsInjector && */
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_DISPLAY) &&
          (
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_VGA) ||
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_OTHER)
          )
        ) {
          UINT32  LevelW = 0xC0000000, IntelDisable = 0x03;

          MsgLog ("Inject Display:");

          //        gGraphics.DeviceID = Pci.Hdr.DeviceId;

          switch (Pci.Hdr.VendorId) {
            case 0x1002:
              MsgLog (" ATI/AMD\n");

              if (gSettings.InjectATI) {
                //can't do this in one step because of C-conventions
                TmpDirty    = SetupAtiDevprop (Entry, &PCIdevice);
                StringDirty |=  TmpDirty;
              } else {
                DBG (" - injection not set\n");
              }
              break;

            case 0x8086:
              MsgLog (" Intel\n");
              if (gSettings.InjectIntel) {
                TmpDirty    = SetupGmaDevprop (&PCIdevice);
                StringDirty |=  TmpDirty;
                //MsgLog (" - Intel GFX revision  =0x%x\n", PCIdevice.revision);
              } else {
                DBG (" - injection not set\n");
              }

              if (gSettings.IntelBacklight) {
                /*Status = */PciIo->Mem.Write (
                      PciIo,
                      EfiPciIoWidthUint32,
                      0,
                      0xC8250,
                      1,
                      &LevelW
                    );
              }

              if (gSettings.FakeIntel == 0x00008086) {
                PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, 0x50, 1, &IntelDisable);
              }
              break;

            case 0x10de:
              MsgLog (" Nvidia\n");
              if (gSettings.InjectNVidia) {
                TmpDirty    = SetupNvidiaDevprop (&PCIdevice);
                StringDirty |=  TmpDirty;
              } else {
                DBG (" - injection not set\n");
              }
              break;

            default:
              MsgLog (" Unknown\n");
              break;
          }
        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_ETHERNET)
        ) {
          //MsgLog ("Ethernet device found\n");
          if (!(gSettings.FixDsdt & FIX_LAN)) {
            MsgLog ("Inject LAN:\n");

            TmpDirty = SetupEthernetDevprop (&PCIdevice);
            StringDirty |=  TmpDirty;
          }
        }

        // HDA
        else if (
          //gSettings.HDAInjection &&
          (gSettings.HDALayoutId > 0) &&
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_MEDIA) &&
          (
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_HDA) ||
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_AUDIO)
          )
        ) {
          //no HDMI injection
          if (
            (Pci.Hdr.VendorId != 0x1002) &&
            (Pci.Hdr.VendorId != 0x10de)
          ) {
            MsgLog ("Inject HDA:\n");
            TmpDirty    = SetupHdaDevprop (PciIo, &PCIdevice/*, Entry->OSVersion */);
            StringDirty |= TmpDirty;
          }
        }
      }
    }
  }

  if (StringDirty) {
    EFI_PHYSICAL_ADDRESS    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS; //0xFE000000;

    stringlength = string->length * 2;

    //DBG ("stringlength = %d\n", stringlength);
    // gDeviceProperties            = AllocateAlignedPages EFI_SIZE_TO_PAGES (stringlength + 1), 64);

    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiACPIReclaimMemory,
                    EFI_SIZE_TO_PAGES (stringlength + 1),
                    &BufferPtr
                  );

    if (!EFI_ERROR (Status)) {
      mProperties       = (UINT8 *)(UINTN)BufferPtr;
      gDeviceProperties = (VOID *)DevpropGenerateString (string);
      gDeviceProperties[stringlength] = 0;
      //     DBG (gDeviceProperties);
      //     DBG ("\n");
      //     StringDirty = FALSE;
      //-------
      mPropSize = (UINT32)AsciiStrLen (gDeviceProperties) / 2;
      //     DBG ("Preliminary size of mProperties=%d\n", mPropSize);
      mPropSize = Hex2Bin (gDeviceProperties, mProperties, mPropSize);
      //     DBG ("Final size of mProperties=%d\n", mPropSize);
      //---------
    }
  }

  //MsgLog ("CurrentMode: Width=%d Height=%d\n", UGAWidth, UGAHeight);
}

EFI_STATUS
SaveSettings () {
  // TODO: SetVariable ()..
  // here we can apply user settings instead of default one
  gMobile = gSettings.Mobile;

  if (
    (gSettings.BusSpeed != 0) &&
    (gSettings.BusSpeed > 10 * kilo) &&
    (gSettings.BusSpeed < 500 * kilo)
  ) {
    gCPUStructure.ExternalClock = gSettings.BusSpeed;
    gCPUStructure.FSBFrequency  = MultU64x64 (gSettings.BusSpeed, kilo); //kHz -> Hz
    gCPUStructure.MaxSpeed      = (UINT32)(DivU64x32 ((UINT64)gSettings.BusSpeed * gCPUStructure.MaxRatio, 10000)); //kHz->MHz
  }

  if (
    (gSettings.CpuFreqMHz > 100) &&
    (gSettings.CpuFreqMHz < 20000)
  ) {
    gCPUStructure.MaxSpeed = gSettings.CpuFreqMHz;
  }

  gCPUStructure.CPUFrequency = MultU64x64 (gCPUStructure.MaxSpeed, Mega);

  return EFI_SUCCESS;
}

CHAR16 *
GetOtherKextsDir () {
  CHAR16    *SrcDir = PoolPrint (DIR_KEXTS_OTHER, OEMPath);

  if (!FileExists (SelfVolume->RootDir, SrcDir)) {
    FreePool (SrcDir);
    SrcDir = PoolPrint (DIR_KEXTS_OTHER, DIR_CLOVER);
    if (!FileExists (SelfVolume->RootDir, SrcDir)) {
      FreePool (SrcDir);
      SrcDir = NULL;
    }
  }

  return SrcDir;
}

//dmazar
CHAR16 *
GetOSVersionKextsDir (
  CHAR8   *OSVersion
) {
  CHAR16    *SrcDir;
  CHAR8     FixedVersion[6], *DotPtr;

  if (OSVersion != NULL) {
    AsciiStrnCpyS (FixedVersion, ARRAY_SIZE (FixedVersion), OSVersion, 5);
    // OSVersion may contain minor version too (can be 10.x or 10.x.y)
    if ((DotPtr = AsciiStrStr (FixedVersion, ".")) != NULL) {
      DotPtr = AsciiStrStr (DotPtr+1, "."); // second dot
    }

    if (DotPtr != NULL) {
      *DotPtr = 0;
    }
  }

  //MsgLog ("OS=%s\n", OSTypeStr);

  // find source injection folder with kexts
  // note: we are just checking for existance of particular folder, not checking if it is empty or not
  // check OEM subfolders: version specific or default to Other
  //SrcDir = PoolPrint (L"%s\\kexts\\%a", OEMPath, FixedVersion);
  //SrcDir = PoolPrint (DIR_KEXTS, OEMPath);
  SrcDir = AllocateZeroPool (SVALUE_MAX_SIZE);
  StrCpyS (SrcDir, SVALUE_MAX_SIZE, PoolPrint (DIR_KEXTS, OEMPath));
  StrCatS (SrcDir, SVALUE_MAX_SIZE, PoolPrint (L"\\%a", FixedVersion));

  if (!FileExists (SelfVolume->RootDir, SrcDir)) {
    FreePool (SrcDir);
    //SrcDir = PoolPrint (L"%s\\%a", DIR_KEXTS, FixedVersion);
    //SrcDir = PoolPrint (DIR_KEXTS, DIR_CLOVER);
    //StrCat (SrcDir, PoolPrint (L"\\%a", FixedVersion));
    SrcDir = AllocateZeroPool (SVALUE_MAX_SIZE);
    StrCpyS (SrcDir, SVALUE_MAX_SIZE, PoolPrint (DIR_KEXTS, DIR_CLOVER));
    StrCatS (SrcDir, SVALUE_MAX_SIZE, PoolPrint (L"\\%a", FixedVersion));

    if (!FileExists (SelfVolume->RootDir, SrcDir)) {
      FreePool (SrcDir);
      SrcDir = NULL;
    }
  }

  return SrcDir;
}

EFI_STATUS
SetFSInjection (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS              Status;
  REFIT_VOLUME            *Volume;
  FSINJECTION_PROTOCOL    *FSInject;
  CHAR16                  *SrcDir = NULL;
  FSI_STRING_LIST         *Blacklist = 0, *ForceLoadKexts = NULL;
  UINTN                   Index = 0;

  DbgHeader ("FSInjection");

  Volume = Entry->Volume;

  // get FSINJECTION_PROTOCOL
  Status = gBS->LocateProtocol (&gFSInjectProtocolGuid, NULL, (VOID **)&FSInject);
  if (EFI_ERROR (Status)) {
    //Print (L"- No FSINJECTION_PROTOCOL, Status = %r\n", Status);
    MsgLog (" - ERROR: gFSInjectProtocolGuid not found!\n");
    return EFI_NOT_STARTED;
  }

  // check if blocking of caches is needed
  if (OSFLAG_ISSET (Entry->Flags, OSFLAG_NOCACHES)) {
    MsgLog (" - Blocking kext caches\n");
    // add caches to blacklist
    Blacklist = FSInject->CreateStringList ();
    if (Blacklist == NULL) {
      MsgLog (" - ERROR: Not enough memory!\n");
      return EFI_NOT_STARTED;
    }

    while (Index < OsxPathLCachesCount) {
      FSInject->AddStringToList (Blacklist, OsxPathLCaches[Index++]);
    }

    if (gSettings.BlockKexts[0] != L'\0') {
      FSInject->AddStringToList (Blacklist, PoolPrint (L"%s\\%s", OSX_PATH_SLE, gSettings.BlockKexts));
    }
  }

  // check if kext injection is needed
  // (will be done only if caches are blocked or if boot.efi refuses to load kernelcache)
  //SrcDir = NULL;
  if (OSFLAG_ISSET (Entry->Flags, OSFLAG_WITHKEXTS)) {
    SrcDir = GetOtherKextsDir ();
    Status = FSInject->Install (
                          Volume->DeviceHandle,
                          OSX_PATH_SLE,
                          SelfVolume->DeviceHandle,
                          SrcDir,
                          Blacklist,
                          ForceLoadKexts
                        );

    MsgLog (" - FSInjection, Src: %s, Status: %r\n", SrcDir, Status);
    FreePool (SrcDir);

    SrcDir = GetOSVersionKextsDir (Entry->OSVersion);
    Status = FSInject->Install (
                          Volume->DeviceHandle,
                          OSX_PATH_SLE,
                          SelfVolume->DeviceHandle,
                          SrcDir,
                          Blacklist,
                          ForceLoadKexts
                        );

    MsgLog (" - FSInjection, Src: %s, Status: %r\n", SrcDir, Status);
    FreePool (SrcDir);
  } else {
    MsgLog (" - Skipping kext injection (not requested)\n");
  }

  // prepare list of kext that will be forced to load
  ForceLoadKexts = FSInject->CreateStringList ();
  if (ForceLoadKexts == NULL) {
    MsgLog (" - Error: not enough memory!\n");
    return EFI_NOT_STARTED;
  }

  KextPatcherRegisterKexts (FSInject, ForceLoadKexts, Entry);

  // reinit Volume->RootDir? it seems it's not needed.

  return Status;
}
