/*
 *  Copyright 2009 Jasmin Fazlic All rights reserved.
 *
 *
 *  Cleaned and merged by iNDi
 */
// UEFI adaptation by usr-sse2, then slice, dmazar

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_INJECT
#define DEBUG_INJECT -1
#endif
#else
#ifdef DEBUG_INJECT
#undef DEBUG_INJECT
#endif
#define DEBUG_INJECT DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_INJECT, __VA_ARGS__)

DevPropString   *gDevPropString = NULL;
//UINT8           *stringdata    = NULL;
UINT32          gDevPropStringLength   = 0;
UINT32          gDevicesNumber = 1;
UINT32          BuiltinSet    = 0;
UINT32          mPropSize = 0;
UINT8           *mProperties = NULL;
CHAR8           *gDeviceProperties = NULL;

UINT32          cPropSize = 0;
UINT8           *cProperties = NULL;
CHAR8           *cDeviceProperties = NULL;

#define DEVICE_PROPERTIES_SIGNATURE SIGNATURE_64 ('A','P','P','L','E','D','E','V')

typedef struct _APPLE_GETVAR_PROTOCOL APPLE_GETVAR_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *APPLE_GETVAR_PROTOCOL_GET_DEVICE_PROPS) (
  IN     APPLE_GETVAR_PROTOCOL   *This,
  IN     CHAR8                   *Buffer,
  IN OUT UINT32                  *BufferSize
);

struct _APPLE_GETVAR_PROTOCOL {
  UINT64    Sign;
  EFI_STATUS (EFIAPI *Unknown1)(IN VOID *);
  EFI_STATUS (EFIAPI *Unknown2)(IN VOID *);
  EFI_STATUS (EFIAPI *Unknown3)(IN VOID *);
  APPLE_GETVAR_PROTOCOL_GET_DEVICE_PROPS  GetDevProps;
  APPLE_GETVAR_PROTOCOL_GET_DEVICE_PROPS  GetDevProps1;
};

STATIC
EFI_STATUS
EFIAPI
GetDeviceProps (
  IN     APPLE_GETVAR_PROTOCOL   *This,
  IN     CHAR8                   *Buffer,
  IN OUT UINT32                  *BufferSize
);

APPLE_GETVAR_PROTOCOL mDeviceProperties = {
  DEVICE_PROPERTIES_SIGNATURE,
  NULL,
  NULL,
  NULL,
  GetDeviceProps,
  GetDeviceProps,
};

typedef
EFI_STATUS
(EFIAPI *EFI_SCREEN_INFO_FUNCTION) (
  VOID    *This,
  UINT64  *BaseAddress,
  UINT64  *FrameBufferSize,
  UINT32  *ByterPerRow,
  UINT32  *Width,
  UINT32  *Height,
  UINT32  *ColorDepth
);

typedef struct {
  EFI_SCREEN_INFO_FUNCTION    GetScreenInfo;
} EFI_INTERFACE_SCREEN_INFO;

EFI_STATUS
EFIAPI
GetDeviceProps (
  IN     APPLE_GETVAR_PROTOCOL   *This,
  IN     CHAR8                   *Buffer,
  IN OUT UINT32                  *BufferSize
) {
  if (gSettings.EFIStringInjector && (cProperties != NULL) && (cPropSize > 1)) {
    if (*BufferSize < cPropSize) {
      *BufferSize = cPropSize;
      return EFI_BUFFER_TOO_SMALL;
    }

    *BufferSize = cPropSize;
    CopyMem (Buffer, cProperties,  cPropSize);
    return EFI_SUCCESS;
  } else if (/* !gSettings.EFIStringInjector && */ (mProperties != NULL) && (mPropSize > 1)) {
    if (*BufferSize < mPropSize) {
      *BufferSize = mPropSize;
      return EFI_BUFFER_TOO_SMALL;
    }

    *BufferSize = mPropSize;
    CopyMem (Buffer, mProperties,  mPropSize);
    return EFI_SUCCESS;
  }

  *BufferSize = 0;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetScreenInfo (
  VOID    *This,
  UINT64  *BaseAddress,
  UINT64  *FrameBufferSize,
  UINT32  *ByterPerRow,
  UINT32  *Width,
  UINT32  *Height,
  UINT32  *ColorDepth
) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *mGraphicsOutput=NULL;
  EFI_STATUS                      Status;

  Status = gBS->HandleProtocol (
                  gST->ConsoleOutHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)&mGraphicsOutput
                );

  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  //this print never occured so this procedure is redundant
  //  Print (L"GetScreenInfo called with args: %lx %lx %lx %lx %lx %lx\n",
  //        BaseAddress, FrameBufferSize, ByterPerRow, Width, Height, ColorDepth);

  *FrameBufferSize = (UINT64)mGraphicsOutput->Mode->FrameBufferSize;
  *BaseAddress = (UINT64)mGraphicsOutput->Mode->FrameBufferBase;
  *Width = (UINT32)mGraphicsOutput->Mode->Info->HorizontalResolution;
  *Height = (UINT32)mGraphicsOutput->Mode->Info->VerticalResolution;
  *ColorDepth = UGAColorDepth /* 32 */;
  *ByterPerRow = (UINT32)(mGraphicsOutput->Mode->Info->PixelsPerScanLine * 32) >> 3;

  //  Print (L"  Screen info: FBsize=%lx FBaddr=%lx W=%d H=%d\n",
  //      *FrameBufferSize, *BaseAddress, *Width, *Height);
  //  PauseForKey (L"--- press any key ---\n");

  return EFI_SUCCESS;
}

EFI_INTERFACE_SCREEN_INFO mScreenInfo = { GetScreenInfo };

EFI_STATUS
SetPrivateVarProto () {
  EFI_STATUS    Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &gImageHandle,
                  &gAppleDevicePropertyProtocolGuid,
                  &mDeviceProperties,
                  &gAppleFramebufferInfoProtocolGuid,
                  &mScreenInfo,
                  NULL
                );

  return Status;
}

DevPropString *
DevpropCreateString () {
  //DBG ("Begin creating strings for devices:\n");
  gDevPropString = (DevPropString *)AllocateZeroPool (sizeof (DevPropString));

  if (gDevPropString == NULL) {
    return NULL;
  }

  gDevPropString->length = 12;
  gDevPropString->WHAT2 = 0x01000000;

  return gDevPropString;
}

CHAR8 *
GetPciDevPath (
  PCI_DT    *PciDt
) {
  UINTN                       Len;
  CHAR8                       *Tmp;
  CHAR16                      *DevPathStr = NULL;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath = NULL;

  //DBG ("GetPciDevPath");
  DevicePath = DevicePathFromHandle (PciDt->DeviceHandle);
  if (!DevicePath) {
    return NULL;
  }

  DevPathStr = FileDevicePathToStr (DevicePath);
  Len = (StrLen (DevPathStr) + 1) * sizeof (CHAR16);
  Tmp = AllocateZeroPool (Len);
  UnicodeStrToAsciiStrS (DevPathStr, Tmp, Len);

  return Tmp;
}

UINT32
PciConfigRead32 (
  PCI_DT    *PciDt,
  UINT8     Reg
) {
  EFI_STATUS            Status;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  PCI_TYPE00            Pci;
  UINT32                Res;

  //  DBG ("PciConfigRead32\n");

  Status = gBS->OpenProtocol (
                  PciDt->DeviceHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (EFI_ERROR (Status)) {
    DBG ("PciConfigRead32 cant open protocol\n");
    return 0;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (Pci) / sizeof (UINT32),
                        &Pci
                      );

  if (EFI_ERROR (Status)) {
    DBG ("PciConfigRead32 cant read pci\n");
    return 0;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        (UINT64)(Reg & ~3),
                        1,
                        &Res
                      );

  if (EFI_ERROR (Status)) {
    DBG ("PciConfigRead32 failed %r\n", Status);
    return 0;
  }

  return Res;
}

//dmazar: replaced by DevpropAddDevicePci

DevPropDevice *
DevpropAddDevicePci (
  DevPropString   *StringBuf,
  PCI_DT          *PciDt
) {
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  DevPropDevice               *Device;
  UINT32                      NumPaths;

  if ((StringBuf == NULL) || (PciDt == NULL)) {
    return NULL;
  }

  DevicePath = DevicePathFromHandle (PciDt->DeviceHandle);

  //DBG ("DevpropAddDevicePci %s ", DevicePathToStr (DevicePath));
  if (!DevicePath) {
    return NULL;
  }

  Device = AllocateZeroPool (sizeof (DevPropDevice));
  if (!Device) {
    return NULL;
  }

  Device->numentries = 0x00;

  //
  // check and copy ACPI_DEVICE_PATH
  //
  if ((DevicePath->Type == ACPI_DEVICE_PATH) && (DevicePath->SubType == ACPI_DP)) {
    //CopyMem (&Device->acpi_dev_path, DevicePath, sizeof (struct ACPIDevPath));
    Device->acpi_dev_path.length = 0x0c;
    Device->acpi_dev_path.type = 0x02;
    Device->acpi_dev_path.subtype = 0x01;
    Device->acpi_dev_path._HID = 0x0a0341d0;
    //Device->acpi_dev_path._UID = gSettings.PCIRootUID;
    Device->acpi_dev_path._UID = (((ACPI_HID_DEVICE_PATH *)DevicePath)->UID) ? 0x80 : 0;

    //DBG ("ACPI HID=%x, UID=%x ", Device->acpi_dev_path._HID, Device->acpi_dev_path._UID);
  } else {
    //DBG ("not ACPI\n");
    FreePool (Device);
    return NULL;
  }

  //
  // copy PCI paths
  //
  for (NumPaths = 0; NumPaths < MAX_PCI_DEV_PATHS; NumPaths++) {
    DevicePath = NextDevicePathNode (DevicePath);
    if ((DevicePath->Type == HARDWARE_DEVICE_PATH) && (DevicePath->SubType == HW_PCI_DP)) {
      CopyMem (&Device->pci_dev_path[NumPaths], DevicePath, sizeof (struct PCIDevPath));
      //DBG (" - PCI[%02d]: f=%x, d=%x ", NumPaths, Device->pci_dev_path[NumPaths].function, Device->pci_dev_path[NumPaths].device);
    } else {
      // not PCI path - break the loop
      //DBG ("not PCI ");
      break;
    }
  }

  if (NumPaths == 0) {
    DBG ("NumPaths == 0 \n");
    FreePool (Device);
    return NULL;
  }

  //  DBG ("-> NumPaths=%d\n", NumPaths);
  Device->num_pci_devpaths = (UINT8)NumPaths;
  Device->length = (UINT32)(24U + (6U * NumPaths));

  Device->path_end.length = 0x04;
  Device->path_end.type = 0x7f;
  Device->path_end.subtype = 0xff;

  Device->string = StringBuf;
  Device->data = NULL;
  StringBuf->length += Device->length;

  if (!StringBuf->entries) {
    StringBuf->entries = (DevPropDevice **)AllocateZeroPool (MAX_NUM_DEVICES * sizeof (Device));
    if (!StringBuf->entries) {
      FreePool (Device);
      return NULL;
    }
  }

  StringBuf->entries[StringBuf->numentries++] = Device;

  return Device;
}

BOOLEAN
DevpropAddValue (
  DevPropDevice   *Device,
  CHAR8           *Nm,
  UINT8           *Vl,
  UINTN           Len
) {
  UINT32  Offset, Off, Length, *DataLength;
  UINT8   *Data, *Newdata;
  UINTN   i, l;

  if (!Device || !Nm || !Vl /* || !Len */) { //rehabman: allow zero length data
    return FALSE;
  }

  /*
    DBG ("DevpropAddValue %a=", Nm);
    for (i=0; i<Len; i++) {
      DBG ("%02X", Vl[i]);
    }
    DBG ("\n");
  */

  l = AsciiStrLen (Nm);
  Length = (UINT32)((l * 2) + Len + (2 * sizeof (UINT32)) + 2);
  Data = (UINT8 *)AllocateZeroPool (Length);
  if (!Data) {
    return FALSE;
  }

  Off = 0;

  Data[Off+1] = (UINT8)(((l * 2) + 6) >> 8);
  Data[Off] = ((l * 2) + 6) & 0x00FF;

  Off += 4;

  for (i = 0 ; i < l ; i++, Off += 2) {
    Data[Off] = *Nm++;
  }

  Off += 2;
  l = Len;
  DataLength = (UINT32 *)&Data[Off];
  *DataLength = (UINT32)(l + 4);
  Off += 4;

  for (i = 0 ; i < l ; i++, Off++) {
    Data[Off] = *Vl++;
  }

  Offset = Device->length - (24 + (6 * Device->num_pci_devpaths));

  Newdata = (UINT8 *)AllocateZeroPool (Length + Offset);

  if (!Newdata) {
    return FALSE;
  }

  if ((Device->data) && (Offset > 1)) {
    CopyMem ((VOID *)Newdata, (VOID *)Device->data, Offset);
  }

  CopyMem ((VOID *)(Newdata + Offset), (VOID *)Data, Length);

  Device->length += Length;
  Device->string->length += Length;
  Device->numentries++;
  Device->data = Newdata;

  FreePool (Data);

  return TRUE;
}

CHAR8 *
DevpropGenerateString (
  DevPropString     *StringBuf
) {
  UINTN   Len = StringBuf->length * 2;
  INT32   i = 0;
  UINT32  x = 0;
  CHAR8   *Buffer = (CHAR8 *)AllocatePool (Len + 1), *Ptr = Buffer;

  //DBG ("DevpropGenerateString\n");
  if (!Buffer) {
    return NULL;
  }

  AsciiSPrint (
    Buffer, Len, "%08x%08x%04x%04x", SwapBytes32 (StringBuf->length), StringBuf->WHAT2,
    SwapBytes16 (StringBuf->numentries), StringBuf->WHAT3
  );

  Buffer += 24;

  while (i < StringBuf->numentries) {
    UINT8   *DataPtr = StringBuf->entries[i]->data;

    AsciiSPrint (
      Buffer, Len, "%08x%04x%04x", SwapBytes32 (StringBuf->entries[i]->length),
      SwapBytes16 (StringBuf->entries[i]->numentries), StringBuf->entries[i]->WHAT2
    ); //FIXME: wrong buffer sizes!

    Buffer += 16;

    AsciiSPrint (
      Buffer, Len, "%02x%02x%04x%08x%08x", StringBuf->entries[i]->acpi_dev_path.type,
      StringBuf->entries[i]->acpi_dev_path.subtype,
      SwapBytes16 (StringBuf->entries[i]->acpi_dev_path.length),
      SwapBytes32 (StringBuf->entries[i]->acpi_dev_path._HID),
      SwapBytes32 (StringBuf->entries[i]->acpi_dev_path._UID)
    );

    Buffer += 24;

    for (x = 0; x < StringBuf->entries[i]->num_pci_devpaths; x++) {
      AsciiSPrint (Buffer, Len, "%02x%02x%04x%02x%02x", StringBuf->entries[i]->pci_dev_path[x].type,
        StringBuf->entries[i]->pci_dev_path[x].subtype,
        SwapBytes16 (StringBuf->entries[i]->pci_dev_path[x].length),
        StringBuf->entries[i]->pci_dev_path[x].function,
        StringBuf->entries[i]->pci_dev_path[x].device
      );

      Buffer += 12;
    }

    AsciiSPrint (
      Buffer, Len, "%02x%02x%04x", StringBuf->entries[i]->path_end.type,
      StringBuf->entries[i]->path_end.subtype,
      SwapBytes16 (StringBuf->entries[i]->path_end.length)
    );

    Buffer += 8;

    for (x = 0; x < (StringBuf->entries[i]->length) - (24 + (6 * StringBuf->entries[i]->num_pci_devpaths)); x++) {
      AsciiSPrint (Buffer, Len, "%02x", *DataPtr++);
      Buffer += 2;
    }

    i++;
  }

  return Ptr;
}

VOID
DevpropFreeString (
  DevPropString   *StringBuf
) {
  INT32   i;

  if (!StringBuf) {
    return;
  }

  for (i = 0; i < StringBuf->numentries; i++) {
    if (StringBuf->entries[i]) {
      if (StringBuf->entries[i]->data) {
        FreePool (StringBuf->entries[i]->data);
      }
    }
  }

  FreePool (StringBuf->entries);
  FreePool (StringBuf);
  //StringBuf = NULL;
}

// Ethernet built-in device injection
BOOLEAN
SetupEthernetDevprop (
  PCI_DT    *Dev
) {
  CHAR8           *DevicePath, Compatible[64];
  DevPropDevice   *Device;
  UINT8           Builtin = 0x0;
  BOOLEAN         Injected = FALSE;
  INT32           i;

  if (!gDevPropString) {
    gDevPropString = DevpropCreateString ();
  }

  DevicePath = GetPciDevPath (Dev);
  Device = DevpropAddDevicePci (gDevPropString, Dev);

  if (!Device) {
    MsgLog (" - Unknown, continue\n");
    return FALSE;
  }

  MsgLog (" - LAN Controller [%04x:%04x] | %a\n", Dev->vendor_id, Dev->device_id, DevicePath);

  if ((Dev->vendor_id != 0x168c) && (BuiltinSet == 0)) {
    BuiltinSet = 1;
    Builtin = 0x01;
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_LAN) {
        continue;
      }

      Injected = TRUE;

      DevpropAddValue (
        Device,
        gSettings.AddProperties[i].Key,
        (UINT8 *)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }
  }

  if (Injected) {
    MsgLog (" - Custom LAN properties injected\n");
    //return TRUE;
  }

  DBG (" - Setting dev.prop built-in=0x%x\n", Builtin);

  DevpropAddValue (Device, "device_type", (UINT8 *)"Ethernet", 9);

  if (gSettings.FakeLAN) {
    UINT32    FakeID = gSettings.FakeLAN >> 16;
    UINTN     Len = ARRAY_SIZE (Compatible);

    DevpropAddValue (Device, "device-id", (UINT8 *)&FakeID, 4);
    AsciiSPrint (Compatible, Len, "pci%x,%x", (gSettings.FakeLAN & 0xFFFF), FakeID);
    AsciiStrCpyS (Compatible, Len, AsciiStrToLower (Compatible));
    DevpropAddValue (Device, "compatible", (UINT8 *)&Compatible[0], 12);
    FakeID = gSettings.FakeLAN & 0xFFFF;
    DevpropAddValue (Device, "vendor-id", (UINT8 *)&FakeID, 4);
    MsgLog (" - With FakeLAN: %a\n", Compatible);
  }

  return DevpropAddValue (Device, "built-in", (UINT8 *)&Builtin, 1);
}

BOOLEAN
IsHDMIAudio (
  EFI_HANDLE  PciDevHandle
) {
  EFI_STATUS            Status;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  UINTN                 Segment, Bus, Device, Function, Index;

  // get device PciIo protocol
  Status = gBS->OpenProtocol (
                  PciDevHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  // get device location
  Status = PciIo->GetLocation (
                    PciIo,
                    &Segment,
                    &Bus,
                    &Device,
                    &Function
                  );

  if (!EFI_ERROR (Status)) {
    // iterate over all GFX devices and check for sibling
    for (Index = 0; Index < NGFX; Index++) {
      if (
        (gGraphics[Index].Segment == Segment) &&
        (gGraphics[Index].Bus == Bus) &&
        (gGraphics[Index].Device == Device)
      ) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

BOOLEAN
SetupHdaDevprop (
  PCI_DT    *Dev
) {
  CHAR8           *DevicePath;
  DevPropDevice   *Device;
  UINT32          HdaVal = 0, i;
  BOOLEAN         Injected = FALSE;

  if (!gSettings.HDALayoutId) {
    return FALSE;
  }

  if (!gDevPropString) {
    gDevPropString = DevpropCreateString ();
  }

  DevicePath = GetPciDevPath (Dev);
  Device = DevpropAddDevicePci (gDevPropString, Dev);

  if (!Device) {
    return FALSE;
  }

  MsgLog (" - HDA Controller [%04x:%04x] | %a\n", Dev->vendor_id, Dev->device_id, DevicePath);

  if (IsHDMIAudio (Dev->DeviceHandle)) {
    if (gSettings.NrAddProperties != 0xFFFE) {
      for (i = 0; i < gSettings.NrAddProperties; i++) {
        if (gSettings.AddProperties[i].Device != DEV_HDMI) {
          continue;
        }

        Injected = TRUE;

        DevpropAddValue (
          Device,
          gSettings.AddProperties[i].Key,
          (UINT8 *)gSettings.AddProperties[i].Value,
          gSettings.AddProperties[i].ValueLen
        );
      }
    }

    if (Injected) {
      DBG (" - custom HDMI properties injected, continue\n");
      //return TRUE;
    } else if (gSettings.UseIntelHDMI) {
      DBG (" - HDMI Audio, setting hda-gfx=onboard-1\n");
      DevpropAddValue (Device, "hda-gfx", (UINT8 *)"onboard-1", 10);
    }
  } else {
    // HDA - determine layout-id
    HdaVal = (UINT32)gSettings.HDALayoutId;
    MsgLog (" - Layout-id: %d\n", HdaVal);

    if (gSettings.NrAddProperties != 0xFFFE) {
      for (i = 0; i < gSettings.NrAddProperties; i++) {
        if (gSettings.AddProperties[i].Device != DEV_HDA) {
          continue;
        }

        Injected = TRUE;

        DevpropAddValue (
          Device,
          gSettings.AddProperties[i].Key,
          (UINT8 *)gSettings.AddProperties[i].Value,
          gSettings.AddProperties[i].ValueLen
        );
      }
    }

    if (!Injected) {
      DevpropAddValue (Device, "layout-id", (UINT8 *)&HdaVal, 4);

      HdaVal = 0; // reuse variable

      if (gSettings.UseIntelHDMI) {
        DevpropAddValue (Device, "hda-gfx", (UINT8 *)"onboard-1", 10);
      }

      DevpropAddValue (Device, "MaximumBootBeepVolume", (UINT8 *)&HdaVal, 1);
      //DevpropAddValue (Device, "PinConfigurations", (UINT8 *)&HdaVal, 1);
    }

  }

  return TRUE;
}
