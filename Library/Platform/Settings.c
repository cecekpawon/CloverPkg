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

S_FILES                           *aConfigs = NULL, *aThemes = NULL, *aTools = NULL, *aDSDTs = NULL;
ACPI_PATCHED_AML                  *ACPIPatchedAML = NULL;
UINTN                             ACPIDropTablesNum = 0, ACPIPatchedAMLNum = 0,
                                  NGFX = 0,         // number of GFX
                                  nLanCards,        // number of LAN cards
                                  nLanPaths;        // number of LAN pathes
SETTINGS_DATA                     gSettings;
LANGUAGES                         gLanguage = english;
GFX_PROPERTIES                    gGraphics[5]; //no more then 4 graphics cards + iGPU
SLOT_DEVICE                       SlotDevices[DEV_INDEX_MAX], SmbiosSlotDevices[DEV_INDEX_MAX]; //assume DEV_XXX, Arpt=6
EFI_EDID_DISCOVERED_PROTOCOL      *EdidDiscovered;
UINT8                             *gEDID = NULL,
                                  *gLanMmio[4],     // their MMIO regions
                                  gLanMac[4][6];    // their MAC addresses
UINT16                            gLanVendor[4];    // their vendors
BOOLEAN                           GetLegacyLanAddress;

extern MEM_STRUCTURE              gRAM;
extern BOOLEAN                    NeedPMfix;

GUI_ANIME                         *GuiAnime = NULL;

// global configuration with default values
REFIT_CONFIG   DefaultConfig = {
  0,                  // UINTN        DisableFlags;
  0,                  // UINTN        HideBadges;
  0,                  // UINTN        HideUIFlags;
  FONT_GRAY,          // FONT_TYPE    Font;
  9,                  // INTN         CharWidth;
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
  0,                  // INTN         PruneScrollRows;
  144,                // INTN         row0TileSize
  64,                 // INTN         row1TileSize
  64,                 // INTN         LayoutBannerOffset
  0,                  // INTN         LayoutButtonOffset
  0,                  // INTN         LayoutTextOffset
  0,                  // INTN         LayoutAnimMoveForMenuX
  16,                 // INTN         ScrollWidth
  20,                 // INTN         ScrollButtonHeight
  5,                  // INTN         ScrollBarDecorationsHeight
  7,                  // INTN         ScrollBackDecorationsHeight
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
  { "Add HDMI",       "AddHDMI_8000000",      FIX_HDMI },
  { "Add MCHC",       "AddMCHC_0008",         FIX_MCHC },
  { "Add PNLF",       "AddPNLF_1000000",      FIX_PNLF },
  { "Fix Airport",    "FixAirport_4000",      FIX_WIFI },
  { "Fix Display",    "FixDisplay_0100",      FIX_DISPLAY },
  { "Fix IMEI",       "AddIMEI_80000",        FIX_IMEI },
  { "Fix IntelGFX",   "FIX_INTELGFX_100000",  FIX_INTELGFX },
  { "Fix LAN",        "FixLAN_2000",          FIX_LAN },
  { "Fix Sound",      "FixHDA_8000",          FIX_HDA },
};

INTN    OptMenuDSDTBitNum = ARRAY_SIZE (OPT_MENU_DSDTBIT);

CHAR16    *InjectKextsDir[2] = { NULL/*, NULL*/, NULL };

UINT32
GetCrc32 (
  UINT8   *Buffer,
  UINTN   Size
) {
  UINTN     i, Len;
  UINT32    Ret = 0, *Tmp;

  Tmp = (UINT32 *)Buffer;

  if (Tmp != NULL) {
    Len = Size >> 2;

    for (i = 0; i < Len; i++) {
      Ret += Tmp[i];
    }
  }

  return Ret;
}

BOOLEAN
GetPropertyBool (
  TagPtr    Prop,
  BOOLEAN   Default
) {
  return (
    (Prop == NULL)
      ? Default
      : (
          (Prop->type == kTagTypeTrue) ||
          (
            (Prop->type == kTagTypeString) &&
            (TO_AUPPER (Prop->string[0]) == 'Y')
          )
        )
  );
}

INTN
GetPropertyInteger (
  TagPtr  Prop,
  INTN    Default
) {
  if (Prop != NULL) {
    if (Prop->type == kTagTypeInteger) {
      return Prop->integer; //(INTN)Prop->string;
    } else if (Prop->type == kTagTypeString) {
      if ((Prop->string[0] == '0') && (TO_AUPPER (Prop->string[1]) == 'X')) {
        return (INTN)AsciiStrHexToUintn (Prop->string);
      }

      if (Prop->string[0] == '-') {
        return -(INTN)AsciiStrDecimalToUintn (Prop->string + 1);
      }

      return (INTN)AsciiStrDecimalToUintn (Prop->string);
    }
  }

  return Default;
}

CHAR8 *
GetPropertyString (
  TagPtr  Prop,
  CHAR8   *Default
) {
  if (
    (Prop != NULL) &&
    (Prop->type == kTagTypeString) &&
    AsciiStrLen (Prop->string)
  ) {
    return Prop->string;
  }

  return Default;
}

//
// returns binary setting in a new allocated buffer and data length in dataLen.
// data can be specified in <data></data> base64 encoded
// or in <string></string> hex encoded
//

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
    if ((Prop->type == kTagTypeData) && Prop->data && Prop->size) {
      // data property
      Data = AllocateCopyPool (Prop->size, Prop->data);

      if (Data != NULL) {
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
  CHAR8   *Str,
  CHAR8   Sep
) {
  MatchOSes   *MOS;
  UINTN       Len = 0, i = 0;
  CHAR8       DoubleSep[2];

  MOS = AllocatePool (sizeof (MatchOSes));
  if (!MOS) {
    goto Finish;
  }

  MOS->count = CountOccurrences (Str, Sep) + 1;

  Len = AsciiStrLen (Str);
  DoubleSep[0] = Sep; DoubleSep[1] = Sep;

  if (
    !Len ||
    (AsciiStrStr (Str, DoubleSep) != NULL) ||
    (Str[0] == Sep) ||
    (Str[Len -1] == Sep)
  ) {
    MOS->count = 0;
    MOS->array[0] = NULL;
    goto Finish;
  }

  if (MOS->count > 1) {
    UINTN    *Indexes = (UINTN *)AllocatePool (MOS->count), Inc = 0;

    for (i = 0; i < Len; ++i) {
      CHAR8 C = Str[i];

      if (C == Sep) {
        Indexes[++Inc] = i;
      }
    }

    Indexes[0] = 0;
    Indexes[MOS->count] = Len;

    for (i = 0; i < MOS->count; i++) {
      UINTN   StartLocation = i ? Indexes[i] + 1 : Indexes[0],
              EndLocation = (i == (MOS->count - 1)) ? Len : Indexes[i + 1],
              NewLen = (EndLocation - StartLocation);

      //DBG ("start %d, end %d\n", StartLocation, EndLocation);

      MOS->array[i] = AllocateCopyPool (NewLen, Str + StartLocation);
      MOS->array[i][NewLen] = '\0';

      if (EndLocation == Len) {
        break;
      }
    }

    FreePool (Indexes);
  } else {
    //DBG ("Str contains only one component and it is our String %s!\n", Str);
    MOS->array[0] = AllocateCopyPool (AsciiStrLen (Str) + 1, Str);
  }

  Finish:

  return MOS;
}

VOID
DeallocMatchOSes (
  MatchOSes   *MOS
) {
  if (MOS) {
    UINTN    i;

    for (i = 0; i < MOS->count; i++) {
      if (MOS->array[i]) {
        FreePool (MOS->array[i]);
      }
    }

    FreePool (MOS);
  }
}

BOOLEAN
IsPatchEnabled (
  CHAR8   *MatchOSEntry,
  CHAR8   *CurrOS
) {
  UINTN       i, MatchOSPartFrom, MatchOSPartTo, CurrOSPart;
  UINT64      ValFrom, ValTo, ValCurrFrom, ValCurrTo;
  BOOLEAN     Ret = TRUE;
  MatchOSes   *MOS;

  if (
    !MatchOSEntry ||
    !AsciiStrLen (MatchOSEntry) ||
    !CurrOS ||
    !AsciiStrLen (CurrOS)
  ) {
    goto Finish; // undefined matched corresponds to old behavior
  }

  MOS = GetStrArraySeparatedByChar (MatchOSEntry, ',');
  if (!MOS) {
    goto Finish; // memory fails -> anyway the patch enabled
  }

  CurrOSPart = CountOccurrences (CurrOS, '.');

  for (i = 0; i < MOS->count; ++i) {
    if (AsciiStrStr (MOS->array[i], ".") != NULL) { // MatchOS
      if (
        (MOS->count == 1) && // only 1 value range allowed, no comma(s)
        (CountOccurrences (MatchOSEntry, '-') == 1)
      ) {
        DeallocMatchOSes (MOS);
        MOS = GetStrArraySeparatedByChar (MatchOSEntry, '-');
        if (MOS && (MOS->count == 2)) { // do more strict
          MatchOSPartFrom = CountOccurrences (MOS->array[0], '.');
          MatchOSPartTo = CountOccurrences (MOS->array[1], '.');

          if (AsciiStriStr (MOS->array[0], "x") != NULL) {
            MatchOSPartFrom = 1;
          }

          if (AsciiStriStr (MOS->array[1], "x") != NULL) {
            MatchOSPartTo = 1;
          }

          CONSTRAIN_MAX (MatchOSPartFrom, CurrOSPart);
          CONSTRAIN_MAX (MatchOSPartTo, CurrOSPart);

          MatchOSPartFrom++;
          MatchOSPartTo++;

          ValFrom = AsciiStrVersionToUint64 (MOS->array[0], 2, (UINT8)MatchOSPartFrom);
          ValCurrFrom = AsciiStrVersionToUint64 (CurrOS, 2, (UINT8)MatchOSPartFrom);
          ValTo = AsciiStrVersionToUint64 (MOS->array[1], 2, (UINT8)MatchOSPartTo);
          ValCurrTo = AsciiStrVersionToUint64 (CurrOS, 2, (UINT8)MatchOSPartTo);

          Ret = (/* (ValFrom < ValTo) && */ (ValFrom <= ValCurrFrom) && (ValTo >= ValCurrTo));
          break;
        }
      } else {
        MatchOSPartFrom = CountOccurrences (MOS->array[i], '.');

        if (AsciiStriStr (MOS->array[i], "x") != NULL) {
          MatchOSPartFrom = 1;
        }

        CONSTRAIN_MAX (MatchOSPartFrom, CurrOSPart);

        MatchOSPartFrom++;

        ValFrom = AsciiStrVersionToUint64 (MOS->array[i], 2, (UINT8)MatchOSPartFrom);
        ValCurrFrom = AsciiStrVersionToUint64 (CurrOS, 2, (UINT8)MatchOSPartFrom);

        if (ValFrom == ValCurrFrom) {
          break;
        }
      }
    } else if ( // MatchBuild
      AsciiStrCmp (MOS->array[i], CurrOS) == 0 // single unique
      //AsciiStrStr (MOS->array[i], CurrOS) != NULL // saverals MatchBuild by commas
    ) {
      break;
    }
  }

  DeallocMatchOSes (MOS);

  Finish:

  return Ret;
}
// End of MatchOS

UINT8
CheckVolumeType (
  UINT8     VolumeType,
  CHAR8     *Str
) {
  UINT8   VolumeTypeTmp = VolumeType;

  if (AsciiStriCmp (Str, "Internal") == 0) {
    VolumeTypeTmp |= VOLTYPE_INTERNAL;
  } else if (AsciiStriCmp (Str, "External") == 0) {
    VolumeTypeTmp |= VOLTYPE_EXTERNAL;
  } else if (AsciiStriCmp (Str, "Optical") == 0) {
    VolumeTypeTmp |= VOLTYPE_OPTICAL;
  } else if (AsciiStriCmp (Str, "FireWire") == 0) {
    VolumeTypeTmp |= VOLTYPE_FIREWIRE;
  }

  return VolumeTypeTmp;
}

UINT8
GetVolumeType (
  TagPtr    DictPointer
) {
  TagPtr    Prop, Prop2 = NULL;
  UINT8     VolumeType = 0;

  Prop = GetProperty (DictPointer, "VolumeType");
  if (Prop != NULL) {
    if (Prop->type == kTagTypeString) {
      VolumeType = CheckVolumeType (0, Prop->string);
    } else if (Prop->type == kTagTypeArray) {
      INTN   i, Count = Prop->size;

      if (Count > 0) {
        for (i = 0; i < Count; i++) {
          if (EFI_ERROR (GetElement (Prop, i, Count, &Prop2))) {
            continue;
          }

          if (Prop2 == NULL) {
            break;
          }

          if (Prop2->type != kTagTypeString) {
            continue;
          }

          VolumeType = CheckVolumeType (VolumeType, Prop2->string);
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
  BOOLEAN   Ret = FALSE;

  if (Entry != NULL) {
    if (gSettings.CustomEntries) {
      CUSTOM_LOADER_ENTRY   *Entries = gSettings.CustomEntries;

      while (Entries->Next != NULL) {
        Entries = Entries->Next;
      }

      Entries->Next = Entry;
    } else {
      gSettings.CustomEntries = Entry;
    }

    Ret = TRUE;
  }

  return Ret;
}

STATIC
BOOLEAN
AddCustomSubEntry (
  IN OUT CUSTOM_LOADER_ENTRY   *Entry,
  IN     CUSTOM_LOADER_ENTRY   *SubEntry
) {
  BOOLEAN   Ret = FALSE;

  if ((Entry != NULL) && (SubEntry != NULL)) {
    if (Entry->SubEntries != NULL) {
      CUSTOM_LOADER_ENTRY   *Entries = Entry->SubEntries;

      while (Entries->Next != NULL) {
        Entries = Entries->Next;
      }

      Entries->Next = SubEntry;
    } else {
      Entry->SubEntries = SubEntry;
    }

    Ret = TRUE;
  }

  return Ret;
}

STATIC
CUSTOM_LOADER_ENTRY *
DuplicateCustomEntry (
  IN CUSTOM_LOADER_ENTRY  *Entry
) {
  CUSTOM_LOADER_ENTRY   *DuplicateEntry = NULL;

  if (Entry != NULL) {
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

      CopyKernelAndKextPatches (
        (KERNEL_AND_KEXT_PATCHES *)(((UINTN)DuplicateEntry) + OFFSET_OF (CUSTOM_LOADER_ENTRY, KernelAndKextPatches)),
        (KERNEL_AND_KEXT_PATCHES *)(((UINTN)Entry) + OFFSET_OF (CUSTOM_LOADER_ENTRY, KernelAndKextPatches))
      );
    }
  }

  return DuplicateEntry;
}

STATIC
BOOLEAN
FillinCustomEntry (
  IN OUT CUSTOM_LOADER_ENTRY    *Entry,
  TagPtr                        DictPointer,
  IN BOOLEAN                    SubEntry
) {
  BOOLEAN   Ret = FALSE;

  if (
    (Entry != NULL) &&
    (DictPointer != NULL) &&
    !GetPropertyBool (GetProperty (DictPointer, "Disabled"), FALSE)
  ) {
    TagPtr    Prop;

    Prop = GetProperty (DictPointer, "Volume");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      if (Entry->Volume) {
        FreePool (Entry->Volume);
      }

      Entry->Volume = PoolPrint (L"%a", Prop->string);
    }

    Prop = GetProperty (DictPointer, "Path");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      if (Entry->Path) {
        FreePool (Entry->Path);
      }

      Entry->Path = PoolPrint (L"%a", Prop->string);
    }

    Prop = GetProperty (DictPointer, "Settings");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      if (Entry->Settings) {
        FreePool (Entry->Settings);
      }

      Entry->Settings = PoolPrint (L"%a", Prop->string);
    }

    Entry->CommonSettings = GetPropertyBool (GetProperty (DictPointer, "CommonSettings"), FALSE);

    Prop = GetProperty (DictPointer, "AddArguments");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      if (Entry->Options != NULL) {
        CHAR16  *OldOptions = Entry->Options;
        Entry->Options = PoolPrint (L"%s %a", OldOptions, Prop->string);
        FreePool (OldOptions);
      } else {
        Entry->Options = PoolPrint (L"%a", Prop->string);
      }
    } else {
      Prop = GetProperty (DictPointer, "Arguments");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        if (Entry->Options != NULL) {
          FreePool (Entry->Options);
        }

        Entry->Options = PoolPrint (L"%a", Prop->string);
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NODEFAULTARGS);
      }
    }

    Prop = GetProperty (DictPointer, "Title");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
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
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
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
          UINT32  Len = (UINT32)(AsciiStrLen (Prop->string) >> 1);

          if (Len > 0) {
            UINT8   *Data  = (UINT8 *)AllocateZeroPool (Len);

            if (Data) {
              Entry->Image = DecodePNG (Data, Hex2Bin (Prop->string, Data, Len));
            }
          }
        } else if (Prop->type == kTagTypeData) {
          Entry->Image = DecodePNG (Prop->data, Prop->size);
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
          UINT32  Len = (UINT32)(AsciiStrLen (Prop->string) >> 1);

          if (Len > 0) {
            UINT8   *Data = (UINT8 *)AllocateZeroPool (Len);

            if (Data) {
              Entry->DriveImage = DecodePNG (Data, Hex2Bin (Prop->string, Data, Len));
            }
          }
        } else if (Prop->type == kTagTypeData) {
          Entry->DriveImage = DecodePNG (Prop->data, Prop->size);
        }
      }
    }

    Prop = GetProperty (DictPointer, "Hotkey");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      Entry->Hotkey = *(Prop->string);
    }

    // Hidden Property, Values:
    // - No (show the entry)
    // - Yes (hide the entry but can be show with F3)
    // - Always (always hide the entry)
    Entry->Flags = OSFLAG_UNSET (Entry->Flags, (OSFLAG_DISABLED + OSFLAG_HIDDEN));
    Prop = GetProperty (DictPointer, "Hidden");
    if (Prop != NULL) {
      if (
        (Prop->type == kTagTypeString) &&
        (AsciiStriCmp (Prop->string, "Always") == 0)
      ) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_DISABLED);
      } else if (GetPropertyBool (Prop, FALSE)) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_HIDDEN);
      } else {
        Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_HIDDEN);
      }
    }

    Prop = GetProperty (DictPointer, "Type");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      if (AsciiStriCmp (Prop->string, "Darwin") == 0) {
        Entry->Type = OSTYPE_DARWIN;
      } else if (AsciiStriCmp (Prop->string, "DarwinInstaller") == 0) {
        Entry->Type = OSTYPE_DARWIN_INSTALLER;
      } else if (AsciiStriCmp (Prop->string, "DarwinRecovery") == 0) {
        Entry->Type = OSTYPE_DARWIN_RECOVERY;
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
      if (OSTYPE_IS_DARWIN_RECOVERY (Entry->Type)) {
        Entry->Title = PoolPrint (L"Recovery");
      } else if (OSTYPE_IS_DARWIN_INSTALLER (Entry->Type)) {
        Entry->Title = PoolPrint (L"Installer");
      }
    }

    if ((Entry->Image == NULL) && (Entry->ImagePath == NULL)) {
      if (OSTYPE_IS_DARWIN_RECOVERY (Entry->Type)) {
        Entry->ImagePath = L"mac";
      }
    }

    if ((Entry->DriveImage == NULL) && (Entry->DriveImagePath == NULL)) {
      if (OSTYPE_IS_DARWIN_RECOVERY (Entry->Type)) {
        Entry->DriveImagePath = L"recovery";
      }
    }

    // OS Specific flags
    if (OSTYPE_IS_DARWIN_GLOB (Entry->Type)) {
      // InjectKexts default values
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_WITHKEXTS);

      Prop = GetProperty (DictPointer, "InjectKexts");
      if (Prop != NULL) {
        if (GetPropertyBool (Prop, TRUE)) {
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

      if (GetPropertyBool (GetProperty (DictPointer, "NoCaches"), FALSE) || gSettings.NoCaches) {
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
        INTN                  i, Count = Prop->size;
        TagPtr                Dict = NULL;

        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NODEFAULTMENU);

        if (Count > 0) {
          for (i = 0; i < Count; i++) {
            if (EFI_ERROR (GetElement (Prop, i, Count, &Dict))) {
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

    Ret = TRUE;
  }

  return Ret;
}

STATIC
BOOLEAN
AddCustomToolEntry (
  IN CUSTOM_TOOL_ENTRY    *Entry
) {
  BOOLEAN   Ret = FALSE;

  if (Entry != NULL) {
    if (gSettings.CustomTool) {
      CUSTOM_TOOL_ENTRY   *Entries = gSettings.CustomTool;

      while (Entries->Next != NULL) {
        Entries = Entries->Next;
      }

      Entries->Next = Entry;
    } else {
      gSettings.CustomTool = Entry;
    }

    Ret = TRUE;
  }

  return Ret;
}

STATIC
BOOLEAN
FillinCustomTool (
  IN OUT    CUSTOM_TOOL_ENTRY *Entry,
  TagPtr    DictPointer
) {
  BOOLEAN   Ret = FALSE;

  if (
    (Entry != NULL) &&
    (DictPointer != NULL) &&
    !GetPropertyBool (GetProperty (DictPointer, "Disabled"), FALSE)
  ) {
    TagPtr    Prop;

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
            UINT8   *Data = (UINT8 *)AllocateZeroPool (Len);

            if (Data != NULL) {
              Entry->Image = DecodePNG (Data, Hex2Bin (Prop->string, Data, Len));
            }
          }
        } else if (Prop->type == kTagTypeData) {
          Entry->Image = DecodePNG (Prop->data, Prop->size);
        }
      }
    }

    Prop = GetProperty (DictPointer, "Hotkey");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      Entry->Hotkey = *Prop->string;
    }

    // Hidden Property, Values:
    // - No (show the entry)
    // - Yes (hide the entry but can be show with F3)
    // - Always (always hide the entry)
    Entry->Flags = OSFLAG_UNSET (Entry->Flags, (OSFLAG_DISABLED + OSFLAG_HIDDEN));
    Prop = GetProperty (DictPointer, "Hidden");
    if (Prop != NULL) {
      if (
        (Prop->type == kTagTypeString) &&
        (AsciiStriCmp (Prop->string, "Always") == 0)
      ) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_DISABLED);
      } else if (GetPropertyBool (Prop, FALSE)) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_HIDDEN);
      } else {
        Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_HIDDEN);
      }
    }
    Ret = TRUE;
  }

  return Ret;
}

VOID
CopyKernelAndKextPatches (
  IN OUT KERNEL_AND_KEXT_PATCHES   *Dst,
  IN     KERNEL_AND_KEXT_PATCHES   *Src
) {
  if ((Dst != NULL) && (Src != NULL)) {
    UINTN  i;

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

      if ((Src->KPATIConnectorsDataLen > 0) && (Src->KPATIConnectorsData != NULL) && (Src->KPATIConnectorsPatch != NULL)) {
        Dst->KPATIConnectorsDataLen  = Src->KPATIConnectorsDataLen;
        Dst->KPATIConnectorsData     = AllocateCopyPool (Src->KPATIConnectorsDataLen, Src->KPATIConnectorsData);
        Dst->KPATIConnectorsPatch    = AllocateCopyPool (Src->KPATIConnectorsDataLen, Src->KPATIConnectorsPatch);
        Dst->KPATIConnectorsWildcard = Src->KPATIConnectorsWildcard;
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
        if ((Src->KextPatches[i].DataLen > 0) && (Src->KextPatches[i].Data != NULL) && (Src->KextPatches[i].Patch != NULL)) {
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
          Dst->KextPatches[Dst->NrKexts].Wildcard       = Src->KextPatches[i].Wildcard;
          Dst->KextPatches[Dst->NrKexts].Count          = Src->KextPatches[i].Count;
          Dst->KextPatches[Dst->NrKexts].Data           = AllocateCopyPool (Src->KextPatches[i].DataLen, Src->KextPatches[i].Data);
          Dst->KextPatches[Dst->NrKexts].Patch          = AllocateCopyPool (Src->KextPatches[i].DataLen, Src->KextPatches[i].Patch);
          Dst->KextPatches[Dst->NrKexts].MatchOS        = AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].MatchOS), Src->KextPatches[i].MatchOS);
          Dst->KextPatches[Dst->NrKexts].MatchBuild     = AllocateCopyPool (AsciiStrSize (Src->KextPatches[i].MatchBuild), Src->KextPatches[i].MatchBuild);
          ++(Dst->NrKexts);
        }

      }
    }

    if ((Src->NrKernels > 0) && (Src->KernelPatches != NULL)) {
      Dst->KernelPatches = AllocatePool (Src->NrKernels * sizeof (KERNEL_PATCH));

      for (i = 0; i < Src->NrKernels; i++) {
        if ((Src->KernelPatches[i].DataLen > 0) && (Src->KernelPatches[i].Data != NULL) && (Src->KernelPatches[i].Patch != NULL)) {
          if (Src->KernelPatches[i].Label) {
            Dst->KernelPatches[Dst->NrKernels].Label    = (CHAR8 *)AllocateCopyPool (AsciiStrSize (Src->KernelPatches[i].Label), Src->KernelPatches[i].Label);
          }

          Dst->KernelPatches[Dst->NrKernels].Disabled   = Src->KernelPatches[i].Disabled;
          Dst->KernelPatches[Dst->NrKernels].DataLen    = Src->KernelPatches[i].DataLen;
          Dst->KernelPatches[Dst->NrKernels].Wildcard   = Src->KernelPatches[i].Wildcard;
          Dst->KernelPatches[Dst->NrKernels].Data       = AllocateCopyPool (Src->KernelPatches[i].DataLen, Src->KernelPatches[i].Data);
          Dst->KernelPatches[Dst->NrKernels].Patch      = AllocateCopyPool (Src->KernelPatches[i].DataLen, Src->KernelPatches[i].Patch);
          Dst->KernelPatches[Dst->NrKernels].Count      = Src->KernelPatches[i].Count;
          Dst->KernelPatches[Dst->NrKernels].MatchOS    = AllocateCopyPool (AsciiStrSize (Src->KernelPatches[i].MatchOS), Src->KernelPatches[i].MatchOS);
          Dst->KernelPatches[Dst->NrKernels].MatchBuild = AllocateCopyPool (AsciiStrSize (Src->KernelPatches[i].MatchBuild), Src->KernelPatches[i].MatchBuild);
          ++(Dst->NrKernels);
        }
      }
    }

    if ((Src->NrBooters) && (Src->BooterPatches != NULL)) {
      Dst->BooterPatches = AllocatePool (Src->NrBooters * sizeof (BOOTER_PATCH));

      for (i = 0; i < Src->NrBooters; i++) {
        if ((Src->BooterPatches[i].DataLen > 0) && (Src->BooterPatches[i].Data != NULL) && (Src->BooterPatches[i].Patch != NULL)) {
          if (Src->BooterPatches[i].Label) {
            Dst->BooterPatches[Dst->NrBooters].Label    = (CHAR8 *)AllocateCopyPool (AsciiStrSize (Src->BooterPatches[i].Label), Src->BooterPatches[i].Label);
          }

          Dst->BooterPatches[Dst->NrBooters].Disabled   = Src->BooterPatches[i].Disabled;
          Dst->BooterPatches[Dst->NrBooters].DataLen    = Src->BooterPatches[i].DataLen;
          Dst->BooterPatches[Dst->NrBooters].Wildcard   = Src->BooterPatches[i].Wildcard;
          Dst->BooterPatches[Dst->NrBooters].Data       = AllocateCopyPool (Src->BooterPatches[i].DataLen, Src->BooterPatches[i].Data);
          Dst->BooterPatches[Dst->NrBooters].Patch      = AllocateCopyPool (Src->BooterPatches[i].DataLen, Src->BooterPatches[i].Patch);
          Dst->BooterPatches[Dst->NrBooters].Count      = Src->BooterPatches[i].Count;
          Dst->BooterPatches[Dst->NrBooters].MatchOS    = AllocateCopyPool (AsciiStrSize (Src->BooterPatches[i].MatchOS), Src->BooterPatches[i].MatchOS);
          ++(Dst->NrBooters);
        }
      }
    }
  }
}

STATIC
VOID
FillinKextPatches (
  IN OUT  KERNEL_AND_KEXT_PATCHES    *Patches,
  IN      TagPtr                      DictPointer
) {
  if ((Patches != NULL) && (DictPointer != NULL)) {
    TagPtr  Prop;

    if (NeedPMfix) {
      Patches->KPKernelPm = TRUE;
      Patches->KPAsusAICPUPM = TRUE;
    }

    gSettings.DebugKP = Patches->KPDebug = GetPropertyBool (GetProperty (DictPointer, "Debug"), FALSE);

    gSettings.FlagsBits = Patches->KPDebug
      ? OSFLAG_SET (gSettings.FlagsBits, OSFLAG_DBGPATCHES)
      : OSFLAG_UNSET (gSettings.FlagsBits, OSFLAG_DBGPATCHES);

    //Patches->KPKernelCpu = GetPropertyBool (GetProperty (DictPointer, "KernelCpu"));

    Prop = GetProperty (DictPointer, "FakeCPUID");
    if (Prop != NULL) {
      Patches->FakeCPUID = (UINT32)GetPropertyInteger (Prop, 0);
      DBG ("FakeCPUID: %x\n", Patches->FakeCPUID);
    }

    Patches->KPAsusAICPUPM = GetPropertyBool (GetProperty (DictPointer, "AsusAICPUPM"), Patches->KPAsusAICPUPM);

    Patches->KPKernelPm = GetPropertyBool (GetProperty (DictPointer, "KernelPm"), Patches->KPKernelPm);

    //Patches->KPLapicPanic = GetPropertyBool (GetProperty (DictPointer, "KernelLapic"), Patches->KPLapicPanic);

    //Patches->KPHaswellE = GetPropertyBool (GetProperty (DictPointer, "KernelHaswellE"), Patches->KPHaswellE);

    if (gSettings.HasGraphics->Ati) {
      Prop = GetProperty (DictPointer, "ATIConnectorsController");
      if (Prop != NULL) {
        UINTN   Len = 0, i = 0;

        // ATIConnectors patch
        Len = (AsciiStrnLenS (Prop->string, 255) * sizeof (CHAR16) + 2);
        Patches->KPATIConnectorsController = AllocateZeroPool (Len);
        AsciiStrToUnicodeStrS (Prop->string, Patches->KPATIConnectorsController, Len);

        Patches->KPATIConnectorsData = GetDataSetting (DictPointer, "ATIConnectorsData", &Len);
        Patches->KPATIConnectorsDataLen = Len;
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
    Patches->KPATIConnectorsWildcard   = 0xFF;

    PatchesNext:

    Prop = GetProperty (DictPointer, "ForceKextsToLoad");
    if (Prop != NULL) {
      INTN   i, Count = Prop->size;

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
          EFI_STATUS  Status = GetElement (Prop, i, Count, &Prop2);

          DBG (" - [%02d]:", i);

          if (EFI_ERROR (Status) || (Prop2 == NULL) || (Prop2->type != kTagTypeString)) {
            DBG (" %r parsing / empty element\n", Status);
            continue;
          }

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

    Prop = GetProperty (DictPointer, "KextsToPatch");
    if (Prop != NULL) {
      INTN   i, Count = Prop->size;
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

          EFI_STATUS Status = GetElement (Prop, i, Count, &Prop2);

          DBG (" - [%02d]:", i);

          if (EFI_ERROR (Status) || (Prop2 == NULL)) {
            DBG (" %r parsing / empty element\n", Status);
            continue;
          }

          Dict = GetProperty (Prop2, "Name");
          if ((Dict == NULL) || (Dict->type != kTagTypeString)) {
            DBG (" patch without Name, skip\n");
            continue;
          }

          KextPatchesName = AllocateCopyPool (255, Dict->string);
          KextPatchesLabel = AllocateCopyPool (255, KextPatchesName);

          Dict = GetProperty (Prop2, "Comment");
          if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
            AsciiStrCatS (KextPatchesLabel, 255, " (");
            AsciiStrCatS (KextPatchesLabel, 255, Dict->string);
            AsciiStrCatS (KextPatchesLabel, 255, ")");
          } else {
            AsciiStrCatS (KextPatchesLabel, 255, " (NoLabel)");
          }

          DBG (" %a", KextPatchesLabel);

          Dict = GetProperty (Prop2, "Disabled");
          if (GetPropertyBool (Dict, FALSE)) {
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
          Patches->KextPatches[Patches->NrKexts].Patch      = AllocateCopyPool (ReplaceLen, TmpPatch);
          Patches->KextPatches[Patches->NrKexts].MatchOS    = NULL;
          Patches->KextPatches[Patches->NrKexts].MatchBuild = NULL;
          Patches->KextPatches[Patches->NrKexts].Filename   = NULL;
          Patches->KextPatches[Patches->NrKexts].Disabled   = FALSE;
          Patches->KextPatches[Patches->NrKexts].Patched    = FALSE;
          Patches->KextPatches[Patches->NrKexts].Name       = AllocateCopyPool (AsciiStrnLenS (KextPatchesName, 255) + 1, KextPatchesName);
          Patches->KextPatches[Patches->NrKexts].Label      = AllocateCopyPool (AsciiStrnLenS (KextPatchesLabel, 255) + 1, KextPatchesLabel);
          Patches->KextPatches[Patches->NrKexts].Count      = GetPropertyInteger (GetProperty (Prop2, "Count"), 0);
          Patches->KextPatches[Patches->NrKexts].Wildcard   = 0xFF;

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
          Patches->KextPatches[Patches->NrKexts].IsPlistPatch = GetPropertyBool (GetProperty (Prop2, "InfoPlistPatch"), FALSE);

          Dict = GetProperty (Prop2, "Wildcard");
          if (Dict != NULL) {
            if (Patches->KextPatches[Patches->NrKexts].IsPlistPatch && (Dict->type == kTagTypeString)) {
              Patches->KextPatches[Patches->NrKexts].Wildcard = (UINT8)*Prop->string;
            } else {
              Patches->KextPatches[Patches->NrKexts].Wildcard = (UINT8)GetPropertyInteger (Dict, 0xFF);
            }
          }

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
      INTN    i, Count = Prop->size;

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
          EFI_STATUS    Status = GetElement (Prop, i, Count, &Prop2);

          DBG (" - [%02d]:", i);

          if (EFI_ERROR (Status) || (Prop2 == NULL)) {
            DBG (" %r parsing / empty element\n", Status);
            continue;
          }

          Dict = GetProperty (Prop2, "Comment");
          KernelPatchesLabel = ((Dict != NULL) && (Dict->type == kTagTypeString))
                                  ? AllocateCopyPool (AsciiStrSize (Dict->string), Dict->string)
                                  : AllocateCopyPool (8, "NoLabel");

          DBG (" %a", KernelPatchesLabel);

          if (GetPropertyBool (GetProperty (Prop2, "Disabled"), FALSE)) {
            DBG (" | patch disabled, skip\n");
            continue;
          }

          TmpData   = GetDataSetting (Prop2, "Find", &FindLen);
          TmpPatch  = GetDataSetting (Prop2, "Replace", &ReplaceLen);

          if (!FindLen || !ReplaceLen || (FindLen != ReplaceLen)) {
            DBG (" | invalid Find/Replace data, skip\n");
            continue;
          }

          Patches->KernelPatches[Patches->NrKernels].Data       = AllocateCopyPool (FindLen, TmpData);
          Patches->KernelPatches[Patches->NrKernels].DataLen    = FindLen;
          Patches->KernelPatches[Patches->NrKernels].Patch      = AllocateCopyPool (ReplaceLen, TmpPatch);
          Patches->KernelPatches[Patches->NrKernels].MatchOS    = NULL;
          Patches->KernelPatches[Patches->NrKernels].MatchBuild = NULL;
          Patches->KernelPatches[Patches->NrKernels].Disabled   = FALSE;
          Patches->KernelPatches[Patches->NrKernels].Label      = AllocateCopyPool (AsciiStrSize (KernelPatchesLabel), KernelPatchesLabel);
          Patches->KernelPatches[Patches->NrKernels].Count      = GetPropertyInteger (GetProperty (Prop2, "Count"), 0);
          Patches->KernelPatches[Patches->NrKernels].Wildcard   = (UINT8)GetPropertyInteger (GetProperty (Prop2, "Wildcard"), 0xFF);

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

    Prop = GetProperty (DictPointer, "BooterToPatch");
    if (Prop != NULL) {
      INTN    i, Count = Prop->size;

      //delete old and create new
      if (Patches->BooterPatches) {
        Patches->NrBooters = 0;
        FreePool (Patches->BooterPatches);
      }

      if (Count > 0) {
        TagPtr          Prop2 = NULL, Dict = NULL;
        BOOTER_PATCH    *newPatches = AllocateZeroPool (Count * sizeof (BOOTER_PATCH));

        Patches->BooterPatches = newPatches;
        MsgLog ("BooterToPatch: %d requested\n", Count);

        for (i = 0; i < Count; i++) {
          CHAR8         *BooterPatchesLabel;
          UINTN         FindLen = 0, ReplaceLen = 0;
          UINT8         *TmpData, *TmpPatch;
          EFI_STATUS    Status = GetElement (Prop, i, Count, &Prop2);

          DBG (" - [%02d]:", i);

          if (EFI_ERROR (Status) || (Prop2 == NULL)) {
            DBG (" %r parsing / empty element\n", Status);
            continue;
          }

          Dict = GetProperty (Prop2, "Comment");
          BooterPatchesLabel = ((Dict != NULL) && (Dict->type == kTagTypeString))
                                  ? AllocateCopyPool (AsciiStrSize (Dict->string), Dict->string)
                                  : AllocateCopyPool (8, "NoLabel");

          DBG (" %a", BooterPatchesLabel);

          if (GetPropertyBool (GetProperty (Prop2, "Disabled"), FALSE)) {
            DBG (" | patch disabled, skip\n");
            continue;
          }

          TmpData   = GetDataSetting (Prop2, "Find", &FindLen);
          TmpPatch  = GetDataSetting (Prop2, "Replace", &ReplaceLen);

          if (!FindLen || !ReplaceLen || (FindLen != ReplaceLen)) {
            DBG (" | invalid Find/Replace data, skip\n");
            continue;
          }

          Patches->BooterPatches[Patches->NrBooters].Data       = AllocateCopyPool (FindLen, TmpData);
          Patches->BooterPatches[Patches->NrBooters].DataLen    = FindLen;
          Patches->BooterPatches[Patches->NrBooters].Patch      = AllocateCopyPool (ReplaceLen, TmpPatch);
          Patches->BooterPatches[Patches->NrBooters].MatchOS    = NULL;
          Patches->BooterPatches[Patches->NrBooters].Disabled   = FALSE;
          Patches->BooterPatches[Patches->NrBooters].Label      = AllocateCopyPool (AsciiStrSize (BooterPatchesLabel), BooterPatchesLabel);
          Patches->BooterPatches[Patches->NrBooters].Count      = GetPropertyInteger (GetProperty (Prop2, "Count"), 0);
          Patches->BooterPatches[Patches->NrBooters].Wildcard   = (UINT8)GetPropertyInteger (GetProperty (Prop2, "Wildcard"), 0xFF);

          FreePool (TmpData);
          FreePool (TmpPatch);
          FreePool (BooterPatchesLabel);

          // check enable/disabled patch (OS based) by Micky1979
          Dict = GetProperty (Prop2, "MatchOS");
          if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
            Patches->BooterPatches[Patches->NrBooters].MatchOS = AllocateCopyPool (AsciiStrSize (Dict->string), Dict->string);
            DBG (" | MatchOS: %a", Patches->BooterPatches[Patches->NrBooters].MatchOS);
          }

          DBG (" | len: %d\n", Patches->BooterPatches[Patches->NrBooters].DataLen);

          Patches->NrBooters++;
        }
      }
    }
  }
}

VOID
GetListOfAcpi () {
  REFIT_DIR_ITER      DirIter;
  EFI_FILE_INFO       *DirEntry;
  CHAR16              *AcpiPath = AllocateZeroPool (SVALUE_MAX_SIZE);
  UINTN               i = 0, i2, y = 0, PathIndex, PathCount = ARRAY_SIZE (SupportedOsType);
  UINT8               LoaderType;
  BOOLEAN             DefFound = FALSE;

  DbgHeader ("GetListOfAcpi");

  for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
    AcpiPath = PoolPrint (DIR_ACPI_PATCHED L"\\%s", SupportedOsType[PathIndex]);

    switch (PathIndex) {
      case 0:
        LoaderType = OSTYPE_DARWIN;
        break;

      case 1:
        LoaderType = OSTYPE_LIN;
        break;

      case 2:
        LoaderType = OSTYPE_WIN;
        break;
    }

    DirIterOpen (SelfRootDir, AcpiPath, &DirIter);

    while (DirIterNext (&DirIter, 2, L"*.aml", &DirEntry)) {
      if (DirEntry->FileName[0] != L'.') {
        if (StriStr (DirEntry->FileName, L"DSDT")) {
          S_FILES   *aTmp = AllocateZeroPool (sizeof (S_FILES));
          CHAR16    *TmpDsdt = RemoveExtension (DirEntry->FileName);

          MsgLog ("- [%02d]: %s: %s\n", i++, SupportedOsType[PathIndex], DirEntry->FileName);

          aTmp->Index = y++;

          if (!DefFound && (StrniCmp (TmpDsdt, gSettings.DsdtName, StrSize (gSettings.DsdtName)) == 0)) {
            OldChosenDSDT = aTmp->Index;
            DefFound = TRUE;
          }

          aTmp->FileName = EfiStrDuplicate (TmpDsdt);
          aTmp->Description = EfiStrDuplicate (SupportedOsType[PathIndex]);
          aTmp->Next = aDSDTs;
          aDSDTs = aTmp;

          FreePool (TmpDsdt);
        } else {
          BOOLEAN   ACPIDisabled = FALSE;
          ACPI_PATCHED_AML    *ACPIPatchedAMLTmp = AllocateZeroPool (sizeof (ACPI_PATCHED_AML));

          MsgLog ("- [%02d]: %s: %s\n", i++, SupportedOsType[PathIndex], DirEntry->FileName);

          ACPIPatchedAMLTmp->FileName = EfiStrDuplicate (DirEntry->FileName);
          ACPIPatchedAMLTmp->Description = EfiStrDuplicate (SupportedOsType[PathIndex]);

          for (i2 = 0; i2 < gSettings.DisabledAMLCount; i2++) {
            if (StriCmp (ACPIPatchedAMLTmp->FileName, gSettings.DisabledAML[i2]) == 0) {
              ACPIDisabled = TRUE;
              break;
            }
          }

          ACPIPatchedAMLTmp->OSType = LoaderType;
          ACPIPatchedAMLTmp->MenuItem.BValue = ACPIDisabled;
          ACPIPatchedAMLTmp->Next = ACPIPatchedAML;
          ACPIPatchedAML = ACPIPatchedAMLTmp;
          ACPIPatchedAMLNum++;
        }
      }
    }

    DirIterClose (&DirIter);

    FreePool (AcpiPath);
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
}

VOID
GetListOfConfigs () {
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  UINTN             i = 0, y = 0;
  BOOLEAN           DefFound = FALSE;

  DbgHeader ("GetListOfConfigs");

  DirIterOpen (SelfRootDir, OEMPath, &DirIter);

  OldChosenConfig = 0;

  while (DirIterNext (&DirIter, 2, L"*.plist", &DirEntry)) {
    if (DirEntry->FileName[0] != L'.') {
      S_FILES   *aTmp = AllocateZeroPool (sizeof (S_FILES));
      CHAR16    *TmpCfg = RemoveExtension (DirEntry->FileName);

      MsgLog ("- [%02d]: %s\n", i++, DirEntry->FileName);

      aTmp->Index = y++;

      if (!DefFound && (StrniCmp (TmpCfg, CONFIG_FILENAME, StrSize (CONFIG_FILENAME)) == 0)) {
        OldChosenConfig = aTmp->Index;
        DefFound = TRUE;
      }

      aTmp->FileName = PoolPrint (TmpCfg);
      aTmp->Next = aConfigs;
      aConfigs = aTmp;

      FreePool (TmpCfg);
    }
  }

  DirIterClose (&DirIter);

  if (y) {
    S_FILES   *aTmp = aConfigs;

    aConfigs = 0;

    while (aTmp) {
      S_FILES   *next = aTmp->Next;

      aTmp->Next = aConfigs;
      aConfigs = aTmp;
      aTmp = next;
    }
  }
}

//
// Theme
//

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
          Status = ParseXML (ThemePtr, 0, &ThemeDict);

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

  // fill default to have an ability change theme
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

  GlobalConfig.ScrollButtonWidth                = DefaultConfig.ScrollButtonWidth;
  GlobalConfig.ScrollButtonHeight               = DefaultConfig.ScrollButtonHeight;
  GlobalConfig.ScrollBarDecorationsHeight       = DefaultConfig.ScrollBarDecorationsHeight;
  GlobalConfig.ScrollBackDecorationsHeight      = DefaultConfig.ScrollBackDecorationsHeight;

  if (Embedded) {
    return EFI_SUCCESS;
  }

  GlobalConfig.BootCampStyle = GetPropertyBool (GetProperty (DictPointer, "BootCampStyle"), FALSE);

  Dict = GetProperty (DictPointer, "Background");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Type");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
      if (TO_AUPPER (Dict2->string[0]) == 'S') {
        GlobalConfig.BackgroundScale = Scale;
      } else if (TO_AUPPER (Dict2->string[0]) == 'T') {
        GlobalConfig.BackgroundScale = Tile;
      }
    }

    Dict2 = GetProperty (Dict, "Path");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
      GlobalConfig.BackgroundName = PoolPrint (L"%a", Dict2->string);
    }

    Dict2 = GetProperty (Dict, "Sharp");
    GlobalConfig.BackgroundSharp  = (INT32)GetPropertyInteger (Dict2, DefaultConfig.BackgroundSharp);

    GlobalConfig.BackgroundDark   = GetPropertyBool (GetProperty (Dict, "Dark"), TRUE);
  }

  Dict = GetProperty (DictPointer, "Banner");
  if (Dict != NULL) {
    // retain for legacy themes.
    if (Dict->type == kTagTypeString) {
      GlobalConfig.BannerFileName = PoolPrint (L"%a", Dict->string);
    } else {
      // for new placement settings
      Dict2 = GetProperty (Dict, "Path");
      if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
        GlobalConfig.BannerFileName = PoolPrint (L"%a", Dict2->string);
      }

      Dict2 = GetProperty (Dict, "ScreenEdgeX");
      if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
        if (AsciiStrCmp (Dict2->string, "left") == 0) {
          GlobalConfig.BannerEdgeHorizontal = SCREEN_EDGE_LEFT;
        } else if (AsciiStrCmp (Dict2->string, "right") == 0) {
          GlobalConfig.BannerEdgeHorizontal = SCREEN_EDGE_RIGHT;
        }
      }

      Dict2 = GetProperty (Dict, "ScreenEdgeY");
      if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
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
    if (GetPropertyBool (GetProperty (Dict, "Swap"), FALSE)) {
      GlobalConfig.HideBadges |= HDBADGES_SWAP;
      //DBG ("OS main and drive as badge\n");
    }

    if (GetPropertyBool (GetProperty (Dict, "Show"), FALSE)) {
      GlobalConfig.HideBadges |= HDBADGES_SHOW;
    }

    if (GetPropertyBool (GetProperty (Dict, "Inline"), FALSE)) {
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
    if (!GetPropertyBool (GetProperty (Dict, "Banner"), TRUE)) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BANNER;
    }

    if (!GetPropertyBool (GetProperty (Dict, "Functions"), TRUE)) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_FUNCS;
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_FUNCS)) {
      if (!GetPropertyBool (GetProperty (Dict, "Help"), TRUE)) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HELP;
      }

      if (!GetPropertyBool (GetProperty (Dict, "Reset"), TRUE)) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_RESET;
      }

      if (!GetPropertyBool (GetProperty (Dict, "Shutdown"), TRUE)) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SHUTDOWN;
      }

      if (!GetPropertyBool (GetProperty (Dict, "Exit"), TRUE)) {
        GlobalConfig.HideUIFlags |= HIDEUI_FLAG_EXIT;
      }
    }

    if (!GetPropertyBool (GetProperty (Dict, "Tools"), TRUE)) {
      GlobalConfig.DisableFlags |= HIDEUI_FLAG_TOOLS;
    }

    if (!GetPropertyBool (GetProperty (Dict, "Label"), TRUE)) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_LABEL;
    }

    if (!GetPropertyBool (GetProperty (Dict, "RevisionText"), TRUE)) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_REVISION_TEXT;
    }

    if (!GetPropertyBool (GetProperty (Dict, "HelpText"), TRUE)) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HELP_TEXT;
    }

    if (!GetPropertyBool (GetProperty (Dict, "MenuTitle"), TRUE)) {
      GlobalConfig.HideUIFlags |= HIDEUI_FLAG_MENU_TITLE;
    }

    if (!GetPropertyBool (GetProperty (Dict, "MenuTitleImage"), TRUE)) {
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
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
      GlobalConfig.SelectionSmallFileName = PoolPrint (L"%a", Dict2->string);
    }

    Dict2 = GetProperty (Dict, "Big");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
      GlobalConfig.SelectionBigFileName = PoolPrint (L"%a", Dict2->string);
    }

    Dict2 = GetProperty (Dict, "Indicator");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
      GlobalConfig.SelectionIndicatorName = PoolPrint (L"%a", Dict2->string);
    }

    GlobalConfig.SelectionOnTop = GetPropertyBool (GetProperty (Dict, "OnTop"), FALSE);

    GlobalConfig.NonSelectedGrey = GetPropertyBool (GetProperty (Dict, "ChangeNonSelectedGrey"), FALSE);
  }

  Dict = GetProperty (DictPointer, "Scroll");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Width");
    GlobalConfig.ScrollButtonWidth = GetPropertyInteger (Dict2, DefaultConfig.ScrollButtonWidth);

    Dict2 = GetProperty (Dict, "Height");
    GlobalConfig.ScrollButtonHeight = GetPropertyInteger (Dict2, DefaultConfig.ScrollButtonHeight);

    Dict2 = GetProperty (Dict, "BarHeight");
    GlobalConfig.ScrollBarDecorationsHeight = GetPropertyInteger (Dict2, DefaultConfig.ScrollBarDecorationsHeight);

    Dict2 = GetProperty (Dict, "ScrollHeight");
    GlobalConfig.ScrollBackDecorationsHeight = GetPropertyInteger (Dict2, DefaultConfig.ScrollBackDecorationsHeight);
  }

  Dict = GetProperty (DictPointer, "Font");
  if (Dict != NULL) {
    Dict2 = GetProperty (Dict, "Type");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
      if ((TO_AUPPER (Dict2->string[0]) == 'A') || (TO_AUPPER (Dict2->string[0]) == 'B')) {
        GlobalConfig.Font = FONT_ALFA;
      } else if ((TO_AUPPER (Dict2->string[0]) == 'G') || (TO_AUPPER (Dict2->string[0]) == 'W')) {
        GlobalConfig.Font = FONT_GRAY;
      } else if (TO_AUPPER (Dict2->string[0]) == 'L') {
        GlobalConfig.Font = FONT_LOAD;
      }
    }

    Dict2 = GetProperty (Dict, "Path");
    if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
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

    Dict2 = GetProperty (Dict, "CharRows");
    GlobalConfig.CharRows = GetPropertyInteger (Dict2, 0);
  }

  Dict = GetProperty (DictPointer, "Anime");
  if (Dict != NULL) {
    INTN   i, Count = Dict->size;

    for (i = 0; i < Count; i++) {
      GUI_ANIME   *Anime;

      if (EFI_ERROR (GetElement (Dict, i, Count, &Dict3))) {
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
      if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
        Anime->Path = PoolPrint (L"%a", Dict2->string);
      }

      Dict2 = GetProperty (Dict3, "Frames");
      Anime->Frames = (UINTN)GetPropertyInteger (Dict2, Anime->Frames);

      Dict2 = GetProperty (Dict3, "FrameTime");
      Anime->FrameTime = (UINTN)GetPropertyInteger (Dict2, Anime->FrameTime);

      Dict2 = GetProperty (Dict3, "ScreenEdgeX");
      if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
        if (AsciiStriCmp (Dict2->string, "left") == 0) {
          Anime->ScreenEdgeHorizontal = SCREEN_EDGE_LEFT;
        } else if (AsciiStriCmp (Dict2->string, "right") == 0) {
          Anime->ScreenEdgeHorizontal = SCREEN_EDGE_RIGHT;
        }
      }

      Dict2 = GetProperty (Dict3, "ScreenEdgeY");
      if ((Dict2 != NULL) && (Dict2->type == kTagTypeString)) {
        if (AsciiStriCmp (Dict2->string, "top") == 0) {
          Anime->ScreenEdgeVertical = SCREEN_EDGE_TOP;
        } else if (AsciiStriCmp (Dict2->string, "bottom") == 0) {
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

      Anime->Once = GetPropertyBool (GetProperty (Dict3, "Once"), FALSE);

      // Add the anime to the list
      if ((Anime->ID == 0) || (Anime->Path == NULL)) {
        FreePool (Anime);
      } else {
        if (GuiAnime != NULL) { //second anime or further
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
        }

        GuiAnime = Anime;
      }
    }
  }

  if (GlobalConfig.BackgroundName == NULL) {
    GlobalConfig.BackgroundName = PoolPrint (L"%s.png", DEFAULT_THEME_BACKGROUND_FILENAME);
  }

  if (GlobalConfig.BannerFileName == NULL) {
    GlobalConfig.BannerFileName = PoolPrint (L"%s.png", BuiltinIconTable[BUILTIN_ICON_BANNER].Path);
  }

  if (GlobalConfig.SelectionSmallFileName == NULL) {
    GlobalConfig.SelectionSmallFileName = PoolPrint (L"%s.png", BuiltinIconTable[BUILTIN_SELECTION_SMALL].Path);
  }

  if (GlobalConfig.SelectionBigFileName == NULL) {
    GlobalConfig.SelectionBigFileName = PoolPrint (L"%s.png", BuiltinIconTable[BUILTIN_SELECTION_BIG].Path);
  }

  if (GlobalConfig.SelectionIndicatorName == NULL) {
    GlobalConfig.SelectionIndicatorName = PoolPrint (L"%s.png", BuiltinIconTable[BUILTIN_SELECTION_INDICATOR].Path);
  }

  if (GlobalConfig.FontFileName == NULL) {
    GlobalConfig.FontFileName = PoolPrint (L"%s.png", DEFAULT_THEME_FONT_FILENAME);
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

  if (gSettings.TextOnly) {
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
    goto Finish;
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
      NvramTheme = GetNvramVariable (NvramData[kCloverTheme].VariableName, NvramData[kCloverTheme].Guid, NULL, &Size);
      if (NvramTheme != NULL) {
        TestTheme = PoolPrint (L"%a", NvramTheme);
        if (StriCmp (TestTheme, CONFIG_THEME_EMBEDDED) == 0) {
          goto Finish;
        }

        if (StriCmp (TestTheme, CONFIG_THEME_RANDOM) == 0) {
          ThemeDict = LoadTheme (RndTheme);
          goto Finish;
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
          goto Finish;
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

  Finish:

  if (TestTheme != NULL) {
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
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  UINTN             i = 0, y = 0;

  DbgHeader ("GetListOfThemes");

  DirIterOpen (SelfRootDir, DIR_THEMES, &DirIter);

  while (DirIterNext (&DirIter, 1, NULL, &DirEntry)) {
    if (
      (DirEntry->FileName[0] != '.') &&
      (StriCmp (DirEntry->FileName, CONFIG_THEME_EMBEDDED) != 0) &&
      (StriCmp (DirEntry->FileName, CONFIG_THEME_RANDOM) != 0)
    ) {
      S_FILES   *aTmp = AllocateZeroPool (sizeof (S_FILES));

      MsgLog ("- [%02d]: %s\n", i++, DirEntry->FileName);

      aTmp->Index = OldChosenTheme = y++;

      aTmp->FileName = PoolPrint (DirEntry->FileName);
      aTmp->Next = aThemes;
      aThemes = aTmp;
    }
  }

  DirIterClose (&DirIter);

  if (y) {
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
ParseBootSettings (
  TagPtr    CurrentDict
) {
  TagPtr  DictPointer, Prop;

  // Boot settings.
  // Discussion. Why Arguments is here? It should be SystemParameters property!
  // we will read them again because of change in GUI menu. It is not only EarlySettings
  DictPointer = GetProperty (CurrentDict, "Boot");
  if (DictPointer != NULL) {
    Prop = GetProperty (DictPointer, "Arguments");
    if (
      (Prop != NULL) &&
      (Prop->type == kTagTypeString) &&
      (AsciiStrStr (gSettings.BootArgs, Prop->string) == NULL)
    ) {
      UINTN   Len = ARRAY_SIZE (gSettings.BootArgs);

      AsciiStrnCpyS (gSettings.BootArgs, Len, Prop->string, Len - 1);
      //gBootChanged = TRUE;
    }

    if (gGuiIsReady) {
      goto SkipInitialBoot;
    }

    Prop = GetProperty (DictPointer, "Timeout");
    if (Prop != NULL) {
      gSettings.Timeout = (INT32)GetPropertyInteger (Prop, -1);
      DBG ("Timeout set to %d\n", gSettings.Timeout);
    }

    gSettings.FastBoot = GetPropertyBool (GetProperty (DictPointer, "Fast"), FALSE);

    gSettings.NoEarlyProgress = GetPropertyBool (GetProperty (DictPointer, "NoEarlyProgress"), TRUE);

    // defaults if "DefaultVolume" is not present or is empty
    gSettings.LastBootedVolume = FALSE;
    //gSettings.DefaultVolume    = NULL;

    Prop = GetProperty (DictPointer, "DefaultVolume");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
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
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      UINTN   Len = (AsciiStrSize (Prop->string) * sizeof (CHAR16));

      gSettings.DefaultLoader = AllocateZeroPool (Len);
      AsciiStrToUnicodeStrS (Prop->string, gSettings.DefaultLoader, Len);
    }

    // XMP memory profiles
    Prop = GetProperty (DictPointer, "XMPDetection");
    if (Prop != NULL) {
      gSettings.XMPDetection = 0;

      if (Prop->type == kTagTypeFalse) {
        gSettings.XMPDetection = -1;
      } else if (Prop->type == kTagTypeString) {
        if (
          (TO_AUPPER (Prop->string[0]) == 'N') ||
          (Prop->string[0] == '-')
        ) {
          gSettings.XMPDetection = -1;
        } else {
          gSettings.XMPDetection = (INT8)AsciiStrDecimalToUintn (Prop->string);
        }
      } else if (Prop->type == kTagTypeInteger) {
        gSettings.XMPDetection = (INT8)(UINTN)Prop->integer;//Prop->string;
      }

      // Check that the setting value is sane
      if ((gSettings.XMPDetection < -1) || (gSettings.XMPDetection > 2)) {
        gSettings.XMPDetection = -1;
      }
    }

    SkipInitialBoot:

    gSettings.DebugLog = GetPropertyBool (GetProperty (DictPointer, "DebugLog"), FALSE);

    gSettings.NeverHibernate = GetPropertyBool (GetProperty (DictPointer, "NeverHibernate"), FALSE);
  }
}

VOID
ParseGUISettings (
  TagPtr    CurrentDict
) {
  TagPtr  DictPointer, Dict, Prop;

  DictPointer = GetProperty (CurrentDict, "GUI");
  if (DictPointer != NULL) {
    gSettings.TextOnly = GetPropertyBool (GetProperty (DictPointer, "TextOnly"), FALSE);

    if (gGuiIsReady) {
      goto SkipInitialBoot;
    }

    Prop = GetProperty (DictPointer, "ScreenResolution");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      GlobalConfig.ScreenResolution = PoolPrint (L"%a", Prop->string);
    }

    // hide by name/uuid
    Prop = GetProperty (DictPointer, "Hide");
    if (Prop != NULL) {
      INTN   i, Count = Prop->size;

      if (Count > 0) {
        gSettings.HVCount = 0;
        gSettings.HVHideStrings = AllocateZeroPool (Count * sizeof (CHAR16 *));

        if (gSettings.HVHideStrings) {
          for (i = 0; i < Count; i++) {
            if (EFI_ERROR (GetElement (Prop, i, Count, &Dict))) {
              continue;
            }

            if (Dict == NULL) {
              break;
            }

            if (Dict->type == kTagTypeString) {
              gSettings.HVHideStrings[gSettings.HVCount] = PoolPrint (L"%a", Dict->string);

              if (gSettings.HVHideStrings[gSettings.HVCount]) {
                DBG ("Hiding entries with string %s\n", gSettings.HVHideStrings[gSettings.HVCount]);
                gSettings.HVCount++;
              }
            }
          }
        }
      }
    }

    // Disable loader scan
    Prop = GetProperty (DictPointer, "Scan");
    if (Prop != NULL) {
      if (Prop->type == kTagTypeDict) {
        gSettings.LinuxScan = GetPropertyBool (GetProperty (Prop, "Linux"), TRUE);
        gSettings.AndroidScan = GetPropertyBool (GetProperty (Prop, "Android"), TRUE);
        gSettings.DisableEntryScan = !GetPropertyBool (GetProperty (Prop, "Entries"), FALSE);
        gSettings.DisableToolScan = !GetPropertyBool (GetProperty (Prop, "Tool"), FALSE);
      } else if (!GetPropertyBool (Prop, TRUE)) {
        gSettings.DisableEntryScan = TRUE;
        gSettings.DisableToolScan  = TRUE;
      }
    }

    // Custom entries
    Prop = GetProperty (DictPointer, "Custom");
    if (Prop != NULL) {
      Dict = GetProperty (Prop, "Entries");
      if (Dict != NULL) {
        CUSTOM_LOADER_ENTRY   *Entry;
        INTN                  i, Count = Dict->size;
        TagPtr                Dict3;

        if (Count > 0) {
          for (i = 0; i < Count; i++) {
            if (EFI_ERROR (GetElement (Dict, i, Count, &Dict3))) {
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

      Dict = GetProperty (Prop, "Tool");
      if (Dict != NULL) {
        CUSTOM_TOOL_ENTRY   *Entry;
        INTN                i, Count = Dict->size;
        TagPtr              Dict2;

        if (Count > 0) {
          for (i = 0; i < Count; i++) {
            if (EFI_ERROR (GetElement (Dict, i, Count, &Dict2))) {
              continue;
            }

            if (Dict2 == NULL) {
              break;
            }

            // Allocate an entry
            Entry = (CUSTOM_TOOL_ENTRY *)AllocateZeroPool (sizeof (CUSTOM_TOOL_ENTRY));
            if (Entry) {
              // Fill it in
              if (!FillinCustomTool (Entry, Dict2) || !AddCustomToolEntry (Entry)) {
                FreePool (Entry);
              }
            }
          }
        }
      }
    }

    SkipInitialBoot:

    Prop = GetProperty (DictPointer, "DarwinDiskTemplate");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      AsciiStrToUnicodeStrS (Prop->string, gSettings.DarwinDiskTemplate, ARRAY_SIZE (gSettings.DarwinDiskTemplate));
    }

    Prop = GetProperty (DictPointer, "DarwinRecoveryDiskTemplate");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      AsciiStrToUnicodeStrS (Prop->string, gSettings.DarwinRecoveryDiskTemplate, ARRAY_SIZE (gSettings.DarwinRecoveryDiskTemplate));
    }

    Prop = GetProperty (DictPointer, "DarwinInstallerDiskTemplate");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      AsciiStrToUnicodeStrS (Prop->string, gSettings.DarwinInstallerDiskTemplate, ARRAY_SIZE (gSettings.DarwinInstallerDiskTemplate));
    }

    Prop = GetProperty (DictPointer, "LinuxDiskTemplate");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      AsciiStrToUnicodeStrS (Prop->string, gSettings.LinuxDiskTemplate, ARRAY_SIZE (gSettings.LinuxDiskTemplate));
    }

    //Prop = GetProperty (DictPointer, "AndroidDiskTemplate");
    //if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    //  AsciiStrToUnicodeStrS (Prop->string, gSettings.AndroidDiskTemplate, ARRAY_SIZE (gSettings.AndroidDiskTemplate));
    //}

    Prop = GetProperty (DictPointer, "WindowsDiskTemplate");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      AsciiStrToUnicodeStrS (Prop->string, gSettings.WindowsDiskTemplate, ARRAY_SIZE (gSettings.WindowsDiskTemplate));
    }

    if (
      !gSettings.TextOnly &&
      (!gGuiIsReady || gThemeChanged)
    ) {
      Prop = GetProperty (DictPointer, "Theme");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        if (GlobalConfig.Theme) {
          FreePool (GlobalConfig.Theme);
        }

        GlobalConfig.Theme = PoolPrint (L"%a", Prop->string);
        MsgLog ("Set theme: %s\n", GlobalConfig.Theme);
        SetThemeIndex ();
      }
    }
  }
}

VOID
ParseSMBIOSSettings (
  TagPtr    CurrentDict
) {
  CHAR16    UStr[40];
  TagPtr    DictPointer, Dict, Prop, Prop2, Prop3;
  BOOLEAN   BuiltInModel = FALSE;

  DictPointer = GetProperty (CurrentDict, "SMBIOS");
  if (DictPointer == NULL) {
    return;
  }

  Prop = GetProperty (DictPointer, "ProductName");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    MACHINE_TYPES   Model;

    AsciiStrCpyS (gSettings.ProductName, ARRAY_SIZE (gSettings.ProductName), Prop->string);
    // let's fill all other fields based on this ProductName to serve as default
    Model = GetModelFromString (gSettings.ProductName);
    BuiltInModel = (Model != MaxMachineType);
    // if new model then fill at least as MinMachineType, except custom ProductName something else?
    SetDMISettingsForModel (BuiltInModel ? Model : MinMachineType + 1, FALSE);
  }

  Prop = GetProperty (DictPointer, "BiosVendor");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.VendorName, ARRAY_SIZE (gSettings.VendorName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BiosVersion");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.RomVersion, ARRAY_SIZE (gSettings.RomVersion), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BiosReleaseDate");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.ReleaseDate, ARRAY_SIZE (gSettings.ReleaseDate), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Manufacturer");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.ManufactureName, ARRAY_SIZE (gSettings.ManufactureName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Version");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.VersionNr, ARRAY_SIZE (gSettings.VersionNr), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Family");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.FamilyName, ARRAY_SIZE (gSettings.FamilyName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "SerialNumber");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    ZeroMem (gSettings.SerialNr, ARRAY_SIZE (gSettings.SerialNr));
    AsciiStrCpyS (gSettings.SerialNr, ARRAY_SIZE (gSettings.SerialNr), Prop->string);
  }

  Prop = GetProperty (DictPointer, "SmUUID");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    if (IsValidGuidAsciiString (Prop->string)) {
      AsciiStrToUnicodeStrS(Prop->string, UStr, ARRAY_SIZE (UStr));
      /* Status = */ StrToGuid (UStr, &gSettings.SmUUID);
      gSettings.SmUUIDConfig = TRUE;
    } else {
      DBG ("Error: invalid SmUUID '%a' - should be in the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\n", Prop->string);
    }
  }

  Prop = GetProperty (DictPointer, "BoardManufacturer");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.BoardManufactureName, ARRAY_SIZE (gSettings.BoardManufactureName), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BoardSerialNumber");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.BoardSerialNumber, ARRAY_SIZE (gSettings.BoardSerialNumber), Prop->string);
  }

  Prop = GetProperty (DictPointer, "Board-ID");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.BoardNumber, ARRAY_SIZE (gSettings.BoardNumber), Prop->string);
  }

  Prop = GetProperty (DictPointer, "BoardVersion");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.BoardVersion, ARRAY_SIZE (gSettings.BoardVersion), Prop->string);
  } else if (!BuiltInModel) {
    AsciiStrCpyS (gSettings.BoardVersion, ARRAY_SIZE (gSettings.BoardVersion), gSettings.ProductName);
  }

  Prop = GetProperty (DictPointer, "BoardType");
  gSettings.BoardType = (UINT8)GetPropertyInteger (Prop, gSettings.BoardType);

  gSettings.Mobile = GetPropertyBool (GetProperty (DictPointer, "Mobile"), FALSE);
  if (!gSettings.Mobile && !BuiltInModel) {
    gSettings.Mobile = (AsciiStriStr (gSettings.ProductName, "MacBook") != NULL);
  }

  Prop = GetProperty (DictPointer, "LocationInChassis");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.LocationInChassis, ARRAY_SIZE (gSettings.LocationInChassis), Prop->string);
  }

  Prop = GetProperty (DictPointer, "ChassisManufacturer");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
    AsciiStrCpyS (gSettings.ChassisManufacturer, ARRAY_SIZE (gSettings.ChassisManufacturer), Prop->string);
  }

  Prop = GetProperty (DictPointer, "ChassisAssetTag");
  if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
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

  Prop = GetProperty (DictPointer, "FirmwareFeaturesMask");
  gFwFeaturesMask = (UINT32)GetPropertyInteger (Prop, gFwFeaturesMask);

  Prop = GetProperty (DictPointer, "PlatformFeature");
  gSettings.PlatformFeature = (UINT64)GetPropertyInteger (Prop, 0xFFFF);

  gSettings.TrustSMBIOS = GetPropertyBool (GetProperty (DictPointer, "Trust"), FALSE);

  // Inject memory tables into SMBIOS
  Prop = GetProperty (DictPointer, "Memory");
  if (Prop != NULL) {
    // Get memory table count
    Prop2   = GetProperty (Prop, "SlotCount");
    gRAM.UserInUse = (UINT8)GetPropertyInteger (Prop2, 0);

    // Get memory channels
    Prop2 = GetProperty (Prop, "Channels");
    gRAM.UserChannels = (UINT8)GetPropertyInteger (Prop2, 0);

    // Get memory tables
    Prop2 = GetProperty (Prop, "Modules");
    if (Prop2 != NULL) {
      INTN    i, Count = Prop2->size;

      Prop3 = NULL;

      for (i = 0; i < Count; i++) {
        UINT8           Slot = MAX_RAM_SLOTS;
        RAM_SLOT_INFO   *SlotPtr;

        if (EFI_ERROR (GetElement (Prop2, i, Count, &Prop3))) {
          continue;
        }

        if (Prop3 == NULL) {
          break;
        }

        // Get memory slot
        Dict = GetProperty (Prop3, "Slot");
        if (Dict == NULL) {
          continue;
        }

        if (Dict->type == kTagTypeString) {
          Slot = (UINT8)AsciiStrDecimalToUintn (Dict->string);
        } else if (Dict->type == kTagTypeInteger) {
          Slot = (UINT8)(UINTN)Dict->integer;
        } else {
          continue;
        }

        if (Slot >= MAX_RAM_SLOTS) {
          break;
        }

        SlotPtr = &gRAM.User[Slot];

        // Get memory size
        Dict = GetProperty (Prop3, "Size");
        SlotPtr->ModuleSize = (UINT32)GetPropertyInteger (Dict, SlotPtr->ModuleSize);

        // Get memory frequency
        Dict = GetProperty (Prop3, "Frequency");
        SlotPtr->Frequency = (UINT32)GetPropertyInteger (Dict, SlotPtr->Frequency);

        // Get memory vendor
        Dict = GetProperty (Prop3, "Vendor");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          SlotPtr->Vendor = Dict->string;
        }

        // Get memory part number
        Dict = GetProperty (Prop3, "Part");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          SlotPtr->PartNo = Dict->string;
        }

        // Get memory serial number
        Dict = GetProperty (Prop3, "Serial");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          SlotPtr->SerialNo = Dict->string;
        }

        // Get memory type
        SlotPtr->Type = MemoryTypeDdr3;
        Dict = GetProperty (Prop3, "Type");
        if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
          if (AsciiStriCmp (Dict->string, "DDR4") == 0) {
            SlotPtr->Type = MemoryTypeDdr4;
          } else if (AsciiStriCmp (Dict->string, "DDR3") == 0) {
            SlotPtr->Type = MemoryTypeDdr3;
          } else if (AsciiStriCmp (Dict->string, "DDR2") == 0) {
            SlotPtr->Type = MemoryTypeDdr2;
          } else if (AsciiStriCmp (Dict->string, "DDR") == 0) {
            SlotPtr->Type = MemoryTypeDdr;
          }
        }

        SlotPtr->InUse = (SlotPtr->ModuleSize > 0);
        if (SlotPtr->InUse && (gRAM.UserInUse <= Slot)) {
          gRAM.UserInUse = Slot + 1;
        }
      }

      if (gRAM.UserInUse > 0) {
        gSettings.InjectMemoryTables = TRUE;
      }
    }
  }

  Prop = GetProperty (DictPointer, "Slots");
  if (Prop != NULL) {
    INTN          DeviceN = 0, Index, Count = Prop->size;
    SLOT_DEVICE   *SlotDevice;

    Prop3 = NULL;

    for (Index = 0; Index < Count; ++Index) {
      INTN    Bus = 0, Device = 0, Function = 0;

      if (EFI_ERROR (GetElement (Prop, Index, Count, &Prop3))) {
        continue;
      }

      if (Prop3 == NULL) {
        break;
      }

      Prop2 = GetProperty (Prop3, "PciAddr");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        CHAR8     *Str = Prop2->string;

        if ((Str[2] != ':') || (Str[5] != '.')) {
          //DBG (" wrong PciAddr string: %a\n", Str);
          continue;
        }

        Bus = HexStrToUint8 (Str);
        Device = HexStrToUint8 (&Str[3]);
        Function = HexStrToUint8 (&Str[6]);
      }

      if (!Index) {
        DBG ("Slots->Device:\n");
      }

      Prop2 = GetProperty (Prop3, "Device");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        if (
          (AsciiStriCmp (Prop2->string, "ATI") != 0) ||
          (AsciiStriCmp (Prop2->string, "NVidia") != 0) ||
          (AsciiStriCmp (Prop2->string, "IntelGFX") != 0) ||
          (AsciiStriCmp (Prop2->string, "LAN") != 0) ||
          (AsciiStriCmp (Prop2->string, "WIFI") != 0) ||
          (AsciiStriCmp (Prop2->string, "Firewire") != 0) ||
          (AsciiStriCmp (Prop2->string, "HDMI") != 0) ||
          (AsciiStriCmp (Prop2->string, "USB") != 0) ||
          (AsciiStriCmp (Prop2->string, "NVME") != 0)
         ) {
          DBG (" - unknown device %a, ignored\n", Prop2->string);
          continue;
        }
      } else {
        DBG (" - no device  property for slot\n");
        continue;
      }

      SlotDevice = &SmbiosSlotDevices[DeviceN];

      SlotDevice->Valid = TRUE;

      SlotDevice->PCIDevice.dev.addr = (UINT32)PCIADDR (Bus, Device, Function);

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
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "%a", Prop2->string);
      } else {
        AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "PCI Slot %d", DeviceN);
      }

      DBG (" - %a\n", SlotDevice->SlotName);

      DeviceN++;
    }
  }
}

VOID
ParseGraphicsSettings (
  TagPtr    CurrentDict
) {
  TagPtr    DictPointer, Dict, Prop;

  DictPointer = GetProperty (CurrentDict, "Graphics");
  if (DictPointer != NULL) {
    UINTN   i;

    gSettings.InjectEDID = GetPropertyBool (GetProperty (DictPointer, "InjectEDID"), FALSE);
    if (gSettings.InjectEDID) {
      gSettings.CustomEDID = GetDataSetting (DictPointer, "CustomEDID", &i);
    }

    Dict = GetProperty (DictPointer, "Inject");
    if (Dict != NULL) {
      if (GetPropertyBool (Dict, FALSE)) {
        gSettings.InjectIntel  = TRUE;
        gSettings.InjectATI    = TRUE;
        gSettings.InjectNVidia = TRUE;
      } else if (Dict->type == kTagTypeDict) {
        gSettings.InjectIntel = GetPropertyBool (GetProperty (Dict, "Intel"), FALSE);
        gSettings.InjectATI = GetPropertyBool (GetProperty (Dict, "ATI"), FALSE);
        gSettings.InjectNVidia = GetPropertyBool (GetProperty (Dict, "NVidia"), FALSE);
      }
    }

    for (i = 0; i < NGFX; i++) {
      gGraphics[i].LoadVBios = gSettings.LoadVBios; //default
    }

    if (gSettings.InjectIntel) {
      Prop = GetProperty (DictPointer, "ig-platform-id");
      gSettings.IgPlatform = (UINT32)GetPropertyInteger (Prop, gSettings.IgPlatform);

      Prop = GetProperty (DictPointer, "snb-platform-id");
      gSettings.IgPlatform = (UINT32)GetPropertyInteger (Prop, gSettings.IgPlatform);
    }

    if (gSettings.InjectIntel || gSettings.InjectATI) {
      Prop = GetProperty (DictPointer, "DualLink");
      gSettings.DualLink = (UINT32)GetPropertyInteger (Prop, gSettings.DualLink);
    }

    if (gSettings.InjectATI) {
      Prop = GetProperty (DictPointer, "FBName");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        AsciiStrToUnicodeStrS (Prop->string, gSettings.FBName, ARRAY_SIZE (gSettings.FBName));
      }
    }

    if (gSettings.InjectATI || gSettings.InjectNVidia) {
      Prop = GetProperty (DictPointer, "VideoPorts");
      gSettings.VideoPorts = (UINT16)GetPropertyInteger (Prop, gSettings.VideoPorts);

      Prop = GetProperty (DictPointer, "BootDisplay");
      gSettings.BootDisplay = (INT8)GetPropertyInteger (Prop, -1);

      Prop = GetProperty (DictPointer, "VRAM");
      gSettings.VRAM = LShiftU64 ((UINTN)GetPropertyInteger (Prop, (INTN)gSettings.VRAM), 20); //Mb -> bytes

      gSettings.LoadVBios = GetPropertyBool (GetProperty (DictPointer, "LoadVBios"), FALSE);

      FillCardList (DictPointer); //#@ Getcardslist
    }

    if (gSettings.InjectNVidia) {
      gSettings.NvidiaSingle = GetPropertyBool (GetProperty (DictPointer, "NvidiaSingle"), TRUE);

      Prop = GetProperty (DictPointer, "display-cfg");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        Hex2Bin (Prop->string, (UINT8 *)&gSettings.Dcfg[0], 8);
      }

      Prop = GetProperty (DictPointer, "NVCAP");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        Hex2Bin (Prop->string, (UINT8 *)&gSettings.NVCAP[0], 20);
        DBG ("Read NVCAP: %a\n", Bytes2HexStr (gSettings.NVCAP, sizeof (gSettings.NVCAP)));
      }
    }
  }
}

VOID
ParseDevicesSettings (
  TagPtr    CurrentDict
) {
  EFI_STATUS    Status;
  TagPtr        DictPointer, Dict, Prop, Prop2, Prop3;

  DictPointer = GetProperty (CurrentDict, "Devices");
  if (DictPointer != NULL) {
    gSettings.UseIntelHDMI = GetPropertyBool (GetProperty (DictPointer, "UseIntelHDMI"), FALSE);
    gSettings.HDALayoutId = (UINT32)GetPropertyInteger (GetProperty (DictPointer, "HDALayoutId"), gSettings.HDALayoutId);
    gSettings.BacklightLevel = (UINT16)GetPropertyInteger (GetProperty (DictPointer, "BacklightLevel"), gSettings.BacklightLevel);
    gSettings.IntelBacklight = GetPropertyBool (GetProperty (DictPointer, "SetIntelBacklight"), FALSE);
    gSettings.NoDefaultProperties = GetPropertyBool (GetProperty (DictPointer, "NoDefaultProperties"), FALSE);
    gSettings.EFIStringInjector = FALSE;

    Dict = GetProperty (DictPointer, "DeviceProperties");
    if (Dict != NULL) {
      gSettings.EFIStringInjector = !GetPropertyBool (GetProperty (Dict, "Disabled"), FALSE);

      if (gSettings.EFIStringInjector) {
        gSettings.EFIStringInjector = FALSE;

        Prop = GetProperty (Dict, "Properties");
        if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
          EFI_PHYSICAL_ADDRESS    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS; //0xFE000000;
          UINTN                   StrLength = AsciiStrLen (Prop->string);

          cDeviceProperties = AllocateZeroPool (StrLength + 1);
          AsciiStrCpyS (cDeviceProperties, StrLength + 1, Prop->string);

          Status = gBS->AllocatePages (
                           AllocateMaxAddress,
                           EfiACPIReclaimMemory,
                           EFI_SIZE_TO_PAGES (StrLength) + 1,
                           &BufferPtr
                         );

          if (!EFI_ERROR (Status)) {
            cProperties = (UINT8 *)(UINTN)BufferPtr;
            cPropSize   = (UINT32)(StrLength >> 1);
            cPropSize   = Hex2Bin (cDeviceProperties, cProperties, cPropSize);
            gSettings.EFIStringInjector = TRUE;
            DBG ("Injected EFIString of length %d\n", cPropSize);
          }
        }
      }
    }

    // can't use EFIString with any Arbitrary / (custom) AddProperties

    if (!gSettings.EFIStringInjector) {
      Prop = GetProperty (DictPointer, "Arbitrary");
      if (Prop != NULL) {
        INTN            Index, Count = Prop->size;
        DEV_PROPERTY    *DevProp;

        if (Count > 0) {
          DBG ("Add %d devices:\n", Count);
          for (Index = 0; Index < Count; Index++) {
            UINTN   DeviceAddr = 0U;

            Status = GetElement (Prop, Index, Count, &Prop2);

            DBG (" - [%02d]:", Index);

             if (EFI_ERROR (Status) || (Prop2 == NULL)) {
              DBG (" %r parsing / empty element\n", Status);
              continue;
            }

            Dict = GetProperty (Prop2, "Comment");
            if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
              DBG (" (%a)", Index, Dict->string);
            }

            Dict = GetProperty (Prop2, "PciAddr");
            if ((Dict != NULL) && (Dict->type == kTagTypeString)) {
              INTN      Bus, Dev, Func;
              CHAR8     *Str = Dict->string;

              if ((Str[2] != ':') || (Str[5] != '.')) {
                DBG (" wrong PciAddr string: %a\n", Str);
                continue;
              }

              Bus   = HexStrToUint8 (Str);
              Dev   = HexStrToUint8 (&Str[3]);
              Func  = HexStrToUint8 (&Str[6]);
              DeviceAddr = PCIADDR (Bus, Dev, Func);

              DBG (" %02x:%02x.%02x\n", Bus, Dev, Func);
            } else {
              DBG (" empty PciAddr\n");
              continue;
            }

            Dict = GetProperty (Prop2, "CustomProperties");
            if ((Dict != NULL) && (Dict->type == kTagTypeArray)) {
              TagPtr    Dict3;
              INTN      PropIndex, PropCount = Dict->size;

              for (PropIndex = 0; PropIndex < PropCount; PropIndex++) {
                UINTN     Size = 0;

                if (!EFI_ERROR (GetElement (Dict, PropIndex, PropCount, &Dict3))) {
                  DevProp = gSettings.AddProperties;
                  gSettings.AddProperties = AllocateZeroPool (sizeof (DEV_PROPERTY));
                  gSettings.AddProperties->Next = DevProp;

                  gSettings.AddProperties->Device = (UINT32)DeviceAddr;

                  Prop3 = GetProperty (Dict3, "Key");
                  if ((Prop3 != NULL) && (Prop3->type == kTagTypeString)) {
                    gSettings.AddProperties->Key = AllocateCopyPool (AsciiStrSize (Prop3->string), Prop3->string);
                  }

                  Prop3 = GetProperty (Dict3, "Value");
                  if ((Prop3 != NULL) && (Prop3->type == kTagTypeString)) {
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
              } //for () device properties
            }
          } //for () devices
        }

        // make sure some properties already added before rejecting 'AddProperties'
        if (gSettings.AddProperties) {
          gSettings.NrAddProperties = 0xFFFE;
        }
      }

      // can't use Arbitrary with AddProperties

      if (!gSettings.AddProperties && (gSettings.NrAddProperties != 0xFFFE)) {
        Prop = GetProperty (DictPointer, "AddProperties");
        if (Prop != NULL) {
          INTN    i, Count = Prop->size, Index = 0;  //begin from 0 if second enter

          if (Count > 0) {
            DBG ("Add %d properties:\n", Count);
            gSettings.AddProperties = AllocateZeroPool (Count * sizeof (DEV_PROPERTY));

            for (i = 0; i < Count; i++) {
              UINTN   Size = 0;

              Status = GetElement (Prop, i, Count, &Dict);

              DBG (" - [%02d]:", i);

              if (EFI_ERROR (Status) || (Dict == NULL)) {
                DBG (" %r parsing / empty element\n", Status);
                continue;
              }

              Prop2 = GetProperty (Dict, "Device");
              if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
                BOOLEAN   Found = FALSE;

                for (i = 0; i < OptDevicesBitNum; ++i) {
                  if (AsciiStriCmp (Prop2->string, ADEVICES[i].Title) == 0) {
                    gSettings.AddProperties[Index].Device = ADEVICES[i].Bit;
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

              Prop2 = GetProperty (Dict, "Key");
              if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
                gSettings.AddProperties[Index].Key = AllocateCopyPool (AsciiStrSize (Prop2->string), Prop2->string);
              }

              Prop2 = GetProperty (Dict, "Value");
              if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
                //first suppose it is Ascii string
                gSettings.AddProperties[Index].Value = AllocateCopyPool (AsciiStrSize (Prop2->string), Prop2->string);
                gSettings.AddProperties[Index].ValueLen = AsciiStrLen (Prop2->string) + 1;
              } else if (Prop2 && (Prop2->type == kTagTypeInteger)) {
                gSettings.AddProperties[Index].Value = AllocatePool (4);
                CopyMem (gSettings.AddProperties[Index].Value, &(Prop2->integer), 4);
                gSettings.AddProperties[Index].ValueLen = 4;
              } else {
                //else  data
                gSettings.AddProperties[Index].Value = GetDataSetting (Dict, "Value", &Size);
                gSettings.AddProperties[Index].ValueLen = Size;
              }

              DBG (", len: %d\n", gSettings.AddProperties[Index].ValueLen);

              ++Index;
            }

            gSettings.NrAddProperties = Index;
          }
        }
      }
    }

    Prop = GetProperty (DictPointer, "FakeID");
    if (Prop != NULL) {
      Prop2 = GetProperty (Prop, "ATI");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        gSettings.FakeATI  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
      }

      Prop2 = GetProperty (Prop, "NVidia");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        gSettings.FakeNVidia  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
      }

      Prop2 = GetProperty (Prop, "IntelGFX");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        gSettings.FakeIntel  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
      }

      Prop2 = GetProperty (Prop, "LAN");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        gSettings.FakeLAN  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
      }

      Prop2 = GetProperty (Prop, "WIFI");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        gSettings.FakeWIFI  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
      }

      Prop2 = GetProperty (Prop, "IMEI");
      if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
        gSettings.FakeIMEI  = (UINT32)AsciiStrHexToUint64 (Prop2->string);
      }
    }
  }
}

VOID
ParseACPISettings (
  TagPtr    CurrentDict
) {
  EFI_STATUS    Status;
  TagPtr        DictPointer, Dict, Prop, Prop2, Prop3;

  DictPointer = GetProperty (CurrentDict, "ACPI");
  if (DictPointer) {
    Prop = GetProperty (DictPointer, "DropTables");
    if (Prop) {
      INTN      i, Count = Prop->size;
      BOOLEAN   Dropped;

      if (Count > 0) {
        MsgLog ("Dropping %d tables:\n", Count);

        for (i = 0; i < Count; i++) {
          UINT32  Signature = 0, TabLength = 0;
          UINT64  TableId = 0;

          Status = GetElement (Prop, i, Count, &Dict);

          MsgLog (" - [%02d]:", i);

          if (EFI_ERROR (Status) || (Dict == NULL)) {
            MsgLog (" %r parsing / empty element\n", Status);
            continue;
          }

          // Get the table signatures to drop
          Prop2 = GetProperty (Dict, "Signature");
          if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
            CHAR8     s1 = 0, s2 = 0, s3 = 0, s4 = 0, *Str = Prop2->string;

            MsgLog (" signature=\"");

            if (Str) {
              if (*Str) {
                s1 = *Str++;
                MsgLog ("%c", s1);
              }
              if (*Str) {
                s2 = *Str++;
                MsgLog ("%c", s2);
              }
              if (*Str) {
                s3 = *Str++;
                MsgLog ("%c", s3);
              }
              if (*Str) {
                s4 = *Str++;
                MsgLog ("%c", s4);
              }
            }

            Signature = SIGNATURE_32 (s1, s2, s3, s4);
            DBG ("\" (%8.8X)", Signature);
          }

          // Get the table ids to drop
          Prop2 = GetProperty (Dict, "TableId");
          if ((Prop2 != NULL) && (Prop2->type == kTagTypeString)) {
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
          Prop2 = GetProperty (Dict, "Length");
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

    Dict = GetProperty (DictPointer, "DSDT");
    if (Dict != NULL) {
      //gSettings.DsdtName by default is "DSDT.aml", but name "BIOS" will mean autopatch
      Prop = GetProperty (Dict, "Name");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        AsciiStrToUnicodeStrS (Prop->string, gSettings.DsdtName, ARRAY_SIZE (gSettings.DsdtName));
      }

      gSettings.DebugDSDT = GetPropertyBool (GetProperty (Dict, "Debug"), FALSE);

      Prop = GetProperty (Dict, "FixMask");
      gSettings.FixDsdt = (UINT32)GetPropertyInteger (Prop, gSettings.FixDsdt);

      Prop = GetProperty (Dict, "Fixes");
      if (Prop != NULL) {
        //DBG ("Fixes will override DSDT fix mask %08x!\n", gSettings.FixDsdt);

        if (Prop->type == kTagTypeDict) {
          INTN    i;

          gSettings.FixDsdt = 0;

          for (i = 0; i < OptMenuDSDTBitNum; ++i) {
            if (GetPropertyBool (GetProperty (Prop, OPT_MENU_DSDTBIT[i].OptLabel), FALSE)) {
              gSettings.FixDsdt |= OPT_MENU_DSDTBIT[i].Bit;
            }
          }
        }
      }

      DBG (" - final DSDT Fix mask: %08x\n", gSettings.FixDsdt);

      Prop = GetProperty (Dict, "Patches");
      if (Prop != NULL) {
        INTN  i, Count = Prop->size;

        if (Count > 0) {
          gSettings.PatchDsdt = AllocateZeroPool (Count * sizeof (PATCH_DSDT));

          MsgLog ("PatchesDSDT: %d requested\n", Count);

          for (i = 0; i < Count; i++) {
            DBG (" - [%02d]:", i);

            Status = GetElement (Prop, i, Count, &Prop2);

            if (EFI_ERROR (Status) || (Prop2 == NULL)) {
              DBG (" %r parsing / empty element", Status);
            } else {
              UINTN       Size = 0;
              PATCH_DSDT  *PatchDsdt =  AllocateZeroPool (sizeof (PATCH_DSDT));

              PatchDsdt->Find = GetDataSetting (Prop2, "Find", &Size);
              PatchDsdt->LenToFind = (UINT32)Size;
              PatchDsdt->Replace = GetDataSetting (Prop2, "Replace", &Size);
              PatchDsdt->LenToReplace = (UINT32)Size;
              PatchDsdt->Comment = NULL;
              PatchDsdt->Wildcard = (UINT8)GetPropertyInteger (GetProperty (Prop2, "Wildcard"), 0xFF);

              Prop3 = GetProperty (Prop2, "Comment");
              if ((Prop3 != NULL) && (Prop3->type == kTagTypeString)) {
                PatchDsdt->Comment = AllocateCopyPool (AsciiStrSize (Prop3->string), Prop3->string);
              }

              DBG (" (%a)", PatchDsdt->Comment ? PatchDsdt->Comment : "NoLabel");

              if (
                !PatchDsdt->LenToFind ||
                !PatchDsdt->LenToReplace
              ) {
                DBG (" invalid data", Status);
                FreePool (PatchDsdt);
              } else {
                PatchDsdt->Disabled = GetPropertyBool (GetProperty (Prop2, "Disabled"), FALSE);
                if (PatchDsdt->Disabled) {
                  DBG (" disabled");
                }

                PatchDsdt->Next = gSettings.PatchDsdt;
                gSettings.PatchDsdt = PatchDsdt;
                gSettings.PatchDsdtNum++;
              }
            }

            DBG ("\n");
          }

          // reverse
          if (gSettings.PatchDsdtNum && gSettings.PatchDsdt) {
            PATCH_DSDT  *aTmp = gSettings.PatchDsdt;

            gSettings.PatchDsdt = 0;

            while (aTmp->Next) {
              PATCH_DSDT   *next = aTmp->Next;

              aTmp->Next = gSettings.PatchDsdt;
              gSettings.PatchDsdt = aTmp;
              aTmp = next;
            }
          }
        } //if count > 0
      } //if prop PatchesDSDT

      gSettings.ReuseFFFF = GetPropertyBool (GetProperty (Dict, "ReuseFFFF"), FALSE);

      //gSettings.DropOEM_DSM = 0xFFFF;

      Prop   = GetProperty (Dict, "DropOEM_DSM");

      if (Prop != NULL) {
        if (Prop->type == kTagTypeInteger) {
          gSettings.DropOEM_DSM = (UINT16)(UINTN)Prop->integer;
        } else if (Prop->type == kTagTypeDict) {
          INTN    i;

          //gSettings.DropOEM_DSM = 0;

          for (i = 0; i < OptDevicesBitNum; ++i) {
            if (GetPropertyBool (GetProperty (Prop, ADEVICES[i].Title), FALSE)) {
              gSettings.DropOEM_DSM |= ADEVICES[i].Bit;
            }
          }
        } /*else if (!GetPropertyBool (Prop, FALSE)) {
          gSettings.DropOEM_DSM = 0;
        }*/
      }
    }

    Dict = GetProperty (DictPointer, "SSDT");
    if (Dict) {
      Prop2 = GetProperty (Dict, "Generate");
      if (Prop2 != NULL) {
        if (GetPropertyBool (Prop2, FALSE)) {
          gSettings.GeneratePStates = TRUE;
          gSettings.GenerateCStates = TRUE;
        } else if (Prop2->type == kTagTypeDict) {
          gSettings.GeneratePStates = GetPropertyBool (GetProperty (Prop2, "PStates"), FALSE);
          gSettings.GenerateCStates = GetPropertyBool (GetProperty (Prop2, "CStates"), FALSE);
        } else if (!GetPropertyBool (Prop2, FALSE)) {
          gSettings.GeneratePStates = FALSE;
          gSettings.GenerateCStates = FALSE;
        }
      }

      gSettings.DropSSDT  = GetPropertyBool (GetProperty (Dict, "DropOem"), FALSE);

      gSettings.EnableC7 = GetPropertyBool (GetProperty (Dict, "EnableC7"), FALSE);
      gSettings.EnableC6 = GetPropertyBool (GetProperty (Dict, "EnableC6"), FALSE);
      gSettings.EnableC4 = GetPropertyBool (GetProperty (Dict, "EnableC4"), FALSE);
      gSettings.EnableC2 = GetPropertyBool (GetProperty (Dict, "EnableC2"), FALSE);

      gSettings.C3Latency = GetPropertyBool (GetProperty (Dict, "C3Latency"), FALSE);

      gSettings.DoubleFirstState = GetPropertyBool (GetProperty (Dict, "DoubleFirstState"), FALSE);

      Prop = GetProperty (Dict, "MinMultiplier");
      if (Prop != NULL) {
        gSettings.MinMultiplier = (UINT8)GetPropertyInteger (Prop, gSettings.MinMultiplier);
        DBG ("MinMultiplier: %d\n", gSettings.MinMultiplier);
      }

      Prop = GetProperty (Dict, "MaxMultiplier");
      if (Prop != NULL) {
        gSettings.MaxMultiplier = (UINT8)GetPropertyInteger (Prop, gSettings.MaxMultiplier);
        DBG ("MaxMultiplier: %d\n", gSettings.MaxMultiplier);
      }

      Prop = GetProperty (Dict, "PluginType");
      if (Prop != NULL) {
        gSettings.PluginType = (UINT8)GetPropertyInteger (Prop, gSettings.PluginType);
        MsgLog ("PluginType: %d\n", gSettings.PluginType);
      }
    }

    gSettings.DropMCFG = GetPropertyBool (GetProperty (DictPointer, "DropMCFG"), FALSE);

    gSettings.SmartUPS   = GetPropertyBool (GetProperty (DictPointer, "SmartUPS"), FALSE);

    Prop = GetProperty (DictPointer, "SortedOrder");
    if (Prop) {
      INTN   i, Count = Prop->size;

      Prop2 = NULL;

      if (Count > 0) {
        gSettings.SortedACPICount = 0;
        gSettings.SortedACPI = AllocateZeroPool (Count * sizeof (CHAR16 *));

        for (i = 0; i < Count; i++) {
          if (
            !EFI_ERROR (GetElement (Prop, i, Count, &Prop2)) &&
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
      INTN   i, Count = Prop->size;

      Prop2 = NULL;

      if (Count > 0) {
        gSettings.DisabledAMLCount = 0;
        gSettings.DisabledAML = AllocateZeroPool (Count * sizeof (CHAR16 *));

        if (gSettings.DisabledAML) {
          for (i = 0; i < Count; i++) {
            if (
              !EFI_ERROR (GetElement (Prop, i, Count, &Prop2)) &&
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
}

VOID
ParseCPUSettings (
  TagPtr    CurrentDict
) {
  TagPtr  DictPointer, Prop;

  DictPointer = GetProperty (CurrentDict, "CPU");
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
    if (Prop != NULL) {
      gSettings.CpuType = (UINT16)GetPropertyInteger (Prop, gSettings.CpuType);
      DBG ("CpuType: %x\n", gSettings.CpuType);
    }

    gSettings.UseARTFreq = GetPropertyBool (GetProperty (DictPointer, "UseARTFrequency"), FALSE);

    Prop = GetProperty (DictPointer, "BusSpeedkHz");
    if (Prop != NULL) {
      gSettings.BusSpeed = (UINT32)GetPropertyInteger (Prop, gSettings.BusSpeed);
      DBG ("BusSpeed: %dkHz\n", gSettings.BusSpeed);
    }

    gSettings.EnableC7 = GetPropertyBool (GetProperty (DictPointer, "C7"), FALSE);
    gSettings.EnableC6 = GetPropertyBool (GetProperty (DictPointer, "C6"), FALSE);
    gSettings.EnableC4 = GetPropertyBool (GetProperty (DictPointer, "C4"), FALSE);
    gSettings.EnableC2 = GetPropertyBool (GetProperty (DictPointer, "C2"), FALSE);

    //Usually it is 0x03e9, but if you want Turbo, you may set 0x00FA
    Prop = GetProperty (DictPointer, "Latency");
    gSettings.C3Latency = (UINT16)GetPropertyInteger (Prop, gSettings.C3Latency);

    if (GetPropertyBool (GetProperty (DictPointer, "TurboDisable"), FALSE)) {
      UINT64  Msr = AsmReadMsr64 (MSR_IA32_MISC_ENABLE);

      gSettings.Turbo = 0;
      Msr &= ~(1ULL << 38);
      AsmWriteMsr64 (MSR_IA32_MISC_ENABLE, Msr);
    }
  }
}

VOID
ParseRtVariablesSettings (
  TagPtr    CurrentDict
) {
  TagPtr  DictPointer, Prop;

  DictPointer = GetProperty (CurrentDict, "RtVariables");
  if (DictPointer != NULL) {
    // ROM: <data>bin data</data> or <string>base 64 encoded bin data</string>
    Prop = GetProperty (DictPointer, "ROM");
    if (Prop != NULL) {
      GetLegacyLanAddress = FALSE;
      gSettings.RtROM = NULL;
      gSettings.RtROMLen = 0;

      if (AsciiStriCmp (Prop->string, "UseMacAddr0") == 0) {
        gSettings.RtROM = &gLanMac[0][0];
        gSettings.RtROMLen = 6;
        GetLegacyLanAddress = TRUE;
      } else if (AsciiStriCmp (Prop->string, "UseMacAddr1") == 0) {
        gSettings.RtROM = &gLanMac[1][0];
        gSettings.RtROMLen = 6;
        GetLegacyLanAddress = TRUE;
      } else {
        UINTN   ROMLength = 0;

        gSettings.RtROM = (AsciiStrStr (Prop->string, "%") != NULL)
          ? StringDataToHex (GetDataROM (Prop->string), &ROMLength)
          : GetDataSetting (DictPointer, "ROM", &ROMLength);

        gSettings.RtROMLen = ROMLength;
      }
    }

    // MLB: <string>some value</string>
    Prop = GetProperty (DictPointer, "MLB");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      gSettings.RtMLB = AllocateCopyPool (AsciiStrSize (Prop->string), Prop->string);
    }

    // CsrActiveConfig
    Prop = GetProperty (DictPointer, "CsrActiveConfig");
    gSettings.CsrActiveConfig = (UINT32)GetPropertyInteger (Prop, gSettings.CsrActiveConfig);

    //BooterConfig
    Prop = GetProperty (DictPointer, "BooterConfig");
    gSettings.BooterConfig = (UINT16)GetPropertyInteger (Prop, gSettings.BooterConfig);
  }

  if (gSettings.RtROM == NULL) {
    gSettings.RtROM = (UINT8 *)&gSettings.SmUUID.Data4[2];
    gSettings.RtROMLen = 6;
  }

  if ((gSettings.RtMLB == NULL) && &gSettings.BoardSerialNumber[0]) {
    gSettings.RtMLB = &gSettings.BoardSerialNumber[0];
  }
}

VOID
ParseSystemParametersSettings (
  TagPtr    CurrentDict
) {
  TagPtr  DictPointer, Prop;

  DictPointer = GetProperty (CurrentDict, "SystemParameters");
  if (DictPointer != NULL) {
    if (gGuiIsReady) {
      goto SkipInitialBoot;
    }

    Prop = GetProperty (DictPointer, "DisableDrivers");
    if (Prop != NULL) {
      INTN   i, Count = Prop->size;

      if (Count > 0) {
        gSettings.BlackListCount = 0;
        gSettings.BlackList = AllocateZeroPool (Count * sizeof (CHAR16 *));

        for (i = 0; i < Count; i++) {
          if (
              !EFI_ERROR (GetElement (Prop, i, Count, &Prop)) &&
              (Prop != NULL) &&
              (Prop->type == kTagTypeString)
          ) {
            gSettings.BlackList[gSettings.BlackListCount++] = PoolPrint (L"%a", Prop->string);
          }
        }
      }
    }

    SkipInitialBoot:

    gSettings.WithKexts = GetPropertyBool (GetProperty (DictPointer, "InjectKexts"), TRUE);
    gSettings.NoCaches = GetPropertyBool (GetProperty (DictPointer, "NoCaches"), FALSE);
    gSettings.FakeSMCOverrides = GetPropertyBool (GetProperty (DictPointer, "FakeSMCOverrides"), TRUE);

    if (GetLegacyLanAddress) {
      Prop = GetProperty (DictPointer, "MacAddress");
      if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
        UINT8   *Ret = AllocateZeroPool (6 * sizeof (UINT8));

        MsgLog ("MAC address: %a\n", Prop->string);

        Ret = StrToMacAddress (Prop->string);
        if (Ret != NULL) {
          GetLegacyLanAddress = FALSE;
          CopyMem (gLanMac[0], Ret, ARRAY_SIZE (gLanMac[0]));
          CopyMem (gLanMac[1], Ret, ARRAY_SIZE (gLanMac[0]));
          FreePool (Ret);
        }
      }
    }
  }
}

VOID
ParsePatchesSettings (
  TagPtr    CurrentDict
) {
  TagPtr        DictPointer;

  if (!gGuiIsReady || gBootChanged) {
    DictPointer = GetProperty (CurrentDict, "Patches");
    if (DictPointer != NULL) {
      FillinKextPatches ((KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF (SETTINGS_DATA, KernelAndKextPatches)), DictPointer);
    }
  }
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
  HasRock      = FileExists (Volume->RootDir, SystemPlistR);

  SystemPlistP = L"\\com.apple.boot.P\\Library\\Preferences\\SystemConfiguration\\com.apple.Boot.plist";
  HasPaper     = FileExists (Volume->RootDir, SystemPlistP);

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
    if (EFI_ERROR (ParseXML (PlistBuffer, 0, &Dict))) {
      FreePool (PlistBuffer);
      return EFI_NOT_FOUND;
    }

    Prop = GetProperty (Dict, "Root UUID");
    if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
      AsciiStrToUnicodeStrS (Prop->string, Uuid, ARRAY_SIZE (Uuid));
      Status = StrToGuid (Uuid, &Volume->RootUUID);
    }

    FreePool (PlistBuffer);
  }

  return Status;
}

VOID
GetDevices () {
  EFI_STATUS              Status;
  UINTN                   i, Segment = 0, Bus = 0, Device = 0, Function = 0, HandleCount = 0;
  UINT8                   DeviceN = 0;
  EFI_HANDLE              *HandleBuffer = NULL;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  PCI_TYPE00              Pci;

  NGFX = 0;

  DbgHeader ("GetDevices");

  // Scan PCI handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );

  if (!EFI_ERROR (Status)) {
    for (i = 0; i < HandleCount; ++i) {
      SLOT_DEVICE   *SlotDevice = &SlotDevices[DeviceN];

      Status = gBS->HandleProtocol (HandleBuffer[i], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
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

        MsgLog ("PCI (%02x|%02x:%02x.%02x) : [%04x:%04x] class=%02x%02x%02x",
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
          (NGFX < ARRAY_SIZE (gGraphics))
        ) {
          GFX_PROPERTIES  *gfx = &gGraphics[NGFX];

          gfx->DeviceID = Pci.Hdr.DeviceId;
          gfx->Segment  = Segment;
          gfx->Bus      = Bus;
          gfx->Device   = Device;
          gfx->Function = Function;
          gfx->Handle   = HandleBuffer[i];

          MsgLog (" - GFX");

          switch (Pci.Hdr.VendorId) {
            case 0x1002:
              MsgLog (" (ATI/AMD)");

              gfx->Vendor = GfxAti;

              //GetAtiModel (&gfx[0], Pci.Hdr.DeviceId);

              AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "PCI Slot %d", DeviceN);
              SlotDevice->SlotType = SlotTypePciExpressX16;

              gSettings.HasGraphics->Ati = TRUE;
              break;

            case 0X10DE:
              MsgLog (" (Nvidia)");

              gfx->Vendor = GfxNvidia;
              gfx->Mmio = (UINT8 *)(UINTN)(Pci.Device.Bar[0] & ~0x0f);
              // get card type
              gfx->Family = (REG32 (gfx->Mmio, 0) >> 20) & 0x1ff;

              AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "PCI Slot %d", DeviceN);

              SlotDevice->SlotType = SlotTypePciExpressX16;

              gSettings.HasGraphics->Nvidia = TRUE;
              break;

            case 0x8086:
              MsgLog (" (Intel)");

              gfx->Vendor = GfxIntel;

              SlotDevice->ForceInject = TRUE;

              gSettings.HasGraphics->Intel = TRUE;

              break;

            default:
              MsgLog (" (Unknown)");

              gfx->Vendor = GfxUnknown;
              //AsciiSPrint (gfx->Model, 64, "pci%x,%x", Pci.Hdr.VendorId, Pci.Hdr.DeviceId);
              //gfx->Ports                  = 1;
              break;
          }

          if (gfx->Vendor != GfxUnknown) {
            NGFX++;
          } else {
            goto EndGetDevice;
          }

        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_OTHER)
        ) {
          MsgLog (" - WIFI");

          AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "Airport");

          SlotDevice->SlotType = SlotTypePciExpressX1;

        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_ETHERNET)
        ) {
          MsgLog (" - LAN");

          if (nLanCards >= 4) {
            DBG (" - [!] too many LAN card in the system (upto 4 limit exceeded), overriding the last one\n");
            //nLanCards = 3; // last one will be rewritten
            continue;
          }

          SlotDevice->SlotType = SlotTypePciExpressX1;

          AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "Ethernet");

          gLanVendor[nLanCards] = Pci.Hdr.VendorId;
          gLanMmio[nLanCards++] = (UINT8 *)(UINTN)(Pci.Device.Bar[0] & ~0x0f);

        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_SERIAL) &&
          (Pci.Hdr.ClassCode[1] == PCI_CLASS_SERIAL_FIREWIRE)
        ) {
          MsgLog (" - SERIAL");

          SlotDevice->SlotType = SlotTypePciExpressX4;

          AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "Firewire");

        } else if (
          (Pci.Hdr.ClassCode[2] == PCI_CLASS_MEDIA) &&
          (
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_HDA) ||
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_AUDIO)
          )
        ) {
          MsgLog (" - AUDIO");

          if (IsHDMIAudio (HandleBuffer[i])) { // (Pci.Hdr.VendorId == 0x1002) || (Pci.Hdr.VendorId == 0x10DE)
            MsgLog (" (HDMI)");

            SlotDevice->SlotType = SlotTypePciExpressX4;

            AsciiSPrint (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), "HDMI Audio");
          }

        } else {
          goto EndGetDevice;
        }

        SlotDevice->SegmentGroupNum                       = (UINT16)Segment;
        SlotDevice->BusNum                                = (UINT8)Bus;
        SlotDevice->DevFuncNum                            = (UINT8)((Device << 3) | (Function & 0x07));
        SlotDevice->Valid                                 = TRUE;
        SlotDevice->SlotID                                = DeviceN;

        SlotDevice->PCIDevice.DeviceHandle                = HandleBuffer[i];
        SlotDevice->PCIDevice.dev.addr                    = (UINT32)PCIADDR (Bus, Device, Function);
        SlotDevice->PCIDevice.vendor_id                   = Pci.Hdr.VendorId;
        SlotDevice->PCIDevice.device_id                   = Pci.Hdr.DeviceId;
        SlotDevice->PCIDevice.revision                    = Pci.Hdr.RevisionID;
        SlotDevice->PCIDevice.subclass                    = Pci.Hdr.ClassCode[0];
        SlotDevice->PCIDevice.class_id                    = *((UINT16 *)(Pci.Hdr.ClassCode+1));
        SlotDevice->PCIDevice.subsys_id.subsys.vendor_id  = Pci.Device.SubsystemVendorID;
        SlotDevice->PCIDevice.subsys_id.subsys.device_id  = Pci.Device.SubsystemID;

        CopyMem (&SlotDevice->PCIDevice.class_code, &Pci.Hdr.ClassCode, ARRAY_SIZE (Pci.Hdr.ClassCode));
        CopyMem (&SlotDevice->PCIDevice.handle, &HandleBuffer[i], sizeof (EFI_HANDLE));

        DeviceN++;

        EndGetDevice:

        MsgLog ("\n");
      }
    }
  }
}

VOID
SyncDevices () {
  UINTN   i, y;

  //DbgHeader ("SyncDevices");

  for (i = 0; i < DEV_INDEX_MAX; i++) {
    SLOT_DEVICE   *SlotDevice = &SlotDevices[i];

    if (!SlotDevice->Valid) {
      continue;
    }

    for (y = 0; y < DEV_INDEX_MAX; y++) {
      SLOT_DEVICE   *SmbiosSlotDevice = &SmbiosSlotDevices[i];

      if (
        !SmbiosSlotDevice->Valid ||
        (SlotDevice->PCIDevice.dev.addr != SmbiosSlotDevice->PCIDevice.dev.addr)
      ) {
        continue;
      }

      SlotDevice->SlotID = SmbiosSlotDevice->SlotID;
      SlotDevice->SlotType = SmbiosSlotDevice->SlotType;
      AsciiStrCpyS (SlotDevice->SlotName, ARRAY_SIZE (SlotDevice->SlotName), SmbiosSlotDevice->SlotName);
    }
  }
}

VOID
SetDevices (
  LOADER_ENTRY    *Entry
) {
  EFI_STATUS              Status;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  BOOLEAN                 StringDirty = FALSE, TmpDirty = FALSE;
  UINTN                   i;

  DbgHeader ("SetDevices");

  for (i = 0; i < DEV_INDEX_MAX; i++) {
    if (SlotDevices[i].Valid || SlotDevices[i].ForceInject) {
      if (gSettings.NrAddProperties == 0xFFFE) {
        DEV_PROPERTY      *Prop = gSettings.AddProperties;
        DevPropDevice     *Device = NULL;
        BOOLEAN           Init = TRUE;

        MsgLog ("Inject CustomProperties: \n");

        if (!gDevPropString) {
          gDevPropString = DevpropCreateString ();
        }

        while (Prop) {
          if (Prop->Device != SlotDevices[i].PCIDevice.dev.addr) {
            Prop = Prop->Next;
            continue;
          }

          if (Init) {
            Device = DevpropAddDevicePci (gDevPropString, &SlotDevices[i].PCIDevice);
            Init = FALSE;
          }

          DevpropAddValue (Device, Prop->Key, (UINT8 *)Prop->Value, Prop->ValueLen);
          StringDirty = TRUE;
          Prop = Prop->Next;
        }

        //MsgLog (" for device %02x:%02x.%02x injected, continue\n", Bus, Device, Function);
        continue;
      }

      // GFX
      if (
        (SlotDevices[i].PCIDevice.class_code[2] == PCI_CLASS_DISPLAY) &&
        (
          (SlotDevices[i].PCIDevice.class_code[1] == PCI_CLASS_DISPLAY_VGA) ||
          (SlotDevices[i].PCIDevice.class_code[1] == PCI_CLASS_DISPLAY_OTHER)
        )
      ) {
        MsgLog ("Inject Display:");

        switch (SlotDevices[i].PCIDevice.vendor_id) {
          case 0x1002:
            MsgLog (" ATI/AMD\n");

            if (gSettings.InjectATI) {
              //can't do this in one step because of C-conventions
              TmpDirty = SetupAtiDevprop (Entry, &SlotDevices[i].PCIDevice);
              StringDirty |=  TmpDirty;
            } else {
              DBG (" - injection not set\n");
            }
            break;

          case 0X10DE:
            MsgLog (" Nvidia\n");
            if (gSettings.InjectNVidia) {
              TmpDirty = SetupNvidiaDevprop (&SlotDevices[i].PCIDevice);
              StringDirty |=  TmpDirty;
            } else {
              DBG (" - injection not set\n");
            }
            break;

          case 0x8086:
            MsgLog (" Intel\n");
            if (gSettings.InjectIntel) {
              TmpDirty = SetupGmaDevprop (&SlotDevices[i].PCIDevice);
              StringDirty |=  TmpDirty;
              //MsgLog (" - Intel GFX revision  =0x%x\n", SlotDevices[i].PCIDevice.revision);
            } else {
              DBG (" - injection not set\n");
            }

            Status = gBS->HandleProtocol (SlotDevices[i].PCIDevice.handle, &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
            if (!EFI_ERROR (Status)) {
              UINT32  LevelW = 0xC0000000, IntelDisable = 0x03;

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
            }
            break;

          default:
            MsgLog (" Unknown\n");
            break;
        }

      } else if (
        (SlotDevices[i].PCIDevice.class_code[2] == PCI_CLASS_NETWORK) &&
        (SlotDevices[i].PCIDevice.class_code[1] == PCI_CLASS_NETWORK_ETHERNET)
      ) {
        //MsgLog ("Ethernet device found\n");
        if (!(gSettings.FixDsdt & FIX_LAN)) {
          MsgLog ("Inject LAN:\n");

          TmpDirty = SetupEthernetDevprop (&SlotDevices[i].PCIDevice);
          StringDirty |=  TmpDirty;
        }
      }

      // HDA
      else if (
        (gSettings.HDALayoutId > 0) &&
        (SlotDevices[i].PCIDevice.class_code[2] == PCI_CLASS_MEDIA) &&
        (
          (SlotDevices[i].PCIDevice.class_code[1] == PCI_CLASS_MEDIA_HDA) ||
          (SlotDevices[i].PCIDevice.class_code[1] == PCI_CLASS_MEDIA_AUDIO)
        )
      ) {
        //no HDMI injection
        if (
          (SlotDevices[i].PCIDevice.vendor_id != 0x1002) &&
          (SlotDevices[i].PCIDevice.vendor_id != 0X10DE)
        ) {
          MsgLog ("Inject HDA:\n");
          TmpDirty    = SetupHdaDevprop (&SlotDevices[i].PCIDevice);
          StringDirty |= TmpDirty;
        }
      }
    }
  }

  if (StringDirty) {
    EFI_PHYSICAL_ADDRESS    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS; //0xFE000000;

    gDevPropStringLength = gDevPropString->length * 2;

    //DBG ("gDevPropStringLength = %d\n", gDevPropStringLength);

    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiACPIReclaimMemory,
                    EFI_SIZE_TO_PAGES (gDevPropStringLength + 1),
                    &BufferPtr
                  );

    if (!EFI_ERROR (Status)) {
      mProperties       = (UINT8 *)(UINTN)BufferPtr;
      gDeviceProperties = (VOID *)DevpropGenerateString (gDevPropString);
      gDeviceProperties[gDevPropStringLength] = 0;
      //DBG (gDeviceProperties);
      //DBG ("\n");
      //StringDirty = FALSE;
      //-------
      mPropSize = (UINT32)AsciiStrLen (gDeviceProperties) / 2;
      //DBG ("Preliminary size of mProperties=%d\n", mPropSize);
      mPropSize = Hex2Bin (gDeviceProperties, mProperties, mPropSize);
      //DBG ("Final size of mProperties=%d\n", mPropSize);
      //---------
    }
  }

  //MsgLog ("CurrentMode: Width=%d Height=%d\n", UGAWidth, UGAHeight);
}

EFI_STATUS
GetUserSettings (
  IN TagPtr     Dict
) {
  if (Dict != NULL) {
    //DBG ("Loading main settings\n");
    DbgHeader ("GetUserSettings");

    ParseBootSettings (Dict);

    ParseGUISettings (Dict);

    ParseGraphicsSettings (Dict);

    ParseDevicesSettings (Dict);

    ParseACPISettings (Dict);

    ParseSMBIOSSettings (Dict);

    ParseCPUSettings (Dict);

    ParsePatchesSettings (Dict);

    ParseRtVariablesSettings (Dict);

    ParseSystemParametersSettings (Dict);

    SaveSettings ();

    return EFI_SUCCESS;
  }

  //DBG ("config.plist read and return %r\n", Status);
  return EFI_BAD_BUFFER_SIZE;
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
  CHAR16        *ConfigOemPath;

  //DbgHeader ("LoadUserSettings");

  // load config
  if ((ConfName == NULL) || (Dict == NULL)) {
    //DBG ("Can't load plist in LoadUserSettings: NULL\n");
    return EFI_NOT_FOUND;
  }

  ConfigOemPath   = PoolPrint (L"%s\\%s.plist", OEMPath, ConfName);

  if (FileExists (SelfRootDir, ConfigOemPath)) {
    Status = LoadFile (SelfRootDir, ConfigOemPath, (UINT8 **)&gConfigPtr, &Size);
    DBG ("Load plist: '%s' ... %r\n", ConfigOemPath, Status);
  }

  if (!EFI_ERROR (Status) && (gConfigPtr != NULL)) {
    Status = ParseXML (gConfigPtr, (UINT32)Size, Dict);
    DBG ("Parsing plist: ... %r\n", Status);
  }

  return Status;
}

VOID
GetDefaultConfig () {
  CopyMem (&GlobalConfig, &DefaultConfig, sizeof (REFIT_CONFIG));

  ZeroMem ((VOID *)&gGraphics, ARRAY_SIZE (gGraphics) * sizeof (GFX_PROPERTIES));

  ZeroMem ((VOID *)&gSettings, sizeof (SETTINGS_DATA));

  gSettings.Timeout = -1;

  gSettings.WithKexts = TRUE;
  gSettings.FakeSMCOverrides = TRUE;
  gSettings.KextPatchesAllowed = TRUE;
  gSettings.KernelPatchesAllowed = TRUE;
  gSettings.BooterPatchesAllowed = TRUE;
  gSettings.LinuxScan = TRUE;
  gSettings.AndroidScan = TRUE;
  gSettings.NvidiaSingle = TRUE;

  gSettings.FlagsBits = OSFLAG_DEFAULTS;

  ZeroMem (gSettings.DarwinDiskTemplate, ARRAY_SIZE (gSettings.DarwinDiskTemplate));
  ZeroMem (gSettings.DarwinRecoveryDiskTemplate, ARRAY_SIZE (gSettings.DarwinRecoveryDiskTemplate));
  ZeroMem (gSettings.DarwinInstallerDiskTemplate, ARRAY_SIZE (gSettings.DarwinInstallerDiskTemplate));
  ZeroMem (gSettings.LinuxDiskTemplate, ARRAY_SIZE (gSettings.LinuxDiskTemplate));
  //ZeroMem (gSettings.AndroidDiskTemplate, ARRAY_SIZE (gSettings.AndroidDiskTemplate));
  ZeroMem (gSettings.WindowsDiskTemplate, ARRAY_SIZE (gSettings.WindowsDiskTemplate));

  StrCpyS (gSettings.DsdtName, ARRAY_SIZE (gSettings.DsdtName), DSDT_NAME);

  gSettings.BacklightLevel       = 0xFFFF; //0x0503; -- the value from MBA52
  gSettings.TrustSMBIOS          = TRUE;

  //gSettings.DefaultBackgroundColor = 0x80000000; //the value to delete the variable

  gSettings.CsrActiveConfig      = 0xFFFF;
  gSettings.BooterConfig         = 0xFFFF;
  gSettings.QPI                  = 0xFFFF;
  gSettings.XMPDetection         = -1;

  //gLanguage = english;
}

VOID
SyncDefaultSettings () {
  MACHINE_TYPES   Model;

  Model = GetDefaultModel ();
  gSettings.CpuType = GetAdvancedCpuType ();

  SetDMISettingsForModel (Model, TRUE);

  if (gCPUStructure.Model >= CPUID_MODEL_IVYBRIDGE) {
    //gSettings.EnableISS = FALSE;
    //gSettings.EnableC2 = TRUE;
    //gSettings.EnableC6 = TRUE;
    gSettings.PluginType = 1;

    if (gCPUStructure.Model == CPUID_MODEL_IVYBRIDGE) {
      gSettings.MinMultiplier = 7;
    }

    //gSettings.DoubleFirstState = FALSE;
    //gSettings.DropSSDT = TRUE;
    gSettings.C3Latency = 0x00FA;
  }

  gSettings.Turbo = gCPUStructure.Turbo;
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

  if (GetLegacyLanAddress) {
    GetMacAddress ();
  }

  return EFI_SUCCESS;
}
