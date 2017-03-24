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
                        DoHibernateWake = FALSE;
EFI_HANDLE              gImageHandle;
EFI_SYSTEM_TABLE        *gST;
EFI_BOOT_SERVICES       *gBS;
EFI_RUNTIME_SERVICES    *gRS;
EFI_DXE_SERVICES        *gDS;

// Splash -->
CHAR16                  **LoadMessages;
UINTN                   MessageNow = 0, MessageClearWidth = 0;
// Splash <--

VOID
DrawLoadMessage (
  CHAR16  *Msg
) {
  UINTN   i, Size, FontHeight = 20;

  MessageNow++;

  Size = MessageNow * sizeof (CHAR16 *);

  LoadMessages = ReallocatePool (Size, Size + sizeof (CHAR16 *), LoadMessages);
  LoadMessages[MessageNow - 1] = EfiStrDuplicate (Msg);

  for (i = 0; i < MessageNow; i++) {
    DrawTextXY (LoadMessages[i], 0, UGAHeight - ((MessageNow - i) * FontHeight), X_IS_LEFT, MessageClearWidth);
  }

  //gBS->Stall (500000);
}

VOID
InitSplash () {
  EG_IMAGE  *Banner = BuiltinIcon (BUILTIN_ICON_BANNER_BLACK);

  MessageClearWidth = (UGAWidth - Banner->Width) >> 1;
  DrawImageArea (Banner, 0, 0, 0, 0, MessageClearWidth, (UGAHeight - Banner->Height) >> 1);
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

  // get TSC freq and init MemLog if needed
  gCPUStructure.TSCCalibr = GetMemLogTscTicksPerSecond (); //ticks for 1second

  // bootstrap
  gST           = SystemTable;
  gImageHandle  = ImageHandle;
  gBS           = SystemTable->BootServices;
  gRS           = SystemTable->RuntimeServices;
  /*Status = */EfiGetSystemConfigurationTable (&gEfiDxeServicesTableGuid, (VOID **)&gDS);

  // To initialize 'SelfRootDir', we should place it here
  Status = InitRefitLib (gImageHandle);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Init assets dir: misc
  /*Status = */MkDir (SelfRootDir, DIR_MISC);
  //Should apply to: "ACPI/origin/" too

  gRS->GetTime (&Now, NULL);

  //InitBooterLog ();

  ZeroMem ((VOID *)&gGraphics[0], sizeof (GFX_PROPERTIES) * 4);

  //DbgHeader ("RefitMain");

  if ((Now.TimeZone < 0) || (Now.TimeZone > 24)) {
    MsgLog ("Now is %d.%d.%d, %d:%d:%d (GMT)\n",
      Now.Day, Now.Month, Now.Year, Now.Hour, Now.Minute, Now.Second);
  } else {
    MsgLog ("Now is %d.%d.%d, %d:%d:%d (GMT+%d)\n",
      Now.Day, Now.Month, Now.Year, Now.Hour, Now.Minute, Now.Second, Now.TimeZone);
  }

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

  ZeroMem ((VOID *)&gSettings, sizeof (SETTINGS_DATA));

  Status = InitializeUnicodeCollationProtocol ();
  if (EFI_ERROR (Status)) {
    DBG ("UnicodeCollation Status=%r\n", Status);
  }

  GetDefaultConfig ();

  PrepatchSmbios ();

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

  GetDevices ();

  DbgHeader ("LoadSettings");
  SyncDefaultSettings ();

  gSettings.ConfigName = /*gSettings.MainConfigName =*/ PoolPrint (CONFIG_FILENAME);
  SetOEMPath (gSettings.ConfigName);

  Status = LoadUserSettings (SelfRootDir, gSettings.ConfigName, &gConfigDict);
  MsgLog ("Load Settings: %s.plist: %r\n", CONFIG_FILENAME, Status);

  if (!GlobalConfig.FastBoot) {
    GetListOfConfigs ();
    GetListOfThemes ();
  }

  if (!EFI_ERROR (Status) && &gConfigDict[0]) {
    Status = GetEarlyUserSettings (gConfigDict);
    DBG ("Load Settings: Early: %r\n", Status);
  }

  MainMenu.TimeoutSeconds = (!GlobalConfig.FastBoot && (GlobalConfig.Timeout >= 0)) ? GlobalConfig.Timeout : 0;

  LoadDrivers ();

  //GetSmcKeys (); // later we can get here SMC information

  //DbgHeader ("InitScreen");

  if (!GlobalConfig.FastBoot) {
    InitScreen (TRUE);
    SetupScreen ();
  } else {
    InitScreen (FALSE);
  }

  InitSplash ();

  DrawLoadMessage (PoolPrint (L"Starting %a", CLOVER_REVISION_STR));

  // Now we have to reinit handles
  Status = ReinitRefitLib ();

  if (EFI_ERROR (Status)){
    //DebugLog (2, " %r", Status);
    return Status;
  }

  DrawLoadMessage (L"Init Hardware");

  //DumpBiosMemoryMap ();

  //GuiEventsInitialize ();

  if (!gSettings.EnabledCores) {
    gSettings.EnabledCores = gCPUStructure.Cores;
  }

  ScanSPD ();

  SetPrivateVarProto ();

  GetAcpiTablesList ();

  MsgLog ("Calibrated TSC frequency = %ld = %ldMHz\n", gCPUStructure.TSCCalibr, DivU64x32 (gCPUStructure.TSCCalibr, Mega));

  if (gCPUStructure.TSCCalibr > 200000000ULL) {  //200MHz
    gCPUStructure.TSCFrequency = gCPUStructure.TSCCalibr;
  }

  gCPUStructure.CPUFrequency  = gCPUStructure.TSCFrequency;
  gCPUStructure.FSBFrequency  = DivU64x32 (
                                  MultU64x32 (gCPUStructure.CPUFrequency, 10),
                                  (gCPUStructure.MaxRatio == 0) ? 1 : gCPUStructure.MaxRatio
                                );
  gCPUStructure.ExternalClock = (UINT32)DivU64x32 (gCPUStructure.FSBFrequency, kilo);
  gCPUStructure.MaxSpeed      = (UINT32)DivU64x32 (gCPUStructure.TSCFrequency + (Mega >> 1), Mega);

  DrawLoadMessage (L"Load Settings");

  if (gConfigDict) {
    if (GetLegacyLanAddress) {
      GetMacAddress ();
    }

    Status = GetUserSettings (SelfRootDir, gConfigDict);
    DBG ("Load Settings: User: %r\n", Status);
  }

  if (gSettings.QEMU) {
    if (!gSettings.UserBusSpeed) {
      gSettings.BusSpeed = 200000;
    }

    gCPUStructure.MaxRatio = (UINT32)DivU64x32 (gCPUStructure.TSCCalibr, gSettings.BusSpeed * kilo);
    DBG ("Set MaxRatio for QEMU: %d\n", gCPUStructure.MaxRatio);
    gCPUStructure.MaxRatio     *= 10;
    gCPUStructure.MinRatio      = 60;
    gCPUStructure.FSBFrequency  = DivU64x32 (
                                    MultU64x32 (gCPUStructure.CPUFrequency, 10),
                                    (gCPUStructure.MaxRatio == 0) ? 1 : gCPUStructure.MaxRatio
                                  );
    gCPUStructure.ExternalClock = (UINT32)DivU64x32 (gCPUStructure.FSBFrequency, kilo);
  }

  HaveDefaultVolume = (gSettings.DefaultVolume != NULL);

  if (
    !HaveDefaultVolume &&
    (GlobalConfig.Timeout == 0) &&
    !ReadAllKeyStrokes ()
  ) {
    // UEFI boot: get gEfiBootDeviceGuid from NVRAM.
    // if present, ScanVolumes () will skip scanning other volumes
    // in the first run.
    // this speeds up loading of default OSX volume.
    GetEfiBootDeviceFromNvram ();
  }

  DrawLoadMessage (L"Scan Entries");

  GetListOfACPI ();

  AfterTool = FALSE;
  gGuiIsReady = TRUE;

  //gBS->Stall (5 * 1000000);

  do {
    MainMenu.EntryCount = 0;
    OptionMenu.EntryCount = 0;
    ScanVolumes ();

    // get boot-args
    SetVariablesFromNvram ();

    if (!GlobalConfig.FastBoot) {
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

      //now it is a time to set RtVariables
      //SetVariablesFromNvram ();

      TmpArgs = PoolPrint (L"%a ", gSettings.BootArgs);
      //DBG ("after NVRAM boot-args=%a\n", gSettings.BootArgs);
      gSettings.OptionsBits = EncodeOptions (TmpArgs);
      //DBG ("initial OptionsBits %x\n", gSettings.OptionsBits);
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

    if (!GlobalConfig.FastBoot) {
      // fixed other menu entries
      if (!(GlobalConfig.DisableFlags & HIDEUI_FLAG_TOOLS)) {
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

    if ((DefaultIndex >= 0) && (DefaultIndex < MainMenu.EntryCount)) {
      DefaultEntry = MainMenu.Entries[DefaultIndex];
    } else {
      DefaultEntry = NULL;
    }

    if (GlobalConfig.FastBoot && DefaultEntry) {
      if (DefaultEntry->Tag == TAG_LOADER) {
        StartLoader ((LOADER_ENTRY *)DefaultEntry);
      }

      MainLoopRunning = FALSE;
      GlobalConfig.FastBoot = FALSE; //Hmm... will never be here
    }

    MainAnime = GetAnime (&MainMenu);
    MainLoopRunning = TRUE;

    AfterTool = FALSE;
    gEvent = 0; //clear to cancel loop

    while (MainLoopRunning) {
      if (
        (GlobalConfig.Timeout == 0) &&
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
      GlobalConfig.Timeout = -1;

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
          gRS->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
          // Warm reboot may not be supported attempt cold reboot
          gRS->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
          // Terminate the screen and just exit
          TerminateScreen ();
          MainLoopRunning = FALSE;
          ReinitDesktop = FALSE;
          AfterTool = TRUE;
          break;

        case TAG_EXIT: // It is not Shut Down, it is Exit from Clover
          TerminateScreen ();
          //gRS->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
          MainLoopRunning = FALSE;   // just in case we get this far
          ReinitDesktop = FALSE;
          AfterTool = TRUE;
          break;

        case TAG_OPTIONS:    // Options like KernelFlags, DSDTname etc.
          gBootChanged = FALSE;
          OptionsMenu (&OptionEntry);

          if (gBootChanged) {
            AfterTool = TRUE;
          }

          // If theme has changed reinit the desktop
          if (gBootChanged || gThemeChanged) {
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
    } //MainLoopRunning

    UninitRefitLib ();

    if (!AfterTool) {
      BdsLibConnectAllEfi ();
    }

    if (ReinitDesktop) {
      DBG ("ReinitRefitLib after theme change\n");
      ReinitRefitLib ();
    }

  } while (ReinitDesktop);

  return EFI_SUCCESS;
}
