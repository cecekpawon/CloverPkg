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

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_SCAN_TOOL
#define DEBUG_SCAN_TOOL -1
#endif
#else
#ifdef DEBUG_SCAN_TOOL
#undef DEBUG_SCAN_TOOL
#endif
#define DEBUG_SCAN_TOOL DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_SCAN_TOOL, __VA_ARGS__)

STATIC CHAR16 *ShellPath[] = {
  L"Shell64.efi",
  L"Shell.efi"
};

STATIC CONST INTN ShellPathCount = ARRAY_SIZE (ShellPath);

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
    !FileExists (Volume->RootDir, LoaderPath)
  ) {
    return FALSE;
  }

  // Allocate the entry
  Entry = AllocateZeroPool (sizeof (LOADER_ENTRY));

  if (Entry == NULL) {
    return FALSE;
  }

  if (FullTitle) {
    Entry->me.Title = EfiStrDuplicate (FullTitle);
  } else {
    Entry->me.Title = PoolPrint (L"Start %s", LoaderTitle);
  }

  Entry->me.Tag             = TAG_TOOL;
  Entry->me.Row             = 1;
  Entry->me.ShortcutLetter  = ShortcutLetter;
  Entry->me.Image           = Image;
  Entry->me.ImageHover      = ImageHover;
  Entry->LoaderPath         = EfiStrDuplicate (LoaderPath);
  Entry->DevicePath         = FileDevicePath (Volume->DeviceHandle, Entry->LoaderPath);
  Entry->DevicePathString   = FileDevicePathToStr (Entry->DevicePath);
  Entry->LoadOptions        = NULL;
  Entry->ToolOptions        = Options ? Options : NULL;
  //Entry->Flags              = 0;

  MsgLog ("- %s\n", LoaderPath);
  AddMenuEntry (&MainMenu, (REFIT_MENU_ENTRY *)Entry);

  return TRUE;
}

VOID
GetListOfTools () {
  REFIT_DIR_ITER    DirIter;
  EFI_FILE_INFO     *DirEntry;
  UINTN             i = 0, y = 0;

  DbgHeader ("GetListOfTools");

  DirIterOpen (SelfRootDir, DIR_TOOLS, &DirIter);

  OldChosenConfig = 0;

  while (DirIterNext (&DirIter, 2, L"*.efi", &DirEntry)) {
    if (DirEntry->FileName[0] != L'.') {
      S_FILES   *aTmp = AllocateZeroPool (sizeof (S_FILES));
      CHAR16    *TmpCfg = EfiStrDuplicate (DirEntry->FileName);

      MsgLog ("- [%02d]: %s\n", i++, DirEntry->FileName);

      TmpCfg = ReplaceExtension (DirEntry->FileName, L"");

      aTmp->Index = y;

      aTmp->FileName = PoolPrint (TmpCfg);
      aTmp->Next = aTools;
      aTools = aTmp;

      FreePool (TmpCfg);

      y++;
    }
  }

  DirIterClose (&DirIter);

  if (y) {
    S_FILES   *aTmp = aTools;

    aTools = 0;

    while (aTmp) {
      S_FILES   *next = aTmp->Next;

      aTmp->Next = aTools;
      aTools = aTmp;
      aTmp = next;
    }
  }
}

VOID
ScanTool () {
  UINTN    i;

  //Print (L"Scanning for tools...\n");

  // look for the EFI shell
  if (!(GlobalConfig.DisableFlags & HIDEUI_FLAG_SHELL)) {
    for (i = 0; i < ShellPathCount; ++i) {
      if (AddToolEntry (
            PoolPrint (L"%s\\%s", DIR_TOOLS, ShellPath[i]),
            NULL,
            L"UEFI Shell 64",
            SelfVolume,
            BuiltinIcon (BUILTIN_ICON_TOOL_SHELL),
            GetSmallHover (BUILTIN_ICON_TOOL_SHELL),
            'S',
            NULL
          )
      ) {
        //MsgLog ("- %s\n", ShellPath[i]);
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

  //DBG ("Custom tool start\n");
  DbgHeader ("AddCustomTool");

  // Traverse the custom entries
  for (Custom = gSettings.CustomTool; Custom; ++i, Custom = Custom->Next) {
    if (OSFLAG_ISSET (Custom->Flags, OSFLAG_DISABLED)) {
      DBG ("Custom tool %d skipped because it is disabled.\n", i);
      continue;
    }

    if (!gSettings.ShowHiddenEntries && OSFLAG_ISSET (Custom->Flags, OSFLAG_HIDDEN)) {
      DBG ("Custom tool %d skipped because it is hidden.\n", i);
      continue;
    }

    if (Custom->Volume) {
      DBG ("Custom tool %d matching \"%s\" ...\n", i, Custom->Volume);
    }

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; ++VolumeIndex) {
      Volume = Volumes[VolumeIndex];

      DBG ("   Checking volume \"%s\" (%s) ... ", Volume->VolName, Volume->DevicePathString);

      // Skip Whole Disc Boot
      if (Volume->RootDir == NULL) {
        DBG ("skipped because volume is not readable\n");
        continue;
      }

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

      // Check the tool exists on the volume
      if (!FileExists (Volume->RootDir, Custom->Path)) {
        DBG ("skipped because path does not exist\n");
        continue;
      }

      MsgLog ("- [%02d]: %s\n", Basename (Custom->Path));

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

      if (Image == NULL) {
        Image = BuiltinIcon (BUILTIN_ICON_TOOL_SHELL);
        ImageHover = GetSmallHover (BUILTIN_ICON_TOOL_SHELL);
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

      DBG ("match!\n");
      break; // break scan volumes, continue scan entries
    }
  }
  //DBG ("Custom tool end\n");
}

VOID
StartTool (
  IN LOADER_ENTRY   *Entry
) {
  DBG ("StartTool: %s\n", Entry->LoaderPath);
  //SaveBooterLog (SelfRootDir, PREBOOT_LOG);
  ClearScreen (&BlackBackgroundPixel);
  // assumes "Start <title>" as assigned below
  //BeginExternalScreen (OSFLAG_ISSET (Entry->Flags, OSFLAG_USEGRAPHICS), Entry->me.Title + 6);

  //
  // Entry->Flags never set, title never used
  //

  BeginExternalScreen (TRUE, NULL);

  StartEFIImage (
    Entry->DevicePath,
    Entry->ToolOptions,
    Basename (Entry->LoaderPath),
    Basename (Entry->LoaderPath),
    NULL,
    NULL
  );

  FinishExternalScreen ();
  //ReinitRefitLib ();
}

BOOLEAN
StartToolFromMenu () {
  BOOLEAN   Ret = FALSE;

  if (gToolPath != NULL) {
    LOADER_ENTRY   *Entry = AllocateZeroPool (sizeof (LOADER_ENTRY));

    Entry->LoaderPath = EfiStrDuplicate (gToolPath);
    Entry->DevicePath = FileDevicePath (SelfVolume->DeviceHandle, Entry->LoaderPath);

    FreePool (gToolPath);
    gToolPath = NULL;

    StartTool (Entry);

    FreePool (Entry);

    Ret = TRUE;
  }

  return Ret;
}
