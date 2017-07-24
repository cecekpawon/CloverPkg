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

BOOLEAN                 gGuiIsReady = FALSE,
                        gThemeNeedInit = TRUE,
                        gDoHibernateWake = FALSE;

// Splash -->
CHAR16                  **LoadMessages;
UINTN                   MessageNow = 0, MessageClearWidth = 0;
// Splash <--

VOID
DrawLoadMessage (
  CHAR16  *Msg
) {
  UINTN   i, Size, RowHeight = 20;

  //if (gSettings.NoEarlyProgress) {
  //  return;
  //}

  MessageNow++;

  Size = MessageNow * sizeof (CHAR16 *);

  LoadMessages = ReallocatePool (Size, Size + sizeof (CHAR16 *), LoadMessages);
  LoadMessages[MessageNow - 1] = EfiStrDuplicate (Msg);

  for (i = 0; i < MessageNow; i++) {
    DrawTextXY (LoadMessages[i], 0, UGAHeight - ((MessageNow - i) * RowHeight), X_IS_LEFT, MessageClearWidth);
  }

  //gBS->Stall (500000);
}

VOID
InitSplash () {
  //if (!gSettings.NoEarlyProgress) {
    EG_IMAGE  *SplashLogo = BuiltinIcon (BUILTIN_ICON_BANNER_BLACK);

    MessageClearWidth = (UGAWidth - SplashLogo->Width) >> 1;
    DrawImageArea (SplashLogo, 0, 0, 0, 0, MessageClearWidth, (UGAHeight - SplashLogo->Height) >> 1);
  //}
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
  UINTN               MenuExit, Size, i;
  BOOLEAN             MainLoopRunning = TRUE,
                      ReinitDesktop = TRUE,
                      AfterTool = FALSE,
                      HaveDefaultVolume;
  REFIT_MENU_ENTRY    *ChosenEntry = NULL,
                      *DefaultEntry = NULL,
                      *OptionEntry = NULL;
  TagPtr              gConfigDict;
  CHAR8               *NvramConfig = NULL;

  // get TSC freq and init MemLog if needed
  gCPUStructure.TSCCalibr = GetMemLogTscTicksPerSecond (); //ticks for 1second

  // bootstrap
  gST           = SystemTable;
  gImageHandle  = ImageHandle;
  gBS           = SystemTable->BootServices;
  gRT           = SystemTable->RuntimeServices;
  /*Status = */EfiGetSystemConfigurationTable (&gEfiDxeServicesTableGuid, (VOID **)&gDS);

  // To initialize 'SelfRootDir', we should place it here
  Status = InitRefitLib (gImageHandle);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Init assets dir: misc
  /*Status = */MkDir (SelfRootDir, DIR_MISC);
  //Should apply to: "ACPI/origin/" too

  gRT->GetTime (&Now, NULL);

  //DbgHeader ("RefitMain");

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

#if EMBED_FSINJECT
  gDriversFlags.FSInjectEmbedded = TRUE;
#endif

#if EMBED_APTIOFIX
  gDriversFlags.AptioFixEmbedded = TRUE;
#endif

  // disable EFI watchdog timer
  gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);

  Status = InitializeUnicodeCollationProtocol ();
  if (EFI_ERROR (Status)) {
    DBG ("UnicodeCollation Status=%r\n", Status);
  }

  InitializeSettings (); // Init gSettings

  if (FileExists (SelfRootDir, DEV_MARK)) {
    gSettings.Dev = TRUE;
  }

  GetAcpiTablesList ();

  PrePatchSmbios ();

  //replace / with _
  Size = AsciiTrimStrLen (gSettings.OEMProduct, 64);
  for (i = 0; i < Size; i++) {
    if (gSettings.OEMProduct[i] == 0x2F) {
      gSettings.OEMProduct[i] = 0x5F;
    }
  }

  Size = AsciiTrimStrLen (gSettings.OEMBoard, 64);
  for (i = 0; i < Size; i++) {
    if (gSettings.OEMBoard[i] == 0x2F) {
      gSettings.OEMBoard[i] = 0x5F;
    }
  }

  MsgLog ("Running on: '%a' with board '%a'\n", gSettings.OEMProduct, gSettings.OEMBoard);

  GetCPUProperties ();

  SyncCPUProperties ();

  SyncDefaultSettings ();

  DbgHeader ("LoadSettings");

  Status = EFI_LOAD_ERROR;

  NvramConfig = GetNvramVariable (NvramData[kCloverConfig].VariableName, NvramData[kCloverConfig].Guid, NULL, &Size);
  if (NvramConfig != NULL) {
    Size = AsciiStrSize (NvramConfig) * sizeof (CHAR16);
    gSettings.ConfigName = AllocateZeroPool (Size);
    AsciiStrToUnicodeStrS (NvramConfig, gSettings.ConfigName, Size);
    Status = LoadUserSettings (SelfRootDir, gSettings.ConfigName, &gConfigDict);
    MsgLog ("Load (Nvram) Settings: %s.plist: %r\n", gSettings.ConfigName, Status);
  }

  if ((NvramConfig == NULL) || EFI_ERROR (Status) || !gConfigDict) {
    gSettings.ConfigName = EfiStrDuplicate (CONFIG_FILENAME);
    Status = LoadUserSettings (SelfRootDir, gSettings.ConfigName, &gConfigDict);
    MsgLog ("Load (Disk) Settings: %s.plist: %r\n", gSettings.ConfigName, Status);
  }

  if (!EFI_ERROR (Status) && &gConfigDict[0]) {
    Status = GetUserSettings (gConfigDict);
    DBG ("Load Settings: %r\n", Status);
  }

  if (!gSettings.FastBoot) {
    InitScreen (TRUE);
    SetupScreen ();
  } else {
    InitScreen (FALSE);
  }

  InitSplash ();

  DrawLoadMessage (PoolPrint (L"Starting %a", CLOVER_REVISION_STR));

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
    //DebugLog (2, " %r", Status);
    return Status;
  }

  HaveDefaultVolume = (gSettings.DefaultVolume != NULL);

  if (
    !HaveDefaultVolume &&
    (gSettings.Timeout == 0) &&
    !ReadAllKeyStrokes ()
  ) {
    // UEFI boot: get gEfiBootDeviceGuid from NVRAM.
    // if present, ScanVolumes () will skip scanning other volumes
    // in the first run.
    // this speeds up loading of default OSX volume.
    GetEfiBootDeviceFromNvram ();
  }

  DrawLoadMessage (L"Scan Assets");

  if (!gSettings.FastBoot) {
    GetListOfConfigs ();
    GetListOfThemes ();
    GetListOfTools ();
  }

  GetListOfAcpi ();

  AfterTool = FALSE;
  gGuiIsReady = TRUE;

  // get boot-args
  SyncBootArgsFromNvram ();

  MainMenu.TimeoutSeconds = (!gSettings.FastBoot && (gSettings.Timeout >= 0)) ? gSettings.Timeout : 0;

  DrawLoadMessage (L"Scan Entries");

  //InitDesktop:

  do {
    FreeList ((VOID ***)&MainMenu.Entries, &MainMenu.EntryCount);

    MainMenu.EntryCount = 0;
    OptionMenu.EntryCount = 0;

    ScanVolumes ();

    if (!gSettings.FastBoot) {
      CHAR16  *TmpArgs;

      if (gThemeNeedInit) {
        InitTheme (TRUE, &Now);
        gThemeNeedInit = FALSE;
      } else if (gThemeChanged) {
        //DBG ("change theme\n");
        InitTheme (FALSE, NULL);
        FreeMenu (&OptionMenu);
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

    DBG ("DefaultIndex=%d and MainMenu.EntryCount=%d\n", DefaultIndex, MainMenu.EntryCount);

    DefaultEntry = ((DefaultIndex >= 0) && (DefaultIndex < MainMenu.EntryCount))
      ? MainMenu.Entries[DefaultIndex]
      : NULL;

    MainLoopRunning = TRUE;

    if (gSettings.FastBoot && DefaultEntry) {
      if (DefaultEntry->Tag == TAG_LOADER) {
        StartLoader ((LOADER_ENTRY *)DefaultEntry);
      }

      MainLoopRunning = FALSE;
      gSettings.FastBoot = FALSE; //Hmm... will never be here
    } else {
      MainAnime = GetAnime (&MainMenu);
    }

    AfterTool = FALSE;
    //gEvent = 0; //clear to cancel loop

    while (MainLoopRunning) {
      if (
        (gSettings.Timeout == 0) &&
        (DefaultEntry != NULL) &&
        !ReadAllKeyStrokes ()
      ) {
        // go straight to DefaultVolume loading
        MenuExit = MENU_EXIT_TIMEOUT;
      } else {
        MainMenu.AnimeRun = MainAnime;
        MenuExit = RunMainMenu (&MainMenu, DefaultIndex, &ChosenEntry);
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

        case TAG_OPTIONS:    // Options like KernelFlags, DSDTname etc.
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

        case TAG_ABOUT:    // About rEFIt
          AboutRefit ();
          break;

        case TAG_HELP:
          HelpRefit ();
          break;

        case TAG_LOADER:   // Boot OS via .EFI loader
          StartLoader ((LOADER_ENTRY *)ChosenEntry);
          break;

        case TAG_TOOL:     // Start a EFI tool
          StartTool ((LOADER_ENTRY *)ChosenEntry);
          MainLoopRunning = FALSE;
          AfterTool = TRUE;
          break;
      }

      if (StartToolFromMenu ()) {
        MainLoopRunning = FALSE;
        AfterTool = TRUE;
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
