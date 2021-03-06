/*
Copyright (c) 2010 - 2011, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

*/

#include <Library/Common/DeviceTreeLib.h>
#include <Library/Platform/Platform.h>
#include <Library/Platform/KernelPatcher.h>

EFI_EVENT   ExitBootServiceEvent = NULL;

VOID
EFIAPI
ClosingEventAndLog (
  IN LOADER_ENTRY   *Entry
) {
  //EFI_STATUS    Status;
  BOOLEAN   CloseBootServiceEvent = TRUE;

  //MsgLog ("Closing Event & Log\n");

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    CloseBootServiceEvent = FALSE;

    if (OSTYPE_IS_DARWIN (Entry->LoaderType)) {
      SetupBooterLog ();
    }
  }

  if (CloseBootServiceEvent) {
    gBS->CloseEvent (ExitBootServiceEvent);
  }

  if (gSettings.DebugLog) {
    SaveBooterLog (gSelfRootDir, DEBUG_LOG);
  }
}

STATIC
VOID
VerboseMessage (
  IN CHAR8          *Message,
  IN UINTN          Sec,
  IN LOADER_ENTRY   *Entry
) {
  if (BIT_ISSET (Entry->Flags, OPT_VERBOSE)) {
    AsciiPrint (Message);

    if (Sec > 0) {
      gBS->Stall (Sec * 1000000);
    }
  }
}

VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
) {
  LOADER_ENTRY   *Entry = (LOADER_ENTRY *)Context;

  //DBG ("ExitBootServices called\n");

  VerboseMessage ("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", 0, Entry);

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    if (BIT_ISUNSET (Entry->Flags, OPT_VERBOSE)) {
  #if BOOT_GRAY
      hehe (); // Draw dark gray Apple logo.
  #endif
    }

    //
    // Patch kernel and kexts if needed
    //

    KernelAndKextsPatcherStart (Entry);

    if (!gSettings.FakeSMCLoaded) {
      VerboseMessage ("FakeSMC NOT loaded\n", 5, Entry);
    }

    if (OSTYPE_IS_DARWIN (Entry->LoaderType)) {
      SaveDarwinLog ();
    }

    // Store prev boot-args (if any) after being used by boot.efi
    if (gSettings.BootArgsNVRAM) {
      SetNvramVariable (
        gNvramData[kNvBootArgs].VariableName,
        gNvramData[kNvBootArgs].Guid,
        gNvramData[kNvBootArgs].Attribute,
        AsciiStrLen (gSettings.BootArgsNVRAM),
        gSettings.BootArgsNVRAM
      );
    }
  }
}

EFI_STATUS
EventsInitialize (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS    Status;
  VOID          *Registration = NULL;

  //
  // Register notify for exit boot services
  //
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_CALLBACK,
                  OnExitBootServices,
                  Entry,
                  &ExitBootServiceEvent
                );

  if (!EFI_ERROR (Status)) {
    /*Status = */gBS->RegisterProtocolNotify (
                         &gEfiStatusCodeRuntimeProtocolGuid,
                         ExitBootServiceEvent,
                         &Registration
                       );
  }

  return EFI_SUCCESS;
}
