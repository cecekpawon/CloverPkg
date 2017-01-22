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

DRIVERS_FLAGS           gDriversFlags = {FALSE, FALSE, FALSE, FALSE};  //the initializer is not needed for global variables

STATIC
EFI_STATUS
LoadEFIImageList (
  IN  EFI_DEVICE_PATH   **DevicePaths,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
) {
  EFI_STATUS      Status, ReturnStatus;
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

  // load the image into memory
  ReturnStatus = Status = EFI_NOT_FOUND;  // in case the list is empty

  for (DevicePathIndex = 0; (DevicePaths[DevicePathIndex] != NULL); DevicePathIndex++) {
    ReturnStatus = Status = gBS->LoadImage (
                                    FALSE,
                                    SelfImageHandle,
                                    DevicePaths[DevicePathIndex],
                                    NULL,
                                    0,
                                    &ChildImageHandle
                                  );

    DBG (" ... %r", Status);

    if (ReturnStatus != EFI_NOT_FOUND) {
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

  if (!EFI_ERROR (ReturnStatus)) { //why unload driver?!
    if (NewImageHandle != NULL) {
      *NewImageHandle = ChildImageHandle;
    }

    goto bailout;
  }

  // unload the image, we don't care if it works or not...
  Status = gBS->UnloadImage (ChildImageHandle);

bailout:

  DBG ("\n");

  return ReturnStatus;
}

STATIC
EFI_STATUS
StartEFILoadedImage (
  IN  EFI_HANDLE    ChildImageHandle,
  IN  CHAR16        *LoadOptions,
  IN  CHAR16        *LoadOptionsPrefix,
  IN  CHAR16        *ImageTitle,
  OUT UINTN         *ErrorInStep
) {
  EFI_STATUS          Status, ReturnStatus;
  EFI_LOADED_IMAGE    *ChildLoadedImage;
  CHAR16              ErrorInfo[AVALUE_MAX_SIZE], *FullLoadOptions = NULL;

  //DBG ("Starting %s\n", ImageTitle);

  if (ErrorInStep != NULL) {
    *ErrorInStep = 0;
  }

  ReturnStatus = Status = EFI_NOT_FOUND;  // in case no image handle was specified

  if (ChildImageHandle == NULL) {
    if (ErrorInStep != NULL) {
      *ErrorInStep = 1;
    }

    goto bailout;
  }

  // set load options
  if (LoadOptions != NULL) {
    ReturnStatus = Status = gBS->HandleProtocol (
                                    ChildImageHandle,
                                    &gEfiLoadedImageProtocolGuid,
                                    (VOID **) &ChildLoadedImage
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

  ReturnStatus = Status = gBS->StartImage (ChildImageHandle, NULL, NULL);

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

  if (!EFI_ERROR (ReturnStatus)) { //why unload driver?!
    //gBS->CloseEvent (ExitBootServiceEvent);
    goto bailout;
  }

bailout_unload:

  // unload the image, we don't care if it works or not...
  Status = gBS->UnloadImage (ChildImageHandle);

  if (FullLoadOptions != NULL) {
    FreePool (FullLoadOptions);
  }

bailout:

  return ReturnStatus;
}

STATIC
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

STATIC
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
FilterKextPatches (
  IN LOADER_ENTRY   *Entry
) {
  if (
    gSettings.KextPatchesAllowed &&
    (Entry->KernelAndKextPatches->KextPatches != NULL) &&
    Entry->KernelAndKextPatches->NrKexts
  ) {
    INTN    i = 0;

    MsgLog ("Filtering KextPatches:\n");

    for (; i < Entry->KernelAndKextPatches->NrKexts; ++i) {
      BOOLEAN   NeedBuildVersion = (
                  (Entry->BuildVersion != NULL) &&
                  (Entry->KernelAndKextPatches->KextPatches[i].MatchBuild != NULL)
                );

      // If dot exist in the patch name, store string after last dot to Filename for FSInject to load kext
      if (CountOccurrences (Entry->KernelAndKextPatches->KextPatches[i].Name, '.') >= 2) {
        CHAR16  *Str = AllocateZeroPool (SVALUE_MAX_SIZE);
        UINTN   Len;

        Str = FindExtension (PoolPrint (L"%a", Entry->KernelAndKextPatches->KextPatches[i].Name));
        Len = StrLen (Str) + 1;
        Entry->KernelAndKextPatches->KextPatches[i].Filename = AllocateZeroPool (Len);

        UnicodeStrToAsciiStrS (
          Str,
          Entry->KernelAndKextPatches->KextPatches[i].Filename,
          Len
        );

        FreePool (Str);
      }

      MsgLog (" - [%02d]: %a | %a | [OS: %a %s| MatchOS: %a | MatchBuild: %a]",
        i,
        Entry->KernelAndKextPatches->KextPatches[i].Label,
        Entry->KernelAndKextPatches->KextPatches[i].IsPlistPatch ? "PlistPatch" : "BinPatch",
        Entry->OSVersion,
        NeedBuildVersion ? PoolPrint (L"(%a) ", Entry->BuildVersion) : L"",
        Entry->KernelAndKextPatches->KextPatches[i].MatchOS
          ? Entry->KernelAndKextPatches->KextPatches[i].MatchOS
          : "All",
        NeedBuildVersion
          ? Entry->KernelAndKextPatches->KextPatches[i].MatchBuild
          : "All"
      );

      if (NeedBuildVersion) {
        Entry->KernelAndKextPatches->KextPatches[i].Disabled = !IsPatchEnabled (
          Entry->KernelAndKextPatches->KextPatches[i].MatchBuild, Entry->BuildVersion);

        MsgLog (" ==> %a\n", Entry->KernelAndKextPatches->KextPatches[i].Disabled ? "not allowed" : "allowed");

        //if (!Entry->KernelAndKextPatches->KextPatches[i].Disabled) {
          continue; // If user give MatchOS, should we ignore MatchOS / keep reading 'em?
        //}
      }

      Entry->KernelAndKextPatches->KextPatches[i].Disabled = !IsPatchEnabled (
        Entry->KernelAndKextPatches->KextPatches[i].MatchOS, Entry->OSVersion);

      MsgLog (" ==> %a\n", Entry->KernelAndKextPatches->KextPatches[i].Disabled ? "not allowed" : "allowed");
    }
  }
}

VOID
FilterKernelPatches (
  IN LOADER_ENTRY   *Entry
) {
  if (
    gSettings.KernelPatchesAllowed &&
    (Entry->KernelAndKextPatches->KernelPatches != NULL) &&
    Entry->KernelAndKextPatches->NrKernels
  ) {
    INTN    i = 0;

    MsgLog ("Filtering KernelPatches:\n");

    for (; i < Entry->KernelAndKextPatches->NrKernels; ++i) {
      BOOLEAN   NeedBuildVersion = (
                  (Entry->BuildVersion != NULL) &&
                  (Entry->KernelAndKextPatches->KernelPatches[i].MatchBuild != NULL)
                );

      MsgLog (" - [%02d]: %a | [OS: %a %s| MatchOS: %a | MatchBuild: %a]",
        i,
        Entry->KernelAndKextPatches->KernelPatches[i].Label,
        Entry->OSVersion,
        NeedBuildVersion ? PoolPrint (L"(%a) ", Entry->BuildVersion) : L"",
        Entry->KernelAndKextPatches->KernelPatches[i].MatchOS
          ? Entry->KernelAndKextPatches->KernelPatches[i].MatchOS
          : "All",
        NeedBuildVersion
          ? Entry->KernelAndKextPatches->KernelPatches[i].MatchBuild
          : "All"
      );

      if (NeedBuildVersion) {
        Entry->KernelAndKextPatches->KernelPatches[i].Disabled = !IsPatchEnabled (
          Entry->KernelAndKextPatches->KernelPatches[i].MatchBuild, Entry->BuildVersion);

        MsgLog (" ==> %a\n", Entry->KernelAndKextPatches->KernelPatches[i].Disabled ? "not allowed" : "allowed");

        //if (!Entry->KernelAndKextPatches->KernelPatches[i].Disabled) {
          continue; // If user give MatchOS, should we ignore MatchOS / keep reading 'em?
        //}
      }

      Entry->KernelAndKextPatches->KernelPatches[i].Disabled = !IsPatchEnabled (
        Entry->KernelAndKextPatches->KernelPatches[i].MatchOS, Entry->OSVersion);

      MsgLog (" ==> %a\n", Entry->KernelAndKextPatches->KernelPatches[i].Disabled ? "not allowed" : "allowed");
    }
  }
}

VOID
ReadSIPCfg () {
  UINT32    csrCfg = gSettings.CsrActiveConfig & CSR_VALID_FLAGS;
  CHAR16    *csrLog = AllocateZeroPool (SVALUE_MAX_SIZE);

  if (csrCfg & CSR_ALLOW_UNTRUSTED_KEXTS)
    StrCatS (csrLog, SVALUE_MAX_SIZE, L"CSR_ALLOW_UNTRUSTED_KEXTS");
  if (csrCfg & CSR_ALLOW_UNRESTRICTED_FS)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_UNRESTRICTED_FS"));
  if (csrCfg & CSR_ALLOW_TASK_FOR_PID)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_TASK_FOR_PID"));
  if (csrCfg & CSR_ALLOW_KERNEL_DEBUGGER)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_KERNEL_DEBUGGER"));
  if (csrCfg & CSR_ALLOW_APPLE_INTERNAL)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_APPLE_INTERNAL"));
  if (csrCfg & CSR_ALLOW_UNRESTRICTED_DTRACE)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_UNRESTRICTED_DTRACE"));
  if (csrCfg & CSR_ALLOW_UNRESTRICTED_NVRAM)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_UNRESTRICTED_NVRAM"));
  if (csrCfg & CSR_ALLOW_DEVICE_CONFIGURATION)
    StrCatS (csrLog, SVALUE_MAX_SIZE, PoolPrint (L"%a%a", StrLen (csrLog) ? " | " : "", "CSR_ALLOW_DEVICE_CONFIGURATION"));

  if (StrLen (csrLog)) MsgLog ("CSR_CFG: %s\n", csrLog);

  FreePool (csrLog);
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

VOID
EFIAPI
ClosingEventAndLog (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS    Status;
  BOOLEAN       CloseBootServiceEvent = TRUE;

  MsgLog ("Closing Event & Log\n");

  if (OSTYPE_IS_OSX_GLOB (Entry->LoaderType)) {
    if (DoHibernateWake) {
      // When doing hibernate wake, save to DataHub only up to initial size of log
      SavePreBootLog = FALSE;
    } else {
      // delete boot-switch-vars if exists
      /*Status = */gRT->SetVariable (
                  L"boot-switch-vars", &gEfiAppleBootGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  0, NULL
                );

      CloseBootServiceEvent = FALSE;
    }

    SetupBooterLog (!DoHibernateWake);
  }

  if (CloseBootServiceEvent) {
    //gBS->CloseEvent (OnReadyToBootEvent);
    gBS->CloseEvent (ExitBootServiceEvent);
    //gBS->CloseEvent (mSimpleFileSystemChangeEvent);
    //gBS->CloseEvent (mVirtualAddressChangeEvent);
  }

  if (SavePreBootLog) {
    Status = SaveBooterLog (SelfRootDir, PREBOOT_LOG);
    if (EFI_ERROR (Status)) {
      /*Status = */SaveBooterLog (NULL, PREBOOT_LOG);
    }
  }
}

//
// EFI OS loader functions
//

STATIC
VOID
StartLoader (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS          Status;
  EFI_TEXT_STRING     ConOutOutputString = 0;
  EFI_HANDLE          ImageHandle = NULL;
  EFI_LOADED_IMAGE    *LoadedImage;
  CHAR8               *InstallerVersion;
  TagPtr              dict = NULL;

  DbgHeader ("StartLoader");

  if (Entry->Settings) {
    DBG ("Entry->Settings: %s\n", Entry->Settings);
    Status = LoadUserSettings (SelfRootDir, Entry->Settings, &dict);

    if (!EFI_ERROR (Status)) {
      DBG (" - found custom settings for this entry: %s\n", Entry->Settings);
      gBootChanged = TRUE;

      Status = GetUserSettings (SelfRootDir, dict);
      if (EFI_ERROR (Status)) {
        DBG (" - ... but: %r\n", Status);
      } else {
        if ((gSettings.CpuFreqMHz > 100) && (gSettings.CpuFreqMHz < 20000)) {
          gCPUStructure.MaxSpeed = gSettings.CpuFreqMHz;
        }
        //CopyMem (Entry->KernelAndKextPatches,
        //         &gSettings.KernelAndKextPatches,
        //         sizeof (KERNEL_AND_KEXT_PATCHES));
        //DBG ("Custom KernelAndKextPatches copyed to started entry\n");
      }
    } else {
      DBG (" - [!] LoadUserSettings failed: %r\n", Status);
    }
  }

  DBG ("Finally: Bus=%ldkHz CPU=%ldMHz\n",
         DivU64x32 (gCPUStructure.FSBFrequency, kilo),
         gCPUStructure.MaxSpeed);

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

  ClearScreen (&DarkBackgroundPixel);

  if (OSTYPE_IS_OSX_GLOB (Entry->LoaderType)) {
    MsgLog ("GetOSVersion:");

    // Correct OSVersion if it was not found
    // This should happen only for 10.7-10.9 OSTYPE_OSX_INSTALLER
    // For these cases, take OSVersion from loaded boot.efi image in memory
    if (OSTYPE_IS_OSX_INSTALLER (Entry->LoaderType) || !Entry->OSVersion) {
      Status = gBS->HandleProtocol (
                      ImageHandle,
                      &gEfiLoadedImageProtocolGuid,
                      (VOID **) &LoadedImage
                    );

      if (!EFI_ERROR (Status)) {
        // version in boot.efi appears as "Mac OS X 10.?"
        /*
          Start OSName Mac OS X 10.12 End OSName Start OSVendor Apple Inc. End
        */
        InstallerVersion = SearchString (LoadedImage->ImageBase, LoadedImage->ImageSize, "Mac OS X ", 9);

        if (InstallerVersion != NULL) { // string was found
          InstallerVersion += 9; // advance to version location

          if (
            AsciiStrnCmp (InstallerVersion, "10.", 3) /*&&   //
            AsciiStrnCmp (InstallerVersion, "11.", 3) &&     // When Tim Cook migrate to Windoze
            AsciiStrnCmp (InstallerVersion, "12.", 3)*/      //
          ) {
            InstallerVersion = NULL; // flag known version was not found
          }

          if (InstallerVersion != NULL) { // known version was found in image
            UINTN   Len = AsciiStrLen (InstallerVersion);

            if (Entry->OSVersion != NULL) {
              FreePool (Entry->OSVersion);
            }

            Entry->OSVersion = AllocateCopyPool ((Len + 1), InstallerVersion);
            Entry->OSVersion[Len] = '\0';
            //DBG ("Corrected OSVersion: %a\n", Entry->OSVersion);
          }
        }
      }

      if (Entry->BuildVersion != NULL) {
        FreePool (Entry->BuildVersion);
        Entry->BuildVersion = NULL;
      }
    }

    if (Entry->BuildVersion != NULL) {
      MsgLog (" %a (%a)\n", Entry->OSVersion, Entry->BuildVersion);
    } else {
      MsgLog (" %a\n", Entry->OSVersion);
    }

    if (OSX_GT (Entry->OSVersion, "10.11")) {
      if (OSFLAG_ISSET (Entry->Flags, OSFLAG_NOSIP)) {
        gSettings.CsrActiveConfig = (UINT32)0x7F;
        gSettings.BooterConfig = 0x28;
      }

      ReadSIPCfg ();
    }

    FilterKextPatches (Entry);

    FilterKernelPatches (Entry);

    // Set boot argument for kernel if no caches, this should force kernel loading
    if (
      OSFLAG_ISSET (Entry->Flags, OSFLAG_NOCACHES) &&
      !BootArgsExists (Entry->LoadOptions, L"Kernel=")
    ) {
      CHAR16  *TempOptions,
              *KernelLocation = OSX_LE (Entry->OSVersion, "10.9")
                                  ? L"\"Kernel=/mach_kernel\""
                                  // used for 10.10, 10.11, and new version.
                                  : L"\"Kernel=/System/Library/Kernels/kernel\"";

      TempOptions = AddLoadOption (Entry->LoadOptions, KernelLocation);
      FreePool (Entry->LoadOptions);
      Entry->LoadOptions = TempOptions;
    }

    // first patchACPI and find PCIROOT and RTC
    // but before ACPI patch we need smbios patch
    PatchSmbios ();

    PatchACPI (Entry->Volume, Entry->OSVersion);

    // If KPDebug is true boot in verbose mode to see the debug messages
    // Also: -x | -s
    if (
      OSFLAG_ISSET (gSettings.OptionsBits, OPT_SINGLE_USER) ||
      OSFLAG_ISSET (gSettings.OptionsBits, OPT_SAFE) ||
      (
        (Entry->KernelAndKextPatches != NULL) &&
        (
          Entry->KernelAndKextPatches->KPDebug ||
          gSettings.DebugKP ||
          OSFLAG_ISSET (gSettings.FlagsBits, OSFLAG_DBGPATCHES)
        )
      )
    ) {
      gSettings.OptionsBits = OSFLAG_SET (gSettings.OptionsBits, OPT_VERBOSE);
    }

    if (OSFLAG_ISSET (gSettings.OptionsBits, OPT_VERBOSE)) {
      CHAR16  *TempOptions = AddLoadOption (Entry->LoadOptions, L"-v");

      FreePool (Entry->LoadOptions);
      Entry->LoadOptions = TempOptions;
      Entry->Flags = OSFLAG_UNSET (Entry->Flags, OSFLAG_USEGRAPHICS);
    } else {
      Entry->Flags = OSFLAG_SET (Entry->Flags, OSFLAG_USEGRAPHICS);
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
      DoHibernateWake = PrepareHibernation (Entry->Volume);
    }

    if (
      gDriversFlags.AptioFixLoaded &&
      !DoHibernateWake &&
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
    if (!DoHibernateWake) {
      LoadKexts (Entry);
    }

    if (!Entry->LoadOptions) {
      CHAR16  *TempOptions = AddLoadOption (Entry->LoadOptions, L" ");

      FreePool (Entry->LoadOptions);
      Entry->LoadOptions = TempOptions;
    }
  } else if (OSTYPE_IS_WINDOWS_GLOB (Entry->LoaderType)) {
    //DBG ("Closing events for Windows\n");

    PatchACPI_OtherOS (L"Windows", FALSE);
    //PauseForKey (L"continue");
  } else if (OSTYPE_IS_LINUX_GLOB (Entry->LoaderType)) {
    //DBG ("Closing events for Linux\n");

    //FinalizeSmbios ();
    PatchACPI_OtherOS (L"Linux", FALSE);
    //PauseForKey (L"continue");
  }

  if (gSettings.LastBootedVolume) {
    SetStartupDiskVolume (Entry->Volume, OSTYPE_IS_OSX_GLOB (Entry->LoaderType) ? NULL : Entry->LoaderPath);
  } else if (gSettings.DefaultVolume != NULL) {
    // DefaultVolume specified in Config.plist or in Boot Option
    // we'll remove OSX Startup Disk vars which may be present if it is used
    // to reboot into another volume
    RemoveStartupDiskVolume ();
  }

  ClosingEventAndLog (Entry);

  //DBG ("BeginExternalScreen\n");
  BeginExternalScreen (OSFLAG_ISSET (Entry->Flags, OSFLAG_USEGRAPHICS), L"Booting OS");

  if (!OSTYPE_IS_WINDOWS_GLOB (Entry->LoaderType)) {
    if (OSFLAG_ISSET (Entry->Flags, OSFLAG_USEGRAPHICS)) {
      // save orig OutputString and replace it with null implementation
      ConOutOutputString = gST->ConOut->OutputString;
      gST->ConOut->OutputString = NullConOutOutputString;
    }
  }

  //DBG ("StartEFILoadedImage\n");
  StartEFILoadedImage (
    ImageHandle, Entry->LoadOptions,
    Basename (Entry->LoaderPath),
    Basename (Entry->LoaderPath),
    NULL
  );

  if (OSFLAG_ISSET (Entry->Flags, OSFLAG_USEGRAPHICS)) {
    // return back orig OutputString
    gST->ConOut->OutputString = ConOutOutputString;
  }

  FinishExternalScreen ();
}

//
// pre-boot tool functions
//

STATIC
VOID
StartTool (
  IN LOADER_ENTRY   *Entry
) {
  DBG ("StartTool: %s\n", Entry->LoaderPath);
  //SaveBooterLog (SelfRootDir, PREBOOT_LOG);
  ClearScreen (&DarkBackgroundPixel);
  // assumes "Start <title>" as assigned below
  BeginExternalScreen (OSFLAG_ISSET (Entry->Flags, OSFLAG_USEGRAPHICS), Entry->me.Title + 6);

  StartEFIImage (
    Entry->DevicePath,
    Entry->ToolOptions,
    Basename (Entry->LoaderPath),
    Basename (Entry->LoaderPath),
    NULL,
    NULL
  );

  FinishExternalScreen ();
  //ReinitSelfLib ();
}

//
// pre-boot driver functions
//

STATIC
UINTN
ScanDriverDir (
  IN  CHAR16        *Path,
  OUT EFI_HANDLE    **DriversToConnect,
  OUT UINTN         *DriversToConnectNum
) {
  EFI_STATUS                      Status;
  REFIT_DIR_ITER                  DirIter;
  EFI_FILE_INFO                   *DirEntry;
  EFI_HANDLE                      DriverHandle, *DriversArr = NULL;
  EFI_DRIVER_BINDING_PROTOCOL     *DriverBinding;
  CHAR16                          FileName[AVALUE_MAX_SIZE];
  UINTN                           DriversArrSize = 0, DriversArrNum = 0, NumLoad = 0;
  INTN                            i = 0, y;
  BOOLEAN                         Skip;

  // look through contents of the directory
  DirIterOpen (SelfRootDir, Path, &DirIter);

  while (DirIterNext (&DirIter, 2, L"*.EFI", &DirEntry)) {
    Skip = (DirEntry->FileName[0] == L'.');

    MsgLog ("- [%02d]: %s\n", i++, DirEntry->FileName);

    for (y = 0; y < gSettings.BlackListCount; y++) {
      if (StrStr (DirEntry->FileName, gSettings.BlackList[y]) != NULL) {
        Skip = TRUE; // skip this
        MsgLog (" - in blacklist, skip\n");
        break;
      }
    }

    if (Skip) {
      continue;
    }

    UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\%s", Path, DirEntry->FileName);

    if (
      (
        gDriversFlags.FSInjectEmbedded &&
        (StriStr (DirEntry->FileName, L"FSInject") != NULL)
      ) ||
      (
        gDriversFlags.AptioFixEmbedded &&
        (StriStr (DirEntry->FileName, L"AptioFixDrv") != NULL)
      )
    ) {
      continue;
    }

    Status = StartEFIImage (
                FileDevicePath (SelfLoadedImage->DeviceHandle, FileName),
                L"",
                DirEntry->FileName,
                DirEntry->FileName,
                NULL,
                &DriverHandle
              );

    if (EFI_ERROR (Status)) {
      continue;
    }

    NumLoad++;

    // either AptioFix, AptioFix2 or LowMemFix
    //if (StriStr (DirEntry->FileName, L"AptioFixDrv") != NULL) {
    //  gDriversFlags.AptioFixLoaded = TRUE;
    //}

    if (StriStr (FileName, L"HFS") != NULL) {
      gDriversFlags.HFSLoaded = TRUE;
    }

    if (
      (DriverHandle != NULL) &&
      (DriversToConnectNum != 0) &&
      (DriversToConnect != NULL)
    ) {
      // driver loaded - check for EFI_DRIVER_BINDING_PROTOCOL
      Status = gBS->HandleProtocol (DriverHandle, &gEfiDriverBindingProtocolGuid, (VOID **) &DriverBinding);

      if (!EFI_ERROR (Status) && (DriverBinding != NULL)) {
        DBG (" - driver needs connecting\n");

        // standard UEFI driver - we would reconnect after loading - add to array
        if (DriversArrSize == 0) {
          // new array
          DriversArrSize = 16;
          DriversArr = AllocateZeroPool (sizeof (EFI_HANDLE) * DriversArrSize);
        } else if ((DriversArrNum + 1) == DriversArrSize) {
          // extend array
          DriversArr = ReallocatePool (DriversArrSize, DriversArrSize + 16, DriversArr);
          DriversArrSize += 16;
        }

        DriversArr[DriversArrNum++] = DriverHandle;
        //DBG (" driver %s included with Binding=%x\n", FileName, DriverBinding);
        // we'll make array terminated
        DriversArr[DriversArrNum] = NULL;
      }
    }
  }

  Status = DirIterClose (&DirIter);

  if (Status != EFI_NOT_FOUND) {
    UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"while scanning the %s directory", Path);
    CheckError (Status, FileName);
  }

  if ((DriversToConnectNum != 0) && (DriversToConnect != NULL)) {
    *DriversToConnectNum = DriversArrNum;
    *DriversToConnect = DriversArr;
  }

  //release memory for BlackList
  for (i = 0; i < gSettings.BlackListCount; i++) {
    if (gSettings.BlackList[i]) {
      FreePool (gSettings.BlackList[i]);
      gSettings.BlackList[i] = NULL;
    }
  }

  return NumLoad;
}

VOID
DisconnectSomeDevices () {
  EFI_STATUS                        Status;
  EFI_HANDLE                        *Handles, *ControllerHandles;
  EFI_COMPONENT_NAME_PROTOCOL       *CompName;
  CHAR16                            *DriverName;
  UINTN                             HandleCount, Index, Index2, ControllerHandleCount;

  if (gDriversFlags.HFSLoaded) {
    DBG ("- HFS+ driver loaded\n");

    // get all FileSystem handles
    ControllerHandleCount = 0;
    ControllerHandles = NULL;

    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiSimpleFileSystemProtocolGuid,
                    NULL,
                    &ControllerHandleCount,
                    &ControllerHandles
                  );

    HandleCount = 0;
    Handles = NULL;

    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiComponentNameProtocolGuid,
                    NULL,
                    &HandleCount,
                    &Handles
                  );

    if (!EFI_ERROR (Status)) {
      for (Index = 0; Index < HandleCount; Index++) {
        Status = gBS->OpenProtocol (
                         Handles[Index],
                         &gEfiComponentNameProtocolGuid,
                         (VOID **)&CompName,
                         gImageHandle,
                         NULL,
                         EFI_OPEN_PROTOCOL_GET_PROTOCOL
                       );

        if (EFI_ERROR (Status)) {
          //DBG ("CompName %r\n", Status);
          continue;
        }

        Status = CompName->GetDriverName (CompName, "eng", &DriverName);
        if (EFI_ERROR (Status)) {
          continue;
        }

        if (StriStr (DriverName, L"HFS")) {
          for (Index2 = 0; Index2 < ControllerHandleCount; Index2++) {
            Status = gBS->DisconnectController (
                            ControllerHandles[Index2],
                            Handles[Index],
                            NULL
                          );

            //DBG ("Disconnect [%s] from %x: %r\n", DriverName, ControllerHandles[Index2], Status);
          }
        }
      }

      FreePool (Handles);
    }

    FreePool (ControllerHandles);
  }
}

STATIC
VOID
LoadDrivers () {
  EFI_HANDLE          *DriversToConnect = NULL;
  UINTN               DriversToConnectNum = 0, NumLoad = 0;

  EFI_STATUS          Status;
  APTIOFIX_PROTOCOL   *AptioFix;

  DbgHeader ("LoadDrivers");

  NumLoad = ScanDriverDir (DIR_DRIVERS, &DriversToConnect, &DriversToConnectNum);

  if (!NumLoad) {
    NumLoad = ScanDriverDir (DIR_DRIVERS64, &DriversToConnect, &DriversToConnectNum);
  }

  if (DriversToConnectNum > 0) {
    DBG ("%d drivers needs connecting ...\n", DriversToConnectNum);

    // note: our platform driver protocol
    // will use DriversToConnect - do not release it
    RegisterDriversToHighestPriority (DriversToConnect);

    DisconnectSomeDevices ();

    BdsLibConnectAllDriversToAllControllers ();
  }

  if (NumLoad) {
    Status = EfiLibLocateProtocol (&gAptioProtocolGuid, (VOID **)&AptioFix);
    if (!EFI_ERROR (Status) && (AptioFix->Signature == APTIOFIX_SIGNATURE)) {
      DBG ("- AptioFix driver loaded\n");
      gDriversFlags.AptioFixLoaded = TRUE;
    }
  }
}

INTN
FindDefaultEntry () {
  INTN                Index = -1;
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

VOID
SetVariablesFromNvram () {
  CHAR8   *tmpString, *arg = NULL, *lBootArgs;
  UINTN   iNVRAM = 0, iBootArgs = 0, index = 0, index2, len, i;

  DbgHeader ("SetVariablesFromNvram");

  iBootArgs = AsciiStrLen (gSettings.BootArgs);

  if (iBootArgs >= AVALUE_MAX_SIZE) {
    return;
  }

  lBootArgs = AsciiStrToLower (gSettings.BootArgs);

  tmpString = GetNvramVariable (L"boot-args", &gEfiAppleBootGuid, NULL, &iNVRAM);
  iNVRAM = AsciiStrLen (tmpString);

  if (!tmpString || !iNVRAM) {
    return;
  }

  DBG ("Setting BootArgs: %a\n", gSettings.BootArgs);
  DBG ("Found boot-args in NVRAM: %a, size=%d\n", tmpString, iNVRAM);

  CONSTRAIN_MAX (iNVRAM, AVALUE_MAX_SIZE - 1 - iBootArgs);

  arg = AllocatePool (iNVRAM);

  while ((index < iNVRAM) && (tmpString[index] != 0x0)) {
    ZeroMem (arg, iNVRAM + 1);
    index2 = 0;

    if (tmpString[index] != '\"') {
      //DBG ("search space index=%d\n", index);
      while ((index < iNVRAM) && (tmpString[index] != 0x20) && (tmpString[index] != 0x0)) {
        arg[index2++] = tmpString[index++];
      }

      DBG ("...found arg:%a\n", arg);
    } else {
      index++;
      //DBG ("search quote index=%d\n", index);
      while ((index < iNVRAM) && (tmpString[index] != '\"') && (tmpString[index] != 0x0)) {
        arg[index2++] = tmpString[index++];
      }

      if (tmpString[index] == '\"') {
        index++;
      }

      DBG ("...found quoted arg:\n", arg);
    }

    while (tmpString[index] == 0x20) {
      index++;
    }

    // For the moment only arg -s must be ignored
    //if (AsciiStrCmp (arg, "-s") == 0) {
    //  DBG ("...ignoring arg:%a\n", arg);
    //  continue;
    //}

    if (AsciiStrStr (lBootArgs, AsciiStrToLower (arg)) == NULL) {
      len = AsciiStrLen (gSettings.BootArgs);
      CONSTRAIN_MAX (len, AVALUE_MAX_SIZE - 1);

      if ((len + index2) >= AVALUE_MAX_SIZE) {
        DBG ("boot-args overflow... bytes=%d+%d\n", len, index2);
        break;
      }

      gSettings.BootArgs[len++] = 0x20;

      for (i = 0; i < index2; i++) {
        gSettings.BootArgs[len++] = arg[i];
      }

      DBG (" - added\n");
    } else {
      DBG (" - skip\n");
    }
  }

  DBG ("Final BootArgs: %a\n", gSettings.BootArgs);

  FreePool (tmpString);
  FreePool (arg);
  FreePool (lBootArgs);
}

VOID
SetOEMPath (
  CHAR16  *ConfName
) {
  if (ConfName == NULL) {
    OEMPath = PoolPrint (DIR_CLOVER);
  } else if (FileExists (SelfRootDir, PoolPrint (L"%s\\%a\\%s.plist", DIR_OEM, gSettings.OEMProduct, ConfName))) {
    OEMPath = PoolPrint (L"%s\\%a", DIR_OEM, gSettings.OEMProduct);
  } else if (FileExists (SelfRootDir, PoolPrint (L"%s\\%a\\%s.plist", DIR_OEM, gSettings.OEMBoard, ConfName))) {
    OEMPath = PoolPrint (L"%s\\%a", DIR_OEM, gSettings.OEMBoard);
  } else {
    OEMPath = PoolPrint (DIR_CLOVER);
  }
}

VOID
FlashMessage (
  IN CHAR16   *Text,
  IN INTN     XPos,
  IN INTN     YPos,
  IN UINT8    XAlign
) {
  if (!GlobalConfig.NoEarlyProgress && !GlobalConfig.FastBoot  && (GlobalConfig.Timeout > 0)) {
    CHAR16  *Message = PoolPrint (Text);

    DrawTextXY (Message, XPos, YPos, XAlign);
    FreePool (Message);
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
  /*Status = */EfiGetSystemConfigurationTable (&gEfiDxeServicesTableGuid, (VOID **) &gDS);

  // To initialize 'SelfRootDir', we should place it here
  Status = InitRefitLib (gImageHandle);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Init assets dir: misc
  /*Status = */MkDir (SelfRootDir, DIR_MISC);
  //Should apply to: "ACPI/origin/" too

  gRS->GetTime (&Now, NULL);

  InitBooterLog ();

  ZeroMem ((VOID *)&gGraphics[0], sizeof (GFX_PROPERTIES) * 4);

  DbgHeader ("RefitMain");
  //hehe ();

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
  Size = iStrLen (gSettings.OEMProduct, 64);
  for (i = 0; i < Size; i++) {
    if (gSettings.OEMProduct[i] == 0x2F) {
      gSettings.OEMProduct[i] = 0x5F;
    }
  }

  Size = iStrLen (gSettings.OEMBoard, 64);
  for (i = 0; i < Size; i++) {
    if (gSettings.OEMBoard[i] == 0x2F) {
      gSettings.OEMBoard[i] = 0x5F;
    }
  }

  MsgLog ("Running on: '%a' with board '%a'\n", gSettings.OEMProduct, gSettings.OEMBoard);

  GetCPUProperties ();
  GetDevices ();

  DbgHeader ("LoadSettings");
  GetDefaultSettings ();

  gSettings.ConfigName = /*gSettings.MainConfigName =*/ PoolPrint (CONFIG_FILENAME);
  SetOEMPath (gSettings.ConfigName);

  Status = LoadUserSettings (SelfRootDir, gSettings.ConfigName, &gConfigDict);
  MsgLog ("Load Settings: %s.plist: %r\n", CONFIG_FILENAME, Status);

  if (!GlobalConfig.FastBoot) {
    GetListOfConfigs ();
    GetListOfThemes ();
  }

  if (!EFI_ERROR (Status) && &gConfigDict[0]) {
    Status = GetEarlyUserSettings (SelfRootDir, gConfigDict);
    DBG ("Load Settings: Early: %r\n", Status);
  }

  MainMenu.TimeoutSeconds = (!GlobalConfig.FastBoot && (GlobalConfig.Timeout >= 0)) ? GlobalConfig.Timeout : 0;

  LoadDrivers ();

  //GetSmcKeys (); // later we can get here SMC information

  DbgHeader ("InitScreen");

  if (!GlobalConfig.FastBoot) {
    InitScreen (TRUE);
    SetupScreen ();
  } else {
    InitScreen (FALSE);
  }

  // Now we have to reinit handles
  Status = ReinitSelfLib ();

  if (EFI_ERROR (Status)){
    //DebugLog (2, " %r", Status);
    return Status;
  }

  FlashMessage (PoolPrint (L"   Welcome to %a   ", CLOVER_REVISION_STR), (UGAWidth >> 1), (UGAHeight >> 1), X_IS_CENTER);
  FlashMessage (L"... testing hardware ...", (UGAWidth >> 1), (UGAHeight >> 1) + 20, X_IS_CENTER);

  //DumpBiosMemoryMap ();

  //GuiEventsInitialize ();

  if (!gSettings.EnabledCores) {
    gSettings.EnabledCores = gCPUStructure.Cores;
  }

  GetMacAddress ();

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

  FlashMessage (L"... user settings ...", (UGAWidth >> 1), (UGAHeight >> 1) + 20, X_IS_CENTER);

  if (gConfigDict) {
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

  FlashMessage (L"... scan entries ...", (UGAWidth >> 1), (UGAHeight >> 1) + 20, X_IS_CENTER);

  GetListOfACPI ();

  AfterTool = FALSE;
  gGuiIsReady = TRUE;

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

    // font already changed and this message very quirky, clear line here
    //FlashMessage (L"                          ", (UGAWidth >> 1), (UGAHeight >> 1) + 20, X_IS_CENTER);

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
        //continue;
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
      DBG ("ReinitSelfLib after theme change\n");
      ReinitSelfLib ();
    }
  } while (ReinitDesktop);

  return EFI_SUCCESS;
}
