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

#define DBG(...) DebugLog (DEBUG_SCAN_LOADER, __VA_ARGS__)

//#define DUMP_KERNEL_KEXT_PATCHES 1
CHAR16  *SupportedOsType[3] = { OSTYPE_DARWIN_STR, OSTYPE_LINUX_STR, OSTYPE_WINDOWS_STR };

#define BOOT_LOADER_PATH                  L"\\EFI\\BOOT\\BOOTX64.efi"

#define DARWIN_LOADER_PATH                L"\\System\\Library\\CoreServices\\boot.efi"
#define DARWIN_INSTALLER_LOADERBASE_PATH  L"\\System\\Library\\CoreServices\\bootbase.efi"
#define DARWIN_RECOVERY_LOADER_PATH       L"\\com.apple.recovery.boot\\boot.efi"

#define LINUX_ISSUE_PATH                  L"\\etc\\issue"
#define LINUX_BOOT_PATH                   L"\\boot"
#define LINUX_LOADER_PATH                 L"vmlinuz"
#define LINUX_FULL_LOADER_PATH            LINUX_BOOT_PATH L"\\" LINUX_LOADER_PATH
#define LINUX_LOADER_SEARCH_PATH          LINUX_LOADER_PATH L"*"
#define LINUX_DEFAULT_OPTIONS             L"ro add_efi_memmap quiet splash vt.handoff=7"

// Linux loader path data
typedef struct {
  CHAR16  *Path;
  CHAR16  *Title;
  CHAR16  *Icon;
  CHAR8   *Issue;
} LINUX_PATH_DATA;

STATIC LINUX_PATH_DATA LinuxEntryData[] = {
  { L"\\EFI\\Gentoo\\grubx64.efi",      L"Gentoo",        L"gentoo,linux",    "Gentoo" },
  { L"\\EFI\\RedHat\\grubx64.efi",      L"RedHat",        L"redhat,linux",    "Redhat" },
  { L"\\EFI\\ubuntu\\grubx64.efi",      L"Ubuntu",        L"ubuntu,linux",    "Ubuntu" },
  { L"\\EFI\\LinuxMint\\grubx64.efi",   L"Mint",          L"mint,linux",      "Linux Mint" },
  { L"\\EFI\\Fedora\\grubx64.efi",      L"Fedora",        L"fedora,linux",    "Fedora" },
  { L"\\EFI\\opensuse\\grubx64.efi",    L"OpenSuse",      L"suse,linux",      "openSUSE" },
  { L"\\EFI\\debian\\grubx64.efi",      L"Debian",        L"debian,linux",    "Debian" },
  { L"\\EFI\\arch\\grubx64.efi",        L"Arch",          L"arch,linux",      NULL },

  //{ L"\\EFI\\grub\\grubx64.efi",        L"Grub",          L"grub,linux" },
  //{ L"\\EFI\\kubuntu\\grubx64.efi",     L"kubuntu",       L"kubuntu,linux",   "kubuntu" },
  //{ L"\\EFI\\arch_grub\\grubx64.efi",   L"Arch",          L"arch,linux" },
  //{ L"\\EFI\\Gentoo\\kernelx64.efi",    L"Gentoo kernel", L"gentoo,linux" },
  //{ L"\\EFI\\SuSe\\elilo.efi",          L"OpenSuse",      L"suse,linux" },
};

STATIC CONST UINTN LinuxEntryDataCount = ARRAY_SIZE (LinuxEntryData);

STATIC CHAR16 *LinuxInitImagePath[] = {
   L"initrd%s",
   L"initrd.img%s",
   L"initrd%s.img",
   L"initramfs%s",
   L"initramfs.img%s",
   L"initramfs%s.img",
};

STATIC CONST UINTN LinuxInitImagePathCount = ARRAY_SIZE (LinuxInitImagePath);

#define ANDX86_FINDLEN 3

typedef struct {
  CHAR16   *Path;
  CHAR16   *Title;
  CHAR16   *Icon;
  CHAR16   *Find[ANDX86_FINDLEN];
} ANDX86_PATH_DATA;

STATIC ANDX86_PATH_DATA AndroidEntryData[] = {
  { L"\\EFI\\remixos\\grubx64.efi", L"Remix",   L"remix,grub,linux",    { L"\\isolinux\\isolinux.bin",  L"\\initrd.img",          L"\\kernel" } },
  { L"\\EFI\\boot\\grubx64.efi",    L"Phoenix", L"phoenix,grub,linux",  { L"\\phoenix\\kernel",         L"\\phoenix\\initrd.img", L"\\phoenix\\ramdisk.img" } },
  { L"\\EFI\\boot\\bootx64.efi",    L"Chrome",  L"chrome,grub,linux",   { L"\\syslinux\\vmlinuz.A",     L"\\syslinux\\vmlinuz.B", L"\\syslinux\\ldlinux.sys" } },
};

STATIC CONST UINTN AndroidEntryDataCount = ARRAY_SIZE (AndroidEntryData);

/*
STATIC CHAR16 *OSXInstallerPaths[] = {
  L"\\Mac OS X Install Data\\boot.efi",
  L"\\macOS Install Data\\boot.efi",
  L"\\OS X Install Data\\boot.efi",
  L"\\.IABootFiles\\boot.efi"
};

STATIC CONST UINTN OSXInstallerPathsCount = ARRAY_SIZE (OSXInstallerPaths);
*/

STATIC CHAR16   *WINEFIPaths[] = {
  L"\\EFI\\MICROSOFT\\BOOT\\bootmgfw.efi",
  //L"\\EFI\\MICROSOFT\\BOOT\\bootmgfw-orig.efi",
  L"\\EFI\\MICROSOFT\\BOOT\\cdboot.efi"
};

STATIC CONST UINTN WINEFIPathsCount = ARRAY_SIZE (WINEFIPaths);

STATIC
BOOLEAN
IsBootExists (
  IN CHAR16   *Path,
  IN CHAR16   **Lists
) {
  INTN      Count = ARRAY_SIZE (Lists), Index = 0;
  BOOLEAN   Found = FALSE;

  while (!Found && (Index < Count)) {
    Found = (StriCmp (Path, Lists[Index++]) == 0);
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

  if (StriCmp (Path, DARWIN_INSTALLER_LOADERBASE_PATH) == 0) {
    return OSTYPE_DARWIN_INSTALLER;
  } else if (StriCmp (Path, DARWIN_RECOVERY_LOADER_PATH) == 0) {
    return OSTYPE_DARWIN_RECOVERY;
  } else if (StriCmp (Path, DARWIN_LOADER_PATH) == 0) {
    return OSTYPE_DARWIN;
  } else if (IsBootExists (Path, WINEFIPaths)) {
    return OSTYPE_WINEFI;
  } else if (StrniCmp (Path, LINUX_FULL_LOADER_PATH, StrSize (LINUX_FULL_LOADER_PATH)) == 0) {
    return OSTYPE_LINEFI;
  } else {
    UINTN   Index= 0;

    while (Index < AndroidEntryDataCount) {
      if (StriCmp (Path, AndroidEntryData[Index].Path) == 0) {
        return OSTYPE_LIN;
      }

      ++Index;
    }

    Index = 0;
    while (Index < LinuxEntryDataCount) {
      if (StriCmp (Path, LinuxEntryData[Index].Path) == 0) {
        return OSTYPE_LIN;
      }

      ++Index;
    }
  }

  return OSTYPE_OTHER;
}

STATIC
CHAR16 *
LinuxIconNameFromPath (
  IN CHAR16             *Path,
  IN EFI_FILE_PROTOCOL  *RootDir
) {
  UINTN   Index = 0;

  if (gSettings.TextOnly || IsEmbeddedTheme ()) {
    goto Finish;
  }

  while (Index < AndroidEntryDataCount) {
    if (StriCmp (Path, AndroidEntryData[Index].Path) == 0) {
      return AndroidEntryData[Index].Icon;
    }

    ++Index;
  }

  Index = 0;
  while (Index < LinuxEntryDataCount) {
    if (StriCmp (Path, LinuxEntryData[Index].Path) == 0) {
      return LinuxEntryData[Index].Icon;
    }

    ++Index;
  }

  // Try to open the linux issue
  if ((RootDir != NULL) && (StrniCmp (Path, LINUX_FULL_LOADER_PATH, StrSize (LINUX_FULL_LOADER_PATH)) == 0)) {
    CHAR8   *Issue = NULL;
    UINTN   IssueLen = 0;

    if (!EFI_ERROR (LoadFile (RootDir, LINUX_ISSUE_PATH, (UINT8 **)&Issue, &IssueLen)) && (Issue != NULL)) {
      if (IssueLen > 0) {
        for (Index = 0; Index < LinuxEntryDataCount; ++Index) {
          if (
            (LinuxEntryData[Index].Issue != NULL) &&
            (AsciiStrStr (Issue, LinuxEntryData[Index].Issue) != NULL)
          ) {
            FreePool (Issue);
            return LinuxEntryData[Index].Icon;
          }
        }
      }

      FreePool (Issue);
    }
  }

Finish:
  return L"linux";
}

STATIC
CHAR16 *
LinuxKernelOptions (
  IN EFI_FILE_PROTOCOL  *Dir,
  IN CHAR16             *Version,
  IN CHAR16             *PartUUID,
  IN CHAR16             *Options OPTIONAL
) {
  UINTN   Index = 0;

  if ((Dir == NULL) || (PartUUID == NULL)) {
    return (Options == NULL) ? NULL : EfiStrDuplicate (Options);
  }

  while (Index < LinuxInitImagePathCount) {
    CHAR16  *InitRd = PoolPrint (LinuxInitImagePath[Index++], (Version == NULL) ? L"" : Version);

    if (InitRd != NULL) {
      if (FileExists (Dir, InitRd)) {
        CHAR16  *CustomOptions = PoolPrint (L"root=/dev/disk/by-partuuid/%s initrd=%s\\%s %s %s",
                                   PartUUID, LINUX_BOOT_PATH, InitRd,
                                   LINUX_DEFAULT_OPTIONS, (Options == NULL) ? L"" : Options
                                 );

        FreePool (InitRd);

        return CustomOptions;
      }

      FreePool (InitRd);
    }
  }

  return PoolPrint (L"root=/dev/disk/by-partuuid/%s %s %s",
            PartUUID, LINUX_DEFAULT_OPTIONS, (Options == NULL) ? L"" : Options
          );
}

STATIC
BOOLEAN
IsFirstRootUUID (
  REFIT_VOLUME    *Volume
) {
  UINTN           VolumeIndex;
  REFIT_VOLUME    *ScanedVolume;

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    ScanedVolume = Volumes[VolumeIndex];

    if (ScanedVolume == Volume) {
      return TRUE;
    }

    if (CompareGuid (&ScanedVolume->RootUUID, &Volume->RootUUID)) {
      return FALSE;
    }
  }

  return TRUE;
}

//Set Entry->VolName to .disk_label.contentDetails if it exists
STATIC
EFI_STATUS
GetDarwinVolumeName (
  LOADER_ENTRY    *Entry
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  CHAR16        *TargetNameFile = L"\\System\\Library\\CoreServices\\.disk_label.contentDetails";
  CHAR8         *FileBuffer, *TargetString;
  UINTN         FileLen = 0, Len;

  if (FileExists (Entry->Volume->RootDir, TargetNameFile)) {
    Status = LoadFile (Entry->Volume->RootDir, TargetNameFile, (UINT8 **)&FileBuffer, &FileLen);
    if (!EFI_ERROR (Status)) {
      Len = FileLen + 1;

      //Create null terminated string
      TargetString = AllocateZeroPool (Len);
      CopyMem ((VOID *)TargetString, (VOID *)FileBuffer, FileLen);
      DBG (" - found disk_label with contents:%a\n", TargetString);

      //Convert to Unicode
      Entry->VolName = AllocateZeroPool (Len);
      AsciiStrToUnicodeStrS (TargetString, Entry->VolName, Len);

      DBG (" - created name: %s\n", Entry->VolName);

      FreePool (FileBuffer);
      FreePool (TargetString);
    }
  }

  return Status;
}

BOOLEAN
ReplacePlaceholderTemplate (
  IN OUT  CHAR16  **Str,
  IN OUT  CHAR16  **Buffer,
  IN OUT  UINTN   *i,
  IN      UINTN   Len,
  IN      CHAR16  *Placeholder,
  IN      CHAR16  *Replacement
) {
  UINTN     PlaceholderLen = StrLen (Placeholder),
            ReplacementLen = StrLen (Replacement);
  BOOLEAN   Ret = FALSE;

  if (!StrnCmp (*Str, Placeholder, PlaceholderLen)) {
    if (ReplacementLen) {
      StrCatS (*Buffer + *i, Len, Replacement);
      *i += ReplacementLen;
    }

    *Str += PlaceholderLen;

    Ret = TRUE;
  }

  return Ret;
}

CHAR16 *
TranslateLoaderTitleTemplate (
  IN OUT  LOADER_ENTRY    *Entry,
  IN      CHAR16          *Label,
  IN      CHAR16          *Path
) {
  CHAR16  *Buffer, *Tpl = DEF_DISK_TEMPLATE, *Platform = L"Unknown",
          OSVersion[8], OSBuildVersion[8];
  UINTN   i = 0, TplLen = 0, Len;
  UINT8   Major = 0, Minor = 0, Revision = 0;

  if (Entry->OSVersion) {
    AsciiStrToUnicodeStrS (Entry->OSVersion, OSVersion, ARRAY_SIZE (OSVersion));
  } else {
    *OSVersion = 0;
  }

  if (Entry->OSBuildVersion) {
    AsciiStrToUnicodeStrS (Entry->OSBuildVersion, OSBuildVersion, ARRAY_SIZE (OSBuildVersion));
  } else {
    *OSBuildVersion = 0;
  }

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    Platform = OSTYPE_DARWIN_STR;
    Major = Entry->OSVersionMajor;
    Minor = Entry->OSVersionMinor;
    Revision = Entry->OSRevision;
  }

  switch (Entry->LoaderType) {
    case OSTYPE_DARWIN:
      TplLen = StrLen (gSettings.DarwinDiskTemplate);
      if (TplLen) {
        Tpl = gSettings.DarwinDiskTemplate;
      } else {
        Tpl = DEF_DARWIN_DISK_TEMPLATE;
      }
      break;

    case OSTYPE_DARWIN_RECOVERY:
      TplLen = StrLen (gSettings.DarwinRecoveryDiskTemplate);
      if (TplLen) {
        Tpl = gSettings.DarwinRecoveryDiskTemplate;
      } else {
        Tpl = DEF_DARWIN_RECOVERY_TEMPLATE;
      }
      break;

    case OSTYPE_DARWIN_INSTALLER:
      TplLen = StrLen (gSettings.DarwinInstallerDiskTemplate);
      if (TplLen) {
        Tpl = gSettings.DarwinInstallerDiskTemplate;
      } else {
        Tpl = DEF_DARWIN_INSTALLER_TEMPLATE;
      }
      break;

    case OSTYPE_WIN:
    case OSTYPE_WINEFI:
      Platform = OSTYPE_WINDOWS_STR;
      TplLen = StrLen (gSettings.WindowsDiskTemplate);
      if (TplLen) {
        Tpl = gSettings.WindowsDiskTemplate;
      } else {
        Tpl = DEF_WINDOWS_TEMPLATE;
      }
      break;

    case OSTYPE_LIN:
    case OSTYPE_LINEFI:
      Platform = OSTYPE_LINUX_STR;
      TplLen = StrLen (gSettings.LinuxDiskTemplate);
      if (TplLen) {
        Tpl = gSettings.LinuxDiskTemplate;
      } else {
        Tpl = DEF_LINUX_TEMPLATE;
      }
      break;

    default:
      break;
  }

  if (!TplLen) {
    TplLen = StrLen (Tpl);
  }

  if ((Label == NULL) || !StrLen (Label)) {
    Label = L"";
  }

  if ((Path == NULL) || !StrLen (Path)) {
    Path = L"";
  }

  Len = (
          TplLen +
          StrLen (Label) +
          StrLen (Path) +
          StrLen (Platform) +
          StrLen (OSVersion) +
          StrLen (OSBuildVersion) +
          6 // Major, Minor, Revision
        );

  Buffer = AllocateZeroPool (Len);

  while (!IS_NULL (*Tpl) && (*Tpl == 0x20)) {
    Tpl++;
  }

  while (!IS_NULL (*Tpl)) {
    if (*Tpl == L'$') {
      Tpl++;
      continue;
    }

    if (
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"label", Label) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"path", Path) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"platform", Platform) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"version", OSVersion) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"build", OSBuildVersion) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"major", PoolPrint (L"%d", Major)) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"minor", PoolPrint (L"%d", Minor)) &&
      !ReplacePlaceholderTemplate (&Tpl, &Buffer, &i, Len, L"revision", PoolPrint (L"%d", Revision))
    ) {
      Buffer[i++] = *(Tpl++);
    }
  }

  Buffer[i] = 0;

  StrCleanSpaces (&Buffer);

  return Buffer;
}

STATIC
LOADER_ENTRY *
CreateLoaderEntry (
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
  EFI_DEVICE_PATH   *LoaderDevicePath;
  CHAR16            *LoaderDevicePathString, *FilePathAsString, ShortcutLetter,
                    *OSIconName, *HoverImage, *OSIconNameHover = NULL;
  LOADER_ENTRY      *Entry;
  INTN              i;
  CHAR8             *Indent = "    ",
                    *OSVersion = NULL, *OSBuildVersion = NULL;
  EG_IMAGE          *ImageTmp;
  SVersion          *DarwinOSVersion;

  // Check parameters are valid
  if ((LoaderPath == NULL) || (*LoaderPath == 0) || (Volume == NULL)) {
    return NULL;
  }

  // Get the loader device path
  LoaderDevicePath = FileDevicePath (Volume->DeviceHandle, (*LoaderPath == L'\\') ? (LoaderPath + 1) : LoaderPath);

  if (LoaderDevicePath == NULL) {
    return NULL;
  }

  LoaderDevicePathString = FileDevicePathToStr (LoaderDevicePath);

  if (LoaderDevicePathString == NULL) {
    return NULL;
  }

  // Ignore this loader if it's self path
  FilePathAsString = FileDevicePathToStr (SelfFullDevicePath);

  if (FilePathAsString) {
    INTN    Comparison = StriCmp (FilePathAsString, LoaderDevicePathString);

    FreePool (FilePathAsString);
    if (Comparison == 0) {
      DBG ("%a skipped because path `%s` is self path!\n", Indent, LoaderDevicePathString);
      FreePool (LoaderDevicePathString);
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

          if (StriCmp (Loader->DevicePathString, LoaderDevicePathString) == 0) {
            DBG ("%a skipped because path `%s` already exists for another entry!\n", Indent, LoaderDevicePathString);
            FreePool (LoaderDevicePathString);
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
        (OSFLAG_ISSET (Custom->Flags, OSFLAG_DISABLED) ||
        (OSFLAG_ISSET (Custom->Flags, OSFLAG_HIDDEN) && !gSettings.ShowHiddenEntries))
      ) {
        INTN  VolumeMatch = 0,
              VolumeTypeMatch = 0,
              PathMatch = 0,
              TypeMatch = 0;

        // Check if volume match
        if (Custom->Volume != NULL) {
          // Check if the string matches the volume
          VolumeMatch = (
            (StrStr (Volume->DevicePathString, Custom->Volume) != NULL) ||
             ((Volume->VolName != NULL) && (StrStr (Volume->VolName, Custom->Volume) != NULL))
          ) ? 1 : -1;
        }

        // Check if the volume_type match
        if (Custom->VolumeType != 0) {
          VolumeTypeMatch = MEDIA_VALID (Volume->DiskKind, Custom->VolumeType) ? 1 : -1;
        }

        // Check if the path match
        if (Custom->Path != NULL) {
          // Check if the loader path match
          PathMatch = (StriCmp (Custom->Path, LoaderPath) == 0) ? 1 : -1;
        }

        // Check if the type match
        if (Custom->Type != 0) {
          TypeMatch = OSTYPE_COMPARE (Custom->Type, OSType) ? 1 : -1;
        }

        if ((VolumeMatch == -1) || (VolumeTypeMatch == -1) || (PathMatch == -1) || (TypeMatch == -1)) {
          UINTN   AddComma = 0;

          DBG ("%aNot match custom entry %d: ", Indent, CustomIndex);

          if (VolumeMatch != 0) {
            DBG ("Volume: %s", VolumeMatch == 1 ? L"match" : L"not match");

            AddComma++;
          }

          if (PathMatch != 0) {
            DBG ("%sPath: %s",
                (AddComma ? L", " : L""),
                PathMatch == 1 ? L"match" : L"not match");

            AddComma++;
          }

          if (VolumeTypeMatch != 0) {
            DBG ("%sVolumeType: %s",
                (AddComma ? L", " : L""),
                VolumeTypeMatch == 1 ? L"match" : L"not match");

            AddComma++;
          }

          if (TypeMatch != 0) {
            DBG ("%sType: %s",
                (AddComma ? L", " : L""),
                TypeMatch == 1 ? L"match" : L"not match");
          }

          DBG ("\n");
        } else {
          // Custom entry match
          DBG ("%aSkipped because matching custom entry %d!\n", Indent, CustomIndex);
          FreePool (LoaderDevicePathString);
          return NULL;
        }
      }

      Custom = Custom->Next;
      ++CustomIndex;
    }
  }

  if (OSTYPE_IS_DARWIN_GLOB (OSType)) {
    GetDarwinVersion (OSType, Volume->RootDir, &OSVersion, &OSBuildVersion);
    DarwinOSVersion = StrToVersion (OSVersion);

    //if (!DARWIN_OS_VER_MINIMUM (DarwinOSVersion->VersionMajor, DarwinOSVersion->VersionMinor)) {
    if (OSX_LT (OSVersion, DARWIN_OS_VER_STR_MINIMUM)) {
      return NULL;
    }
  }

  // prepare the menu entry
  Entry                     = AllocateZeroPool (sizeof (LOADER_ENTRY));
  Entry->me.Tag             = TAG_LOADER;
  Entry->me.Row             = 0;
  Entry->Volume             = Volume;

  Entry->LoaderPath         = EfiStrDuplicate (LoaderPath);
  Entry->VolName            = Volume->VolName;
  Entry->DevicePath         = LoaderDevicePath;
  Entry->DevicePathString   = LoaderDevicePathString;
  Entry->Flags              = OSFLAG_SET (Flags, OSFLAG_USEGRAPHICS);

  if (LoaderOptions) {
    if (OSFLAG_ISSET (Flags, OSFLAG_NODEFAULTARGS)) {
      Entry->LoadOptions    = EfiStrDuplicate (LoaderOptions);
    } else {
      Entry->LoadOptions    = PoolPrint (L"%a %s", gSettings.BootArgs, LoaderOptions);
    }
  } else if ((AsciiStrLen (gSettings.BootArgs) > 0) && OSFLAG_ISUNSET (Flags, OSFLAG_NODEFAULTARGS)) {
    Entry->LoadOptions      = PoolPrint (L"%a", gSettings.BootArgs);
  }

  Entry->LoaderType         = OSType;
  Entry->OSVersion          = NULL;
  Entry->OSBuildVersion     = NULL;
  Entry->OSVersionMajor     = 0;
  Entry->OSVersionMinor     = 0;
  Entry->OSRevision         = 0;

  // detect specific loaders
  OSIconName = NULL;
  ShortcutLetter = 0;

  switch (OSType) {
    case OSTYPE_DARWIN:
    case OSTYPE_DARWIN_RECOVERY:
    case OSTYPE_DARWIN_INSTALLER:
      //GetDarwinVersion (&Entry);
      Entry->OSVersion = AllocateCopyPool (AsciiStrSize (OSVersion), OSVersion);
      Entry->OSBuildVersion = AllocateCopyPool (AsciiStrSize (OSBuildVersion), OSBuildVersion);
      Entry->OSVersionMajor = DarwinOSVersion->VersionMajor;
      Entry->OSVersionMinor = DarwinOSVersion->VersionMinor;
      Entry->OSRevision = DarwinOSVersion->Revision;

      //OSIconName = GetOSIconName (Entry->OSVersion);// Sothor - Get OSIcon name using OSVersion
      OSIconName = GetOSIconName (DarwinOSVersion);

      if ((OSType == OSTYPE_DARWIN) && IsDarwinHibernated (Volume)) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_HIBERNATED);
      }

      ShortcutLetter = 'M';
      if ((Entry->VolName == NULL) || (StrLen (Entry->VolName) == 0)) {
        // else no sense to override it with dubious name
        GetDarwinVolumeName (Entry);
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
      OSIconName = LinuxIconNameFromPath (LoaderPath, Volume->RootDir);
      ShortcutLetter = 'L';
      break;

    case OSTYPE_OTHER:
    case OSTYPE_EFI:
    default:
      OSIconName = L"unknown";
      Entry->LoaderType = OSTYPE_OTHER;
      break;
  }

  if (FullTitle) { // Custom
    Entry->me.Title = EfiStrDuplicate (FullTitle);
  } else if ((Entry->VolName == NULL) || (StrLen (Entry->VolName) == 0)) {
    //DBG ("encounter Entry->VolName ==%s and StrLen (Entry->VolName) ==%d\n",Entry->VolName, StrLen (Entry->VolName));
    //if (GlobalConfig.BootCampStyle) {
    //  Entry->me.Title = PoolPrint (L"%s", ((LoaderTitle != NULL) ? LoaderTitle : Basename (Volume->DevicePathString)));
    //} else {
    //  Entry->me.Title = PoolPrint (L"Boot %s from %s", (LoaderTitle != NULL) ? LoaderTitle : Basename (LoaderPath),
    //                                Basename (Volume->DevicePathString));
    //}

    Entry->me.Title = TranslateLoaderTitleTemplate (
                        Entry,
                        GlobalConfig.BootCampStyle // label
                          ? (LoaderTitle != NULL) ? LoaderTitle : Basename (Volume->DevicePathString)
                          : (LoaderTitle != NULL) ? LoaderTitle : Basename (LoaderPath),
                        GlobalConfig.BootCampStyle // path
                          ? NULL
                          : Basename (Volume->DevicePathString)
                      );
  } else {
    //DBG ("encounter LoaderTitle ==%s and Entry->VolName ==%s\n", LoaderTitle, Entry->VolName);
    //if (GlobalConfig.BootCampStyle) {
    //  if ((StriCmp (LoaderTitle, L"Mac OS X") == 0) || (StriCmp (LoaderTitle, L"Recovery") == 0)) {
    //    Entry->me.Title = PoolPrint (L"%s", Entry->VolName);
    //  } else {
    //    Entry->me.Title = PoolPrint (L"%s", (LoaderTitle != NULL) ? LoaderTitle : Basename (LoaderPath));
    //  }
    //} else {
    //  Entry->me.Title = PoolPrint (L"Boot %s from %s", (LoaderTitle != NULL) ? LoaderTitle : Basename (LoaderPath),
    //                                Entry->VolName);
    //}

    Entry->me.Title = TranslateLoaderTitleTemplate (
                        Entry,
                        GlobalConfig.BootCampStyle // label
                          ? NULL
                          //: OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)
                          //  ? NULL
                          //  : (LoaderTitle != NULL) ? LoaderTitle : Basename (LoaderPath),
                          : (LoaderTitle != NULL) ? LoaderTitle : Basename (LoaderPath),
                        Entry->VolName // path
                      );
  }

  //DBG ("Entry->me.Title =%s\n", Entry->me.Title);
  // just an example that UI can show hibernated volume to the user
  // should be better to show it on entry image
  if (OSFLAG_ISSET (Entry->Flags, OSFLAG_HIBERNATED)) {
    Entry->me.Title = PoolPrint (L"%s (hibernated)", Entry->me.Title);
  }

  Entry->me.ShortcutLetter = (Hotkey == 0) ? ShortcutLetter : Hotkey;

  if (!gSettings.TextOnly) {
    // Load DriveImage
    Entry->me.DriveImage = (DriveImage != NULL) ? DriveImage : ScanVolumeDefaultIcon (Volume->DiskKind, Entry->LoaderType);

    if (IsEmbeddedTheme ()) {
      goto Finish;
    }

    ImageTmp = LoadOSIcon (OSIconName, &OSIconNameHover, L"unknown", FALSE, TRUE);
    Entry->me.Image = Image ? Image : ImageTmp;

    // DBG ("HideBadges=%d Volume=%s ", GlobalConfig.HideBadges, Volume->VolName);
    if (GlobalConfig.HideBadges & HDBADGES_SHOW) {
      if (GlobalConfig.HideBadges & HDBADGES_SWAP) {
        Entry->me.BadgeImage = CopyScaledImage (Entry->me.DriveImage, GlobalConfig.BadgeScale);
        // DBG (" Show badge as Drive.");
      } else {
        Entry->me.BadgeImage = CopyScaledImage (Entry->me.Image, GlobalConfig.BadgeScale);
        if (OSIconNameHover != NULL) {
          // DBG (" Show badge as OSImage.");
          HoverImage = AllocateZeroPool (sizeof (OSIconNameHover));
          HoverImage = PoolPrint (L"icons\\%s.png", OSIconNameHover);
          Entry->me.ImageHover = LoadHoverIcon (HoverImage);
          FreePool (HoverImage);
        }
      }
    }

    FreePool (OSIconNameHover);
  }

Finish:
  FreePool (OSIconName);

  Entry->KernelAndKextPatches = (
    (Patches == NULL) ? (KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF (SETTINGS_DATA, KernelAndKextPatches)) : Patches
  );

  //DBG ("%aLoader entry created for '%s'\n", indent, Entry->DevicePathString);

  return Entry;
}

BOOLEAN
IsKernelIs64BitOnly (
  IN LOADER_ENTRY   *Entry
) {
  return OSX_GE (Entry->OSVersion, DARWIN_OS_VER_STR_MINIMUM);
}

//
//  Draw Drive + icon on submenu entries.
//

STATIC
EG_IMAGE *
GetIconDetail (
  IN REFIT_MENU_ENTRY   MenuEntry
) {
  EG_IMAGE    *rImage,
              *TopImg = NULL,
              *BaseImg = CopyImage (MenuEntry.DriveImage);
  INTN        Width, Height;

  if (GlobalConfig.HideBadges & HDBADGES_SHOW) {
    if (GlobalConfig.HideBadges & HDBADGES_SWAP) {
      FreeImage (BaseImg);
      BaseImg = CopyImage (MenuEntry.ImageHover ? MenuEntry.ImageHover : MenuEntry.Image);
      TopImg = CopyImage (MenuEntry.BadgeImage);
    } else {
      TopImg = CopyImage (MenuEntry.ImageHover ? MenuEntry.ImageHover : MenuEntry.BadgeImage);
    }
  }

  Width   = BaseImg->Width + 16;
  Height  = BaseImg->Height + 16;

  rImage = CreateFilledImage (Width, Height, TRUE, &TransparentBackgroundPixel);

  ComposeImage (rImage, BaseImg, 8, 8);

  if (TopImg != NULL) {
    ComposeImage (rImage, TopImg,
      MenuEntry.ImageHover ? 8 : ((GlobalConfig.BadgeOffsetX != 0xFFFF) ? (GlobalConfig.BadgeOffsetX + 8) : (Width - TopImg->Width)),
      MenuEntry.ImageHover ? 8 : ((GlobalConfig.BadgeOffsetY != 0xFFFF) ? (GlobalConfig.BadgeOffsetY + 8) : (Height - TopImg->Height))
    );

    FreeImage (TopImg);
  }

  FreeImage (BaseImg);

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

  FileName = Basename (Entry->LoaderPath);

  // create the submenu
  SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
  SubScreen->Title = Entry->me.Title
                      ? EfiStrDuplicate (Entry->me.Title)
                      : PoolPrint (L"Boot Options for %s on %s", Entry->me.Title, Entry->VolName);
  SubScreen->TitleImage = Entry->me.Image ? GetIconDetail (Entry->me) : Entry->me.DriveImage;
  SubScreen->ID = Entry->LoaderType + 20;
  //DBG ("get anime for os=%d\n", SubScreen->ID);
  SubScreen->AnimeRun = GetAnime (SubScreen);
  VolumeSize = RShiftU64 (MultU64x32 (Volume->BlockIO->Media->LastBlock, Volume->BlockIO->Media->BlockSize), 20);

  AddMenuInfoLine (SubScreen, PoolPrint (L"Volume size: %dMb", VolumeSize));
  SplitMenuInfo (SubScreen, PoolPrint (L"Path: %s", FileDevicePathToStr (Entry->DevicePath)), AddMenuInfoLine);

  Guid = FindGPTPartitionGuidInDevicePath (Volume->DevicePath);
  if (Guid) {
    CHAR8   *GuidStr = AllocateZeroPool (50);

    AsciiSPrint (GuidStr, 50, "%g", Guid);
    AddMenuInfoLine (SubScreen, PoolPrint (L"UUID: %a", GuidStr));
    FreePool (GuidStr);
  }

  SplitMenuInfo (SubScreen, PoolPrint (L"Options: %s", Entry->LoadOptions), AddMenuInfoLine);

  SubEntry = DuplicateLoaderEntry (Entry);
  if (SubEntry) {
    AddOptionEntries (SubScreen, SubEntry);
    SubEntry->me.Title = L"Boot with selected options";
    SubEntry->me.ID = MENU_ENTRY_ID_BOOT;
    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
  }

  AddMenuEntry (SubScreen, &MenuEntryReturn);
  Entry->me.SubScreen = SubScreen;
  //DBG ("    Added '%s': OSType='%d', OSVersion='%a'\n", Entry->me.Title, Entry->LoaderType, Entry->OSVersion);
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
    !FileExists (Volume->RootDir, LoaderPath)
  ) {
    return FALSE;
  }

  MsgLog ("        - %s\n", LoaderPath);
  //DBG ("        AddLoaderEntry for Volume Name=%s\n", Volume->VolName);

  //don't add hided entries
  for (HVi = 0; HVi < gSettings.HVCount; HVi++) {
    if (StriStr (LoaderPath, gSettings.HVHideStrings[HVi])) {
      DBG ("        hiding entry: %s\n", LoaderPath);
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
    if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
      if (gSettings.WithKexts) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_WITHKEXTS);
      }

      if (gSettings.DebugKP) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_DBGPATCHES);
      }

      if (gSettings.KextPatchesAllowed) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_ALLOW_KEXT_PATCHES);
      }

      if (gSettings.KernelPatchesAllowed) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_ALLOW_KERNEL_PATCHES);
      }

      if (gSettings.BooterPatchesAllowed) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_ALLOW_BOOTER_PATCHES);
      }

      if (gSettings.NoCaches) {
        Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_NOCACHES);
      }
    }

    AddDefaultMenu (Entry);
    AddMenuEntry (&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return TRUE;
  }

  return FALSE;
}

VOID
ScanLoader () {
  UINTN         VolumeIndex, Index;
  REFIT_VOLUME  *Volume;
  EFI_GUID      *PartGUID;

  //DBG ("Scanning loaders...\n");
  DbgHeader ("ScanLoader");

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];

    if (Volume->RootDir == NULL) { // || Volume->VolName == NULL)
      //DBG (", no file system\n", VolumeIndex);
      continue;
    }

    MsgLog ("- [%02d]: '%s'", VolumeIndex, Volume->VolName);

    if (Volume->VolName == NULL) {
      Volume->VolName = L"Unknown";
    }

    // skip volume if its kind is configured as disabled
    if (MEDIA_VALID (Volume->DiskKind, GlobalConfig.DisableFlags)) {
      MsgLog (", hidden\n");
      continue;
    }

    if (Volume->Hidden) {
      MsgLog (", hidden\n");
      continue;
    }

    MsgLog ("\n");

    //
    // Darwin
    //

    // Use standard location for boot.efi, unless the file /.IAPhysicalMedia is present
    // That file indentifies a 2nd-stage Install Media, so when present, skip standard path to avoid entry duplication
    if (FileExists (Volume->RootDir, DARWIN_INSTALLER_LOADERBASE_PATH)) {
      AddLoaderEntry (DARWIN_INSTALLER_LOADERBASE_PATH, NULL, L"Installer", Volume, NULL, OSTYPE_DARWIN_INSTALLER, 0);
    }

    else if (FileExists (Volume->RootDir, DARWIN_RECOVERY_LOADER_PATH)) {
      AddLoaderEntry (DARWIN_RECOVERY_LOADER_PATH, NULL, L"Recovery", Volume, NULL, OSTYPE_DARWIN_RECOVERY, 0);
    }

    else if (EFI_ERROR (GetRootUUID (Volume)) || IsFirstRootUUID (Volume)) {
      AddLoaderEntry (DARWIN_LOADER_PATH, NULL, L"MacOS", Volume, NULL, OSTYPE_DARWIN, 0);
    }

    //
    // Windows
    //

    Index = 0;
    while (Index < WINEFIPathsCount) {
      AddLoaderEntry (WINEFIPaths[Index++], NULL, L"Microsoft", Volume, NULL, OSTYPE_WINEFI, 0);
    }

    //
    // Android
    //

    if (gSettings.AndroidScan) {
      // check for Android loaders
      for (Index = 0; Index < AndroidEntryDataCount; ++Index) {
        UINTN   aIndex, aFound = 0;

        if (FileExists (Volume->RootDir, AndroidEntryData[Index].Path)) {
          for (aIndex = 0; aIndex < ANDX86_FINDLEN; ++aIndex) {
            if (
              (AndroidEntryData[Index].Find[aIndex] == NULL) ||
              FileExists (Volume->RootDir, AndroidEntryData[Index].Find[aIndex])
            ) {
              ++aFound;
            }
          }

          if (aFound && (aFound == aIndex)) {
            AddLoaderEntry (
              AndroidEntryData[Index].Path, L"", AndroidEntryData[Index].Title, Volume,
              LoadOSIcon (AndroidEntryData[Index].Icon, NULL, L"unknown", FALSE, TRUE),
              OSTYPE_LIN, OSFLAG_NODEFAULTARGS
            );
          }
        }
      }
    }

    //
    // Linux
    //

    if (gSettings.LinuxScan) {
      // check for linux loaders
      for (Index = 0; Index < LinuxEntryDataCount; ++Index) {
        AddLoaderEntry (
          LinuxEntryData[Index].Path, L"", LinuxEntryData[Index].Title, Volume,
          LoadOSIcon (LinuxEntryData[Index].Icon, NULL, L"unknown", FALSE, TRUE),
          OSTYPE_LIN, OSFLAG_NODEFAULTARGS
        );
      }

      // check for linux kernels
      PartGUID = FindGPTPartitionGuidInDevicePath (Volume->DevicePath);
      if ((PartGUID != NULL) && (Volume->RootDir != NULL)) {
        REFIT_DIR_ITER  Iter;
        EFI_FILE_INFO   *FileInfo = NULL;
        EFI_TIME        PreviousTime;
        CHAR16          *Path = NULL, *Options,
                        PartUUID[40]; // Get the partition UUID and make sure it's lower case

        ZeroMem (&PreviousTime, sizeof (EFI_TIME));
        UnicodeSPrint (PartUUID, sizeof (PartUUID), L"%g", PartGUID);

        // open the /boot directory (or whatever directory path)
        DirIterOpen (Volume->RootDir, LINUX_BOOT_PATH, &Iter);
        // Check which kernel scan to use

        if (gSettings.KernelScan != KERNEL_SCAN_NONE) {
          gSettings.KernelScan = KERNEL_SCAN_ALL;
        }

        switch (gSettings.KernelScan) {
          case KERNEL_SCAN_ALL:
            // get all the filename matches
            while (DirIterNext (&Iter, 2, LINUX_LOADER_SEARCH_PATH, &FileInfo)) {
              if (FileInfo != NULL) {
                if (FileInfo->FileSize > 0) {
                  // get the kernel file path
                  Path = PoolPrint (L"%s\\%s", LINUX_BOOT_PATH, FileInfo->FileName);
                  if (Path != NULL) {
                    Options = LinuxKernelOptions (Iter.DirHandle, Basename (Path) + StrLen (LINUX_LOADER_PATH), StrToLower (PartUUID), NULL);

                    // Add the entry
                    AddLoaderEntry (
                      Path,
                      (Options == NULL) ? LINUX_DEFAULT_OPTIONS : Options,
                      NULL, Volume, NULL, OSTYPE_LINEFI, OSFLAG_NODEFAULTARGS
                    );

                    if (Options != NULL) {
                      FreePool (Options);
                    }

                    FreePool (Path);
                  }
                }

                // free the file info
                FreePool (FileInfo);
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
        DirIterClose (&Iter);
      }
    } //if linux scan

    //DBG ("search for  optical UEFI\n");
    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
      AddLoaderEntry (BOOT_LOADER_PATH, L"", L"UEFI optical", Volume, NULL, OSTYPE_OTHER, 0);
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
    DBG ("Custom %sentry %d skipped because it didn't have a ", IsSubEntry ? L"sub " : L"", CustomIndex);

    if (Custom->Type == 0) {
      DBG ("Type.\n");
    } else {
      DBG ("Path.\n");
    }

    return;
  }

  if (OSFLAG_ISSET (Custom->Flags, OSFLAG_DISABLED)) {
    DBG ("Custom %sentry %d skipped because it is disabled.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }

  if (!gSettings.ShowHiddenEntries && OSFLAG_ISSET (Custom->Flags, OSFLAG_HIDDEN)) {
    DBG ("Custom %sentry %d skipped because it is hidden.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }

  DBG ("Custom %sentry %d :\n", IsSubEntry ? L"sub " : L"", CustomIndex);

  if (Custom->Title) {
    DBG (" - Title:\"%s\"\n", Custom->Title);
  }

  if (Custom->FullTitle) {
    DBG (" - FullTitle:\"%s\"\n", Custom->FullTitle);
  }

  if (CustomPath) {
    DBG (" - Path:\"%s\"\n", CustomPath);
  }

  if (Custom->Options != NULL) {
    DBG (" - Options:\"%s\"\n", Custom->Options);
  }

  DBG (" - Type:%d Flags:0x%X matching ", Custom->Type, Custom->Flags);

  if (Custom->Volume) {
    DBG ("Volume: \"%s\"\n", Custom->Volume);
  } else {
    DBG ("all volumes\n");
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

    DBG ("    Checking volume \"%s\" (%s) ... ", Volume->VolName, Volume->DevicePathString);

    // skip volume if its kind is configured as disabled
    if (MEDIA_VALID (Volume->DiskKind, GlobalConfig.DisableFlags)) {
      DBG ("skipped because media is disabled\n");
      continue;
    }

    if (Custom->VolumeType != 0) {
      if (MEDIA_INVALID (Volume->DiskKind, Custom->VolumeType)) {
        DBG ("skipped because media is ignored\n");
        continue;
      }
    }

    if (Volume->Hidden) {
      DBG ("skipped because volume is hidden\n");
      continue;
    }

    // Check for exact volume matches
    if (Custom->Volume) {
      if (
        (StrStr (Volume->DevicePathString, Custom->Volume) == NULL) &&
        ((Volume->VolName == NULL) || (StrStr (Volume->VolName, Custom->Volume) == NULL))
      ) {
        DBG ("skipped\n");
        continue;
      }
    }

    // Check the volume is readable and the entry exists on the volume
    if (Volume->RootDir == NULL) {
      DBG ("skipped because filesystem is not readable\n");
      continue;
    }

    if (
      (StriCmp (CustomPath, DARWIN_LOADER_PATH) == 0) &&
      FileExists (Volume->RootDir, L"\\.IAPhysicalMedia")
      //FileExists (Volume->RootDir, DARWIN_INSTALLER_LOADERBASE_PATH)
    ) {
      DBG ("skipped standard OSX path because volume is 2nd stage Install Media\n");
      continue;
    }

    Guid = FindGPTPartitionGuidInDevicePath (Volume->DevicePath);

    // Open the boot directory to search for kernels
    if (FindCustomPath) {
      // Get the partition UUID and make sure it's lower case
      if (Guid == NULL) {
        DBG ("skipped because volume does not have partition uuid\n");
        continue;
      }

      UnicodeSPrint (PartUUID, sizeof (PartUUID), L"%g", Guid);

      // open the /boot directory (or whatever directory path)
      DirIterOpen (Volume->RootDir, LINUX_BOOT_PATH, Iter);

      Custom->KernelScan = KERNEL_SCAN_ALL;
    } else if (!FileExists (Volume->RootDir, CustomPath)) {
      DBG ("skipped because path does not exist\n");
      continue;
    }

    // Change to custom image if needed
    Image = Custom->Image;
    if ((Image == NULL) && Custom->ImagePath) {
      ImageHoverPath = PoolPrint (
                          L"%s_hover.%s",
                          ReplaceExtension (Custom->ImagePath, L""),
                          FindExtension (Custom->ImagePath)
                        );

      Image = LoadImage (Volume->RootDir, Custom->ImagePath);
      if (Image == NULL) {
        Image = LoadImage (ThemeDir, Custom->ImagePath);
        if (Image == NULL) {
          Image = LoadImage (SelfDir, Custom->ImagePath);
          if (Image == NULL) {
            Image = LoadImage (SelfRootDir, Custom->ImagePath);
            if (Image != NULL) {
              ImageHover = LoadImage (SelfRootDir, ImageHoverPath);
            }
          } else {
            ImageHover = LoadImage (SelfDir, ImageHoverPath);
          }
        } else {
          ImageHover = LoadImage (ThemeDir, ImageHoverPath);
        }
      } else {
        ImageHover = LoadImage (Volume->RootDir, ImageHoverPath);
      }
      FreePool (ImageHoverPath);
    } else {
      // Image base64 data
    }

    // Change to custom drive image if needed
    DriveImage = Custom->DriveImage;
    if ((DriveImage == NULL) && Custom->DriveImagePath) {
      DriveImage = LoadImage (Volume->RootDir, Custom->DriveImagePath);
      if (DriveImage == NULL) {
        DriveImage = LoadImage (ThemeDir, Custom->DriveImagePath);
        if (DriveImage == NULL) {
          DriveImage = LoadImage (SelfDir, Custom->DriveImagePath);
          if (DriveImage == NULL) {
            DriveImage = LoadImage (SelfRootDir, Custom->DriveImagePath);
            if (DriveImage == NULL) {
              DriveImage = LoadBuiltinIcon (Custom->DriveImagePath);
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
        if (!DirIterNext (Iter, 2, LINUX_LOADER_SEARCH_PATH, &FileInfo) || (FileInfo == NULL)) {
          DBG ("\n");
          break;
        }

        // who knows....
        if (FileInfo->FileSize == 0) {
          FreePool (FileInfo);
          continue;
        }

        // get the kernel file path
        CustomPath = PoolPrint (L"%s\\%s", LINUX_BOOT_PATH, FileInfo->FileName);
        // free the file info
        FreePool (FileInfo);
      }

      if (CustomPath == NULL) {
        DBG ("skipped\n");
        break;
      }

      // Check to make sure we should update custom options or not
      if (FindCustomPath && OSFLAG_ISUNSET (Custom->Flags, OSFLAG_NODEFAULTARGS)) {
        // Find the init ram image and select root
        CustomOptions = LinuxKernelOptions (Iter->DirHandle, Basename (CustomPath) + StrLen (LINUX_LOADER_PATH), StrToLower (PartUUID), Custom->Options);
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
                  (Ptr->Path != NULL) && (StrCmp (CustomPath, Ptr->Path) == 0) &&
                  (
                    (Custom->VolumeType == Ptr->VolumeType) ||
                    MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
                  )
                );
              }
            } else if (
                (StrStr (Volume->DevicePathString, Custom->Volume) == NULL) &&
                ((Volume->VolName == NULL) || (StrStr (Volume->VolName, Custom->Volume) == NULL))
            ) {
              if (Custom->Volume == NULL) {
                if (Custom->Path != Ptr->Path) { // More precise volume match
                  // Better path match
                  BetterMatch = (
                    (Ptr->Path != NULL) && (StrCmp (CustomPath, Ptr->Path) == 0) &&
                    (
                      (Custom->VolumeType == Ptr->VolumeType) ||
                      MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
                    )
                  );
                } else if (Custom->VolumeType != Ptr->VolumeType) {
                  // More precise volume type match
                  BetterMatch = (
                    (Custom->VolumeType == 0) &&
                    MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
                  );
                } else {
                  // Better match
                  BetterMatch = TRUE;
                }
              } else if (Custom->Path != Ptr->Path) { // Duplicate volume match
                // Better path match
                BetterMatch = (
                  (Ptr->Path != NULL) && (StrCmp (CustomPath, Ptr->Path) == 0) &&
                  (
                    (Custom->VolumeType == Ptr->VolumeType) ||
                    MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
                  )
                );
              } else if (Custom->VolumeType != Ptr->VolumeType) { // Duplicate path match
                // More precise volume type match
                BetterMatch = (
                  (Custom->VolumeType == 0) &&
                  MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
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
                MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
              );
            } else if (StrCmp (CustomPath, Ptr->Path) == 0) {
              if (Custom->Path == NULL) {
                // More precise path and volume type match
                BetterMatch = (
                  (Custom->VolumeType == Ptr->VolumeType) ||
                  MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
                );
              } else if (Custom->VolumeType != Ptr->VolumeType) {
                // More precise volume type match
                BetterMatch = (
                  (Custom->VolumeType == 0) &&
                  MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
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
              MEDIA_VALID (Volume->DiskKind, Custom->VolumeType)
            );
          } else { // Duplicate entry
            BetterMatch = (i <= CustomIndex);
          }

          if (BetterMatch) {
            break;
          }
        }

        if (BetterMatch) {
          DBG ("skipped because custom entry %d is a better match and will produce a duplicate entry\n", i);
          continue;
        }
      }

      DBG ("match!\n");

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
        DBG ("Custom settings: %s.plist will %a be applied\n",
            Custom->Settings, Custom->CommonSettings ? "not" : "");

        if (!Custom->CommonSettings) {
          Entry->Settings = Custom->Settings;
        }

        if (ImageHover != NULL) {
          Entry->me.ImageHover = ImageHover;
        }

        if (OSFLAG_ISUNSET (Custom->Flags, OSFLAG_NODEFAULTMENU)) {
          AddDefaultMenu (Entry);
        } else if (Custom->SubEntries != NULL) {
          UINTN               CustomSubIndex = 0;
          REFIT_MENU_SCREEN   *SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN)); // Add subscreen

          if (SubScreen) {
            SubScreen->Title =  Entry->me.Title
                                  ? EfiStrDuplicate (Entry->me.Title)
                                  : PoolPrint (L"Boot Options for %s on %s", (Custom->Title != NULL) ? Custom->Title : CustomPath, Entry->VolName);

            SubScreen->TitleImage = Entry->me.Image ? GetIconDetail (Entry->me) : Entry->me.DriveImage;
            SubScreen->ID = Custom->Type + 20;
            SubScreen->AnimeRun = GetAnime (SubScreen);

            VolumeSize = RShiftU64 (MultU64x32 (Volume->BlockIO->Media->LastBlock, Volume->BlockIO->Media->BlockSize), 20);

            AddMenuInfoLine (SubScreen, PoolPrint (L"Volume size: %dMb", VolumeSize));
            SplitMenuInfo (SubScreen, PoolPrint (L"Path: %s", FileDevicePathToStr (Entry->DevicePath)), AddMenuInfoLine);

            if (Guid) {
              CHAR8   *GuidStr = AllocateZeroPool (50);

              AsciiSPrint (GuidStr, 50, "%g", Guid);
              AddMenuInfoLine (SubScreen, PoolPrint (L"UUID: %a", GuidStr));
              FreePool (GuidStr);
            }

            SplitMenuInfo (SubScreen, PoolPrint (L"Options: %s", Entry->LoadOptions), AddMenuInfoLine);

            //DBG ("Create sub entries\n");
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

            AddMenuEntry (SubScreen, &MenuEntryReturn);
            Entry->me.SubScreen = SubScreen;
          }
        }

        AddMenuEntry (IsSubEntry ? SubMenu : &MainMenu, (REFIT_MENU_ENTRY *)Entry);
      }

      // cleanup custom
      if (FindCustomPath) {
        FreePool (CustomPath);
        FreePool (CustomOptions);
      }
    } while (FindCustomPath && (Custom->KernelScan == KERNEL_SCAN_ALL));

    // Close the kernel boot directory
    if (FindCustomPath) {
      DirIterClose (Iter);
    }
  }
}

// Add custom entries
VOID
AddCustomEntries () {
  CUSTOM_LOADER_ENTRY   *Custom;
  UINTN                 Index, i = 0;

  if (!gSettings.CustomEntries) {
    return;
  }

  //DBG ("Custom entries start\n");
  DbgHeader ("AddCustomEntries");
  // Traverse the custom entries
  for (Custom = gSettings.CustomEntries; Custom; ++i, Custom = Custom->Next) {
    if ((Custom->Path == NULL) && (Custom->Type != 0)) {
      if (OSTYPE_IS_DARWIN (Custom->Type)) {
        AddCustomEntry (i, DARWIN_LOADER_PATH, Custom, NULL);

      } else if (OSTYPE_IS_DARWIN_RECOVERY (Custom->Type)) {
        AddCustomEntry (i, DARWIN_RECOVERY_LOADER_PATH, Custom, NULL);

      } else if (OSTYPE_IS_DARWIN_INSTALLER (Custom->Type)) {
        //Index = 0;
        //while (Index < OSXInstallerPathsCount) {
        //  AddCustomEntry (i, OSXInstallerPaths[Index++], Custom, NULL);
        //}
        AddCustomEntry (i, DARWIN_LOADER_PATH, Custom, NULL);

      } else if (OSTYPE_IS_WINDOWS (Custom->Type)) {
        AddCustomEntry (i, WINEFIPaths[0], Custom, NULL);

      } else if (OSTYPE_IS_LINUX (Custom->Type)) {
        Index= 0;
        while (Index < AndroidEntryDataCount) {
          AddCustomEntry (i, AndroidEntryData[Index++].Path, Custom, NULL);
        }

        Index = 0;
        while (Index < LinuxEntryDataCount) {
          AddCustomEntry (i, LinuxEntryData[Index++].Path, Custom, NULL);
        }

        AddCustomEntry (i, NULL, Custom, NULL);
      //} else if (Custom->Type == OSTYPE_LINEFI) {
      //  AddCustomEntry (i, NULL, Custom, NULL);
      } else {
        AddCustomEntry (i, BOOT_LOADER_PATH, Custom, NULL);
      }

    } else {
      AddCustomEntry (i, Custom->Path, Custom, NULL);
    }
  }
  //DBG ("Custom entries finish\n");
}

LOADER_ENTRY *
DuplicateLoaderEntry (
  IN LOADER_ENTRY   *Entry
) {
  if (Entry == NULL) {
    return NULL;
  }

  LOADER_ENTRY *DuplicateEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));

  if (DuplicateEntry) {
    DuplicateEntry->me.Tag                = Entry->me.Tag;
    //DuplicateEntry->me.AtClick          = ActionEnter;
    DuplicateEntry->Volume                = Entry->Volume;
    DuplicateEntry->DevicePathString      = EfiStrDuplicate (Entry->DevicePathString);
    DuplicateEntry->LoadOptions           = EfiStrDuplicate (Entry->LoadOptions);
    DuplicateEntry->LoaderPath            = EfiStrDuplicate (Entry->LoaderPath);
    DuplicateEntry->VolName               = EfiStrDuplicate (Entry->VolName);
    DuplicateEntry->DevicePath            = Entry->DevicePath;
    DuplicateEntry->Flags                 = Entry->Flags;
    DuplicateEntry->LoaderType            = Entry->LoaderType;
    DuplicateEntry->OSVersion             = Entry->OSVersion;
    DuplicateEntry->OSBuildVersion        = Entry->OSBuildVersion;
    DuplicateEntry->KernelAndKextPatches  = Entry->KernelAndKextPatches;
  }

  return DuplicateEntry;
}

INTN
FindDefaultEntry () {
  INTN                Index;
  REFIT_VOLUME        *Volume;
  LOADER_ENTRY        *Entry;
  BOOLEAN             SearchForLoader;

  //DBG ("FindDefaultEntry ...\n");
  DbgHeader ("FindDefaultEntry");

  Index = FindStartupDiskVolume (&MainMenu);

  if (Index >= 0) {
    DBG ("Boot redirected to Entry %d. '%s'\n", Index, MainMenu.Entries[Index]->Title);
    return Index;
  }

  //
  // if not found, then try DefaultVolume from config.plist
  // if not null or empty, search volume that matches gSettings.DefaultVolume
  //
  if (gSettings.DefaultVolume != NULL) {
    // if not null or empty, also search for loader that matches gSettings.DefaultLoader
    SearchForLoader = ((gSettings.DefaultLoader != NULL) && (gSettings.DefaultLoader[0] != L'\0'));

    for (Index = 0; ((Index < (INTN)MainMenu.EntryCount) && (MainMenu.Entries[Index]->Row == 0)); Index++) {
      Entry = (LOADER_ENTRY *)MainMenu.Entries[Index];
      if (!Entry->Volume) {
        continue;
      }

      Volume = Entry->Volume;
      if (
        (
          (Volume->VolName == NULL) ||
          (StrCmp (Volume->VolName, gSettings.DefaultVolume) != 0)
        ) &&
        (StrStr (Volume->DevicePathString, gSettings.DefaultVolume) == NULL)
      ) {
        continue;
      }

      if (
        SearchForLoader &&
        (
          (Entry->me.Tag != TAG_LOADER) ||
          (StriStr (Entry->LoaderPath, gSettings.DefaultLoader) == NULL)
        )
      ) {
        continue;
      }

      DBG (" - found entry %d. '%s', Volume '%s', DevicePath '%s'\n",
        Index, Entry->me.Title, Volume->VolName, Entry->DevicePathString);

      return Index;
    }

  }

  DBG ("Default boot entry not found\n");

  return -1;
}

CHAR16 *
AddLoadOption (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  // If either option strings are null nothing to do
  if (LoadOptions == NULL) {
    if (LoadOption == NULL) {
      return NULL;
    }

    // Duplicate original options as nothing to add
    return EfiStrDuplicate (LoadOption);
  }

  // If there is no option or it is already present duplicate original
  else if ((LoadOption == NULL) || BootArgsExists (LoadOptions, LoadOption)) {
    return EfiStrDuplicate (LoadOptions);
  }

  // Otherwise add option
  return PoolPrint (L"%s %s", LoadOptions, LoadOption);
}

CHAR16 *
RemoveLoadOption (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  CHAR16    *Placement, *NewLoadOptions;
  UINTN     Length, Offset, OptionLength;

  //DBG ("LoadOptions: '%s', remove LoadOption: '%s'\n", LoadOptions, LoadOption);
  // If there are no options then nothing to do
  if (LoadOptions == NULL) {
    return NULL;
  }

  // If there is no option to remove then duplicate original
  if (LoadOption == NULL) {
    return EfiStrDuplicate (LoadOptions);
  }

  // If not present duplicate original
  Placement = StriStr (LoadOptions, LoadOption);
  if (Placement == NULL) {
    return EfiStrDuplicate (LoadOptions);
  }

  // Get placement of option in original options
  Offset = (Placement - LoadOptions);
  Length = StrLen (LoadOptions);
  OptionLength = StrLen (LoadOption);

  // If this is just part of some larger option (contains non-space at the beginning or end)
  if (
    ((Offset > 0) && (LoadOptions[Offset - 1] != L' ')) ||
    (((Offset + OptionLength) < Length) && (LoadOptions[Offset + OptionLength] != L' '))
  ) {
    return EfiStrDuplicate (LoadOptions);
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
    NewLoadOptions = EfiStrDuplicate (LoadOptions + OptionLength);
  } else {
    // The rest of LoadOptions is Length - OptionLength, but we may need additional space and ending 0
    NewLoadOptions = AllocateZeroPool ((Length - OptionLength + 2) * sizeof (CHAR16));
    // Copy preceeding substring
    CopyMem (NewLoadOptions, LoadOptions, Offset * sizeof (CHAR16));

    if ((Offset + OptionLength) < Length) {
      // Copy following substring, but include one space also
      OptionLength--;

      CopyMem (
        NewLoadOptions + Offset,
        LoadOptions + Offset + OptionLength,
        (Length - OptionLength - Offset) * sizeof (CHAR16)
      );
    }
  }

  return NewLoadOptions;
}

CHAR16 *
ToggleLoadOptions (
  IN  BOOLEAN   State,
  IN  CHAR16    *LoadOptions,
  IN  CHAR16    *LoadOption
) {
  return State ? AddLoadOption (LoadOptions, LoadOption) : RemoveLoadOption (LoadOptions, LoadOption);
}

//
// Null ConOut OutputString () implementation - for blocking
// text output from boot.efi when booting in graphics mode
//

EFI_STATUS
EFIAPI
NullConOutOutputString (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN CHAR16                           *String
) {
  return EFI_SUCCESS;
}


EFI_STATUS
LoadEFIImageList (
  IN  EFI_DEVICE_PATH   **DevicePaths,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
) {
  EFI_STATUS      Status = EFI_NOT_FOUND;
  EFI_HANDLE      ChildImageHandle = 0;
  UINTN           DevicePathIndex;
  CHAR16          ErrorInfo[AVALUE_MAX_SIZE];

  DBG ("Loading %s", ImageTitle);

  if (ErrorInStep != NULL) {
    *ErrorInStep = 0;
  }

  if (NewImageHandle != NULL) {
    *NewImageHandle = NULL;
  }

  for (DevicePathIndex = 0; (DevicePaths[DevicePathIndex] != NULL); DevicePathIndex++) {
    Status = gBS->LoadImage (
                    FALSE,
                    SelfImageHandle,
                    DevicePaths[DevicePathIndex],
                    NULL,
                    0,
                    &ChildImageHandle
                  );

    DBG (" ... %r", Status);

    if (!EFI_ERROR (Status)) {
      break;
    }
  }

  UnicodeSPrint (ErrorInfo, ARRAY_SIZE (ErrorInfo), L"while loading %s", ImageTitle);

  if (CheckError (Status, ErrorInfo)) {
    if (ErrorInStep != NULL) {
      *ErrorInStep = 1;
    }

    goto bailout;
  }

  if (!EFI_ERROR (Status)) { //why unload driver?!
    if (NewImageHandle != NULL) {
      *NewImageHandle = ChildImageHandle;
    }

    goto bailout;
  }

  // unload the image, we don't care if it works or not...
  /* Status = */ gBS->UnloadImage (ChildImageHandle);

bailout:

  DBG ("\n");

  return Status;
}

EFI_STATUS
StartEFILoadedImage (
  IN  EFI_HANDLE    ChildImageHandle,
  IN  CHAR16        *LoadOptions,
  IN  CHAR16        *LoadOptionsPrefix,
  IN  CHAR16        *ImageTitle,
  OUT UINTN         *ErrorInStep
) {
  EFI_STATUS          Status = EFI_NOT_FOUND;
  EFI_LOADED_IMAGE    *ChildLoadedImage;
  CHAR16              ErrorInfo[AVALUE_MAX_SIZE], *FullLoadOptions = NULL;

  //DBG ("Starting %s\n", ImageTitle);

  if (ErrorInStep != NULL) {
    *ErrorInStep = 0;
  }

  if (ChildImageHandle == NULL) {
    if (ErrorInStep != NULL) {
      *ErrorInStep = 1;
    }

    goto bailout;
  }

  // set load options
  if (LoadOptions != NULL) {
    Status = gBS->HandleProtocol (
                    ChildImageHandle,
                    &gEfiLoadedImageProtocolGuid,
                    (VOID **)&ChildLoadedImage
                  );

    if (CheckError (Status, L"while getting a LoadedImageProtocol handle")) {
      if (ErrorInStep != NULL) {
        *ErrorInStep = 2;
      }

      goto bailout_unload;
    }

    if (LoadOptionsPrefix != NULL) {
      FullLoadOptions = PoolPrint (L"%s %s ", LoadOptionsPrefix, LoadOptions);
      // NOTE: That last space is also added by the EFI shell and seems to be significant
      //  when passing options to Apple's boot.efi...
      LoadOptions = FullLoadOptions;
    }

    // NOTE: We also include the terminating null in the length for safety.
    ChildLoadedImage->LoadOptions = (VOID *)LoadOptions;
    ChildLoadedImage->LoadOptionsSize = (UINT32)StrSize (LoadOptions);

    //DBG ("Using load options '%s'\n", LoadOptions);
  }

  //DBG ("Image loaded at: %p\n", ChildLoadedImage->ImageBase);

  // close open file handles
  UninitRefitLib ();

  // turn control over to the image
  //
  // Before calling the image, enable the Watchdog Timer for
  // the 5 Minute period - Slice - NO! For slow driver and slow disk we need more
  //
  //gBS->SetWatchdogTimer (5 * 60, 0x0000, 0x00, NULL);

  Status = gBS->StartImage (ChildImageHandle, NULL, NULL);

  //
  // Clear the Watchdog Timer after the image returns
  //
  //gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);

  // control returns here when the child image calls Exit ()
  if (ImageTitle) {
    UnicodeSPrint (ErrorInfo, ARRAY_SIZE (ErrorInfo), L"returned from %s", ImageTitle);
  }

  if (CheckError (Status, ErrorInfo)) {
    if (ErrorInStep != NULL) {
      *ErrorInStep = 3;
    }
  }

  if (!EFI_ERROR (Status)) { //why unload driver?!
    //gBS->CloseEvent (ExitBootServiceEvent);
    goto bailout;
  }

  // re-open file handles
  //ReinitRefitLib();

bailout_unload:

  // unload the image, we don't care if it works or not...
  /* Status = */ gBS->UnloadImage (ChildImageHandle);

  if (FullLoadOptions != NULL) {
    FreePool (FullLoadOptions);
  }

bailout:

  return Status;
}

EFI_STATUS
LoadEFIImage (
  IN  EFI_DEVICE_PATH   *DevicePath,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
) {
  EFI_DEVICE_PATH   *DevicePaths[2];

  // Load the image now
  DevicePaths[0] = DevicePath;
  DevicePaths[1] = NULL;

  return LoadEFIImageList (DevicePaths, ImageTitle, ErrorInStep, NewImageHandle);
}

EFI_STATUS
StartEFIImage (
  IN  EFI_DEVICE_PATH   *DevicePath,
  IN  CHAR16            *LoadOptions,
  IN  CHAR16            *LoadOptionsPrefix,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
) {
  EFI_STATUS    Status;
  EFI_HANDLE    ChildImageHandle = NULL;

  Status = LoadEFIImage (DevicePath, ImageTitle, ErrorInStep, &ChildImageHandle);

  if (!EFI_ERROR (Status)) {
    Status = StartEFILoadedImage (
                ChildImageHandle,
                LoadOptions,
                LoadOptionsPrefix,
                ImageTitle,
                ErrorInStep
              );
  }

  if (NewImageHandle != NULL) {
    *NewImageHandle = ChildImageHandle;
  }

  return Status;
}

VOID
StartLoader (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS          Status;
  EFI_TEXT_STRING     ConOutOutputString = 0;
  EFI_HANDLE          ImageHandle = NULL;
  EFI_LOADED_IMAGE    *LoadedImage;
  TagPtr              Dict = NULL;
  BOOLEAN             UseGraphicsMode = TRUE;

  DbgHeader ("StartLoader");

  if (Entry->Settings) {
    DBG ("Entry->Settings: %s\n", Entry->Settings);
    Status = LoadUserSettings (SelfRootDir, Entry->Settings, &Dict);

    if (!EFI_ERROR (Status)) {
      DBG (" - found custom settings for this entry: %s\n", Entry->Settings);
      gBootChanged = TRUE;

      Status = GetUserSettings (Dict);
      if (EFI_ERROR (Status)) {
        DBG (" - ... but: %r\n", Status);
      } else {
        if ((gSettings.CpuFreqMHz > 100) && (gSettings.CpuFreqMHz < 20000)) {
          gCPUStructure.MaxSpeed = gSettings.CpuFreqMHz;
        }
      }
    } else {
      DBG (" - [!] LoadUserSettings failed: %r\n", Status);
    }
  }

  DBG ("Finally: Bus=%ldkHz CPU=%ldMHz\n",
    DivU64x32 (gCPUStructure.FSBFrequency, kilo),
    gCPUStructure.MaxSpeed
  );

  // Unified
  Entry->Flags = (UINT16)(gSettings.OptionsBits | gSettings.FlagsBits);
  gSettings.OptionsBits = gSettings.FlagsBits = Entry->Flags;

  //DumpKernelAndKextPatches (Entry->KernelAndKextPatches);

  // Load image into memory (will be started later)
  Status = LoadEFIImage (
              Entry->DevicePath,
              Basename (Entry->LoaderPath),
              NULL,
              &ImageHandle
            );

  if (EFI_ERROR (Status)) {
    DBG ("Image is not loaded, status=%r\n", Status);
    return; // no reason to continue if loading image failed
  }

#if BOOT_GRAY
  ClearScreen (&GrayBackgroundPixel);
#else
  ClearScreen (&BlackBackgroundPixel);
#endif

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    CHAR8   *BooterOSVersion = NULL;

    gSettings.BooterPatchesAllowed = OSFLAG_ISSET (Entry->Flags, OSFLAG_ALLOW_BOOTER_PATCHES);
    gSettings.KextPatchesAllowed = OSFLAG_ISSET (Entry->Flags, OSFLAG_ALLOW_KEXT_PATCHES);
    gSettings.KernelPatchesAllowed = OSFLAG_ISSET (Entry->Flags, OSFLAG_ALLOW_KERNEL_PATCHES);

    Status = gBS->HandleProtocol (
                    ImageHandle,
                    &gEfiLoadedImageProtocolGuid,
                    (VOID **)&LoadedImage
                  );

    if (!EFI_ERROR (Status)) {
      BOOT_EFI_HEADER   *BootEfiHeader = ParseBooterHeader (LoadedImage->ImageBase);

      if (BootEfiHeader) {
        // version in boot.efi appears as "Mac OS X 10.?"
        /*
          Start OSName Mac OS X 10.12 End OSName Start OSVendor Apple Inc. End
        */
        BooterOSVersion = SearchString ((CHAR8 *)LoadedImage->ImageBase + BootEfiHeader->TextOffset, BootEfiHeader->TextSize, "Mac OS X ", 9);

        if (BooterOSVersion != NULL) { // string was found
          BooterOSVersion += 9; // advance to version location

          if (
            AsciiStrnCmp (BooterOSVersion, "10.", 3) /* &&
            AsciiStrnCmp (BooterOSVersion, "11.", 3) &&
            AsciiStrnCmp (BooterOSVersion, "12.", 3) */
          ) {
            DBG ("NO BooterOSVersion\n");
            FreePool (BooterOSVersion);
            BooterOSVersion = NULL;
          } else { // known version was found in image
            MsgLog ("Found BooterOSVersion: %a\n", BooterOSVersion);

            if (OSX_LT (BooterOSVersion, DARWIN_OS_VER_STR_MINIMUM)) {
              MsgLog ("Unsupported version\n");
              return;
            }

            PatchBooter (
              Entry,
              LoadedImage,
              BootEfiHeader,
              BooterOSVersion
            );
          }
        }
      }
    }

    MsgLog ("OS Version:");

    // Correct OSVersion if it was not found
    // This should happen only for 10.7-10.9 OSTYPE_OSX_INSTALLER
    // For these cases, take OSVersion from loaded boot.efi image in memory
    if (/* OSTYPE_IS_DARWIN_INSTALLER (Entry->LoaderType) || */ !Entry->OSVersion && BooterOSVersion) {
      UINTN   Len = AsciiStrLen (BooterOSVersion);

      Entry->OSVersion = AllocateCopyPool ((Len + 1), BooterOSVersion);
      Entry->OSVersion[Len] = '\0';

      //if (Entry->OSBuildVersion != NULL) {
      //  FreePool (Entry->OSBuildVersion);
      //  Entry->OSBuildVersion = NULL;
      //}
    }

    if (BooterOSVersion != NULL) {
      FreePool (BooterOSVersion);
    }

    if (Entry->OSBuildVersion != NULL) {
      MsgLog (" %a (%a)\n", Entry->OSVersion, Entry->OSBuildVersion);
    } else {
      MsgLog (" %a\n", Entry->OSVersion);
    }

    if (OSX_GT (Entry->OSVersion, DARWIN_OS_VER_STR_ELCAPITAN)) {
    //if (
    //  (Entry->OSVersionMajor > DARWIN_OS_VER_MAJOR_10) ||
    //  (
    //    (Entry->OSVersionMajor == DARWIN_OS_VER_MAJOR_10) &&
    //    (Entry->OSVersionMinor > DARWIN_OS_VER_MINOR_ELCAPITAN)
    //  )
    //) {
      if (OSFLAG_ISSET (Entry->Flags, OSFLAG_NOSIP)) {
        gSettings.CsrActiveConfig = (UINT32)DEF_NOSIP_CSR_ACTIVE_CONFIG;
        gSettings.BooterConfig = (UINT16)DEF_NOSIP_BOOTER_CONFIG;
      }

      ReadCsrCfg ();
    }

    FilterKextPatches (Entry);

    FilterKernelPatches (Entry);

    // Set boot argument for kernel if no caches, this should force kernel loading
    if (
      OSFLAG_ISSET (Entry->Flags, OSFLAG_NOCACHES) &&
      !BootArgsExists (Entry->LoadOptions, L"Kernel=")
    ) {
      CHAR16  *TempOptions,
              *KernelLocation = OSX_LE (Entry->OSVersion, DARWIN_OS_VER_STR_MAVERICKS)
                                  ? L"\"Kernel=/mach_kernel\""
                                  // used for 10.10, 10.11, and new version.
                                  : L"\"Kernel=/System/Library/Kernels/kernel\"";

      TempOptions = AddLoadOption (Entry->LoadOptions, KernelLocation);
      FreePool (Entry->LoadOptions);
      Entry->LoadOptions = TempOptions;
    }

    // first PatchDarwinACPI and find PCIROOT and RTC
    // but before ACPI patch we need smbios patch
    PatchSmbios ();

    PatchACPI (Entry->LoaderType);

    // If KPDebug is true boot in verbose mode to see the debug messages
    // Also: -x | -s
    if (
      OSFLAG_ISSET (Entry->Flags, OPT_SINGLE_USER) ||
      OSFLAG_ISSET (Entry->Flags, OPT_SAFE) ||
      (
        (Entry->KernelAndKextPatches != NULL) &&
        (
          Entry->KernelAndKextPatches->KPDebug ||
          gSettings.DebugKP ||
          OSFLAG_ISSET (Entry->Flags, OSFLAG_DBGPATCHES)
        )
      )
    ) {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OPT_VERBOSE);
    }

    Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_USEGRAPHICS);
    UseGraphicsMode = OSFLAG_ISUNSET (Entry->Flags, OPT_VERBOSE);
    if (!UseGraphicsMode) {
      Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_USEGRAPHICS);
    }

    //DbgHeader ("RestSetupOSX");

    GetEdidDiscovered ();

    SetDevices (Entry);

    SetFSInjection (Entry);
    //PauseForKey (L"SetFSInjection");

    SetVariablesForOSX ();

    EventsInitialize (Entry);

    FinalizeSmbios ();

    SetupDataForOSX ();

    SetCPUProperties ();

    if (OSFLAG_ISSET (Entry->Flags, OSFLAG_HIBERNATED)) {
      gDoHibernateWake = PrepareHibernation (Entry->Volume);
    }

    if (
      gDriversFlags.AptioFixLoaded &&
      !gDoHibernateWake &&
      !BootArgsExists (Entry->LoadOptions, L"slide=")
    ) {
      // Add slide=0 argument for ML+ if not present
      CHAR16  *TempOptions = AddLoadOption (Entry->LoadOptions, L"slide=0");

      FreePool (Entry->LoadOptions);
      Entry->LoadOptions = TempOptions;
    }

    //DBG ("LoadKexts\n");
    // LoadKexts writes to DataHub, where large writes can prevent hibernate wake
    // (happens when several kexts present in Clover's kexts dir)
    if (!gDoHibernateWake) {
      LoadKexts (Entry);
    }

    if (!Entry->LoadOptions) {
      CHAR16  *TempOptions = AddLoadOption (Entry->LoadOptions, L" ");

      FreePool (Entry->LoadOptions);
      Entry->LoadOptions = TempOptions;
    }

  } else if (OSTYPE_IS_WINDOWS_GLOB (Entry->LoaderType)) {
    //DBG ("Closing events for Windows\n");

    PatchACPI (/* FALSE,*/ Entry->LoaderType);
    //PauseForKey (L"continue");

  } else if (OSTYPE_IS_LINUX_GLOB (Entry->LoaderType)) {
    //DBG ("Closing events for Linux\n");

    //FinalizeSmbios ();
    PatchACPI (/* FALSE,*/ Entry->LoaderType);
    //PauseForKey (L"continue");
  }

  SaveOemDsdt (FALSE, Entry->LoaderType);

  if (gSettings.LastBootedVolume) {
    SetStartupDiskVolume (Entry->Volume, OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType) ? NULL : Entry->LoaderPath);
  } else if (gSettings.DefaultVolume != NULL) {
    // DefaultVolume specified in Config.plist or in Boot Option
    // we'll remove OSX Startup Disk vars which may be present if it is used
    // to reboot into another volume
    RemoveStartupDiskVolume ();
  }

  // re-Unified
  gSettings.OptionsBits = Entry->Flags;
  DecodeOptions (Entry);

  MsgLog ("LoadOptions: %s\n", Entry->LoadOptions);

  ClosingEventAndLog (Entry);

  //DBG ("BeginExternalScreen\n");
  BeginExternalScreen (UseGraphicsMode, L"Booting OS");

  if (UseGraphicsMode && !OSTYPE_IS_WINDOWS_GLOB (Entry->LoaderType)) {
    // save orig OutputString and replace it with null implementation
    ConOutOutputString = gST->ConOut->OutputString;
    gST->ConOut->OutputString = NullConOutOutputString;
  }

  //DBG ("StartEFILoadedImage\n");
  StartEFILoadedImage (
    ImageHandle,
    Entry->LoadOptions,
    Basename (Entry->LoaderPath),
    Basename (Entry->LoaderPath),
    NULL
  );

  if (UseGraphicsMode) {
    // return back orig OutputString
    gST->ConOut->OutputString = ConOutOutputString;
  }

  FinishExternalScreen ();
}
