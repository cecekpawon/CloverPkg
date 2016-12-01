/*
 * refit/scan/tool.c
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

#include "Platform.h"

#ifndef DEBUG_ALL
#define DEBUG_SCAN_TOOL 1
#else
#define DEBUG_SCAN_TOOL DEBUG_ALL
#endif

#if DEBUG_SCAN_TOOL == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SCAN_TOOL, __VA_ARGS__)
#endif

STATIC CHAR16 *ShellPath[] = {
  L"Shell64U.efi",
  L"Shell64.efi",
  L"Shell.efi"
};

STATIC CONST INTN ShellPathCount = ARRAY_SIZE(ShellPath);

STATIC
BOOLEAN
AddToolEntry (
  IN CHAR16         *LoaderPath,
  IN CHAR16         *FullTitle,
  IN CHAR16         *LoaderTitle,
  IN REFIT_VOLUME   *Volume,
  IN EG_IMAGE       *Image,
  IN EG_IMAGE       *ImageHover,
  IN CHAR16         ShortcutLetter,
  IN CHAR16         *Options
) {
  LOADER_ENTRY    *Entry;

  // Check the loader exists
  if (
    (LoaderPath == NULL) ||
    (Volume == NULL) ||
    (Volume->RootDir == NULL) ||
    !FileExists(Volume->RootDir, LoaderPath)
  ) {
    return FALSE;
  }

  // Allocate the entry
  Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));

  if (Entry == NULL) {
    return FALSE;
  }

  if (FullTitle) {
    Entry->me.Title = EfiStrDuplicate(FullTitle);
  } else {
    Entry->me.Title = PoolPrint(L"Start %s", LoaderTitle);
  }

  Entry->me.Tag             = TAG_TOOL;
  Entry->me.Row             = 1;
  Entry->me.ShortcutLetter  = ShortcutLetter;
  Entry->me.Image           = Image;
  Entry->me.ImageHover      = ImageHover;
  Entry->LoaderPath         = EfiStrDuplicate(LoaderPath);
  Entry->DevicePath         = FileDevicePath(Volume->DeviceHandle, Entry->LoaderPath);
  Entry->DevicePathString   = FileDevicePathToStr(Entry->DevicePath);
  Entry->LoadOptions        = NULL;
  Entry->ToolOptions        = Options ? Options : NULL;

  DBG("found tool %s\n", LoaderPath);
  AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);

  return TRUE;
}

VOID
ScanTool () {
  INTN    i;

  //Print(L"Scanning for tools...\n");

  // look for the EFI shell
  if (!(GlobalConfig.DisableFlags & HIDEUI_FLAG_SHELL)) {
    for (i = 0; i < ShellPathCount; ++i) {
      if (AddToolEntry (
            PoolPrint(L"%s\\%s", DIR_TOOLS, ShellPath[i]),
            NULL,
            L"UEFI Shell 64",
            SelfVolume,
            BuiltinIcon(BUILTIN_ICON_TOOL_SHELL),
            GetSmallHover(BUILTIN_ICON_TOOL_SHELL),
            'S',
            NULL
          )
      ) {
        break;
      }
    }
  }
}

// Add custom tool entries
VOID
AddCustomTool () {
  UINTN               VolumeIndex, i = 0;
  REFIT_VOLUME        *Volume;
  CUSTOM_TOOL_ENTRY   *Custom;
  EG_IMAGE            *Image, *ImageHover = NULL;
  CHAR16              *ImageHoverPath;

  //DBG("Custom tool start\n");
  DbgHeader("AddCustomTool");

  // Traverse the custom entries
  for (Custom = gSettings.CustomTool; Custom; ++i, Custom = Custom->Next) {
    if (OSFLAG_ISSET(Custom->Flags, OSFLAG_DISABLED)) {
      DBG("Custom tool %d skipped because it is disabled.\n", i);
      continue;
    }

    if (!gSettings.ShowHiddenEntries && OSFLAG_ISSET(Custom->Flags, OSFLAG_HIDDEN)) {
      DBG("Custom tool %d skipped because it is hidden.\n", i);
      continue;
    }

    if (Custom->Volume) {
      DBG("Custom tool %d matching \"%s\" ...\n", i, Custom->Volume);
    }

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; ++VolumeIndex) {
      Volume = Volumes[VolumeIndex];

      DBG("   Checking volume \"%s\" (%s) ... ", Volume->VolName, Volume->DevicePathString);

      // Skip Whole Disc Boot
      if (Volume->RootDir == NULL) {
        DBG("skipped because volume is not readable\n");
        continue;
      }

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

      // Check the tool exists on the volume
      if (!FileExists(Volume->RootDir, Custom->Path)) {
        DBG("skipped because path does not exist\n");
        continue;
      }

      // Change to custom image if needed
      Image = Custom->Image;
      if ((Image == NULL) && Custom->ImagePath) {
        ImageHoverPath = EfiStrDuplicate(Custom->ImagePath);
        ReplaceExtension(ImageHoverPath, L"");
        ImageHoverPath = PoolPrint(L"%s_hover.%s", ImageHoverPath, egFindExtension(Custom->ImagePath));
        Image = egLoadImage(Volume->RootDir, Custom->ImagePath, TRUE);
        if (Image == NULL) {
          Image = egLoadImage(ThemeDir, Custom->ImagePath, TRUE);
          if (Image == NULL) {
            Image = egLoadImage(SelfDir, Custom->ImagePath, TRUE);
            if (Image == NULL) {
              Image = egLoadImage(SelfRootDir, Custom->ImagePath, TRUE);
              if (Image != NULL) {
                ImageHover = egLoadIcon(SelfRootDir, ImageHoverPath, 32);
              }
            } else {
              ImageHover = egLoadIcon(SelfDir, ImageHoverPath, 32);
            }
          } else {
            ImageHover = egLoadIcon(ThemeDir, ImageHoverPath, 32);
          }
        } else {
          ImageHover = egLoadIcon(Volume->RootDir, ImageHoverPath, 32);
        }
        FreePool(ImageHoverPath);
      } else {
        // Image base64 data
      }

      if (Image == NULL) {
        Image = BuiltinIcon(BUILTIN_ICON_TOOL_SHELL);
        ImageHover = GetSmallHover(BUILTIN_ICON_TOOL_SHELL);
      }

      // Create a legacy entry for this volume
      AddToolEntry (
        Custom->Path,
        Custom->FullTitle,
        Custom->Title,
        Volume,
        Image,
        ImageHover,
        Custom->Hotkey,
        Custom->Options
      );

      DBG("match!\n");
      break; // break scan volumes, continue scan entries
    }
  }
  //DBG("Custom tool end\n");
}
