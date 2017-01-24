/** @file
  BDS Lib functions which relate with connect the device

Copyright (c) 2004 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/Platform/Platform.h>

/**
  This function will connect all current system handles recursively.

  gBS->ConnectController () service is invoked for each handle exist in system handler buffer.
  If the handle is bus type handler, all childrens also will be connected recursively
  by gBS->ConnectController ().

  @retval EFI_SUCCESS           All handles and it's child handle have been connected
  @retval EFI_STATUS            Error status returned by of gBS->LocateHandleBuffer ().

**/
EFI_STATUS
EFIAPI
BdsLibConnectAllEfi () {
  EFI_STATUS    Status;
  UINTN         HandleCount, Index;
  EFI_HANDLE    *HandleBuffer;

  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    //Status =
    gBS->ConnectController (HandleBuffer[Index], NULL, NULL, TRUE);
  }

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ScanDeviceHandles (
  EFI_HANDLE      ControllerHandle,
  UINTN           *HandleCount,
  EFI_HANDLE      **HandleBuffer,
  UINT32          **HandleType
) {
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInfo;
  EFI_STATUS                          Status;
  EFI_GUID                            **ProtocolGuidArray;
  UINTN                               HandleIndex, ArrayCount, ProtocolIndex,
                                      OpenInfoCount, OpenInfoIndex, ChildIndex;

  *HandleCount  = 0;
  *HandleBuffer = NULL;
  *HandleType   = NULL;

  //
  // Retrieve the list of all handles from the handle database
  //
  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, HandleCount, HandleBuffer);

  if (EFI_ERROR (Status)) {
    goto Error;
  }

  *HandleType = AllocatePool (*HandleCount * sizeof (UINT32));

  if (*HandleType == NULL) {
    goto Error;
  }

  for (HandleIndex = 0; HandleIndex < *HandleCount; HandleIndex++) {
    (*HandleType)[HandleIndex] = EFI_HANDLE_TYPE_UNKNOWN;
    //
    // Retrieve the list of all the protocols on each handle
    //
    Status = gBS->ProtocolsPerHandle (
                    (*HandleBuffer)[HandleIndex],
                    &ProtocolGuidArray,
                    &ArrayCount
                  );

    if (!EFI_ERROR (Status)) {
      for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiLoadedImageProtocolGuid)) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverBindingProtocolGuid)) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverConfigurationProtocolGuid)) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverDiagnosticsProtocolGuid)) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentName2ProtocolGuid)) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentNameProtocolGuid) ) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDevicePathProtocolGuid)) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DEVICE_HANDLE;
        }

        //
        // Retrieve the list of agents that have opened each protocol
        //
        Status = gBS->OpenProtocolInformation (
                         (*HandleBuffer)[HandleIndex],
                         ProtocolGuidArray[ProtocolIndex],
                         &OpenInfo,
                         &OpenInfoCount
                      );

        if (!EFI_ERROR (Status)) {
          for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
            if (OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle) {
              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER) {
                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                  if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                    (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                  }
                }
              }

              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) {
                (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                  if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                    (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                  }
                }
              }
            }
          }

          FreePool (OpenInfo);
        }
      }

      FreePool (ProtocolGuidArray);
    }
  }

  return EFI_SUCCESS;

Error:
  if (*HandleType != NULL) {
    FreePool (*HandleType);
  }

  if (*HandleBuffer != NULL) {
    FreePool (*HandleBuffer);
  }

  *HandleCount  = 0;
  *HandleBuffer = NULL;
  *HandleType   = NULL;

  return Status;
}

EFI_STATUS
BdsLibConnectMostlyAllEfi () {
  EFI_STATUS            Status;
  UINTN                 AllHandleCount = 0, Index, HandleCount = 0, HandleIndex;
  EFI_HANDLE            *AllHandleBuffer = NULL, *HandleBuffer = NULL;
  UINT32                *HandleType = NULL;
  BOOLEAN               Parent, Device;
  EFI_PCI_IO_PROTOCOL   *PciIo = NULL;
  PCI_TYPE00            Pci;

  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &AllHandleCount, &AllHandleBuffer);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < AllHandleCount; Index++) {
    Status = ScanDeviceHandles (AllHandleBuffer[Index], &HandleCount, &HandleBuffer, &HandleType);

    if (EFI_ERROR (Status)) {
      goto Done;
    }

    Device = TRUE;

    if (HandleType[Index] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE) {
      Device = FALSE;
    }

    if (HandleType[Index] & EFI_HANDLE_TYPE_IMAGE_HANDLE) {
      Device = FALSE;
    }

    if (Device) {
      Parent = FALSE;

      for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        if (HandleType[HandleIndex] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
          Parent = TRUE;
        }
      }

      if (!Parent) {
        if (HandleType[Index] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
          Status = gBS->HandleProtocol (AllHandleBuffer[Index], &gEfiPciIoProtocolGuid, (VOID *)&PciIo);
          if (!EFI_ERROR (Status)) {
            Status = PciIo->Pci.Read (PciIo,EfiPciIoWidthUint32, 0, sizeof (Pci) / sizeof (UINT32), &Pci);
            if (!EFI_ERROR (Status)) {
              if (IS_PCI_VGA (&Pci) == TRUE) {
                gBS->DisconnectController (AllHandleBuffer[Index], NULL, NULL);
              }
            }
          }

          Status = gBS->ConnectController (AllHandleBuffer[Index], NULL, NULL, TRUE);
        }
      }
    }

    FreePool (HandleBuffer);
    FreePool (HandleType);
  }

Done:
  FreePool (AllHandleBuffer);
  return Status;
}

/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the corresponding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.

**/

VOID
EFIAPI
BdsLibConnectAllDriversToAllControllers () {
  EFI_STATUS  Status;

  do {
    //
    // Connect All EFI 1.10 drivers following EFI 1.10 algorithm
    //
    //BdsLibConnectAllEfi ();
    BdsLibConnectMostlyAllEfi ();
    //
    // Check to see if it's possible to dispatch an more DXE drivers.
    // The BdsLibConnectAllEfi () may have made new DXE drivers show up.
    // If anything is Dispatched Status == EFI_SUCCESS and we will try
    // the connect again.
    //
    Status = gDS->Dispatch ();
  } while (!EFI_ERROR (Status));
}
