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
#include <Library/Platform/Nvidia.h>

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

CHAR8   generic_name[128];

CONST CHAR8 *nvidia_compatible_0[]        = { "@0,compatible",  "NVDA,NVMac"     };
CONST CHAR8 *nvidia_compatible_1[]        = { "@1,compatible",  "NVDA,NVMac"     };
CONST CHAR8 *nvidia_device_type_0[]       = { "@0,device_type", "display"        };
CONST CHAR8 *nvidia_device_type_1[]       = { "@1,device_type", "display"        };
CONST CHAR8 *nvidia_device_type_parent[]  = { "device_type",    "NVDA,Parent"    };
CONST CHAR8 *nvidia_device_type_child[]   = { "device_type",    "NVDA,Child"     };
CONST CHAR8 *nvidia_name_0[]              = { "@0,name",        "NVDA,Display-A" };
CONST CHAR8 *nvidia_name_1[]              = { "@1,name",        "NVDA,Display-B" };
//CONST CHAR8 *nvidia_slot_name[]           = { "AAPL,slot-name", "Slot-1"        };

UINT8 default_NVCAP[]= {
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
  0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
  0x00, 0x00, 0x00, 0x00
};

#define NVCAP_LEN (sizeof(default_NVCAP) / sizeof(UINT8))

//UINT8 default_dcfg_0[]= {0x03, 0x01, 0x03, 0x00};
//UINT8 default_dcfg_1[]= {0xff, 0xff, 0x00, 0x01};
//
//#define DCFG0_LEN ( sizeof (default_dcfg_0) / sizeof (UINT8) )
//#define DCFG1_LEN ( sizeof (default_dcfg_1) / sizeof (UINT8) )
//
//UINT8 default_NVPM[]= {
//  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//  0x00, 0x00, 0x00, 0x00
//};
//
//#define NVPM_LEN ( sizeof (default_NVPM) / sizeof (UINT8) )

#define S_NVIDIAMODEL "Nvidia Graphics"

//STATIC nvidia_pci_info_t nvidia_card_generic[] = {
//  { 0x10DE0000, "Nvidia Graphics" }
//};

EFI_STATUS
ReadNvidiaPRAMIN (
  pci_dt_t    *nvda_dev,
  VOID        *rom,
  UINT16      arch
) {
  EFI_STATUS            Status;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  PCI_TYPE00            Pci;
  UINT32                vbios_vram = 0, old_bar0_pramin = 0;

  DBG ("read_nVidia_ROM\n");

  Status = gBS->OpenProtocol (
                  nvda_dev->DeviceHandle,
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

  if (arch >= 0x50) {
    DBG ("Using PRAMIN fixups\n");
    /*Status = */PciIo->Mem.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              NV_PDISPLAY_OFFSET + 0x9f04,///4,
                              1,
                              &vbios_vram
                            );

    vbios_vram = (vbios_vram & ~0xff) << 8;

    /*Status = */PciIo->Mem.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              NV_PMC_OFFSET + 0x1700,///4,
                              1,
                              &old_bar0_pramin
                            );

    if (vbios_vram == 0)
      vbios_vram = (old_bar0_pramin << 16) + 0xf0000;

    vbios_vram >>= 16;

    /*Status = */PciIo->Mem.Write (
                                PciIo,
                                EfiPciIoWidthUint32,
                                0,
                                NV_PMC_OFFSET + 0x1700,///4,
                                1,
                                &vbios_vram
                             );
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        NV_PRAMIN_OFFSET,
                        NVIDIA_ROM_SIZE,
                        rom
                      );

  if (arch >= 0x50) {
    /*Status = */PciIo->Mem.Write (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              NV_PMC_OFFSET + 0x1700,///4,
                              1,
                              &old_bar0_pramin
                            );
  }

  if (EFI_ERROR (Status)) {
    DBG ("read_nVidia_ROM failed\n");
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ReadNvidiaPROM (
  pci_dt_t    *nvda_dev,
  VOID        *rom
) {
  EFI_STATUS              Status;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  PCI_TYPE00              Pci;
  UINT32                  value;

  DBG ("PROM\n");
  Status = gBS->OpenProtocol (
                  nvda_dev->DeviceHandle,
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

  value = NV_PBUS_PCI_NV_20_ROM_SHADOW_DISABLED;
  /*Status = */PciIo->Mem.Write (
                            PciIo,
                            EfiPciIoWidthUint32,
                            0,
                            NV_PBUS_PCI_NV_20 * 4,
                            1,
                            &value
                          );

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        NV_PROM_OFFSET,
                        NVIDIA_ROM_SIZE,
                        rom
                      );

  value = NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED;
  /*Status = */PciIo->Mem.Write (
                            PciIo,
                            EfiPciIoWidthUint32,
                            0,
                            NV_PBUS_PCI_NV_20 * 4,
                            1,
                            &value
                          );

  if (EFI_ERROR (Status)) {
    DBG ("read_nVidia_ROM failed\n");
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
INT32
PatchNvidiaROM (
  UINT8   *rom
) {
  UINT8   num_outputs = 0, i = 0, dcbtable_version, headerlength = 0, numentries = 0,
          recordlength = 0, channel1 = 0, channel2 = 0, *dcbtable, *togroup;
  INT32   has_lvds = FALSE;
  UINT16  dcbptr;

  struct dcbentry {
    UINT8   type;
    UINT8   index;
    UINT8   *heads;
  } entries[MAX_NUM_DCB_ENTRIES];

  //DBG ("PatchNvidiaROM\n");
  if (!rom || ((rom[0] != 0x55) && (rom[1] != 0xaa))) {
    DBG ("FALSE ROM signature: 0x%02x%02x\n", rom[0], rom[1]);
    return PATCH_ROM_FAILED;
  }

  //  dcbptr = SwapBytes16 (read16 (rom, 0x36)); //double swap !!!
  dcbptr = *(UINT16 *)&rom[0x36];
  if (!dcbptr) {
    DBG ("no dcb table found\n");
    return PATCH_ROM_FAILED;
  }
  //  else
  //    DBG ("dcb table at offset 0x%04x\n", dcbptr);

  dcbtable = &rom[dcbptr];
  dcbtable_version  = dcbtable[0];

  if (dcbtable_version >= 0x20) {
    UINT32  sig;

    if (dcbtable_version >= 0x30) {
      headerlength = dcbtable[1];
      numentries   = dcbtable[2];
      recordlength = dcbtable[3];
      sig = *(UINT32 *)&dcbtable[6];
    } else {
      sig = *(UINT32 *)&dcbtable[4];
      headerlength = 8;
    }

    if (sig != 0x4edcbdcb) {
      DBG ("Bad display config block signature (0x%8x)\n", sig); //Azi: issue #48
      return PATCH_ROM_FAILED;
    }
  } else if (dcbtable_version >= 0x14) { /* some NV15/16, and NV11+ */
    CHAR8   sig[8]; // = { 0 };

    AsciiStrnCpyS (sig, ARRAY_SIZE (sig), (CHAR8 *)&dcbtable[-7], 7);
    sig[7] = 0;
    recordlength = 10;

    if (AsciiStrCmp (sig, "DEV_REC")) {
      DBG ("Bad Display Configuration Block signature (%a)\n", sig);
      return PATCH_ROM_FAILED;
    }
  } else {
    DBG ("ERROR: dcbtable_version is 0x%X\n", dcbtable_version);
    return PATCH_ROM_FAILED;
  }

  if (numentries >= MAX_NUM_DCB_ENTRIES) {
    numentries = MAX_NUM_DCB_ENTRIES;
  }

  for (i = 0; i < numentries; i++) {
    UINT32 connection = *(UINT32 *)&dcbtable[headerlength + recordlength * i];

    /* Should we allow discontinuous DCBs? Certainly DCB I2C tables can be discontinuous */
    if ((connection & 0x0000000f) == 0x0000000f) { /* end of records */
      continue;
    }

    if (connection == 0x00000000) { /* seen on an NV11 with DCB v1.5 */
      continue;
    }

    if ((connection & 0xf) == 0x6) { /* we skip type 6 as it doesnt appear on macbook nvcaps */
      continue;
    }

    entries[num_outputs].type = connection & 0xf;
    entries[num_outputs].index = num_outputs;
    entries[num_outputs++].heads = (UINT8 *)&(dcbtable[(headerlength + recordlength * i) + 1]);
  }

  for (i = 0; i < num_outputs; i++) {
    if (entries[i].type == 3) {
      has_lvds =TRUE;
      //DBG ("found LVDS\n");
      channel1 |= (0x1 << entries[i].index);
      entries[i].type = TYPE_GROUPED;
    }
  }

  // if we have a LVDS output, we group the rest to the second channel
  if (has_lvds) {
    for (i = 0; i < num_outputs; i++) {
      if (entries[i].type == TYPE_GROUPED) {
        continue;
      }

      channel2 |= (0x1 << entries[i].index);
      entries[i].type = TYPE_GROUPED;
    }
  } else {
    INT32 x;
    // we loop twice as we need to generate two channels
    for (x = 0; x <= 1; x++) {
      for (i=0; i<num_outputs; i++) {
        if (entries[i].type == TYPE_GROUPED) {
          continue;
        }
        // if type is TMDS, the prior output is ANALOG
        // we always group ANALOG and TMDS
        // if there is a TV output after TMDS, we group it to that channel as well
        if (i && entries[i].type == 0x2) {
          switch (x) {
            case 0:
              //DBG ("group channel 1\n");
              channel1 |= (0x1 << entries[i].index);
              entries[i].type = TYPE_GROUPED;

              if (entries[i - 1].type == 0x0) {
                channel1 |= (0x1 << entries[i - 1].index);
                entries[i - 1].type = TYPE_GROUPED;
              }

              // group TV as well if there is one
              if (((i + 1) < num_outputs) && (entries[i + 1].type == 0x1)) {
                //  DBG ("group tv1\n");
                channel1 |= (0x1 << entries[i + 1].index);
                entries[i + 1].type = TYPE_GROUPED;
              }
              break;

            case 1:
              //DBG ("group channel 2 : %d\n", i);
              channel2 |= ( 0x1 << entries[i].index);
              entries[i].type = TYPE_GROUPED;

              if (entries[i - 1].type == 0x0) {
                channel2 |= (0x1 << entries[i - 1].index);
                entries[i - 1].type = TYPE_GROUPED;
              }
              // group TV as well if there is one
              if (((i + 1) < num_outputs) && (entries[i + 1].type == 0x1)) {
                //  DBG ("group tv2\n");
                channel2 |= ( 0x1 << entries[i + 1].index);
                entries[i + 1].type = TYPE_GROUPED;
              }
              break;

            default:
              break;
          }

          break;
        }
      }
    }
  }

  // if we have left ungrouped outputs merge them to the empty channel
  togroup = &channel2; // = (channel1 ? (channel2 ? NULL : &channel2) : &channel1);

  for (i = 0; i < num_outputs; i++) {
    if (entries[i].type != TYPE_GROUPED) {
      //DBG ("%d not grouped\n", i);
      if (togroup) {
        *togroup |= (0x1 << entries[i].index);
      }

      entries[i].type = TYPE_GROUPED;
    }
  }

  if (channel1 > channel2) {
    UINT8   buff = channel1;

    channel1 = channel2;
    channel2 = buff;
  }

  default_NVCAP[6] = channel1;
  default_NVCAP[8] = channel2;

  // patching HEADS
  for (i = 0; i < num_outputs; i++) {
    if (channel1 & (1 << i)) {
      *entries[i].heads = 1;
    } else if (channel2 & (1 << i)) {
      *entries[i].heads = 2;
    }
  }

  return (has_lvds ? PATCH_ROM_SUCCESS_HAS_LVDS : PATCH_ROM_SUCCESS);
}

UINT64
MemDetect (
  UINT16      nvCardType,
  pci_dt_t    *nvda_dev
) {
  UINT64    vram_size = 0;

  if (nvCardType < NV_ARCH_50) {
    vram_size  = (UINT64)(REG32 (nvda_dev->regs, NV04_PFB_FIFO_DATA));
    vram_size &= NV10_PFB_FIFO_DATA_RAM_AMOUNT_MB_MASK;
  } else if (nvCardType < NV_ARCH_C0) {
    vram_size = (UINT64)(REG32 (nvda_dev->regs, NV04_PFB_FIFO_DATA));
    vram_size |= LShiftU64 (vram_size & 0xff, 32);
    vram_size &= 0xffffffff00ll;
  } else { // >= NV_ARCH_C0
    vram_size = LShiftU64 (REG32 (nvda_dev->regs, NVC0_MEM_CTRLR_RAM_AMOUNT), 20);
    //    vram_size *= REG32 (nvda_dev->regs, NVC0_MEM_CTRLR_COUNT);
    vram_size = MultU64x32 (vram_size, REG32 (nvda_dev->regs, NVC0_MEM_CTRLR_COUNT));
  }

  // Then, Workaround for 9600M GT, GT 210/420/430/440/525M/540M & GTX 560M
  switch (nvda_dev->device_id) {
    case 0x0647: // 9600M GT 0647
      vram_size = 512 * 1024 * 1024;
      break;

    case 0x0649:  // 9600M GT 0649
      // 10DE06491043202D 1GB VRAM
      if (((nvda_dev->subsys_id.subsys.vendor_id << 16) | nvda_dev->subsys_id.subsys.device_id) == 0x1043202D) {
        vram_size = 1024 * 1024 * 1024;
      }
      break;

    case 0x0A65: // GT 210
    case 0x0DE0: // GT 440
    case 0x0DE1: // GT 430
    case 0x0DE2: // GT 420
    case 0x0DEC: // GT 525M 0DEC
    case 0x0DF4: // GT 540M
    case 0x0DF5: // GT 525M 0DF5
      vram_size = 1024 * 1024 * 1024;
      break;

    case 0x1251: // GTX 560M
      vram_size = 1536 * 1024 * 1024;
      break;

    default:
      break;
  }

  DBG ("MemDetected %ld\n", vram_size);
  return vram_size;
}

VOID
DevpropAddNvidiaTemplate (
  DevPropDevice   *device,
  INTN            n_ports
) {
  INTN    pnum;
  CHAR8   nkey[24], nval[24];

  //DBG ("DevpropAddNvidiaTemplate\n");

  if (!device) {
    return;
  }

  for (pnum = 0; pnum < n_ports; pnum++) {
    AsciiSPrint (nkey, 24, "@%d,name", pnum);
    AsciiSPrint (nval, 24, "NVDA,Display-%c", (65 + pnum));
    DevpropAddValue (device, nkey, (UINT8 *)nval, 14);

    AsciiSPrint (nkey, 24, "@%d,compatible", pnum);
    DevpropAddValue (device, nkey, (UINT8 *)"NVDA,NVMac", 10);

    AsciiSPrint (nkey, 24, "@%d,device_type", pnum);
    DevpropAddValue (device, nkey, (UINT8 *)"display", 7);

    //if (!gSettings.NoDefaultProperties) {
    //  AsciiSPrint (nkey, 24, "@%d,display-cfg", pnum);
    //  if (pnum == 0) {
    //    DevpropAddValue (device, nkey, (gSettings.Dcfg[0] != 0) ? &gSettings.Dcfg[0] : default_dcfg_0, DCFG0_LEN);
    //  } else {
    //    DevpropAddValue (device, nkey, (gSettings.Dcfg[1] != 0) ? &gSettings.Dcfg[4] : default_dcfg_1, DCFG1_LEN);
    //  }
    //}
  }

  if (devices_number == 1) {
    DevpropAddValue (device, "device_type", (UINT8 *)"NVDA,Parent", 11);
  } else {
    DevpropAddValue (device, "device_type", (UINT8 *)"NVDA,Child", 10);
  }
}

BOOLEAN
SetupNvidiaDevprop (
  pci_dt_t    *nvda_dev
) {
  EFI_STATUS                Status = EFI_NOT_FOUND;
  DevPropDevice             *device = NULL;
  BOOLEAN                   load_vbios = gSettings.LoadVBios, Injected = FALSE;
  UINT8                     *rom = NULL, *buffer = NULL, connector_type_1[]= {0x00, 0x08, 0x00, 0x00};
  UINT16                    nvCardType = 0;
  UINT32                    bar[7], device_id, subsys_id,  boot_display = 1;
  UINT64                    videoRam = 0;
  CHAR16                    FileName[64], *RomPath = Basename (PoolPrint (DIR_ROM, L""));
  UINTN                     bufferLen, j, n_ports = 0;
  CONST INT32               MAX_BIOS_VERSION_LENGTH = 32;
  INT32                     i, version_start, crlf_count = 0, nvPatch = 0;
  option_rom_pci_header_t   *rom_pci_header;
  CHAR8                     *model = NULL, *devicepath, *s, *s1,
                            *version_str = (CHAR8 *)AllocateZeroPool (MAX_BIOS_VERSION_LENGTH);
  CARDLIST                  *nvcard;

  devicepath = GetPciDevPath (nvda_dev);
  bar[0] = PciConfigRead32 (nvda_dev, PCI_BASE_ADDRESS_0);
  nvda_dev->regs = (UINT8 *)(UINTN)(bar[0] & ~0x0f);
  device_id = ((nvda_dev->vendor_id << 16) | nvda_dev->device_id);
  subsys_id = ((nvda_dev->subsys_id.subsys.vendor_id << 16) | nvda_dev->subsys_id.subsys.device_id);

  // get card type
  nvCardType = (REG32 (nvda_dev->regs, 0) >> 20) & 0x1ff;

  // First check if any value exist in the plist
  nvcard = FindCardWithIds (device_id, subsys_id);

  if (nvcard) {
    if (nvcard->VideoRam > 0) {
      //VideoRam * 1024 * 1024 == VideoRam << 20
      //videoRam = LShiftU64 (nvcard->VideoRam, 20);
      videoRam = nvcard->VideoRam;
      model = (CHAR8 *)AllocateCopyPool (AsciiStrSize (nvcard->Model), nvcard->Model);
      n_ports = nvcard->VideoPorts;
      load_vbios = nvcard->LoadVBios;
    }
  } else {
    // Amount of VRAM in kilobytes (?) no, it is already in bytes!!!
    if (gSettings.VRAM != 0) {
      videoRam = gSettings.VRAM/* << 20 */;
    } else {
      videoRam = MemDetect (nvCardType, nvda_dev);
    }
  }

  if (load_vbios) {
    UnicodeSPrint (
      FileName, ARRAY_SIZE (FileName), L"%s\\10de_%04x_%04x_%04x.rom",
      RomPath, nvda_dev->device_id, nvda_dev->subsys_id.subsys.vendor_id, nvda_dev->subsys_id.subsys.device_id
    );

    if (FileExists (OEMDir, FileName)) {
      DBG (" - Found specific VBIOS ROM file (10de_%04x_%04x_%04x.rom)\n",
        nvda_dev->device_id, nvda_dev->subsys_id.subsys.vendor_id, nvda_dev->subsys_id.subsys.device_id
      );

      Status = LoadFile (OEMDir, FileName, &buffer, &bufferLen);
    } else {
      UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\10de_%04x.rom", RomPath, nvda_dev->device_id);
      if (FileExists (OEMDir, FileName)) {
        DBG (" - Found generic VBIOS ROM file (10de_%04x.rom)\n", nvda_dev->device_id);

        Status = LoadFile (OEMDir, FileName, &buffer, &bufferLen);
      }
    }

    if (EFI_ERROR (Status)) {
      FreePool (RomPath);
      RomPath = PoolPrint (DIR_ROM, DIR_CLOVER);

      UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\10de_%04x_%04x_%04x.rom",
        RomPath, nvda_dev->device_id, nvda_dev->subsys_id.subsys.vendor_id, nvda_dev->subsys_id.subsys.device_id
      );

      if (FileExists (SelfRootDir, FileName)) {
        DBG (" - Found specific VBIOS ROM file (10de_%04x_%04x_%04x.rom)\n",
          nvda_dev->device_id, nvda_dev->subsys_id.subsys.vendor_id, nvda_dev->subsys_id.subsys.device_id
        );

        Status = LoadFile (SelfRootDir, FileName, &buffer, &bufferLen);
      } else {
        UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\10de_%04x.rom", RomPath, nvda_dev->device_id);

        if (FileExists (SelfRootDir, FileName)) {
          DBG (" - Found generic VBIOS ROM file (10de_%04x.rom)\n", nvda_dev->device_id);

          Status = LoadFile (SelfRootDir, FileName, &buffer, &bufferLen);
        }
      }
    }
  }

  FreePool (RomPath);

  if (EFI_ERROR (Status)) {
    rom = AllocateZeroPool (NVIDIA_ROM_SIZE + 1);
    // PRAMIN first
    ReadNvidiaPRAMIN (nvda_dev, rom, nvCardType);

    //DBG ("%x%x\n", rom[0], rom[1]);
    rom_pci_header = NULL;

    if ((rom[0] != 0x55) || (rom[1] != 0xaa)) {
      ReadNvidiaPROM (nvda_dev, rom);

      if ((rom[0] != 0x55) || (rom[1] != 0xaa)) {
        DBG (" - ERROR: Unable to locate nVidia Video BIOS\n");
        FreePool (rom);
        rom = NULL;
      }
    }
  } else if (buffer) {
    if ((buffer[0] != 0x55) && (buffer[1] != 0xaa)) {
      i = 0;

      while (i < bufferLen) {
        //DBG ("%x%x\n", buffer[i], buffer[i + 1]);
        if ((buffer[i] == 0x55) && (buffer[i + 1] == 0xaa)) {
          DBG (" - header found at: %d\n", i);
          bufferLen -= i;
          rom = AllocateZeroPool (bufferLen);

          for (j = 0; j < bufferLen; j++) {
            rom[j] = buffer[i + j];
          }

          break;
        }

        i += 512;
      }
    }

    if (!rom) {
      rom = buffer;
    }

    DBG (" - using loaded ROM image\n");
  }

  if (!rom || !buffer) {
    DBG (" - there are no ROM loaded / no VBIOS read from hardware\n");
  }

  if (rom) {
    if ((nvPatch = PatchNvidiaROM (rom)) == PATCH_ROM_FAILED) {
      DBG (" - ERROR: ROM Patching Failed!\n");
    }

    rom_pci_header = (option_rom_pci_header_t *)(rom + *(UINT16 *)&rom[24]);

    // check for 'PCIR' sig
    if (rom_pci_header->signature != 0x52494350) {
      DBG (" - incorrect PCI ROM signature: 0x%x\n", rom_pci_header->signature);
    }

    // get bios version

    // only search the first 384 bytes
    for (i = 0; i < 0x180; i++) {
      if ((rom[i] == 0x0D) && (rom[i + 1] == 0x0A)) {
        crlf_count++;
        // second 0x0D0A was found, extract bios version
        if (crlf_count == 2) {
          if (rom[i - 1] == 0x20) {
            i--; // strip last " "
          }

          for (version_start = i; version_start > (i - MAX_BIOS_VERSION_LENGTH); version_start--) {
            // find start
            if (rom[version_start] == 0x00) {
              version_start++;

              // strip "Version "
              if (AsciiStrnCmp ((CONST CHAR8 *)rom + version_start, "Version ", 8) == 0) {
                version_start += 8;
              }

              s = (CHAR8 *)(rom + version_start);
              s1 = version_str;

              while ((*s > ' ') && (*s < 'z') && ((INTN)(s1 - version_str) < MAX_BIOS_VERSION_LENGTH)) {
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
    AsciiSPrint (version_str, sizeof (version_str), "1.0");
  }

  DBG (" - Version: %a\n", version_str);

  if (!model) {
    model = (CHAR8 *)AllocateCopyPool (AsciiStrSize (S_NVIDIAMODEL), S_NVIDIAMODEL);
  }

  MsgLog (" - %a | %dMB NV%02x [%04x:%04x] | %a => device #%d\n",
      model, (UINT32)(RShiftU64 (videoRam, 20)),
      nvCardType, nvda_dev->vendor_id, nvda_dev->device_id,
      devicepath, devices_number);

  if (!string) {
    string = DevpropCreateString ();
  }

  device = DevpropAddDevicePci (string, nvda_dev);

  DBG (" - VideoPorts:");

  if (n_ports > 0) {
    DBG (" user defined (GUI-menu): %d\n", n_ports);
  } else if (gSettings.VideoPorts > 0) {
    n_ports = gSettings.VideoPorts;
    DBG (" user defined from config.plist: %d\n", n_ports);
  } else {
    n_ports = 2; //default
    DBG (" undefined, default to: %d\n", n_ports);
  }

  //There are custom properties, injected if set by user
  if (gSettings.NvidiaSingle && (devices_number >= 1)) {
    DBG (" - NvidiaSingle: skip injecting other then first card\n");
    goto done;
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_NVIDIA) {
        continue;
      }

      Injected = TRUE;

      DevpropAddValue (
        device,
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
    DevpropAddValue (device, "device-id", (UINT8 *)&FakeID, 4);
    FakeID = gSettings.FakeNVidia & 0xFFFF;
    DevpropAddValue (device, "vendor-id", (UINT8 *)&FakeID, 4);
  }

  if (gSettings.InjectEDID && gSettings.CustomEDID) {
    DevpropAddValue (device, "AAPL00,override-no-connect", gSettings.CustomEDID, 128);
  }

  if (
    (devices_number == 1) &&
    ((gSettings.BootDisplay >= 0) && (gSettings.BootDisplay < (INT8)n_ports))
  ) {
    CHAR8   nkey[24];

    AsciiSPrint (nkey, 24, "@%d,AAPL,boot-display", gSettings.BootDisplay);
    DevpropAddValue (device, nkey, (UINT8 *)&boot_display, 4);
    DBG (" - BootDisplay: %d\n", gSettings.BootDisplay);
  }

  DevpropAddValue (device, "model", (UINT8 *)model, (UINT32)AsciiStrLen (model));
  DevpropAddValue (device, "rom-revision", (UINT8 *)version_str, (UINT32)AsciiStrLen (version_str));

  if (gSettings.NVCAP[0] != 0) {
    DevpropAddValue (device, "NVCAP", &gSettings.NVCAP[0], NVCAP_LEN);
    DBG (" - NVCAP: %a\n", Bytes2HexStr (gSettings.NVCAP, sizeof (gSettings.NVCAP)));
  }

  if (gSettings.UseIntelHDMI) {
    DevpropAddValue (device, "hda-gfx", (UINT8 *)"onboard-2", 10);
  }

  if (videoRam != 0) {
    DevpropAddValue (device, "VRAM,totalsize", (UINT8 *)&videoRam, 8);
  } else {
    DBG ("- [!] Warning: VideoRAM is not detected and not set\n");
  }

  DevpropAddNvidiaTemplate (device, n_ports);

  //there are default or calculated properties, can be skipped
  if (gSettings.NoDefaultProperties) {
    //MsgLog (" - no default properties\n");
    goto done;
  }

  if (!gSettings.UseIntelHDMI) {
    DevpropAddValue (device, "hda-gfx", (UINT8 *)"onboard-1", 10);
  }

  if (gSettings.BootDisplay < 0) {
    // if not set this is default property
    DevpropAddValue (device, "@0,AAPL,boot-display", (UINT8 *)&boot_display, 4);
  }

  if (nvPatch == PATCH_ROM_SUCCESS_HAS_LVDS) {
    UINT8 built_in = 0x01;
    DevpropAddValue (device, "@0,built-in", &built_in, 1);
    // HDMI is not LVDS
    DevpropAddValue (device, "@1,connector-type", connector_type_1, 4);
  } else {
    DevpropAddValue (device, "@0,connector-type", connector_type_1, 4);
  }

  //DevpropAddValue (device, "NVPM", default_NVPM, NVPM_LEN);

  if (gSettings.NVCAP[0] == 0) {
    DevpropAddValue (device, "NVCAP", default_NVCAP, NVCAP_LEN);
    DBG (" - NVCAP: %a\n", Bytes2HexStr (default_NVCAP, sizeof (default_NVCAP)));
  }

done:

  devices_number++;
  FreePool (version_str);

  if (model != NULL) {
    FreePool (model);
  }

  if (buffer != NULL) {
    FreePool (buffer);
  }

  if (rom != NULL) {
    FreePool (rom);
  }

  return TRUE;
}
