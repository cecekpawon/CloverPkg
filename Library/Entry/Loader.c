/*
 * refit/scan/loader.c
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_SCAN_LOADER
#define DEBUG_SCAN_LOADER -1
#endif
#else
#ifdef DEBUG_SCAN_LOADER
#undef DEBUG_SCAN_LOADER
#endif
#define DEBUG_SCAN_LOADER DEBUG_ALL
#endif

#define DBG(...) DebugLog(DEBUG_SCAN_LOADER, __VA_ARGS__)

//#define DUMP_KERNEL_KEXT_PATCHES 1

#define BOOT_LOADER_PATH L"\\EFI\\BOOT\\BOOTX64.efi"

#define MACOSX_LOADER_PATH L"\\System\\Library\\CoreServices\\boot.efi"
#define MACOSX_RECOVERY_LOADER_PATH L"\\com.apple.recovery.boot\\boot.efi"

#define LINUX_ISSUE_PATH L"\\etc\\issue"
#define LINUX_BOOT_PATH L"\\boot"
#define LINUX_BOOT_ALT_PATH L"\\boot"
#define LINUX_LOADER_PATH L"vmlinuz"
#define LINUX_FULL_LOADER_PATH LINUX_BOOT_PATH L"\\" LINUX_LOADER_PATH
#define LINUX_LOADER_SEARCH_PATH L"vmlinuz*"
#define LINUX_DEFAULT_OPTIONS L"ro add_efi_memmap quiet splash vt.handoff=7"

// Linux loader path data
typedef struct LINUX_PATH_DATA
{
  CHAR16 *Path;
  CHAR16 *Title;
  CHAR16 *Icon;
  CHAR8  *Issue;
} LINUX_PATH_DATA;

STATIC LINUX_PATH_DATA LinuxEntryData[] = {
  { L"\\EFI\\grub\\grubx64.efi",        L"Grub",                L"grub,linux" },
  { L"\\EFI\\Gentoo\\grubx64.efi",      L"Gentoo Linux",        L"gentoo,linux",    "Gentoo" },
  //{ L"\\EFI\\Gentoo\\kernelx64.efi",    L"Gentoo Linux kernel", L"gentoo,linux" },
  { L"\\EFI\\RedHat\\grubx64.efi",      L"RedHat Linux",        L"redhat,linux",    "Redhat" },
  { L"\\EFI\\ubuntu\\grubx64.efi",      L"Ubuntu Linux",        L"ubuntu,linux",    "Ubuntu" },
  { L"\\EFI\\kubuntu\\grubx64.efi",     L"kubuntu Linux",       L"kubuntu,linux",   "kubuntu" },
  { L"\\EFI\\LinuxMint\\grubx64.efi",   L"Mint Linux",          L"mint,linux",      "Linux Mint" },
  { L"\\EFI\\Fedora\\grubx64.efi",      L"Fedora Linux",        L"fedora,linux",    "Fedora" },
  { L"\\EFI\\opensuse\\grubx64.efi",    L"OpenSuse Linux",      L"suse,linux",      "openSUSE" },
  { L"\\EFI\\debian\\grubx64.efi",      L"Debian Linux",        L"debian,linux",    "Debian" },
  { L"\\EFI\\arch\\grubx64.efi",        L"Arch Linux",          L"arch,linux" },
  //{ L"\\EFI\\arch_grub\\grubx64.efi",   L"Arch Linux",          L"arch,linux" },
  // --
  { L"\\EFI\\SuSe\\elilo.efi",          L"OpenSuse Linux",      L"suse,linux" },
};

STATIC CONST UINTN LinuxEntryDataCount = ARRAY_SIZE(LinuxEntryData);

STATIC CHAR16 *LinuxInitImagePath[] = {
   L"initrd%s",
   L"initrd.img%s",
   L"initrd%s.img",
   L"initramfs%s",
   L"initramfs.img%s",
   L"initramfs%s.img",
};

STATIC CONST UINTN LinuxInitImagePathCount = ARRAY_SIZE(LinuxInitImagePath);

#define ANDX86_FINDLEN 3

typedef struct ANDX86_PATH_DATA {
  CHAR16   *Path;
  CHAR16   *Title;
  CHAR16   *Icon;
  CHAR16   *Find[ANDX86_FINDLEN];
} ANDX86_PATH_DATA;

STATIC ANDX86_PATH_DATA AndroidEntryData[] = {
  //{ L"\\EFI\\boot\\grubx64.efi", L"Grub", L"grub,linux" },
  //{ L"\\EFI\\boot\\bootx64.efi", L"Grub", L"grub,linux" },
  { L"\\EFI\\remixos\\grubx64.efi", L"Remix",   L"remix,grub,linux",    { L"\\isolinux\\isolinux.bin", L"\\initrd.img", L"\\kernel" } },
  { L"\\EFI\\boot\\grubx64.efi",    L"Phoenix", L"phoenix,grub,linux",  { L"\\phoenix\\kernel", L"\\phoenix\\initrd.img", L"\\phoenix\\ramdisk.img" } },
  { L"\\EFI\\boot\\bootx64.efi",    L"Chrome",  L"chrome,grub,linux",   { L"\\syslinux\\vmlinuz.A", L"\\syslinux\\vmlinuz.B", L"\\syslinux\\ldlinux.sys" } },
};

STATIC CONST UINTN AndroidEntryDataCount = ARRAY_SIZE(AndroidEntryData);

STATIC CHAR16 *OSXInstallerPaths[] = {
  L"\\Mac OS X Install Data\\boot.efi",
  L"\\macOS Install Data\\boot.efi",
  L"\\OS X Install Data\\boot.efi",
  L"\\.IABootFiles\\boot.efi"
};

STATIC CONST UINTN OSXInstallerPathsCount = ARRAY_SIZE(OSXInstallerPaths);

STATIC CHAR16 *WINEFIPaths[] = {
  L"\\EFI\\MICROSOFT\\BOOT\\bootmgfw.efi",
  L"\\EFI\\MICROSOFT\\BOOT\\bootmgfw-orig.efi",
  L"\\bootmgr.efi",
  L"\\EFI\\MICROSOFT\\BOOT\\cdboot.efi"
};

STATIC CONST UINTN WINEFIPathsCount = ARRAY_SIZE(WINEFIPaths);

extern BOOLEAN CopyKernelAndKextPatches(IN OUT KERNEL_AND_KEXT_PATCHES *Dst, IN KERNEL_AND_KEXT_PATCHES *Src);

STATIC
BOOLEAN
IsBootExists (
  IN CHAR16   *Path,
  IN CHAR16   **Lists
) {
  INTN      Count = ARRAY_SIZE(Lists), Index = 0;
  BOOLEAN   Found = FALSE;

  while (!Found && (Index < Count)) {
    Found = (BOOLEAN)(StriCmp(Path, Lists[Index++]) == 0);
  }

  return Found;
}

UINT8
GetOSTypeFromPath (
  IN CHAR16   *Path
) {
  if (Path == NULL) {
    return OSTYPE_OTHER;
  }

  if (StriCmp(Path, MACOSX_LOADER_PATH) == 0) {
    return OSTYPE_OSX;
  } else if (IsBootExists(Path, OSXInstallerPaths)) {
    return OSTYPE_OSX_INSTALLER;
  } else if (StriCmp(Path, MACOSX_RECOVERY_LOADER_PATH) == 0) {
    return OSTYPE_RECOVERY;
  } else if (IsBootExists(Path, WINEFIPaths)) {
    return OSTYPE_WINEFI;
  } else if (StrniCmp(Path, LINUX_FULL_LOADER_PATH, StrLen(LINUX_FULL_LOADER_PATH)) == 0) {
    return OSTYPE_LINEFI;
  } else {
    UINTN   Index= 0;

    while (Index < AndroidEntryDataCount) {
      if (StriCmp(Path, AndroidEntryData[Index].Path) == 0) {
        return OSTYPE_LIN;
      }

      ++Index;
    }

    Index = 0;
    while (Index < LinuxEntryDataCount) {
      if (StriCmp(Path, LinuxEntryData[Index].Path) == 0) {
        return OSTYPE_LIN;
      }

      ++Index;
    }
  }

  return OSTYPE_OTHER;
}

STATIC
CHAR16
*LinuxIconNameFromPath (
  IN CHAR16             *Path,
  IN EFI_FILE_PROTOCOL  *RootDir
) {
  UINTN Index = 0;

  if (GlobalConfig.TextOnly || IsEmbeddedTheme()) {
    goto FINISH;
  }

  while (Index < AndroidEntryDataCount) {
    if (StriCmp(Path, AndroidEntryData[Index].Path) == 0) {
      return AndroidEntryData[Index].Icon;
    }

    ++Index;
  }

  Index = 0;
  while (Index < LinuxEntryDataCount) {
    if (StriCmp(Path, LinuxEntryData[Index].Path) == 0) {
      return LinuxEntryData[Index].Icon;
    }

    ++Index;
  }

  // Try to open the linux issue
  if ((RootDir != NULL) && (StrniCmp(Path, LINUX_FULL_LOADER_PATH, StrLen(LINUX_FULL_LOADER_PATH)) == 0)) {
    CHAR8   *Issue = NULL;
    UINTN   IssueLen = 0;

    if (!EFI_ERROR(egLoadFile(RootDir, LINUX_ISSUE_PATH, (UINT8 **)&Issue, &IssueLen)) && (Issue != NULL)) {
      if (IssueLen > 0) {
        for (Index = 0; Index < LinuxEntryDataCount; ++Index) {
          if (
            (LinuxEntryData[Index].Issue != NULL) &&
            (AsciiStrStr(Issue, LinuxEntryData[Index].Issue) != NULL)
          ) {
            FreePool(Issue);
            return LinuxEntryData[Index].Icon;
          }
        }
      }

      FreePool(Issue);
    }
  }

FINISH:
  return L"linux";
}

STATIC
CHAR16
*LinuxKernelOptions (
  IN EFI_FILE_PROTOCOL  *Dir,
  IN CHAR16             *Version,
  IN CHAR16             *PartUUID,
  IN CHAR16             *Options OPTIONAL
) {
  UINTN Index = 0;

  if ((Dir == NULL) || (PartUUID == NULL)) {
    return (Options == NULL) ? NULL : EfiStrDuplicate(Options);
  }

  while (Index < LinuxInitImagePathCount) {
    CHAR16  *InitRd = PoolPrint(LinuxInitImagePath[Index++], (Version == NULL) ? L"" : Version);

    if (InitRd != NULL) {
      if (FileExists(Dir, InitRd)) {
        CHAR16 *CustomOptions = PoolPrint(L"root=/dev/disk/by-partuuid/%s initrd=%s\\%s %s %s",
                                  PartUUID, LINUX_BOOT_ALT_PATH, InitRd,
                                  LINUX_DEFAULT_OPTIONS, (Options == NULL) ? L"" : Options
                                );

        FreePool(InitRd);

        return CustomOptions;
      }

      FreePool(InitRd);
    }
  }

  return PoolPrint(L"root=/dev/disk/by-partuuid/%s %s %s",
            PartUUID, LINUX_DEFAULT_OPTIONS, (Options == NULL) ? L"" : Options
          );
}

STATIC
BOOLEAN
isFirstRootUUID (
  REFIT_VOLUME    *Volume
) {
  UINTN           VolumeIndex;
  REFIT_VOLUME    *scanedVolume;

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    scanedVolume = Volumes[VolumeIndex];

    if (scanedVolume == Volume) {
      return TRUE;
    }

    if (CompareGuid(&scanedVolume->RootUUID, &Volume->RootUUID)) {
      return FALSE;
    }

  }

  return TRUE;
}

//Set Entry->VolName to .disk_label.contentDetails if it exists
STATIC
EFI_STATUS
GetOSXVolumeName (
  LOADER_ENTRY    *Entry
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  CHAR16        *targetNameFile = L"\\System\\Library\\CoreServices\\.disk_label.contentDetails";
  CHAR8         *fileBuffer, *targetString;
  UINTN         fileLen = 0, Len;

  if (FileExists(Entry->Volume->RootDir, targetNameFile)) {
    Status = egLoadFile(Entry->Volume->RootDir, targetNameFile, (UINT8 **)&fileBuffer, &fileLen);
    if (!EFI_ERROR(Status)) {
      Len = fileLen + 1;

      //Create null terminated string
      targetString = AllocateZeroPool(Len);
      CopyMem((VOID*)targetString, (VOID*)fileBuffer, fileLen);
      DBG(" - found disk_label with contents:%a\n", targetString);

      //Convert to Unicode
      Entry->VolName = AllocateZeroPool(Len);
      AsciiStrToUnicodeStrS(targetString, Entry->VolName, Len);

      DBG(" - created name: %s\n", Entry->VolName);

      FreePool(fileBuffer);
      FreePool(targetString);
    }
  }

  return Status;
}

STATIC
LOADER_ENTRY
*CreateLoaderEntry (
  IN CHAR16                   *LoaderPath,
  IN CHAR16                   *LoaderOptions,
  IN CHAR16                   *FullTitle,
  IN CHAR16                   *LoaderTitle,
  IN REFIT_VOLUME             *Volume,
  IN EG_IMAGE                 *Image,
  IN EG_IMAGE                 *DriveImage,
  IN UINT8                    OSType,
  IN UINT8                    Flags,
  IN CHAR16                   Hotkey,
  IN KERNEL_AND_KEXT_PATCHES  *Patches,
  IN BOOLEAN                  CustomEntry
) {
  EFI_DEVICE_PATH *LoaderDevicePath;
  CHAR16          *LoaderDevicePathString, *FilePathAsString, ShortcutLetter,
                  *OSIconName, *HoverImage, *OSIconNameHover = NULL;
  LOADER_ENTRY    *Entry;
  INTN            i;
  CHAR8           *indent = "    ";
  EG_IMAGE        *ImageTmp;

  // Check parameters are valid
  if ((LoaderPath == NULL) || (*LoaderPath == 0) || (Volume == NULL)) {
    return NULL;
  }

  // Get the loader device path
  LoaderDevicePath = FileDevicePath(Volume->DeviceHandle, (*LoaderPath == L'\\') ? (LoaderPath + 1) : LoaderPath);

  if (LoaderDevicePath == NULL) {
    return NULL;
  }

  LoaderDevicePathString = FileDevicePathToStr(LoaderDevicePath);

  if (LoaderDevicePathString == NULL) {
    return NULL;
  }

  // Ignore this loader if it's self path
  FilePathAsString = FileDevicePathToStr(SelfFullDevicePath);

  if (FilePathAsString) {
    INTN    Comparison = StriCmp(FilePathAsString, LoaderDevicePathString);

    FreePool(FilePathAsString);
    if (Comparison == 0) {
      DBG("%a skipped because path `%s` is self path!\n", indent, LoaderDevicePathString);
      FreePool(LoaderDevicePathString);
      return NULL;
    }
  }

  if (!CustomEntry) {
    CUSTOM_LOADER_ENTRY   *Custom;
    UINTN                  CustomIndex = 0;

    // Ignore this loader if it's device path is already present in another loader
    if (MainMenu.Entries) {
      for (i = 0; i < MainMenu.EntryCount; ++i) {
        REFIT_MENU_ENTRY    *MainEntry = MainMenu.Entries[i];

        // Only want loaders
        if (MainEntry && (MainEntry->Tag == TAG_LOADER)) {
          LOADER_ENTRY    *Loader = (LOADER_ENTRY *)MainEntry;

          if (StriCmp(Loader->DevicePathString, LoaderDevicePathString) == 0) {
            DBG("%a skipped because path `%s` already exists for another entry!\n", indent, LoaderDevicePathString);
            FreePool(LoaderDevicePathString);
            return NULL;
          }
        }
      }
    }

    // If this isn't a custom entry make sure it's not hidden by a custom entry
    Custom = gSettings.CustomEntries;

    while (Custom) {
      // Check if the custom entry is hidden or disabled
      if (
        (OSFLAG_ISSET(Custom->Flags, OSFLAG_DISABLED) ||
        (OSFLAG_ISSET(Custom->Flags, OSFLAG_HIDDEN) && !gSettings.ShowHiddenEntries))
      ) {
        INTN  volume_match = 0,
              volume_type_match = 0,
              path_match = 0,
              type_match = 0;

        // Check if volume match
        if (Custom->Volume != NULL) {
          // Check if the string matches the volume
          volume_match = (
            (StrStr(Volume->DevicePathString, Custom->Volume) != NULL) ||
             ((Volume->VolName != NULL) && (StrStr(Volume->VolName, Custom->Volume) != NULL))
          ) ? 1 : -1;
        }

        // Check if the volume_type match
        if (Custom->VolumeType != 0) {
          volume_type_match = MEDIA_VALID(Volume->DiskKind, Custom->VolumeType) ? 1 : -1;
        }

        // Check if the path match
        if (Custom->Path != NULL) {
          // Check if the loader path match
          path_match = (StriCmp(Custom->Path, LoaderPath) == 0) ? 1 : -1;
        }

        // Check if the type match
        if (Custom->Type != 0) {
          type_match = OSTYPE_COMPARE(Custom->Type, OSType) ? 1 : -1;
        }

        if ((volume_match == -1) || (volume_type_match == -1) || (path_match == -1) || (type_match == -1)) {
          UINTN   add_comma = 0;

          DBG ("%aNot match custom entry %d: ", indent, CustomIndex);

          if (volume_match != 0) {
            DBG("Volume: %s", volume_match == 1 ? L"match" : L"not match");

            add_comma++;
          }

          if (path_match != 0) {
            DBG("%sPath: %s",
                (add_comma ? L", " : L""),
                path_match == 1 ? L"match" : L"not match");

            add_comma++;
          }

          if (volume_type_match != 0) {
            DBG("%sVolumeType: %s",
                (add_comma ? L", " : L""),
                volume_type_match == 1 ? L"match" : L"not match");

            add_comma++;
          }

          if (type_match != 0) {
            DBG("%sType: %s",
                (add_comma ? L", " : L""),
                type_match == 1 ? L"match" : L"not match");
          }

          DBG("\n");
        } else {
          // Custom entry match
          DBG("%aSkipped because matching custom entry %d!\n", indent, CustomIndex);
          FreePool(LoaderDevicePathString);
          return NULL;
        }
      }

      Custom = Custom->Next;
      ++CustomIndex;
    }
  }

  // prepare the menu entry
  Entry                     = AllocateZeroPool(sizeof(LOADER_ENTRY));
  Entry->me.Tag             = TAG_LOADER;
  Entry->me.Row             = 0;
  Entry->Volume             = Volume;

  Entry->LoaderPath         = EfiStrDuplicate(LoaderPath);
  Entry->VolName            = Volume->VolName;
  Entry->DevicePath         = LoaderDevicePath;
  Entry->DevicePathString   = LoaderDevicePathString;
  Entry->Flags              = OSFLAG_SET(Flags, OSFLAG_USEGRAPHICS);

  if (LoaderOptions) {
    if (OSFLAG_ISSET(Flags, OSFLAG_NODEFAULTARGS)) {
      Entry->LoadOptions    = EfiStrDuplicate(LoaderOptions);
    } else {
      Entry->LoadOptions    = PoolPrint(L"%a %s", gSettings.BootArgs, LoaderOptions);
    }
  } else if ((AsciiStrLen(gSettings.BootArgs) > 0) && OSFLAG_ISUNSET(Flags, OSFLAG_NODEFAULTARGS)) {
    Entry->LoadOptions      = PoolPrint(L"%a", gSettings.BootArgs);
  }

  Entry->LoaderType         = OSType;
  Entry->BuildVersion       = NULL;
  Entry->OSVersion          = GetOSVersion(Entry);

  // detect specific loaders
  OSIconName = NULL;
  ShortcutLetter = 0;

  switch (OSType) {
    case OSTYPE_OSX:
    case OSTYPE_RECOVERY:
    case OSTYPE_OSX_INSTALLER:
      OSIconName = GetOSIconName(Entry->OSVersion);// Sothor - Get OSIcon name using OSVersion

      if ((OSType == OSTYPE_OSX) && IsOsxHibernated(Volume)) {
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_HIBERNATED);
      }

      ShortcutLetter = 'M';
      if ((Entry->VolName == NULL) || (StrLen(Entry->VolName) == 0)) {
        // else no sense to override it with dubious name
        GetOSXVolumeName(Entry);
      }
      break;

    case OSTYPE_WIN:
    case OSTYPE_WINEFI:
      OSIconName = L"vista,win";
      //ShortcutLetter = 'V';
      ShortcutLetter = 'W';
      break;

    case OSTYPE_LIN:
    case OSTYPE_LINEFI:
      OSIconName = LinuxIconNameFromPath(LoaderPath, Volume->RootDir);
      ShortcutLetter = 'L';
      break;

    case OSTYPE_OTHER:
    case OSTYPE_EFI:
    default:
      OSIconName = L"unknown";
      Entry->LoaderType = OSTYPE_OTHER;
      break;
  }

  if (FullTitle) {
    Entry->me.Title = EfiStrDuplicate(FullTitle);
  } else if ((Entry->VolName == NULL) || (StrLen(Entry->VolName) == 0)) {
    //DBG("encounter Entry->VolName ==%s and StrLen(Entry->VolName) ==%d\n",Entry->VolName, StrLen(Entry->VolName));
    if (GlobalConfig.BootCampStyle) {
      Entry->me.Title = PoolPrint(L"%s", ((LoaderTitle != NULL) ? LoaderTitle : Basename(Volume->DevicePathString)));
    } else {
      Entry->me.Title = PoolPrint(L"Boot %s from %s", (LoaderTitle != NULL) ? LoaderTitle : Basename(LoaderPath),
                                    Basename(Volume->DevicePathString));
    }
  } else {
    //DBG("encounter LoaderTitle ==%s and Entry->VolName ==%s\n", LoaderTitle, Entry->VolName);
    if (GlobalConfig.BootCampStyle) {
      if ((StriCmp(LoaderTitle, L"Mac OS X") == 0) || (StriCmp(LoaderTitle, L"Recovery") == 0)) {
        Entry->me.Title = PoolPrint(L"%s", Entry->VolName);
      } else {
        Entry->me.Title = PoolPrint(L"%s", (LoaderTitle != NULL) ? LoaderTitle : Basename(LoaderPath));
      }
    } else {
      Entry->me.Title = PoolPrint(L"Boot %s from %s", (LoaderTitle != NULL) ? LoaderTitle : Basename(LoaderPath),
                                    Entry->VolName);
    }
  }

  //DBG("Entry->me.Title =%s\n", Entry->me.Title);
  // just an example that UI can show hibernated volume to the user
  // should be better to show it on entry image
  if (OSFLAG_ISSET(Entry->Flags, OSFLAG_HIBERNATED)) {
    Entry->me.Title = PoolPrint(L"%s (hibernated)", Entry->me.Title);
  }

  Entry->me.ShortcutLetter = (Hotkey == 0) ? ShortcutLetter : Hotkey;

  if (!GlobalConfig.TextOnly) {
    // Load DriveImage
    Entry->me.DriveImage = (DriveImage != NULL) ? DriveImage : ScanVolumeDefaultIcon(Volume, Entry->LoaderType);

    if (IsEmbeddedTheme()) {
      goto FINISH;
    }

    ImageTmp = LoadOSIcon(OSIconName, &OSIconNameHover, L"unknown", 128, FALSE, TRUE);
    Entry->me.Image = Image ? Image : ImageTmp;

    // DBG("HideBadges=%d Volume=%s ", GlobalConfig.HideBadges, Volume->VolName);
    if (GlobalConfig.HideBadges & HDBADGES_SHOW) {
      if (GlobalConfig.HideBadges & HDBADGES_SWAP) {
        Entry->me.BadgeImage = egCopyScaledImage(Entry->me.DriveImage, GlobalConfig.BadgeScale);
        // DBG(" Show badge as Drive.");
      } else {
        Entry->me.BadgeImage = egCopyScaledImage(Entry->me.Image, GlobalConfig.BadgeScale);
        if (OSIconNameHover != NULL) {
          // DBG(" Show badge as OSImage.");
          HoverImage = AllocateZeroPool(sizeof(OSIconNameHover));
          HoverImage = GetIconsExt(PoolPrint(L"icons\\%s", OSIconNameHover), L"icns");
          Entry->me.ImageHover = LoadHoverIcon(HoverImage, 128);
          FreePool(HoverImage);
        }
      }
    }

    FreePool(OSIconNameHover);
  }

FINISH:
  FreePool(OSIconName);

  Entry->KernelAndKextPatches = (
    (Patches == NULL) ? (KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF(SETTINGS_DATA, KernelAndKextPatches)) : Patches
  );

  //DBG("%aLoader entry created for '%s'\n", indent, Entry->DevicePathString);

  return Entry;
}

BOOLEAN IsKernelIs64BitOnly(IN LOADER_ENTRY *Entry) {
  return OSX_GE(Entry->OSVersion, "10.8");
}

//
//  Draw Drive + icon on submenu entries.
//

STATIC
EG_IMAGE
*GetIconDetail (
  IN REFIT_MENU_ENTRY   MenuEntry
) {
  EG_IMAGE    *rImage,
              *TopImg = NULL,
              *BaseImg = egCopyImage(MenuEntry.DriveImage);
  INTN        Width, Height;

  if (GlobalConfig.HideBadges & HDBADGES_SHOW) {
    if (GlobalConfig.HideBadges & HDBADGES_SWAP) {
      egFreeImage(BaseImg);
      BaseImg = egCopyImage(MenuEntry.ImageHover ? MenuEntry.ImageHover : MenuEntry.Image);
      TopImg = egCopyImage(MenuEntry.BadgeImage);
    } else {
      TopImg = egCopyImage(MenuEntry.ImageHover ? MenuEntry.ImageHover : MenuEntry.BadgeImage);
    }
  }

  Width   = BaseImg->Width + 16;
  Height  = BaseImg->Height + 16;

  rImage = egCreateFilledImage(Width, Height, TRUE, &MenuBackgroundPixel);

  egComposeImage(rImage, BaseImg, 8, 8);

  if (TopImg != NULL) {
    egComposeImage(rImage, TopImg,
      MenuEntry.ImageHover ? 8 : ((GlobalConfig.BadgeOffsetX != 0xFFFF) ? (GlobalConfig.BadgeOffsetX + 8) : (Width - TopImg->Width)),
      MenuEntry.ImageHover ? 8 : ((GlobalConfig.BadgeOffsetY != 0xFFFF) ? (GlobalConfig.BadgeOffsetY + 8) : (Height - TopImg->Height))
    );

    egFreeImage(TopImg);
  }

  egFreeImage(BaseImg);

  return rImage;
}

STATIC
VOID
AddDefaultMenu (
  IN LOADER_ENTRY   *Entry
) {
  LOADER_ENTRY        *SubEntry;
  REFIT_MENU_SCREEN   *SubScreen;
  REFIT_VOLUME        *Volume;
  UINT64              VolumeSize;
  EFI_GUID            *Guid = NULL;
  CHAR16              *FileName;

  if (Entry == NULL) {
    return;
  }

  Volume = Entry->Volume;

  if (Volume == NULL) {
    return;
  }

  FileName = Basename(Entry->LoaderPath);

  // create the submenu
  SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
  SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", Entry->me.Title, Entry->VolName);
  SubScreen->TitleImage = Entry->me.Image ? GetIconDetail(Entry->me) : Entry->me.DriveImage;
  SubScreen->ID = Entry->LoaderType + 20;
  //DBG("get anime for os=%d\n", SubScreen->ID);
  SubScreen->AnimeRun = GetAnime(SubScreen);
  VolumeSize = RShiftU64(MultU64x32(Volume->BlockIO->Media->LastBlock, Volume->BlockIO->Media->BlockSize), 20);

  AddMenuInfoLine(SubScreen, PoolPrint(L"Volume size: %dMb", VolumeSize));
  SplitMenuInfo(SubScreen, PoolPrint(L"Path: %s", FileDevicePathToStr(Entry->DevicePath)), AddMenuInfoLine);

  Guid = FindGPTPartitionGuidInDevicePath(Volume->DevicePath);
  if (Guid) {
    CHAR8   *GuidStr = AllocateZeroPool(50);

    AsciiSPrint(GuidStr, 50, "%g", Guid);
    AddMenuInfoLine(SubScreen, PoolPrint(L"UUID: %a", GuidStr));
    FreePool(GuidStr);
  }

  SplitMenuInfo(SubScreen, PoolPrint(L"Options: %s", Entry->LoadOptions), AddMenuInfoLine);

  SubEntry = DuplicateLoaderEntry(Entry);
  if (SubEntry) {
    AddOptionEntries(SubScreen, SubEntry);
    SubEntry->me.Title = L"Boot with selected options";
    SubEntry->me.ID = MENU_ENTRY_ID_BOOT;
    AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
  }

  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->me.SubScreen = SubScreen;
  //DBG("    Added '%s': OSType='%d', OSVersion='%a'\n", Entry->me.Title, Entry->LoaderType, Entry->OSVersion);
}

STATIC
BOOLEAN
AddLoaderEntry (
  IN CHAR16         *LoaderPath,
  IN CHAR16         *LoaderOptions,
  IN CHAR16         *LoaderTitle,
  IN REFIT_VOLUME   *Volume,
  IN EG_IMAGE       *Image,
  IN UINT8          OSType,
  IN UINT8          Flags
) {
  LOADER_ENTRY    *Entry;
  INTN             HVi;

  if (
    (LoaderPath == NULL) ||
    (Volume == NULL) ||
    (Volume->RootDir == NULL) ||
    !FileExists(Volume->RootDir, LoaderPath)
  ) {
    return FALSE;
  }

  MsgLog("        - %s\n", LoaderPath);
  //DBG("        AddLoaderEntry for Volume Name=%s\n", Volume->VolName);

  //don't add hided entries
  for (HVi = 0; HVi < gSettings.HVCount; HVi++) {
    if (StriStr(LoaderPath, gSettings.HVHideStrings[HVi])) {
      DBG("        hiding entry: %s\n", LoaderPath);
      return FALSE;
    }
  }

  Entry = CreateLoaderEntry (
            LoaderPath,
            LoaderOptions,
            NULL,
            LoaderTitle,
            Volume,
            Image,
            NULL,
            OSType,
            Flags,
            0,
            NULL,
            FALSE
          );

  if (Entry != NULL) {
    if (OSTYPE_IS_OSX_GLOB(Entry->LoaderType)) {
      if (gSettings.WithKexts) {
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_WITHKEXTS);
      }

      if (OSFLAG_ISSET(gSettings.FlagsBits, OSFLAG_DBGPATCHES) || gSettings.DebugKP) {
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_DBGPATCHES);
      }

      if (gSettings.NoCaches) {
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_NOCACHES);
      }
    }

    AddDefaultMenu(Entry);
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return TRUE;
  }

  return FALSE;
}

VOID
ScanLoader() {
  UINTN         VolumeIndex, Index;
  REFIT_VOLUME  *Volume;
  EFI_GUID      *PartGUID;

  //DBG("Scanning loaders...\n");
  DbgHeader("ScanLoader");

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];

    if (Volume->RootDir == NULL) { // || Volume->VolName == NULL)
      //DBG(", no file system\n", VolumeIndex);
      continue;
    }

    MsgLog("- [%02d]: '%s'", VolumeIndex, Volume->VolName);

    if (Volume->VolName == NULL) {
      Volume->VolName = L"Unknown";
    }

    // skip volume if its kind is configured as disabled
    if (MEDIA_VALID(Volume->DiskKind, GlobalConfig.DisableFlags)) {
      MsgLog(", hidden\n");
      continue;
    }

    if (Volume->Hidden) {
      MsgLog(", hidden\n");
      continue;
    }

    MsgLog("\n");

    // Use standard location for boot.efi, unless the file /.IAPhysicalMedia is present
    // That file indentifies a 2nd-stage Install Media, so when present, skip standard path to avoid entry duplication
    if (!FileExists(Volume->RootDir, L"\\.IAPhysicalMedia")) {
      if (EFI_ERROR(GetRootUUID(Volume)) || isFirstRootUUID(Volume)) {
        AddLoaderEntry(MACOSX_LOADER_PATH, NULL, L"Mac OS X", Volume, NULL, OSTYPE_OSX, 0);
      }
    }

    // check for Mac OS X Install Data
    Index = 0;
    while (Index < OSXInstallerPathsCount) {
      AddLoaderEntry(OSXInstallerPaths[Index++], NULL, L"OS X Install", Volume, NULL, OSTYPE_OSX_INSTALLER, 0);
    }

    // check for Mac OS X Recovery Boot
    AddLoaderEntry(MACOSX_RECOVERY_LOADER_PATH, NULL, L"Recovery", Volume, NULL, OSTYPE_RECOVERY, 0);

    // check for Windows
    Index = 0;
    while (Index < WINEFIPathsCount) {
      AddLoaderEntry(WINEFIPaths[Index++], NULL, L"Microsoft EFI", Volume, NULL, OSTYPE_WINEFI, 0);
    }

    if (gSettings.AndroidScan) {
      // check for Android loaders
      for (Index = 0; Index < AndroidEntryDataCount; ++Index) {
        UINTN   aIndex, aFound = 0;

        if (FileExists(Volume->RootDir, AndroidEntryData[Index].Path)) {
          for (aIndex = 0; aIndex < ANDX86_FINDLEN; ++aIndex) {
            if (
              (AndroidEntryData[Index].Find[aIndex] == NULL) ||
              FileExists(Volume->RootDir, AndroidEntryData[Index].Find[aIndex])
            ) {
              ++aFound;
            }
          }

          if (aFound && (aFound == aIndex)) {
            AddLoaderEntry (
              AndroidEntryData[Index].Path, L"", AndroidEntryData[Index].Title, Volume,
              LoadOSIcon(AndroidEntryData[Index].Icon, NULL, L"unknown", 128, FALSE, TRUE),
              OSTYPE_LIN, OSFLAG_NODEFAULTARGS
            );
          }
        }
      }
    }

    if (gSettings.LinuxScan) {
      // check for linux loaders
      for (Index = 0; Index < LinuxEntryDataCount; ++Index) {
        AddLoaderEntry (
          LinuxEntryData[Index].Path, L"", LinuxEntryData[Index].Title, Volume,
          LoadOSIcon(LinuxEntryData[Index].Icon, NULL, L"unknown", 128, FALSE, TRUE),
          OSTYPE_LIN, OSFLAG_NODEFAULTARGS
        );
      }

      // check for linux kernels
      PartGUID = FindGPTPartitionGuidInDevicePath(Volume->DevicePath);
      if ((PartGUID != NULL) && (Volume->RootDir != NULL)) {
        REFIT_DIR_ITER  Iter;
        EFI_FILE_INFO   *FileInfo = NULL;
        EFI_TIME        PreviousTime;
        CHAR16          *Path = NULL, *Options,
                        PartUUID[40]; // Get the partition UUID and make sure it's lower case

        ZeroMem(&PreviousTime, sizeof(EFI_TIME));
        UnicodeSPrint(PartUUID, sizeof(PartUUID), L"%g", PartGUID);

        // open the /boot directory (or whatever directory path)
        DirIterOpen(Volume->RootDir, LINUX_BOOT_PATH, &Iter);
        // Check which kernel scan to use

        if (gSettings.KernelScan != KERNEL_SCAN_NONE) {
          gSettings.KernelScan = KERNEL_SCAN_ALL;
        }

        switch (gSettings.KernelScan) {
          case KERNEL_SCAN_ALL:
            // get all the filename matches
            while (DirIterNext(&Iter, 2, LINUX_LOADER_SEARCH_PATH, &FileInfo)) {
              if (FileInfo != NULL) {
                if (FileInfo->FileSize > 0) {
                  // get the kernel file path
                  Path = PoolPrint(L"%s\\%s", LINUX_BOOT_PATH, FileInfo->FileName);
                  if (Path != NULL) {
                    Options = LinuxKernelOptions(Iter.DirHandle, Basename(Path) + StrLen(LINUX_LOADER_PATH), StrToLower(PartUUID), NULL);

                    // Add the entry
                    AddLoaderEntry (
                      Path,
                      (Options == NULL) ? LINUX_DEFAULT_OPTIONS : Options,
                      NULL, Volume, NULL, OSTYPE_LINEFI, OSFLAG_NODEFAULTARGS
                    );

                    if (Options != NULL) {
                      FreePool(Options);
                    }

                    FreePool(Path);
                  }
                }

                // free the file info
                FreePool(FileInfo);
                FileInfo = NULL;
              }
            }
            break;

          case KERNEL_SCAN_NONE:
          default:
            // No kernel scan
            break;
        }

        //close the directory
        DirIterClose(&Iter);
      }
    } //if linux scan

    //DBG("search for  optical UEFI\n");
    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
      AddLoaderEntry(BOOT_LOADER_PATH, L"", L"UEFI optical", Volume, NULL, OSTYPE_OTHER, 0);
    }
  }
}

STATIC
VOID
AddCustomEntry (
  IN UINTN                  CustomIndex,
  IN CHAR16                 *CustomPath,
  IN CUSTOM_LOADER_ENTRY    *Custom,
  IN REFIT_MENU_SCREEN      *SubMenu
) {
  UINTN             VolumeIndex;
  REFIT_VOLUME      *Volume;
  REFIT_DIR_ITER    SIter, *Iter = &SIter;
  CHAR16            PartUUID[40];
  BOOLEAN           IsSubEntry = (SubMenu != NULL), FindCustomPath = (CustomPath == NULL);

  if (Custom == NULL) {
    return;
  }

  if (FindCustomPath && (Custom->Type != OSTYPE_LINEFI)) {

    DBG("Custom %sentry %d skipped because it didn't have a ", IsSubEntry ? L"sub " : L"", CustomIndex);

    if (Custom->Type == 0) {
      DBG("Type.\n");
    } else {
      DBG("Path.\n");
    }

    return;
  }

  if (OSFLAG_ISSET(Custom->Flags, OSFLAG_DISABLED)) {
    DBG("Custom %sentry %d skipped because it is disabled.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }

  if (!gSettings.ShowHiddenEntries && OSFLAG_ISSET(Custom->Flags, OSFLAG_HIDDEN)) {
    DBG("Custom %sentry %d skipped because it is hidden.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }

  DBG("Custom %sentry %d :\n", IsSubEntry ? L"sub " : L"", CustomIndex);

  if (Custom->Title) {
    DBG(" - Title:\"%s\"\n", Custom->Title);
  }

  if (Custom->FullTitle) {
    DBG(" - FullTitle:\"%s\"\n", Custom->FullTitle);
  }

  if (CustomPath) {
    DBG(" - Path:\"%s\"\n", CustomPath);
  }

  if (Custom->Options != NULL) {
    DBG(" - Options:\"%s\"\n", Custom->Options);
  }

  DBG(" - Type:%d Flags:0x%X matching ", Custom->Type, Custom->Flags);

  if (Custom->Volume) {
    DBG("Volume: \"%s\"\n", Custom->Volume);
  } else {
    DBG("all volumes\n");
  }

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; ++VolumeIndex) {
    CUSTOM_LOADER_ENTRY   *CustomSubEntry;
    LOADER_ENTRY          *Entry = NULL;
    EG_IMAGE              *Image, *DriveImage, *ImageHover = NULL;
    CHAR16                *ImageHoverPath;
    EFI_GUID              *Guid = NULL;
    UINT64                VolumeSize;

    Volume = Volumes[VolumeIndex];

    if ((Volume == NULL) || (Volume->RootDir == NULL)) {
      continue;
    }

    if (Volume->VolName == NULL) {
      Volume->VolName = L"Unknown";
    }

    DBG("    Checking volume \"%s\" (%s) ... ", Volume->VolName, Volume->DevicePathString);

    // skip volume if its kind is configured as disabled
    if (MEDIA_VALID(Volume->DiskKind, GlobalConfig.DisableFlags)) {
      DBG("skipped because media is disabled\n");
      continue;
    }

    if (Custom->VolumeType != 0) {
      if (MEDIA_INVALID(Volume->DiskKind, Custom->VolumeType)) {
        DBG("skipped because media is ignored\n");
        continue;
      }
    }

    if (Volume->Hidden) {
      DBG("skipped because volume is hidden\n");
      continue;
    }

    // Check for exact volume matches
    if (Custom->Volume) {
      if (
        (StrStr(Volume->DevicePathString, Custom->Volume) == NULL) &&
        ((Volume->VolName == NULL) || (StrStr(Volume->VolName, Custom->Volume) == NULL))
      ) {
        DBG("skipped\n");
        continue;
      }
    }

    // Check the volume is readable and the entry exists on the volume
    if (Volume->RootDir == NULL) {
      DBG("skipped because filesystem is not readable\n");
      continue;
    }

    if (
      (StriCmp(CustomPath, MACOSX_LOADER_PATH) == 0) &&
      FileExists(Volume->RootDir, L"\\.IAPhysicalMedia")
    ) {
      DBG("skipped standard OSX path because volume is 2nd stage Install Media\n");
      continue;
    }

    Guid = FindGPTPartitionGuidInDevicePath(Volume->DevicePath);

    // Open the boot directory to search for kernels
    if (FindCustomPath) {
      // Get the partition UUID and make sure it's lower case
      if (Guid == NULL) {
        DBG("skipped because volume does not have partition uuid\n");
        continue;
      }

      UnicodeSPrint(PartUUID, sizeof(PartUUID), L"%g", Guid);

      // open the /boot directory (or whatever directory path)
      DirIterOpen(Volume->RootDir, LINUX_BOOT_PATH, Iter);

      Custom->KernelScan = KERNEL_SCAN_ALL;
    } else if (!FileExists(Volume->RootDir, CustomPath)) {
      DBG("skipped because path does not exist\n");
      continue;
    }

    // Change to custom image if needed
    Image = Custom->Image;
    if ((Image == NULL) && Custom->ImagePath) {
      ImageHoverPath = PoolPrint(
                          L"%s_hover.%s",
                          ReplaceExtension(Custom->ImagePath, L""),
                          egFindExtension(Custom->ImagePath)
                        );
      Image = egLoadImage(Volume->RootDir, Custom->ImagePath, TRUE);

      if (Image == NULL) {
        Image = egLoadImage(ThemeDir, Custom->ImagePath, TRUE);
        if (Image == NULL) {
          Image = egLoadImage(SelfDir, Custom->ImagePath, TRUE);
          if (Image == NULL) {
            Image = egLoadImage(SelfRootDir, Custom->ImagePath, TRUE);
            if (Image != NULL) {
              ImageHover = egLoadImage(SelfRootDir, ImageHoverPath, TRUE);
            }
          } else {
            ImageHover = egLoadImage(SelfDir, ImageHoverPath, TRUE);
          }
        } else {
          ImageHover = egLoadImage(ThemeDir, ImageHoverPath, TRUE);
        }
      } else {
        ImageHover = egLoadImage(Volume->RootDir, ImageHoverPath, TRUE);
      }
      FreePool(ImageHoverPath);
    } else {
      // Image base64 data
    }

    // Change to custom drive image if needed
    DriveImage = Custom->DriveImage;
    if ((DriveImage == NULL) && Custom->DriveImagePath) {
      DriveImage = egLoadImage(Volume->RootDir, Custom->DriveImagePath, TRUE);
      if (DriveImage == NULL) {
        DriveImage = egLoadImage(ThemeDir, Custom->DriveImagePath, TRUE);
        if (DriveImage == NULL) {
          DriveImage = egLoadImage(SelfDir, Custom->DriveImagePath, TRUE);
          if (DriveImage == NULL) {
            DriveImage = egLoadImage(SelfRootDir, Custom->DriveImagePath, TRUE);
            if (DriveImage == NULL) {
              DriveImage = LoadBuiltinIcon(Custom->DriveImagePath);
            }
          }
        }
      }
    }

    do { // Search for linux kernels
      CHAR16    *CustomOptions = Custom->Options;

      if (FindCustomPath && (Custom->KernelScan == KERNEL_SCAN_ALL)) {
        EFI_FILE_INFO   *FileInfo = NULL;

        // Get the next kernel path or stop looking
        if (!DirIterNext(Iter, 2, LINUX_LOADER_SEARCH_PATH, &FileInfo) || (FileInfo == NULL)) {
          DBG("\n");
          break;
        }

        // who knows....
        if (FileInfo->FileSize == 0) {
          FreePool(FileInfo);
          continue;
        }

        // get the kernel file path
        CustomPath = PoolPrint(L"%s\\%s", LINUX_BOOT_PATH, FileInfo->FileName);
        // free the file info
        FreePool(FileInfo);
      }

      if (CustomPath == NULL) {
        DBG("skipped\n");
        break;
      }

      // Check to make sure we should update custom options or not
      if (FindCustomPath && OSFLAG_ISUNSET(Custom->Flags, OSFLAG_NODEFAULTARGS)) {
        // Find the init ram image and select root
        CustomOptions = LinuxKernelOptions(Iter->DirHandle, Basename(CustomPath) + StrLen(LINUX_LOADER_PATH), StrToLower(PartUUID), Custom->Options);
      }

      // Check to make sure that this entry is not hidden or disabled by another custom entry
      if (!IsSubEntry) {
        CUSTOM_LOADER_ENTRY   *Ptr;
        UINTN                  i = 0;
        BOOLEAN                BetterMatch = FALSE;

        for (Ptr = gSettings.CustomEntries; Ptr != NULL; ++i, Ptr = Ptr->Next) {
          // Don't match against this custom
          if (Ptr == Custom) {
            continue;
          }

          // Can only match the same types
          if (Custom->Type != Ptr->Type) {
            continue;
          }

          // Check if the volume string matches
          if (Custom->Volume != Ptr->Volume) {
            if (Ptr->Volume == NULL) {
              // Less precise volume match
              if (Custom->Path != Ptr->Path) {
                // Better path match
                BetterMatch = (
                  (Ptr->Path != NULL) && (StrCmp(CustomPath, Ptr->Path) == 0) &&
                  (
                    (Custom->VolumeType == Ptr->VolumeType) ||
                    MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                  )
                );
              }
            } else if (
                (StrStr(Volume->DevicePathString, Custom->Volume) == NULL) &&
                ((Volume->VolName == NULL) || (StrStr(Volume->VolName, Custom->Volume) == NULL))
            ) {
              if (Custom->Volume == NULL) {
                if (Custom->Path != Ptr->Path) { // More precise volume match
                  // Better path match
                  BetterMatch = (
                    (Ptr->Path != NULL) && (StrCmp(CustomPath, Ptr->Path) == 0) &&
                    (
                      (Custom->VolumeType == Ptr->VolumeType) ||
                      MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                    )
                  );
                } else if (Custom->VolumeType != Ptr->VolumeType) {
                  // More precise volume type match
                  BetterMatch = (
                    (Custom->VolumeType == 0) &&
                    MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                  );
                } else {
                  // Better match
                  BetterMatch = TRUE;
                }
              } else if (Custom->Path != Ptr->Path) { // Duplicate volume match
                // Better path match
                BetterMatch = (
                  (Ptr->Path != NULL) && (StrCmp(CustomPath, Ptr->Path) == 0) &&
                  (
                    (Custom->VolumeType == Ptr->VolumeType) ||
                    MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                  )
                );
              } else if (Custom->VolumeType != Ptr->VolumeType) { // Duplicate path match
                // More precise volume type match
                BetterMatch = (
                  (Custom->VolumeType == 0) &&
                  MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                );
              } else {
                // Duplicate entry
                BetterMatch = (i <= CustomIndex);
              }
            }
          } else if (Custom->Path != Ptr->Path) { // Duplicate volume match
            if (Ptr->Path == NULL) {
              // Less precise path match
              BetterMatch = (
                (Custom->VolumeType != Ptr->VolumeType) &&
                MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
              );
            } else if (StrCmp(CustomPath, Ptr->Path) == 0) {
              if (Custom->Path == NULL) {
                // More precise path and volume type match
                BetterMatch = (
                  (Custom->VolumeType == Ptr->VolumeType) ||
                  MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                );
              } else if (Custom->VolumeType != Ptr->VolumeType) {
                // More precise volume type match
                BetterMatch = (
                  (Custom->VolumeType == 0) &&
                  MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
                );
              } else {
                // Duplicate entry
                BetterMatch = (i <= CustomIndex);
              }
            }
          } else if (Custom->VolumeType != Ptr->VolumeType) { // Duplicate path match
            // More precise volume type match
            BetterMatch = (
              (Custom->VolumeType == 0) &&
              MEDIA_VALID(Volume->DiskKind, Custom->VolumeType)
            );
          } else { // Duplicate entry
            BetterMatch = (i <= CustomIndex);
          }
          if (BetterMatch) {
            break;
          }
        }

        if (BetterMatch) {
          DBG("skipped because custom entry %d is a better match and will produce a duplicate entry\n", i);
          continue;
        }
      }

      DBG("match!\n");

      // Create an entry for this volume
      Entry = CreateLoaderEntry (
        CustomPath,
        CustomOptions,
        Custom->FullTitle,
        Custom->Title,
        Volume,
        Image,
        DriveImage,
        Custom->Type,
        Custom->Flags,
        Custom->Hotkey,
        NULL,
        TRUE
      );

      if (Entry != NULL) {
        DBG("Custom settings: %s.plist will %a be applied\n",
            Custom->Settings, Custom->CommonSettings ? "not" : "");

        if (!Custom->CommonSettings) {
          Entry->Settings = Custom->Settings;
        }

        if (ImageHover != NULL) {
          Entry->me.ImageHover = ImageHover;
        }

        if (OSFLAG_ISUNSET(Custom->Flags, OSFLAG_NODEFAULTMENU)) {
          AddDefaultMenu(Entry);
        } else if (Custom->SubEntries != NULL) {
          UINTN   CustomSubIndex = 0;
          // Add subscreen
          REFIT_MENU_SCREEN   *SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));

          if (SubScreen) {
            SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", (Custom->Title != NULL) ? Custom->Title : CustomPath, Entry->VolName);
            //SubScreen->TitleImage = Entry->me.Image;
            SubScreen->TitleImage = Entry->me.Image ? GetIconDetail(Entry->me) : Entry->me.DriveImage;
            SubScreen->ID = Custom->Type + 20;
            SubScreen->AnimeRun = GetAnime(SubScreen);
            VolumeSize = RShiftU64(MultU64x32(Volume->BlockIO->Media->LastBlock, Volume->BlockIO->Media->BlockSize), 20);

            AddMenuInfoLine(SubScreen, PoolPrint(L"Volume size: %dMb", VolumeSize));
            SplitMenuInfo(SubScreen, PoolPrint(L"Path: %s", FileDevicePathToStr(Entry->DevicePath)), AddMenuInfoLine);

            if (Guid) {
              CHAR8   *GuidStr = AllocateZeroPool(50);

              AsciiSPrint(GuidStr, 50, "%g", Guid);
              AddMenuInfoLine(SubScreen, PoolPrint(L"UUID: %a", GuidStr));
              FreePool(GuidStr);
            }

            SplitMenuInfo(SubScreen, PoolPrint(L"Options: %s", Entry->LoadOptions), AddMenuInfoLine);
            //DBG("Create sub entries\n");
            for (CustomSubEntry = Custom->SubEntries; CustomSubEntry; CustomSubEntry = CustomSubEntry->Next) {
              if (!CustomSubEntry->Settings) {
                CustomSubEntry->Settings = Custom->Settings;
              }

              AddCustomEntry (
                CustomSubIndex++,
                (CustomSubEntry->Path != NULL) ? CustomSubEntry->Path : CustomPath,
                CustomSubEntry,
                SubScreen
              );
            }

            AddMenuEntry(SubScreen, &MenuEntryReturn);
            Entry->me.SubScreen = SubScreen;
          }
        }

        AddMenuEntry(IsSubEntry ? SubMenu : &MainMenu, (REFIT_MENU_ENTRY *)Entry);
      }

      // cleanup custom
      if (FindCustomPath) {
        FreePool(CustomPath);
        FreePool(CustomOptions);
      }
    } while (FindCustomPath && (Custom->KernelScan == KERNEL_SCAN_ALL));

    // Close the kernel boot directory
    if (FindCustomPath) {
      DirIterClose(Iter);
    }
  }
}

// Add custom entries
VOID
AddCustomEntries() {
  CUSTOM_LOADER_ENTRY   *Custom;
  UINTN                 Index, i = 0;

  if (!gSettings.CustomEntries) {
    return;
  }

  //DBG("Custom entries start\n");
  DbgHeader("AddCustomEntries");
  // Traverse the custom entries
  for (Custom = gSettings.CustomEntries; Custom; ++i, Custom = Custom->Next) {
    if ((Custom->Path == NULL) && (Custom->Type != 0)) {
      if (OSTYPE_IS_OSX(Custom->Type)) {
        AddCustomEntry(i, MACOSX_LOADER_PATH, Custom, NULL);
      } else if (OSTYPE_IS_OSX_RECOVERY(Custom->Type)) {
        AddCustomEntry(i, MACOSX_RECOVERY_LOADER_PATH, Custom, NULL);
      } else if (OSTYPE_IS_OSX_INSTALLER(Custom->Type)) {
        Index = 0;
        while (Index < OSXInstallerPathsCount) {
          AddCustomEntry(i, OSXInstallerPaths[Index++], Custom, NULL);
        }
      } else if (OSTYPE_IS_WINDOWS(Custom->Type)) {
        AddCustomEntry(i, L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi", Custom, NULL);
      } else if (OSTYPE_IS_LINUX(Custom->Type)) {
        Index= 0;
        while (Index < AndroidEntryDataCount) {
          AddCustomEntry(i, AndroidEntryData[Index++].Path, Custom, NULL);
        }

        Index = 0;
        while (Index < LinuxEntryDataCount) {
          AddCustomEntry(i, LinuxEntryData[Index++].Path, Custom, NULL);
        }
      } else if (Custom->Type == OSTYPE_LINEFI) {
        AddCustomEntry(i, NULL, Custom, NULL);
      } else {
        AddCustomEntry(i, BOOT_LOADER_PATH, Custom, NULL);
      }
    } else {
      AddCustomEntry(i, Custom->Path, Custom, NULL);
    }
  }
  //DBG("Custom entries finish\n");
}

LOADER_ENTRY
*DuplicateLoaderEntry (
  IN LOADER_ENTRY   *Entry
) {
  if(Entry == NULL) {
    return NULL;
  }

  LOADER_ENTRY *DuplicateEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));

  if (DuplicateEntry) {
    DuplicateEntry->me.Tag                = Entry->me.Tag;
    //DuplicateEntry->me.AtClick          = ActionEnter;
    DuplicateEntry->Volume                = Entry->Volume;
    DuplicateEntry->DevicePathString      = EfiStrDuplicate(Entry->DevicePathString);
    DuplicateEntry->LoadOptions           = EfiStrDuplicate(Entry->LoadOptions);
    DuplicateEntry->LoaderPath            = EfiStrDuplicate(Entry->LoaderPath);
    DuplicateEntry->VolName               = EfiStrDuplicate(Entry->VolName);
    DuplicateEntry->DevicePath            = Entry->DevicePath;
    DuplicateEntry->Flags                 = Entry->Flags;
    DuplicateEntry->LoaderType            = Entry->LoaderType;
    DuplicateEntry->OSVersion             = Entry->OSVersion;
    DuplicateEntry->BuildVersion          = Entry->BuildVersion;
    DuplicateEntry->KernelAndKextPatches  = Entry->KernelAndKextPatches;
  }

  return DuplicateEntry;
}

CHAR16
*ToggleLoadOptions (
      UINT32    State,
  IN  CHAR16    *LoadOptions,
  IN  CHAR16    *LoadOption
) {
  return State ? AddLoadOption(LoadOptions, LoadOption) : RemoveLoadOption(LoadOptions, LoadOption);
}

CHAR16
*AddLoadOption (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  // If either option strings are null nothing to do
  if (LoadOptions == NULL) {
    if (LoadOption == NULL) {
      return NULL;
    }

    // Duplicate original options as nothing to add
    return EfiStrDuplicate(LoadOption);
  }

  // If there is no option or it is already present duplicate original
  else if ((LoadOption == NULL) || BootArgsExists(LoadOptions, LoadOption)) {
    return EfiStrDuplicate(LoadOptions);
  }

  // Otherwise add option
  return PoolPrint(L"%s %s", LoadOptions, LoadOption);
}

CHAR16
*RemoveLoadOption (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  CHAR16    *Placement, *NewLoadOptions;
  UINTN     Length, Offset, OptionLength;

  //DBG("LoadOptions: '%s', remove LoadOption: '%s'\n", LoadOptions, LoadOption);
  // If there are no options then nothing to do
  if (LoadOptions == NULL) {
    return NULL;
  }

  // If there is no option to remove then duplicate original
  if (LoadOption == NULL) {
    return EfiStrDuplicate(LoadOptions);
  }

  // If not present duplicate original
  Placement = StriStr(LoadOptions, LoadOption);
  if (Placement == NULL) {
    return EfiStrDuplicate(LoadOptions);
  }

  // Get placement of option in original options
  Offset = (Placement - LoadOptions);
  Length = StrLen(LoadOptions);
  OptionLength = StrLen(LoadOption);

  // If this is just part of some larger option (contains non-space at the beginning or end)
  if (
    ((Offset > 0) && (LoadOptions[Offset - 1] != L' ')) ||
    (((Offset + OptionLength) < Length) && (LoadOptions[Offset + OptionLength] != L' '))
  ) {
    return EfiStrDuplicate(LoadOptions);
  }

  // Consume preceeding spaces
  while ((Offset > 0) && (LoadOptions[Offset - 1] == L' ')) {
    OptionLength++;
    Offset--;
  }

  // Consume following spaces
  while (LoadOptions[Offset + OptionLength] == L' ') {
    OptionLength++;
  }

  // If it's the whole string return NULL
  if (OptionLength == Length) {
    return NULL;
  }

  if (Offset == 0) {
    // Simple case - we just need substring after OptionLength position
    NewLoadOptions = EfiStrDuplicate(LoadOptions + OptionLength);
  } else {
    // The rest of LoadOptions is Length - OptionLength, but we may need additional space and ending 0
    NewLoadOptions = AllocateZeroPool((Length - OptionLength + 2) * sizeof(CHAR16));
    // Copy preceeding substring
    CopyMem(NewLoadOptions, LoadOptions, Offset * sizeof(CHAR16));

    if ((Offset + OptionLength) < Length) {
      // Copy following substring, but include one space also
      OptionLength--;

      CopyMem (
        NewLoadOptions + Offset,
        LoadOptions + Offset + OptionLength,
        (Length - OptionLength - Offset) * sizeof(CHAR16)
      );
    }
  }

  return NewLoadOptions;
}
