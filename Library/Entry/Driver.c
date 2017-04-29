#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_SCAN_DRIVER
#define DEBUG_SCAN_DRIVER -1
#endif
#else
#ifdef DEBUG_SCAN_DRIVER
#undef DEBUG_SCAN_DRIVER
#endif
#define DEBUG_SCAN_DRIVER DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_SCAN_DRIVER, __VA_ARGS__)

DRIVERS_FLAGS   gDriversFlags = { FALSE, FALSE, FALSE, FALSE };  //the initializer is not needed for global variables

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
  CHAR16                          Str[AVALUE_MAX_SIZE];
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

    UnicodeSPrint (Str, ARRAY_SIZE (Str), L"%s\\%s", Path, DirEntry->FileName);

    if (
      (
        gDriversFlags.FSInjectEmbedded &&
        (StriStr (DirEntry->FileName, L"FSInject") != NULL)
      ) ||
      (
        (gDriversFlags.AptioFixEmbedded || gDriversFlags.AptioFixLoaded) &&
        (
          (StriStr (DirEntry->FileName, L"AptioFix") != NULL) ||
          (StriStr (DirEntry->FileName, L"LowMemFix") != NULL)
        )
      ) ||
      (
        gDriversFlags.HFSLoaded &&
        (StriStr (DirEntry->FileName, L"HFS") != NULL)
      )
    ) {
      continue;
    }

    Status = StartEFIImage (
                FileDevicePath (SelfLoadedImage->DeviceHandle, Str),
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

    if (StriStr (Str, L"HFS") != NULL) {
      gDriversFlags.HFSLoaded = TRUE;
    }

    if (
      !gDriversFlags.AptioFixEmbedded && !gDriversFlags.AptioFixLoaded &&
      (
        (StriStr (DirEntry->FileName, L"AptioFix") != NULL) ||
        (StriStr (DirEntry->FileName, L"LowMemFix") != NULL)
      )
    ) {
      DBG ("- AptioFix driver loaded\n");
      gDriversFlags.AptioFixLoaded = TRUE;
    }

    if (
      (DriverHandle != NULL) &&
      (DriversToConnectNum != 0) &&
      (DriversToConnect != NULL)
    ) {
      // driver loaded - check for EFI_DRIVER_BINDING_PROTOCOL
      Status = gBS->HandleProtocol (DriverHandle, &gEfiDriverBindingProtocolGuid, (VOID **)&DriverBinding);

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

  if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
    UnicodeSPrint (Str, ARRAY_SIZE (Str), L"while scanning the %s directory", Path);
    CheckError (Status, Str);
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

STATIC
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

VOID
LoadDrivers () {
  EFI_HANDLE          *DriversToConnect = NULL;
  UINTN               DriversToConnectNum = 0, NumLoad = 0;

  EFI_STATUS          Status;
  APTIOFIX_PROTOCOL   *AptioFix;

  DbgHeader ("LoadDrivers");

  NumLoad = ScanDriverDir (DIR_DRIVERS, &DriversToConnect, &DriversToConnectNum);

  if (NumLoad) {
    if (DriversToConnectNum > 0) {
      DBG ("%d drivers needs connecting ...\n", DriversToConnectNum);

      // note: our platform driver protocol
      // will use DriversToConnect - do not release it
      RegisterDriversToHighestPriority (DriversToConnect);

      DisconnectSomeDevices ();

      BdsLibConnectAllDriversToAllControllers ();
    }

    if (!gDriversFlags.AptioFixLoaded) {
      Status = EfiLibLocateProtocol (&gAptioFixProtocolGuid, (VOID **)&AptioFix);
      if (!EFI_ERROR (Status) && (AptioFix->Signature == APTIOFIX_SIGNATURE)) {
        DBG ("- AptioFix driver loaded\n");
        gDriversFlags.AptioFixLoaded = TRUE;
      }
    }
  }
}
