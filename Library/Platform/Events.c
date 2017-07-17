/*
Copyright (c) 2010 - 2011, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

*/

#include <Library/Platform/Platform.h>
#include <Library/Platform/DeviceTree.h>
#include <Library/Platform/KernelPatcher.h>

EFI_EVENT   ExitBootServiceEvent = NULL;

VOID
EFIAPI
ClosingEventAndLog (
  IN LOADER_ENTRY   *Entry
) {
  //EFI_STATUS    Status;
  BOOLEAN       CloseBootServiceEvent = TRUE;

  //MsgLog ("Closing Event & Log\n");

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    if (gDoHibernateWake) {
      // When doing hibernate wake, save to DataHub only up to initial size of log
    } else {
      // delete boot-switch-vars if exists
      /*Status = */gRT->SetVariable (
                          L"boot-switch-vars", &gEfiAppleBootGuid,
                          NVRAM_ATTR_RT_BS_NV,
                          0, NULL
                        );

      CloseBootServiceEvent = FALSE;
    }

    if (OSTYPE_IS_DARWIN (Entry->LoaderType)) {
      SetupBooterLog (/*!gDoHibernateWake*/);
    }
  }

  if (CloseBootServiceEvent) {
    gBS->CloseEvent (ExitBootServiceEvent);
  }

  if (gSettings.DebugLog) {
    SaveBooterLog (SelfRootDir, DEBUG_LOG);
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

  /*
  if (gDoHibernateWake) {
    gST->ConOut->OutputString (gST->ConOut, L"wake!!!");
    gBS->Stall (5 * 1000000);     // 5 seconds delay
    return;
  }
  */

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
