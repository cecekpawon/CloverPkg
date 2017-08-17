/*
 * refit/main.c
 * Main code for the boot menu
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
#ifndef DEBUG_MAIN
#define DEBUG_MAIN -1
#endif
#else
#ifdef DEBUG_MAIN
#undef DEBUG_MAIN
#endif
#define DEBUG_MAIN DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_MAIN, __VA_ARGS__)

// variables

BOOLEAN   gThemeNeedInit = TRUE;

// Splash -->
CHAR16    **gLoadMessages, *gMessageDots = L"...";
UINTN     gMessageNow = 0, gMessageClearWidth = 0, gRowHeight = 20;
// Splash <--

VOID
DrawLoadMessage (
  CHAR16  *Msg
) {
  if (!gSettings.NoEarlyProgress) {
    UINTN   i, Size, Index;

    gMessageNow++;

    Size = gMessageNow * sizeof (CHAR16 *);

    gLoadMessages = ReallocatePool (Size, Size + sizeof (CHAR16 *), gLoadMessages);
    gLoadMessages[gMessageNow - 1] = EfiStrDuplicate (Msg);

    for (i = 0; i < gMessageNow; i++) {
      Index = (gMessageNow - i);
      DrawTextXY (gLoadMessages[i], 0, GlobalConfig.UGAHeight - (Index * gRowHeight), X_IS_LEFT, gMessageClearWidth);
    }

    i = 3;
    while (i) {
      DrawTextXY (PoolPrint (L"%s %s", gLoadMessages[gMessageNow - 1], &gMessageDots[--i]), 0, GlobalConfig.UGAHeight - (Index * gRowHeight), X_IS_LEFT, gMessageClearWidth);
      gBS->Stall (30000);
    }
  }
}

VOID
FreeLoadMessage () {
  if (gMessageNow && !gSettings.NoEarlyProgress) {
    UINTN   i;

    for (i = 0; (i < gMessageNow) && gLoadMessages[i]; i++) {
      FreePool (gLoadMessages[i]);
    }
  }
}

VOID
InitSplash () {
  CHAR8   *NvramConfig = NULL;
  UINTN   Size;

  NvramConfig = GetNvramVariable (gNvramData[kNvCloverNoEarlyProgress].VariableName, gNvramData[kNvCloverNoEarlyProgress].Guid, NULL, &Size);
  if ((NvramConfig != NULL) && (AsciiStrCmp (NvramConfig, kXMLTagTrue) == 0)) {
    gSettings.NoEarlyProgress = TRUE;
    FreePool (NvramConfig);
  } else {
    EG_IMAGE  *SplashLogo = BuiltinIcon (BUILTIN_ICON_BANNER_BLACK);

    gMessageClearWidth = (GlobalConfig.UGAWidth - SplashLogo->Width) >> 1;
    DrawImageArea (SplashLogo, 0, 0, 0, 0, gMessageClearWidth, (GlobalConfig.UGAHeight - SplashLogo->Height) >> 1);
  }
}

//
// main entry point
//
EFI_STATUS
EFIAPI
RefitMain (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
) {
  EFI_TIME            Now;
  EFI_STATUS          Status;
  INTN                DefaultIndex;
  UINTN               MenuExit, Size;
  BOOLEAN             MainLoopRunning = TRUE,
                      ReinitDesktop = TRUE,
                      AfterTool = FALSE,
                      HaveDefaultVolume;
  REFIT_MENU_ENTRY    *ChosenEntry = NULL,
                      *DefaultEntry = NULL,
                      *OptionEntry = NULL;
  TagPtr              ConfigDict;
  CHAR8               *NvramConfig = NULL;

  // bootstrap
  gImageHandle  = ImageHandle;
  /*Status = */EfiGetSystemConfigurationTable (&gEfiDxeServicesTableGuid, (VOID **)&gDS);

  // To initialize 'gSelfRootDir', we should place it here
  Status = InitRefitLib (gImageHandle);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Init assets dir: misc
  /*Status = */MkDir (gSelfRootDir, DIR_MISC);
  // Should apply to: "ACPI/origin/" too

  gRT->GetTime (&Now, NULL);

  MsgLog ("Now is %d.%d.%d, %d:%d:%d (GMT+%d)\n",
    Now.Year, Now.Month, Now.Day, Now.Hour, Now.Minute, Now.Second,
    ((Now.TimeZone < 0) || (Now.TimeZone > 24)) ? 0 : Now.TimeZone
  );

  MsgLog ("Starting %a on %a\n", CLOVER_REVISION_STR, CLOVER_BUILDDATE);
  MsgLog (" - %a\n", CLOVER_BASED_INFO);
  MsgLog (" - EDK II (rev %a)\n", EDK2_REVISION);
  MsgLog (" - [%a]\n", CLOVER_BUILDINFOS_STR);
  MsgLog (" - UEFI Revision %d.%02d\n",
    gST->Hdr.Revision >> 16,
    gST->Hdr.Revision & ((1 << 16) - 1)
  );
  MsgLog (" - Firmware: %s rev %d.%d\n",
    gST->FirmwareVendor,
    gST->FirmwareRevision >> 16,
    gST->FirmwareRevision & ((1 << 16) - 1)
  );

  // disable EFI watchdog timer
  gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);

  Status = InitializeUnicodeCollationProtocol ();
  if (EFI_ERROR (Status)) {
    DBG ("UnicodeCollation Status=%r\n", Status);
  }

  InitializeSettings ();

  InitScreen ();

  InitSplash ();

  DrawLoadMessage (PoolPrint (L"Starting %a", CLOVER_REVISION_STR));

  if (FileExists (gSelfRootDir, DEV_MARK)) {
    gSettings.Dev = TRUE;
  }

  DrawLoadMessage (L"Gather ACPI Info");
  GetAcpiTablesList ();

  DrawLoadMessage (L"Gather Smbios Info");
  PrePatchSmbios ();

  MsgLog ("Running on: '%a (%a)' with board '%a'\n", gSettings.OEMVendor, gSettings.OEMProduct, gSettings.OEMBoard);

  DrawLoadMessage (L"Gather CPU Info");
  GetCPUProperties ();

  SyncCPUProperties ();

  SyncDefaultSettings ();

  DrawLoadMessage (L"Load Settings");
  DbgHeader ("LoadSettings");

  Status = EFI_LOAD_ERROR;

  NvramConfig = GetNvramVariable (gNvramData[kNvCloverConfig].VariableName, gNvramData[kNvCloverConfig].Guid, NULL, &Size);
  if (NvramConfig != NULL) {
    Size = AsciiStrSize (NvramConfig) * sizeof (CHAR16);
    gSettings.ConfigName = AllocateZeroPool (Size);
    AsciiStrToUnicodeStrS (NvramConfig, gSettings.ConfigName, Size);
    Status = LoadUserSettings (gSelfRootDir, gSettings.ConfigName, &ConfigDict);
    MsgLog ("Load (Nvram) Settings: %s.plist: %r\n", gSettings.ConfigName, Status);
  }

  if ((NvramConfig == NULL) || EFI_ERROR (Status) || !ConfigDict) {
    gSettings.ConfigName = EfiStrDuplicate (CONFIG_FILENAME);
    Status = LoadUserSettings (gSelfRootDir, gSettings.ConfigName, &ConfigDict);
    MsgLog ("Load (Disk) Settings: %s.plist: %r\n", gSettings.ConfigName, Status);
  }

  if (!EFI_ERROR (Status) && &ConfigDict[0]) {
    Status = GetUserSettings (ConfigDict);
    DBG ("Load Settings: %r\n", Status);
  }

  DrawLoadMessage (L"Init Hardware");
  SetPrivateVarProto ();
  InitializeEdidOverride ();

  DrawLoadMessage (L"Scan SPD");
  ScanSPD ();

  DrawLoadMessage (L"Scan Devices");
  GetDevices ();

  DrawLoadMessage (L"Scan Drivers");
  LoadDrivers ();

  //GetSmcKeys (); // later we can get here SMC information

  // Now we have to reinit handles
  Status = ReinitRefitLib ();

  if (EFI_ERROR (Status)) {
    return Status;
  }

  HaveDefaultVolume = (gSettings.DefaultVolume != NULL);

  if (
    !HaveDefaultVolume &&
    (gSettings.Timeout == 0) &&
    !ReadAllKeyStrokes ()
  ) {
    // UEFI boot: get gEfiBootDeviceGuid from NVRAM.
    // if present, ScanVolumes () will skip scanning other volumes in the first run.
    // this speeds up loading of default OSX volume.
    GetEfiBootDeviceFromNvram ();
  }

  DrawLoadMessage (L"Scan Assets");

  if (!gSettings.FastBoot) {
    ScanConfigs ();
    ScanThemes ();
    ScanTools ();
  }

  ScanAcpi ();

  AfterTool = FALSE;
  GlobalConfig.GUIReady = TRUE;

  // get boot-args
  SyncBootArgsFromNvram ();

  gMainMenu.TimeoutSeconds = (!gSettings.FastBoot && (gSettings.Timeout >= 0)) ? gSettings.Timeout : 0;

  DrawLoadMessage (L"Scan Entries");

  FreeLoadMessage ();

  //InitDesktop:

  do {
    FreeList ((VOID ***)&gMainMenu.Entries, &gMainMenu.EntryCount);

    gMainMenu.EntryCount = 0;
    gOptionMenu.EntryCount = 0;

    ScanVolumes ();

    if (!gSettings.FastBoot) {
      CHAR16  *TmpArgs;

      if (gThemeNeedInit) {
        InitTheme (TRUE, &Now);
        gThemeNeedInit = FALSE;
      } else if (gThemeChanged) {
        InitTheme (FALSE, NULL);
        FreeMenu (&gOptionMenu);
      }

      gThemeChanged = FALSE;
      MsgLog ("Choosing theme: %s\n", GlobalConfig.Theme);

      TmpArgs = PoolPrint (L"%a", gSettings.BootArgs);
      gSettings.OptionsBits = EncodeOptions (TmpArgs);
      FreePool (TmpArgs);

      FillInputs (TRUE);
    }

    // Add custom entries
    AddCustomEntries ();

    if (gSettings.DisableEntryScan) {
      DBG ("Entry scan disabled\n");
    } else {
      ScanLoader ();
    }

    if (!gSettings.FastBoot) {
      // fixed other menu entries
      if (BIT_ISUNSET (GlobalConfig.DisableFlags, HIDEUI_FLAG_TOOLS)) {
        AddCustomTool ();
        if (!gSettings.DisableToolScan) {
          ScanTool ();
        }
      }

      DrawFuncIcons ();
    }

    // wait for user ACK when there were errors
    FinishTextScreen (FALSE);

    DefaultIndex = FindDefaultEntry ();

    DBG ("DefaultIndex=%d and gMainMenu.EntryCount=%d\n", DefaultIndex, gMainMenu.EntryCount);

    DefaultEntry = ((DefaultIndex >= 0) && ((UINTN)DefaultIndex < gMainMenu.EntryCount))
      ? gMainMenu.Entries[DefaultIndex]
      : NULL;

    MainLoopRunning = TRUE;

    if (gSettings.FastBoot && DefaultEntry) {
      if (DefaultEntry->Tag == TAG_LOADER) {
        StartLoader ((LOADER_ENTRY *)DefaultEntry);
      }

      MainLoopRunning = FALSE;
      gSettings.FastBoot = FALSE; //Hmm... will never be here
    } else {
      gMainAnime = GetAnime (&gMainMenu);
    }

    AfterTool = FALSE;

    while (MainLoopRunning) {
      if (
        (gSettings.Timeout == 0) &&
        (DefaultEntry != NULL) &&
        !ReadAllKeyStrokes ()
      ) {
        // go straight to DefaultVolume loading
        MenuExit = MENU_EXIT_TIMEOUT;
      } else {
        gMainMenu.AnimeRun = gMainAnime;
        MenuExit = RunMainMenu (&gMainMenu, DefaultIndex, &ChosenEntry);
      }

      // disable default boot - have sense only in the first run
      gSettings.Timeout = -1;

      if ((DefaultEntry != NULL) && (MenuExit == MENU_EXIT_TIMEOUT)) {
        if (DefaultEntry->Tag == TAG_LOADER) {
          StartLoader ((LOADER_ENTRY *)DefaultEntry);
        }
        break;
      }

      if (MenuExit == MENU_EXIT_OPTIONS) {
        gBootChanged = FALSE;
        OptionsMenu (&OptionEntry);

        if (gBootChanged) {
          AfterTool = TRUE;
          MainLoopRunning = FALSE;
          break;
        }

        continue;
      }

      if (MenuExit == MENU_EXIT_HELP) {
        HelpRefit ();
        continue;
      }

      // Hide toggle
      if (MenuExit == MENU_EXIT_HIDE_TOGGLE) {
        gSettings.ShowHiddenEntries = !gSettings.ShowHiddenEntries;
        AfterTool = TRUE;
        break;
      }

      // We don't allow exiting the main menu with the Escape key.
      if (MenuExit == MENU_EXIT_ESCAPE) {
        break;   //refresh main menu
      }

      //
      if (MenuExit == MENU_EXIT_OTHER) {
        MainLoopRunning = FALSE;
        AfterTool = TRUE;
        break;   //refresh main menu
      }

      switch (ChosenEntry->Tag) {
        case TAG_RESET:    // Restart
          // Attempt warm reboot
          gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
          // Warm reboot may not be supported attempt cold reboot
          gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
          // Terminate the screen and just exit
          TerminateScreen ();
          MainLoopRunning = FALSE;
          ReinitDesktop = FALSE;
          AfterTool = TRUE;
          break;

        case TAG_EXIT: // It is not Shut Down, it is Exit from Clover
          TerminateScreen ();
          //gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
          MainLoopRunning = FALSE;   // just in case we get this far
          ReinitDesktop = FALSE;
          AfterTool = TRUE;
          break;

        case TAG_OPTIONS:
          gBootChanged = FALSE;
          OptionsMenu (&OptionEntry);

          // If theme has changed reinit the desktop
          if (gBootChanged || gThemeChanged) {
            if (gBootChanged) {
              AfterTool = TRUE;
            }

            MainLoopRunning = FALSE;
          }
          break;

        case TAG_ABOUT:
          AboutRefit ();
          break;

        case TAG_HELP:
          HelpRefit ();
          break;

        case TAG_LOADER:
          StartLoader ((LOADER_ENTRY *)ChosenEntry);
          break;

        case TAG_TOOL:
          StartTool ((LOADER_ENTRY *)ChosenEntry);
          MainLoopRunning = FALSE;
          AfterTool = TRUE;
          break;
      }

      if (gToolPath) {
        FreePool (gToolPath);
        gToolPath = NULL;
        MainLoopRunning = FALSE;
        AfterTool = TRUE;
        break;
      }
    } //MainLoopRunning

    UninitRefitLib ();

    if (!AfterTool) {
      BdsLibConnectAllEfi ();
    }

    if (ReinitDesktop) {
      DBG ("ReinitRefitLib after theme change\n");
      ReinitRefitLib ();
      //goto InitDesktop;
    }
  } while (ReinitDesktop);

  return EFI_SUCCESS;
}
