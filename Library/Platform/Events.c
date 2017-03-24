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
#if 0
EFI_EVENT   mVirtualAddressChangeEvent = NULL;
EFI_EVENT   OnReadyToBootEvent = NULL;
EFI_EVENT   mSimpleFileSystemChangeEvent = NULL;
#endif

VOID
EFIAPI
ClosingEventAndLog (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS    Status;
  BOOLEAN       CloseBootServiceEvent = TRUE;

  //MsgLog ("Closing Event & Log\n");

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    if (DoHibernateWake) {
      // When doing hibernate wake, save to DataHub only up to initial size of log
      SavePreBootLog = FALSE;
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
      SetupBooterLog (/*!DoHibernateWake*/);
    }
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

VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
) {
  LOADER_ENTRY   *Entry = (LOADER_ENTRY *)Context;

  //DBG ("ExitBootServices called\n");

  /*
  if (DoHibernateWake) {
    gST->ConOut->OutputString (gST->ConOut, L"wake!!!");
    gBS->Stall (5000000);     // 5 seconds delay
    return;
  }
  */

  gST->ConOut->OutputString (gST->ConOut, L"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

  if (OSTYPE_IS_DARWIN_GLOB (Entry->LoaderType)) {
    if (OSFLAG_ISUNSET (Entry->Flags, OPT_VERBOSE)) {

  #if BOOT_GRAY
      hehe (); // Draw dark gray Apple logo.
  #endif
    }

    //
    // Patch kernel and kexts if needed
    //

    KernelAndKextsPatcherStart (Entry);

    if (OSTYPE_IS_DARWIN (Entry->LoaderType)) {
      SaveDarwinLog ();
    }
  }
}

#if 0
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT    Event,
  IN VOID         *Context
) {
//
}

VOID
EFIAPI
VirtualAddressChangeEvent (
  IN EFI_EVENT    Event,
  IN VOID         *Context
) {
//  EfiConvertPointer (0x0, (VOID **)&mProperty);
//  EfiConvertPointer (0x0, (VOID **)&mSmmCommunication);
}

VOID
EFIAPI
OnSimpleFileSystem (
  IN EFI_EVENT    Event,
  IN VOID         *Context
) {
  EFI_TPL   OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  gEvent = 1;
  //ReinitRefitLib ();
  //ScanVolumes ();
  //enter GUI
  // DrawMenuText (L"OnSimpleFileSystem", 0, 0, UGAHeight-40, 1);
  // MsgLog ("OnSimpleFileSystem occured\n");

  gBS->RestoreTPL (OldTpl);
}

EFI_STATUS
GuiEventsInitialize () {
  EFI_STATUS      Status;
  EFI_EVENT       Event;
  VOID            *RegSimpleFileSystem = NULL;

  gEvent = 0;
  Status = gBS->CreateEvent (
                 EVT_NOTIFY_SIGNAL,
                 TPL_NOTIFY,
                 OnSimpleFileSystem,
                 NULL,
                 &Event
                );

  if (!EFI_ERROR (Status)) {
    Status = gBS->RegisterProtocolNotify (
                    &gEfiSimpleFileSystemProtocolGuid,
                    Event,
                    &RegSimpleFileSystem
                  );
  }

  return Status;
}
#endif

EFI_STATUS
EventsInitialize (
  IN LOADER_ENTRY   *Entry
) {
  EFI_STATUS    Status;
  VOID          *Registration = NULL;

  //
  // Register the event to reclaim variable for OS usage.
  //
  //EfiCreateEventReadyToBoot (&OnReadyToBootEvent);
  /*
  EfiCreateEventReadyToBootEx (
   TPL_NOTIFY,
   OnReadyToBoot,
   NULL,
   &OnReadyToBootEvent
   );
  */

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

  //
  // Register the event to convert the pointer for runtime.
  //
  /*
  gBS->CreateEventEx (
        EVT_NOTIFY_SIGNAL,
        TPL_NOTIFY,
        VirtualAddressChangeEvent,
        NULL,
        &gEfiEventVirtualAddressChangeGuid,
        &mVirtualAddressChangeEvent
      );
  */

  return EFI_SUCCESS;
}
