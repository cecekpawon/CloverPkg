/*
 *  Copyright 2009 Jasmin Fazlic All rights reserved.
 *
 *
 *  Cleaned and merged by iNDi
 */
// UEFI adaptation by usr-sse2, then slice, dmazar

#include <Library/Platform/Platform.h>

#ifndef DEBUG_INJECT
#ifndef DEBUG_ALL
#define DEBUG_INJECT 1
#else
#define DEBUG_INJECT DEBUG_ALL
#endif
#endif

#if DEBUG_INJECT == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_INJECT, __VA_ARGS__)
#endif

DevPropString   *string = NULL;
UINT8           *stringdata    = NULL;
UINT32          stringlength   = 0;
UINT32          devices_number = 1;
UINT32          builtin_set    = 0;


EFI_GUID gDevicePropertiesGuid = {
  0x91BD12FE, 0xF6C3, 0x44FB, {0xA5, 0xB7, 0x51, 0x22, 0xAB, 0x30, 0x3A, 0xE0}
};

/*
EFI_GUID gAppleScreenInfoProtocolGuid = {
  0xe316e100, 0x0751, 0x4c49, {0x90, 0x56, 0x48, 0x6c, 0x7e, 0x47, 0x29, 0x03}
};
*/

extern EFI_GUID gAppleScreenInfoProtocolGuid;

UINT32    mPropSize = 0;
UINT8     *mProperties = NULL;
CHAR8     *gDeviceProperties = NULL;

UINT32    cPropSize = 0;
UINT8     * cProperties = NULL;
CHAR8     * cDeviceProperties = NULL;

#define DEVICE_PROPERTIES_SIGNATURE SIGNATURE_64('A','P','P','L','E','D','E','V')

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
  EFI_STATUS(EFIAPI *Unknown1)(IN VOID *);
  EFI_STATUS(EFIAPI *Unknown2)(IN VOID *);
  EFI_STATUS(EFIAPI *Unknown3)(IN VOID *);
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
  UINT64  *baseAddress,
  UINT64  *frameBufferSize,
  UINT32  *byterPerRow,
  UINT32  *Width,
  UINT32  *Height,
  UINT32  *colorDepth
);

typedef struct {
  EFI_SCREEN_INFO_FUNCTION GetScreenInfo;
} EFI_INTERFACE_SCREEN_INFO;

EFI_STATUS
EFIAPI
GetDeviceProps (
  IN     APPLE_GETVAR_PROTOCOL   *This,
  IN     CHAR8                   *Buffer,
  IN OUT UINT32                  *BufferSize
) {
  if (!gSettings.StringInjector && (mProperties != NULL) && (mPropSize > 1)) {
    if (*BufferSize < mPropSize) {
      *BufferSize = mPropSize;
      return EFI_BUFFER_TOO_SMALL;
    }

    *BufferSize = mPropSize;
    CopyMem(Buffer, mProperties,  mPropSize);
    return EFI_SUCCESS;
  } else if ((cProperties != NULL) && (cPropSize > 1)) {
    if (*BufferSize < cPropSize) {
      *BufferSize = cPropSize;
      return EFI_BUFFER_TOO_SMALL;
    }

    *BufferSize = cPropSize;
    CopyMem(Buffer, cProperties,  cPropSize);
    return EFI_SUCCESS;
  }

  *BufferSize = 0;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetScreenInfo (
  VOID    *This,
  UINT64  *baseAddress,
  UINT64  *frameBufferSize,
  UINT32  *bpr,
  UINT32  *w,
  UINT32  *h,
  UINT32  *colorDepth
) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *mGraphicsOutput=NULL;
  EFI_STATUS                      Status;

  Status = gBS->HandleProtocol (
                  gST->ConsoleOutHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **) &mGraphicsOutput
                );

  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  //this print never occured so this procedure is redundant
  //  Print(L"GetScreenInfo called with args: %lx %lx %lx %lx %lx %lx\n",
  //        baseAddress, frameBufferSize, bpr, w, h, colorDepth);
  *frameBufferSize = (UINT64)mGraphicsOutput->Mode->FrameBufferSize;
  *baseAddress = (UINT64)mGraphicsOutput->Mode->FrameBufferBase;
  *w = (UINT32)mGraphicsOutput->Mode->Info->HorizontalResolution;
  *h = (UINT32)mGraphicsOutput->Mode->Info->VerticalResolution;
  *colorDepth = 32;
  *bpr = (UINT32)(mGraphicsOutput->Mode->Info->PixelsPerScanLine*32) >> 3;
  //  Print(L"  Screen info: FBsize=%lx FBaddr=%lx w=%d h=%d\n",
  //      *frameBufferSize, *baseAddress, *w, *h);
  //  PauseForKey(L"--- press any key ---\n");

  return EFI_SUCCESS;
}

EFI_INTERFACE_SCREEN_INFO mScreenInfo = { GetScreenInfo };

EFI_STATUS
SetPrivateVarProto(VOID) {
  EFI_STATUS    Status;

  //This must be independent install
  /*Status = */gBS->InstallMultipleProtocolInterfaces (
                       &gImageHandle,
                       &gAppleScreenInfoProtocolGuid,
                       &mScreenInfo,
                       NULL
                     );

  Status = gBS->InstallMultipleProtocolInterfaces (
                   &gImageHandle,
                   &gDevicePropertiesGuid,
                   &mDeviceProperties,
                   NULL
                 );

  return Status;
}


DevPropString *devprop_create_string(VOID) {
  //  DBG("Begin creating strings for devices:\n");
  string = (DevPropString*)AllocateZeroPool(sizeof(DevPropString));

  if (string == NULL) {
    return NULL;
  }

  string->length = 12;
  string->WHAT2 = 0x01000000;
  return string;
}

CHAR8
*get_pci_dev_path (
  pci_dt_t    *PciDt
) {
  CHAR8                       *tmp;
  CHAR16                      *devpathstr = NULL;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath = NULL;

  //DBG("get_pci_dev_path");
  DevicePath = DevicePathFromHandle (PciDt->DeviceHandle);
  if (!DevicePath) {
    return NULL;
  }

  devpathstr = FileDevicePathToStr(DevicePath);
  tmp = AllocateZeroPool((StrLen(devpathstr)+1)*sizeof(CHAR16));
  UnicodeStrToAsciiStr(devpathstr, tmp);

  return tmp;
}

UINT32
pci_config_read32 (
  pci_dt_t    *PciDt,
  UINT8       reg
) {
  //  DBG("pci_config_read32\n");
  EFI_STATUS            Status;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  PCI_TYPE00            Pci;
  UINT32                res;

  Status = gBS->OpenProtocol (
                  PciDt->DeviceHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID**)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (EFI_ERROR(Status)){
    DBG("pci_config_read cant open protocol\n");
    return 0;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof(Pci) / sizeof(UINT32),
                        &Pci
                      );

  if (EFI_ERROR(Status)) {
    DBG("pci_config_read cant read pci\n");
    return 0;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        (UINT64)(reg & ~3),
                        1,
                        &res
                      );

  if (EFI_ERROR(Status)) {
    DBG("pci_config_read32 failed %r\n", Status);
    return 0;
  }

  return res;
}

//dmazar: replaced by devprop_add_device_pci

DevPropDevice
*devprop_add_device_pci (
  DevPropString   *StringBuf,
  pci_dt_t        *PciDt
) {
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  DevPropDevice               *device;
  UINT32                      NumPaths;

  if ((StringBuf == NULL) || (PciDt == NULL)) {
    return NULL;
  }

  DevicePath = DevicePathFromHandle(PciDt->DeviceHandle);

  //DBG("devprop_add_device_pci %s ", DevicePathToStr(DevicePath));
  if (!DevicePath) {
    return NULL;
  }

  device = AllocateZeroPool(sizeof(DevPropDevice));
  if (!device) {
    return NULL;
  }

  device->numentries = 0x00;

  //
  // check and copy ACPI_DEVICE_PATH
  //
  if ((DevicePath->Type == ACPI_DEVICE_PATH) && (DevicePath->SubType == ACPI_DP)) {
    //CopyMem(&device->acpi_dev_path, DevicePath, sizeof(struct ACPIDevPath));
    device->acpi_dev_path.length = 0x0c;
    device->acpi_dev_path.type = 0x02;
    device->acpi_dev_path.subtype = 0x01;
    device->acpi_dev_path._HID = 0x0a0341d0;
    //device->acpi_dev_path._UID = gSettings.PCIRootUID;
    device->acpi_dev_path._UID = (((ACPI_HID_DEVICE_PATH*)DevicePath)->UID) ? 0x80 : 0;

    //DBG("ACPI HID=%x, UID=%x ", device->acpi_dev_path._HID, device->acpi_dev_path._UID);
  } else {
    //DBG("not ACPI\n");
    FreePool(device);
    return NULL;
  }

  //
  // copy PCI paths
  //
  for (NumPaths = 0; NumPaths < MAX_PCI_DEV_PATHS; NumPaths++) {
    DevicePath = NextDevicePathNode(DevicePath);
    if ((DevicePath->Type == HARDWARE_DEVICE_PATH) && (DevicePath->SubType == HW_PCI_DP)) {
      CopyMem(&device->pci_dev_path[NumPaths], DevicePath, sizeof(struct PCIDevPath));
      //DBG(" - PCI[%02d]: f=%x, d=%x ", NumPaths, device->pci_dev_path[NumPaths].function, device->pci_dev_path[NumPaths].device);
    } else {
      // not PCI path - break the loop
      //DBG("not PCI ");
      break;
    }
  }

  if (NumPaths == 0) {
    DBG("NumPaths == 0 \n");
    FreePool(device);
    return NULL;
  }

  //  DBG("-> NumPaths=%d\n", NumPaths);
  device->num_pci_devpaths = (UINT8)NumPaths;
  device->length = (UINT32)(24U + (6U * NumPaths));

  device->path_end.length = 0x04;
  device->path_end.type = 0x7f;
  device->path_end.subtype = 0xff;

  device->string = StringBuf;
  device->data = NULL;
  StringBuf->length += device->length;

  if (!StringBuf->entries) {
    StringBuf->entries = (DevPropDevice**)AllocateZeroPool(MAX_NUM_DEVICES * sizeof(device));
    if (!StringBuf->entries) {
      FreePool(device);
      return NULL;
    }
  }

  StringBuf->entries[StringBuf->numentries++] = device;

  return device;
}

BOOLEAN
devprop_add_value (
  DevPropDevice   *device,
  CHAR8           *nm,
  UINT8           *vl,
  UINTN           len
) {
  UINT32  offset, off, length, *datalength;
  UINT8   *data, *newdata;
  UINTN   i, l;

  if (!device || !nm || !vl /*|| !len*/) { //rehabman: allow zero length data
    return FALSE;
  }

  /*
    DBG("devprop_add_value %a=", nm);
    for (i=0; i<len; i++) {
      DBG("%02X", vl[i]);
    }
    DBG("\n");
  */

  l = AsciiStrLen(nm);
  length = (UINT32)((l * 2) + len + (2 * sizeof(UINT32)) + 2);
  data = (UINT8*)AllocateZeroPool(length);
  if (!data)
    return FALSE;

  off= 0;

  data[off+1] = (UINT8)(((l * 2) + 6) >> 8);
  data[off] =   ((l * 2) + 6) & 0x00FF;

  off += 4;

  for (i = 0 ; i < l ; i++, off += 2) {
    data[off] = *nm++;
  }

  off += 2;
  l = len;
  datalength = (UINT32*)&data[off];
  *datalength = (UINT32)(l + 4);
  off += 4;

  for (i = 0 ; i < l ; i++, off++) {
    data[off] = *vl++;
  }

  offset = device->length - (24 + (6 * device->num_pci_devpaths));

  newdata = (UINT8*)AllocateZeroPool((length + offset));

  if (!newdata) {
    return FALSE;
  }

  if ((device->data) && (offset > 1)) {
    CopyMem((VOID*)newdata, (VOID*)device->data, offset);
  }

  CopyMem((VOID*)(newdata + offset), (VOID*)data, length);

  device->length += length;
  device->string->length += length;
  device->numentries++;
  device->data = newdata;

  FreePool(data);

  return TRUE;
}

CHAR8
*devprop_generate_string (
  DevPropString     *StringBuf
) {
  UINTN   len = StringBuf->length * 2;
  INT32   i = 0;
  UINT32  x = 0;
  CHAR8   *buffer = (CHAR8*)AllocatePool(len + 1), *ptr = buffer;

  //DBG("devprop_generate_string\n");
  if (!buffer) {
    return NULL;
  }

  AsciiSPrint (
    buffer, len, "%08x%08x%04x%04x", SwapBytes32(StringBuf->length), StringBuf->WHAT2,
    SwapBytes16(StringBuf->numentries), StringBuf->WHAT3
  );

  buffer += 24;

  while(i < StringBuf->numentries) {
    UINT8   *dataptr = StringBuf->entries[i]->data;

    AsciiSPrint (
      buffer, len, "%08x%04x%04x", SwapBytes32(StringBuf->entries[i]->length),
      SwapBytes16(StringBuf->entries[i]->numentries), StringBuf->entries[i]->WHAT2
    ); //FIXME: wrong buffer sizes!

    buffer += 16;

    AsciiSPrint (
      buffer, len, "%02x%02x%04x%08x%08x", StringBuf->entries[i]->acpi_dev_path.type,
      StringBuf->entries[i]->acpi_dev_path.subtype,
      SwapBytes16(StringBuf->entries[i]->acpi_dev_path.length),
      SwapBytes32(StringBuf->entries[i]->acpi_dev_path._HID),
      SwapBytes32(StringBuf->entries[i]->acpi_dev_path._UID)
    );

    buffer += 24;
    for (x = 0; x < StringBuf->entries[i]->num_pci_devpaths; x++) {
      AsciiSPrint(buffer, len, "%02x%02x%04x%02x%02x", StringBuf->entries[i]->pci_dev_path[x].type,
        StringBuf->entries[i]->pci_dev_path[x].subtype,
        SwapBytes16(StringBuf->entries[i]->pci_dev_path[x].length),
        StringBuf->entries[i]->pci_dev_path[x].function,
        StringBuf->entries[i]->pci_dev_path[x].device
      );

      buffer += 12;
    }

    AsciiSPrint (
      buffer, len, "%02x%02x%04x", StringBuf->entries[i]->path_end.type,
      StringBuf->entries[i]->path_end.subtype,
      SwapBytes16(StringBuf->entries[i]->path_end.length)
    );

    buffer += 8;

    for (x = 0; x < (StringBuf->entries[i]->length) - (24 + (6 * StringBuf->entries[i]->num_pci_devpaths)); x++) {
      AsciiSPrint(buffer, len, "%02x", *dataptr++);
      buffer += 2;
    }
    i++;
  }

  return ptr;
}

VOID
devprop_free_string (
  DevPropString   *StringBuf
) {
  INT32   i;

  if (!StringBuf) {
    return;
  }

  for (i = 0; i < StringBuf->numentries; i++) {
    if (StringBuf->entries[i]) {
      if (StringBuf->entries[i]->data) {
        FreePool(StringBuf->entries[i]->data);
      }
    }
  }

  FreePool(StringBuf->entries);
  FreePool(StringBuf);
  //  StringBuf = NULL;
}

// Ethernet built-in device injection
BOOLEAN
set_eth_props (
  pci_dt_t    *eth_dev
) {
  DevPropDevice   *device;
  UINT8           builtin = 0x0;
  BOOLEAN         Injected = FALSE;
  INT32           i;
  CHAR8           compatible[64];


  if (!string) {
    string = devprop_create_string();
  }

  device = devprop_add_device_pci(string, eth_dev);

  if (!device) {
    DBG(" - Unknown, continue\n");
    return FALSE;
  }

  // -------------------------------------------------
  //DBG("LAN Controller [%04x:%04x] %a\n", eth_dev->vendor_id, eth_dev->device_id, devicepath);
  DBG("LAN Controller [%04x:%04x]\n", eth_dev->vendor_id, eth_dev->device_id);

  if ((eth_dev->vendor_id != 0x168c) && (builtin_set == 0)) {
    builtin_set = 1;
    builtin = 0x01;
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_LAN) {
        continue;
      }

      Injected = TRUE;

      devprop_add_value (
        device,
        gSettings.AddProperties[i].Key,
        (UINT8*)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }
  }

  if (Injected) {
    DBG(" - Custom LAN properties injected\n");
    //    return TRUE;
  }

  DBG(" - Setting dev.prop built-in=0x%x\n", builtin);

  devprop_add_value(device, "device_type", (UINT8*)"Ethernet", 9);

  if (gSettings.FakeLAN) {
    UINT32    FakeID = gSettings.FakeLAN >> 16;

    devprop_add_value(device, "device-id", (UINT8*)&FakeID, 4);
    AsciiSPrint(compatible, 64, "pci%x,%x", (gSettings.FakeLAN & 0xFFFF), FakeID);
    AsciiStrCpy(compatible, AsciiStrToLower(compatible));
    devprop_add_value(device, "compatible", (UINT8*)&compatible[0], 12);
    FakeID = gSettings.FakeLAN & 0xFFFF;
    devprop_add_value(device, "vendor-id", (UINT8*)&FakeID, 4);
    DBG(" - With FakeLAN: %a\n", compatible);
  }

  return devprop_add_value(device, "built-in", (UINT8*)&builtin, 1);
}

BOOLEAN
IsHDMIAudio (
  EFI_HANDLE PciDevHandle
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

  if (EFI_ERROR(Status)) {
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

  if (EFI_ERROR(Status)) {
    return FALSE;
  }

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

  return FALSE;
}

BOOLEAN
set_hda_props (
  EFI_PCI_IO_PROTOCOL   *PciIo,
  pci_dt_t              *hda_dev/*,
  CHAR8                 *OSVersion*/
) {
  CHAR8           *devicepath;
  DevPropDevice   *device;
  UINT32          layoutId = 0/*, codecId = 0*/;
  BOOLEAN         Injected = FALSE;
  INT32           i;

  if (!gSettings.HDALayoutId) {
    return FALSE;
  }

  if (!string) {
    string = devprop_create_string();
  }

  devicepath = get_pci_dev_path(hda_dev);
  device = devprop_add_device_pci(string, hda_dev);

  if (!device) {
    return FALSE;
  }

  DBG(" - HDA Controller [%04x:%04x] %a =>", hda_dev->vendor_id, hda_dev->device_id, devicepath);

  if (IsHDMIAudio(hda_dev->DeviceHandle)) {
    if (gSettings.NrAddProperties != 0xFFFE) {
      for (i = 0; i < gSettings.NrAddProperties; i++) {
        if (gSettings.AddProperties[i].Device != DEV_HDMI) {
          continue;
        }

        Injected = TRUE;

        devprop_add_value (
          device,
          gSettings.AddProperties[i].Key,
          (UINT8*)gSettings.AddProperties[i].Value,
          gSettings.AddProperties[i].ValueLen
        );
      }
    }

    if (Injected) {
      DBG("custom HDMI properties injected, continue\n");
      //    return TRUE;
    } else if (gSettings.UseIntelHDMI) {
      DBG(" HDMI Audio, setting hda-gfx=onboard-1\n");
      devprop_add_value(device, "hda-gfx", (UINT8*)"onboard-1", 10);
    }
  } else {
    // HDA - determine layout-id
    layoutId = (UINT32)gSettings.HDALayoutId;
    DBG(" setting specified layout-id=%d\n", layoutId);

    if (gSettings.NrAddProperties != 0xFFFE) {
      for (i = 0; i < gSettings.NrAddProperties; i++) {
        if (gSettings.AddProperties[i].Device != DEV_HDA) {
          continue;
        }

        Injected = TRUE;

        devprop_add_value (
          device,
          gSettings.AddProperties[i].Key,
          (UINT8*)gSettings.AddProperties[i].Value,
          gSettings.AddProperties[i].ValueLen
        );
      }
    }

    if (!Injected) {
      devprop_add_value(device, "layout-id", (UINT8*)&layoutId, 4);

      layoutId = 0; // reuse variable
      if (gSettings.UseIntelHDMI) {
        devprop_add_value(device, "hda-gfx", (UINT8*)"onboard-1", 10);
      }

      devprop_add_value(device, "MaximumBootBeepVolume", (UINT8*)&layoutId, 1);
      //devprop_add_value(device, "PinConfigurations", (UINT8*)&layoutId, 1);
    }

  }

  return TRUE;
}
