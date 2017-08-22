/*
 *  NVidia injector
 *
 *  Copyright (C) 2009  Jasmin Fazlic, iNDi
 *
 *  NVidia injector modified by Fabio (ErmaC) on May 2012,
 *  for allow the cosmetics injection also based on SubVendorID and SubDeviceID.
 *
 *  NVidia injector is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  NVidia driver and injector is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NVidia injector.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Alternatively you can choose to comply with APSL
 *
 *  DCB-Table parsing is based on software (nouveau driver) originally distributed under following license:
 *
 *
 *  Copyright 2005-2006 Erik Waling
 *  Copyright 2006 Stephane Marchesin
 *  Copyright 2007-2009 Stuart Bennett
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 *  OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <Library/Platform/Platform.h>
//#include <Library/Platform/Nvidia.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_NVIDIA
#define DEBUG_NVIDIA -1
#endif
#else
#ifdef DEBUG_NVIDIA
#undef DEBUG_NVIDIA
#endif
#define DEBUG_NVIDIA DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_NVIDIA, __VA_ARGS__)

#define NV_PBUS_PCI_NV_20                         0x00001850
#define NV_PBUS_PCI_NV_20_ROM_SHADOW_DISABLED     (0 << 0)
#define NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED      (1 << 0)
#define NV_PDISPLAY_OFFSET                        0x610000
#define NV_PMC_OFFSET                             0x000000
#define NV_PRAMIN_OFFSET                          0x00700000
#define NV_PROM_OFFSET                            0x300000
#define NVIDIA_ROM_SIZE                           0x20000

UINT8 DefNVCAP[] = {
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
  0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
  0x00, 0x00, 0x00, 0x00
};

#define NVCAP_LEN     ARRAY_SIZE (DefNVCAP)

//UINT8 DefDCFG0[] = { 0x03, 0x01, 0x03, 0x00 };
//UINT8 DefDCFG1[] = { 0xff, 0xff, 0x00, 0x01 };

//#define DCFG0_LEN   ARRAY_SIZE (DefDCFG0)
//#define DCFG1_LEN   ARRAY_SIZE (DefDCFG1)

//UINT8 DefNVPM[]= {
//  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x00, 0x00, 0x00, 0x00
//};

//#define NVPM_LEN  ARRAY_SIZE (DefNVPM)

#define S_NVIDIAMODEL "Nvidia Graphics"

//STATIC nvidia_pci_info_t nvidia_card_generic[] = {
//  { 0x10DE0000, "Nvidia Graphics" }
//};

EFI_STATUS
ReadNvidiaPRAMIN (
  PCI_DT    *Dev,
  VOID      *Rom,
  UINT16    Arch
) {
  EFI_STATUS            Status;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  PCI_TYPE00            Pci;
  UINT32                VRam = 0, Bar0 = 0;

  DBG ("%a\n", __FUNCTION__);

  Status = gBS->OpenProtocol (
                  Dev->DeviceHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (Pci) / sizeof (UINT32),
                        &Pci
                      );

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  if (Arch >= 0x50) {
    DBG ("Using PRAMIN fixups\n");
    /*Status = */PciIo->Mem.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              NV_PDISPLAY_OFFSET + 0x9f04,///4,
                              1,
                              &VRam
                            );

    VRam = (VRam & ~0xff) << 8;

    /*Status = */PciIo->Mem.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              NV_PMC_OFFSET + 0x1700,///4,
                              1,
                              &Bar0
                            );

    if (VRam == 0)
      VRam = (Bar0 << 16) + 0xf0000;

    VRam >>= 16;

    /*Status = */PciIo->Mem.Write (
                                PciIo,
                                EfiPciIoWidthUint32,
                                0,
                                NV_PMC_OFFSET + 0x1700,///4,
                                1,
                                &VRam
                             );
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        NV_PRAMIN_OFFSET,
                        NVIDIA_ROM_SIZE,
                        Rom
                      );

  if (Arch >= 0x50) {
    /*Status = */PciIo->Mem.Write (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              NV_PMC_OFFSET + 0x1700,///4,
                              1,
                              &Bar0
                            );
  }

  if (EFI_ERROR (Status)) {
    DBG (" failed\n");
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ReadNvidiaPROM (
  PCI_DT    *Dev,
  VOID      *Rom
) {
  EFI_STATUS              Status;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  PCI_TYPE00              Pci;
  UINT32                  Val;

  DBG ("%a\n", __FUNCTION__);

  Status = gBS->OpenProtocol (
                  Dev->DeviceHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  Status = PciIo->Pci.Read (PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (Pci) / sizeof (UINT32),
                        &Pci
                      );

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  Val = NV_PBUS_PCI_NV_20_ROM_SHADOW_DISABLED;
  /*Status = */PciIo->Mem.Write (
                            PciIo,
                            EfiPciIoWidthUint32,
                            0,
                            NV_PBUS_PCI_NV_20 * 4,
                            1,
                            &Val
                          );

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        NV_PROM_OFFSET,
                        NVIDIA_ROM_SIZE,
                        Rom
                      );

  Val = NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED;
  /*Status = */PciIo->Mem.Write (
                            PciIo,
                            EfiPciIoWidthUint32,
                            0,
                            NV_PBUS_PCI_NV_20 * 4,
                            1,
                            &Val
                          );

  if (EFI_ERROR (Status)) {
    DBG (" failed\n");
    return Status;
  }

  return EFI_SUCCESS;
}

VOID
DevpropAddNvidiaTemplate (
  DevPropDevice   *Device,
  INTN            PortsNum
) {
  INTN    PNum;
  CHAR8   NKey[24], NVal[24];

  //DBG ("DevpropAddNvidiaTemplate\n");

  if (!Device) {
    return;
  }

  for (PNum = 0; PNum < PortsNum; PNum++) {
    AsciiSPrint (NKey, 24, "@%d,name", PNum);
    AsciiSPrint (NVal, 24, "NVDA,Display-%c", (65 + PNum));
    DevpropAddValue (Device, NKey, (UINT8 *)NVal, 14);

    AsciiSPrint (NKey, 24, "@%d,compatible", PNum);
    DevpropAddValue (Device, NKey, (UINT8 *)"NVDA,NVMac", 10);

    AsciiSPrint (NKey, 24, "@%d,device_type", PNum);
    DevpropAddValue (Device, NKey, (UINT8 *)"display", 7);

    //if (!gSettings.NoDefaultProperties) {
    //  AsciiSPrint (NKey, 24, "@%d,display-cfg", PNum);
    //  if (PNum == 0) {
    //    DevpropAddValue (Device, NKey, (gSettings.Dcfg[0] != 0) ? &gSettings.Dcfg[0] : DefDCFG0, DCFG0_LEN);
    //  } else {
    //    DevpropAddValue (Device, NKey, (gSettings.Dcfg[1] != 0) ? &gSettings.Dcfg[4] : DefDCFG1, DCFG1_LEN);
    //  }
    //}
  }

  DevpropAddValue (
    Device,
    "device_type",
    (gDevicesNumber == 1) ? (UINT8 *)"NVDA,Parent" : (UINT8 *)"NVDA,Child",
    (gDevicesNumber == 1) ? 11 : 10
  );
}

BOOLEAN
SetupNvidiaDevprop (
  PCI_DT    *Dev
) {
  EFI_STATUS                Status = EFI_NOT_FOUND;
  DevPropDevice             *Device = NULL;
  BOOLEAN                   LoadVBios = gSettings.LoadVBios, Injected = FALSE;
  UINT8                     *Rom = NULL, *Buffer = NULL; //, ConnectorType1[] = { 0x00, 0x08, 0x00, 0x00 }
  UINT16                    CardType = 0;
  UINT32                    Bar[7], DeviceId, SubSysId,  BootDisplay = 1;
  UINT64                    VRam = 0;
  CHAR16                    FileName[64];
  UINTN                     BufferLen, j, PortsNum = 0;
  INT32                     MaxBiosVersionLen = 32,
                            i, VersionStart, CrlfCount = 0; //, Patch = 0
  OPTION_ROM_PCI_HEADER     *RomPciHeader;
  CHAR8                     *Model = NULL, *DevicePath,
                            *VersionStr = (CHAR8 *)AllocateZeroPool (MaxBiosVersionLen);
  //CARDLIST                  *NVCard;

  DevicePath = GetPciDevPath (Dev);
  Bar[0] = PciConfigRead32 (Dev, PCI_BASE_ADDRESS_0);
  Dev->regs = (UINT8 *)(UINTN)(Bar[0] & ~0x0f);
  DeviceId = ((Dev->vendor_id << 16) | Dev->device_id);
  SubSysId = ((Dev->subsys_id.subsys.vendor_id << 16) | Dev->subsys_id.subsys.device_id);

  // get card type
  CardType = (REG32 (Dev->regs, 0) >> 20) & 0x1ff;

  /*
  // First check if any value exist in the plist
  NVCard = FindCardWithIds (DeviceId, SubSysId);

  if (NVCard) {
    if (NVCard->VideoRam > 0) {
      //VideoRam * 1024 * 1024 == VideoRam << 20
      //VRam = LShiftU64 (NVCard->VideoRam, 20);
      VRam = NVCard->VideoRam;
      Model = (CHAR8 *)AllocateCopyPool (AsciiStrSize (NVCard->Model), NVCard->Model);
      PortsNum = NVCard->VideoPorts;
      LoadVBios = NVCard->LoadVBios;
    }
  }
  */

  if (LoadVBios) {
    UnicodeSPrint (
      FileName, ARRAY_SIZE (FileName), L"%s\\10de_%04x_%04x_%04x.rom",
      DIR_ROM, Dev->device_id, Dev->subsys_id.subsys.vendor_id, Dev->subsys_id.subsys.device_id
    );

    if (FileExists (gSelfRootDir, FileName)) {
      DBG (" - Found specific VBIOS ROM file (%s)\n", FileName);

      Status = LoadFile (gSelfRootDir, FileName, &Buffer, &BufferLen);
    }
  }

  if (EFI_ERROR (Status)) {
    Rom = AllocateZeroPool (NVIDIA_ROM_SIZE + 1);
    // PRAMIN first
    ReadNvidiaPRAMIN (Dev, Rom, CardType);

    //DBG ("%x%x\n", Rom[0], Rom[1]);
    RomPciHeader = NULL;

    if ((Rom[0] != 0x55) || (Rom[1] != 0xaa)) {
      ReadNvidiaPROM (Dev, Rom);

      if ((Rom[0] != 0x55) || (Rom[1] != 0xaa)) {
        DBG (" - ERROR: Unable to locate Nvidia Video BIOS\n");
        FreePool (Rom);
        Rom = NULL;
      }
    }
  } else if (Buffer) {
    if ((Buffer[0] != 0x55) && (Buffer[1] != 0xaa)) {
      i = 0;

      while (i < BufferLen) {
        //DBG ("%x%x\n", Buffer[i], Buffer[i + 1]);
        if ((Buffer[i] == 0x55) && (Buffer[i + 1] == 0xaa)) {
          DBG (" - header found at: %d\n", i);
          BufferLen -= i;
          Rom = AllocateZeroPool (BufferLen);

          for (j = 0; j < BufferLen; j++) {
            Rom[j] = Buffer[i + j];
          }

          break;
        }

        i += 512;
      }
    }

    if (!Rom) {
      Rom = Buffer;
    }

    DBG (" - using loaded ROM image\n");
  }

  if (!Rom || !Buffer) {
    DBG (" - there are no ROM loaded / VBIOS read from hardware\n");
  }

  if (Rom) {
    RomPciHeader = (OPTION_ROM_PCI_HEADER *)(Rom + *(UINT16 *)&Rom[24]);

    // check for 'PCIR' sig
    if (RomPciHeader->signature != 0x52494350) {
      DBG (" - incorrect PCI ROM signature: 0x%x\n", RomPciHeader->signature);
    }

    // get bios version

    // only search the first 384 bytes
    for (i = 0; i < 0x180; i++) {
      if ((Rom[i] == 0x0D) && (Rom[i + 1] == 0x0A)) {
        CrlfCount++;
        // second 0x0D0A was found, extract bios version
        if (CrlfCount == 2) {
          if (Rom[i - 1] == 0x20) {
            i--; // strip last " "
          }

          for (VersionStart = i; VersionStart > (i - MaxBiosVersionLen); VersionStart--) {
            // find start
            if (Rom[VersionStart] == 0x00) {
              CHAR8   *s, *s1;

              VersionStart++;

              // strip "Version "
              if (AsciiStrnCmp ((CONST CHAR8 *)Rom + VersionStart, "Version ", 8) == 0) {
                VersionStart += 8;
              }

              s = (CHAR8 *)(Rom + VersionStart);
              s1 = VersionStr;

              while ((*s > ' ') && (*s < 'z') && ((INTN)(s1 - VersionStr) < MaxBiosVersionLen)) {
                *s1++ = *s++;
              }

              *s1 = 0;
              break;
            }
          }

          break;
        }
      }
    }
  } else {
    AsciiSPrint (VersionStr, sizeof (VersionStr), "1.0");
  }

  DBG (" - Version: %a\n", VersionStr);

  if (!Model) {
    Model = (CHAR8 *)AllocateCopyPool (AsciiStrSize (S_NVIDIAMODEL), S_NVIDIAMODEL);
  }

  MsgLog (
    " - %a | %dMB NV%02x [%04x:%04x] | %a => device #%d\n",
    Model, (UINT32)(RShiftU64 (VRam, 20)),
    CardType, Dev->vendor_id, Dev->device_id,
    DevicePath, gDevicesNumber
  );

  if (!gDevPropString) {
    gDevPropString = DevpropCreateString ();
  }

  Device = DevpropAddDevicePci (gDevPropString, Dev);

  DBG (" - VideoPorts:");

  if (!PortsNum) {
    if (gSettings.VideoPorts > 0) {
      PortsNum = gSettings.VideoPorts;
      DBG (" user defined from config.plist: %d\n", PortsNum);
    } else {
      PortsNum = 2; //default
      DBG (" undefined, default to: %d\n", PortsNum);
    }
  }

  //There are custom properties, injected if set by user
  if (gSettings.NvidiaSingle && (gDevicesNumber >= 1)) {
    DBG (" - NvidiaSingle: skip injecting the rest of cards\n");
    goto Finish;
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_NVIDIA) {
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

    if (Injected) {
      DBG (" - custom properties injected\n");
    }
  }

  if (gSettings.FakeNVidia) {
    UINT32    FakeID = gSettings.FakeNVidia >> 16;

    MsgLog (" - With FakeID: %x:%x\n",gSettings.FakeNVidia & 0xFFFF, FakeID);
    DevpropAddValue (Device, "device-id", (UINT8 *)&FakeID, 4);
    FakeID = gSettings.FakeNVidia & 0xFFFF;
    DevpropAddValue (Device, "vendor-id", (UINT8 *)&FakeID, 4);
  }

  if (gSettings.InjectEDID && gSettings.CustomEDID) {
    DevpropAddValue (Device, "AAPL00,override-no-connect", gSettings.CustomEDID, 128);
  }

  if (
    (gDevicesNumber == 1) &&
    ((gSettings.BootDisplay >= 0) && (gSettings.BootDisplay < (INT8)PortsNum))
  ) {
    CHAR8   NKey[24];

    AsciiSPrint (NKey, 24, "@%d,AAPL,boot-display", gSettings.BootDisplay);
    DevpropAddValue (Device, NKey, (UINT8 *)&BootDisplay, 4);
    DBG (" - BootDisplay: %d\n", gSettings.BootDisplay);
  }

  DevpropAddValue (Device, "model", (UINT8 *)Model, (UINT32)AsciiStrLen (Model));
  DevpropAddValue (Device, "rom-revision", (UINT8 *)VersionStr, (UINT32)AsciiStrLen (VersionStr));

  if (gSettings.NVCAP[0] != 0) {
    DevpropAddValue (Device, "NVCAP", &gSettings.NVCAP[0], NVCAP_LEN);
    DBG (" - NVCAP: %a\n", Bytes2HexStr (gSettings.NVCAP, sizeof (gSettings.NVCAP)));
  }

  if (gSettings.UseIntelHDMI) {
    DevpropAddValue (Device, "hda-gfx", (UINT8 *)"onboard-2", 10);
  }

  if (VRam != 0) {
    DevpropAddValue (Device, "VRAM,totalsize", (UINT8 *)&VRam, 8);
  } else {
    DBG ("- [!] Warning: VideoRAM is not detected and not set\n");
  }

  DevpropAddNvidiaTemplate (Device, PortsNum);

  if (gSettings.NvidiaNoEFI) {
    DevpropAddValue (Device, "NVDA,noEFI", (UINT8 *)"true", 5);
  }

  //there are default or calculated properties, can be skipped
  if (gSettings.NoDefaultProperties) {
    //MsgLog (" - no default properties\n");
    goto Finish;
  }

  if (!gSettings.UseIntelHDMI) {
    DevpropAddValue (Device, "hda-gfx", (UINT8 *)"onboard-1", 10);
  }

  if (gSettings.BootDisplay < 0) {
    // if not set this is default property
    DevpropAddValue (Device, "@0,AAPL,boot-display", (UINT8 *)&BootDisplay, 4);
  }

  //DevpropAddValue (Device, "NVPM", DefNVPM, NVPM_LEN);

  if (gSettings.NVCAP[0] == 0) {
    DevpropAddValue (Device, "NVCAP", DefNVCAP, NVCAP_LEN);
    DBG (" - NVCAP: %a\n", Bytes2HexStr (DefNVCAP, sizeof (DefNVCAP)));
  }

  Finish:

  gDevicesNumber++;

  FreePool (VersionStr);

  if (Model != NULL) {
    FreePool (Model);
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  if (Rom != NULL) {
    FreePool (Rom);
  }

  return TRUE;
}
