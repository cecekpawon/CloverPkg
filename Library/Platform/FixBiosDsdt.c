/*
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 */
//totally rebuilt by Slice, 2012-2013
// NForce additions by Oscar09, 2013

#include <Library/Platform/AmlGenerator.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_FIX_DSDT
#define DEBUG_FIX_DSDT -1
#endif
#else
#ifdef DEBUG_FIX_DSDT
#undef DEBUG_FIX_DSDT
#endif
#define DEBUG_FIX_DSDT DEBUG_ALL
#endif

#define DBG(...) DebugLog(DEBUG_FIX_DSDT, __VA_ARGS__)

#define S_NETMODEL "Generic Ethernet"

OPER_REGION *gRegions;

// 0=>Display  1=>network  2=>firewire 3=>LPCB 4=>HDAAudio 5=>RTC 6=>TMR 7=>SBUS 8=>PIC 9=>Airport 10=>XHCI 11=>HDMI
CHAR8*  device_name[12];
//CHAR8*  UsbName[10];
//CHAR8*  Netmodel;

//static
CHAR8 dataBuiltin[] = {0x00};
//static
CHAR8 dataBuiltin1[] = {0x01};

BOOLEAN     HDAFIX = TRUE;
BOOLEAN     GFXHDAFIX = TRUE;

BOOLEAN     DisplayName1;
BOOLEAN     DisplayName2;

BOOLEAN     NetworkName;
BOOLEAN     ArptBCM;
BOOLEAN     ArptAtheros;
UINT16      ArptDID;

UINT32      IMEIADR1;
UINT32      IMEIADR2;

// for read computer data

//UINT32    HDAcodecId=0;
UINT32      HDAlayoutId=0;
UINT32      HDAADR1;
UINT32      HDMIADR1;
UINT32      HDMIADR2;

UINT32      DisplayADR1[4];
UINT32      DisplayADR2[4];
UINT32      DisplayVendor[4];
UINT16      DisplayID[4];
UINT32      DisplaySubID[4];
//UINT32    GfxcodecId[2] = {0, 1};
UINT32      GfxlayoutId[2] = {1, 12};
pci_dt_t    Displaydevice[2];

//UINT32 IMEIADR1;
//UINT32 IMEIADR2;

UINT32      NetworkADR1;
UINT32      NetworkADR2;
BOOLEAN     ArptName;
UINT32      ArptADR1;
UINT32      ArptADR2;

CHAR8 ClassFix[] =  { 0x00, 0x00, 0x03, 0x00 };

UINT8 dtgp[] = // Method (DTGP, 5, NotSerialized) ......
{
   0x14, 0x3F, 0x44, 0x54, 0x47, 0x50, 0x05, 0xA0,
   0x30, 0x93, 0x68, 0x11, 0x13, 0x0A, 0x10, 0xC6,
   0xB7, 0xB5, 0xA0, 0x18, 0x13, 0x1C, 0x44, 0xB0,
   0xC9, 0xFE, 0x69, 0x5E, 0xAF, 0x94, 0x9B, 0xA0,
   0x18, 0x93, 0x69, 0x01, 0xA0, 0x0C, 0x93, 0x6A,
   0x00, 0x70, 0x11, 0x03, 0x01, 0x03, 0x6C, 0xA4,
   0x01, 0xA0, 0x06, 0x93, 0x6A, 0x01, 0xA4, 0x01,
   0x70, 0x11, 0x03, 0x01, 0x00, 0x6C, 0xA4, 0x00
};

CHAR8 dtgp_1[] = {  // DTGP (Arg0, Arg1, Arg2, Arg3, RefOf (Local0))
                    // Return (Local0)
   0x44, 0x54, 0x47, 0x50, 0x68, 0x69, 0x6A, 0x6B,
   0x71, 0x60, 0xA4, 0x60
};

CHAR8 pnlf[] = {
  0x5B, 0x82, 0x2D, 0x50, 0x4E, 0x4C, 0x46,                         //Device (PNLF)
  0x08, 0x5F, 0x48, 0x49, 0x44, 0x0C, 0x06, 0x10, 0x00, 0x02,       //  Name (_HID, EisaId ("APP0002"))
  0x08, 0x5F, 0x43, 0x49, 0x44,                                     //  Name (_CID,
  0x0D, 0x62, 0x61, 0x63, 0x6B, 0x6C, 0x69, 0x67, 0x68, 0x74, 0x00, //              "backlight")
  0x08, 0x5F, 0x55, 0x49, 0x44, 0x0A, 0x0A,                         //  Name (_UID, 0x0A)
  0x08, 0x5F, 0x53, 0x54, 0x41, 0x0A, 0x0B                          //  Name (_STA, 0x0B)
};

CHAR8 app2[] = { //Name (_HID, EisaId("APP0002"))
  0x08, 0x5F, 0x48, 0x49, 0x44, 0x0C, 0x06, 0x10, 0x00, 0x02
};

BOOLEAN
CmpNum (
  UINT8     *dsdt,
  INT32     i,
  BOOLEAN   Sure
) {
  return ((Sure && ((dsdt[i-1] == 0x0A) ||
                    (dsdt[i-2] == 0x0B) ||
                    (dsdt[i-4] == 0x0C))) ||
          (!Sure && (((dsdt[i-1] >= 0x0A) && (dsdt[i-1] <= 0x0C)) ||
                     ((dsdt[i-2] == 0x0B) || (dsdt[i-2] == 0x0C)) ||
                     (dsdt[i-4] == 0x0C))));
}

VOID
GetPciADR (
  IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  OUT UINT32                    *Addr1,
  OUT UINT32                    *Addr2,
  OUT UINT32                    *Addr3
) {
  PCI_DEVICE_PATH *PciNode;
  UINTN                       PciNodeCount;
  EFI_DEVICE_PATH_PROTOCOL    *TmpDevicePath = DuplicateDevicePath(DevicePath);

  // default to 0
  if (Addr1 != NULL) {
    *Addr1 = 0;
  }

  if (Addr2 != NULL) {
    *Addr2 = 0xFFFE; //some code we will consider as "non-exists" b/c 0 is meaningful value as well as 0xFFFF
  }

  if (Addr3 != NULL) {
    *Addr3 = 0xFFFE;
  }

  if (!TmpDevicePath) {
    return;
  }

  // sanity check - expecting ACPI path for PciRoot
  if ((TmpDevicePath->Type != ACPI_DEVICE_PATH) && (TmpDevicePath->SubType != ACPI_DP)) {
    return;
  }

  PciNodeCount = 0;
  while (TmpDevicePath && !IsDevicePathEndType(TmpDevicePath)) {
    if ((TmpDevicePath->Type == HARDWARE_DEVICE_PATH) && (TmpDevicePath->SubType == HW_PCI_DP)) {
      PciNodeCount++;
      PciNode = (PCI_DEVICE_PATH *)TmpDevicePath;

      if (PciNodeCount == 1 && Addr1 != NULL) {
        *Addr1 = (PciNode->Device << 16) | PciNode->Function;
      } else if (PciNodeCount == 2 && Addr2 != NULL) {
        *Addr2 = (PciNode->Device << 16) | PciNode->Function;
      } else if (PciNodeCount == 3 && Addr3 != NULL) {
        *Addr3 = (PciNode->Device << 16) | PciNode->Function;
      } else {
        break;
      }
    }

    TmpDevicePath = NextDevicePathNode(TmpDevicePath);
  }

  return;
}

VOID
CheckHardware() {
  EFI_STATUS                  Status;
  EFI_HANDLE                  *HandleBuffer = NULL, Handle;
  EFI_PCI_IO_PROTOCOL         *PciIo;
  PCI_TYPE00                  Pci;
  UINTN                       HandleCount = 0, HandleIndex, Segment,
                              Bus, Device, Function, display = 0;
  pci_dt_t                    PCIdevice;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath = NULL;

   //usb=0;
  // Scan PCI handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );

  if (!EFI_ERROR (Status)) {
    //DBG("PciIo handles count=%d\n", HandleCount);
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
      Handle = HandleBuffer[HandleIndex];
      Status = gBS->HandleProtocol (
                      Handle,
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&PciIo
                    );

      if (!EFI_ERROR (Status)) {
        UINT32    deviceid;

        /* Read PCI BUS */
        PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        Status = PciIo->Pci.Read (
                                  PciIo,
                                  EfiPciIoWidthUint32,
                                  0,
                                  sizeof (Pci) / sizeof (UINT32),
                                  &Pci
                                  );

        deviceid = Pci.Hdr.DeviceId | (Pci.Hdr.VendorId << 16);

        // add for auto patch dsdt get DSDT Device _ADR
        PCIdevice.DeviceHandle = Handle;
        DevicePath = DevicePathFromHandle (Handle);
        if (DevicePath) {
          //DBG("Device patch = %s \n", DevicePathToStr(DevicePath));

          //Display ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_DISPLAY) &&
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_VGA)
          ) {

            #if DEBUG_FIX
            UINT32    dadr1, dadr2;
            #endif

            //PCI_IO_DEVICE *PciIoDevice;

            GetPciADR(DevicePath, &DisplayADR1[display], &DisplayADR2[display], NULL);
            DBG("VideoCard devID=0x%x\n", ((Pci.Hdr.DeviceId << 16) | Pci.Hdr.VendorId));

            #if DEBUG_FIX
            dadr1 = DisplayADR1[display];
            dadr2 = DisplayADR2[display];
            DBG(" - DisplayADR1[%02d]: 0x%x, DisplayADR2[%02d] = 0x%x\n", display, dadr1, display, dadr2);
            #endif

                 //dadr2 = dadr1; //to avoid warning "unused variable" :(
            DisplayVendor[display] = Pci.Hdr.VendorId;
            DisplayID[display] = Pci.Hdr.DeviceId;
            DisplaySubID[display] = (Pci.Device.SubsystemID << 16) | (Pci.Device.SubsystemVendorID << 0);
            // for get display data
            Displaydevice[display].DeviceHandle = HandleBuffer[HandleIndex];
            Displaydevice[display].dev.addr = (UINT32)PCIADDR(Bus, Device, Function);
            Displaydevice[display].vendor_id = Pci.Hdr.VendorId;
            Displaydevice[display].device_id = Pci.Hdr.DeviceId;
            Displaydevice[display].revision = Pci.Hdr.RevisionID;
            Displaydevice[display].subclass = Pci.Hdr.ClassCode[0];
            Displaydevice[display].class_id = *((UINT16*)(Pci.Hdr.ClassCode+1));
            Displaydevice[display].subsys_id.subsys.vendor_id = Pci.Device.SubsystemVendorID;
            Displaydevice[display].subsys_id.subsys.device_id = Pci.Device.SubsystemID;

            //
            // Detect if PCI Express Device
            //
            //
            // Go through the Capability list
            //unused
            /*
            PciIoDevice = PCI_IO_DEVICE_FROM_PCI_IO_THIS (PciIo);
            if (PciIoDevice->IsPciExp) {
              if (display==0)
                Display1PCIE = TRUE;
              else
                Display2PCIE = TRUE;
            }
            DBG("Display %d is %aPCIE\n", display, (PciIoDevice->IsPciExp) ? "" :" not");
            */

            display++;
          }

          //Network ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_ETHERNET)
          ) {
            GetPciADR(DevicePath, &NetworkADR1, &NetworkADR2, NULL);
            //DBG("NetworkADR1 = 0x%x, NetworkADR2 = 0x%x\n", NetworkADR1, NetworkADR2);
            //Netmodel = get_net_model(deviceid);
          }

          //Network WiFi ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_OTHER)
          ) {
            GetPciADR(DevicePath, &ArptADR1, &ArptADR2, NULL);
            //DBG("ArptADR1 = 0x%x, ArptADR2 = 0x%x\n", ArptADR1, ArptADR2);
            //Netmodel = get_arpt_model(deviceid);
            ArptBCM = (Pci.Hdr.VendorId == 0x14e4);

            if (ArptBCM) {
              DBG("Found Airport BCM at 0x%x, 0x%x\n", ArptADR1, ArptADR2);
            }

            ArptAtheros = (Pci.Hdr.VendorId == 0x168c);
            ArptDID = Pci.Hdr.DeviceId;

            if (ArptAtheros) {
              DBG("Found Airport Atheros at 0x%x, 0x%x, DeviceID=0x%04x\n", ArptADR1, ArptADR2, ArptDID);
            }
          }

          //IMEI ADR
          if ((Pci.Hdr.ClassCode[2] == PCI_CLASS_SCC) &&
              (Pci.Hdr.ClassCode[1] == PCI_SUBCLASS_SCC_OTHER)) {
            GetPciADR(DevicePath, &IMEIADR1, &IMEIADR2, NULL);
          }

          // HDA and HDMI Audio
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_MEDIA) &&
            ((Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_HDA) ||
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_AUDIO))
          ) {
            UINT32    /*codecId = 0,*/ layoutId = 0;
            if (!IsHDMIAudio(Handle)) {
              if (gSettings.HDALayoutId > 0) {
                // layoutId is specified - use it
                layoutId = (UINT32)gSettings.HDALayoutId;
                DBG("Audio HDA (addr:0x%x) setting specified layout-id=%d\n", HDAADR1, layoutId);
              }

              HDAFIX = TRUE;
              //HDAcodecId = codecId;
              HDAlayoutId = layoutId;
            } else { //HDMI
              GetPciADR(DevicePath, &HDMIADR1, &HDMIADR2, NULL);
              GFXHDAFIX = TRUE;
            }
          }
        }
        // detected finish
      }
    }
  }
}

UINT8 slash[] = {0x5c, 0};

VOID
InsertScore (
  UINT8     *dsdt,
  UINT32    off2,
  INTN      root
) {
  UINT8     NumNames = 0;
  UINT32    ind = 0, i;
  CHAR8     buf[31];

  if (dsdt[off2 + root] == 0x2E) {
    NumNames = 2;
    off2 += (UINT32)(root + 1);
  } else if (dsdt[off2 + root] == 0x2F) {
    NumNames = dsdt[off2 + root + 1];
    off2 += (UINT32)(root + 2);
  } else if (dsdt[off2 + root] != 0x00) {
    NumNames = 1;
    off2 += (UINT32)root;
  }

  if (NumNames > 4) {
    DBG(" strange NumNames=%d\n", NumNames);
    NumNames = 1;
  }

  NumNames *= 4;
  CopyMem(buf + ind, dsdt + off2, NumNames);
  ind += NumNames;
  // apianti - This generates a memcpy call
  /*
  for (i = 0; i < NumNames; i++) {
    buf[ind++] = dsdt[off2 + i];
  }
  */

  i = 0;
  while (i < 127) {
    buf[ind++] = acpi_cpu_score[i];
    if (acpi_cpu_score[i] == 0) {
      break;
    }

    i++;
  }

  CopyMem(acpi_cpu_score, buf, ind);
  acpi_cpu_score[ind] = 0;
}

VOID
FindCPU (
  UINT8     *dsdt,
  UINT32    length
) {
  UINT32      i, k, size, SBSIZE = 0, SBADR = 0,
              off2, j1;
  BOOLEAN     SBFound = FALSE;

  if (acpi_cpu_score) {
    FreePool(acpi_cpu_score);
  }

  acpi_cpu_score = AllocateZeroPool(128);
  acpi_cpu_count = 0;

  for (i = 0; i < length - 20; i++) {
    if (dsdt[i] == 0x5B && dsdt[i + 1] == 0x83) { // ProcessorOP
      UINT32    j, offset = i + 3 + (dsdt[i + 2] >> 6); // name
      BOOLEAN   add_name = TRUE;

      if (acpi_cpu_count == 0) {         //only first time in the cycle
        CHAR8   c1 = dsdt[offset + 1];

        // I want to determine a scope of PR
        //1. if name begin with \\ this is with score
        //2. else find outer device or scope until \\ is found
        //3. add new name everytime is found
        DBG("first CPU found at %x offset %x\n", i, offset);

        if (dsdt[offset] == '\\') {
          // "\_PR.CPU0"
          j = 1;

          if (c1 == 0x2E) {
            j = 2;
          } else if (c1 == 0x2F) {
            c1 = dsdt[offset + 2];
            j = 2 + (c1 - 2) * 4;
          }

          CopyMem(acpi_cpu_score, dsdt + offset + j, 4);
          DBG("slash found\n");
        } else {
          //--------
          j = i - 1; //usually adr = &5B - 1 = sizefield - 3
          while (j > 0x24) {  //find devices that previous to adr
            //check device
            k = j + 2;

            if (
              (dsdt[j] == 0x5B) &&
              (dsdt[j + 1] == 0x82) &&
              !CmpNum(dsdt, j, TRUE)
            ) { //device candidate
              DBG("device candidate at %x\n", j);

              size = AcpiGetSize(dsdt, k);

              if (size) {
                if (k + size > i + 3) {  //Yes - it is outer
                  off2 = j + 3 + (dsdt[j + 2] >> 6);

                  if (dsdt[off2] == '\\') {
                    // "\_SB.SCL0"
                    InsertScore(dsdt, off2, 1);
                    DBG("acpi_cpu_score calculated as %a\n", acpi_cpu_score);
                    break;
                  } else {
                    InsertScore(dsdt, off2, 0);
                    DBG("device inserted in acpi_cpu_score %a\n", acpi_cpu_score);
                  }
                }  //else not an outer device
              } //else wrong size field - not a device
            } //else not a device

            // check scope
            // a problem 45 43 4F 4E 08   10 84 10 05 5F 53 42 5F
            SBSIZE = 0;

            if (
              (dsdt[j] == '_' && dsdt[j + 1] == 'S' && dsdt[j + 2] == 'B' && dsdt[j + 3] == '_') ||
              (dsdt[j] == '_' && dsdt[j + 1] == 'P' && dsdt[j + 2] == 'R' && dsdt[j + 3] == '_')
            ) {
              DBG("score candidate at %x\n", j);

              for (j1=0; j1 < 10; j1++) {
                if (dsdt[j - j1] != 0x10) {
                  continue;
                }

                if (!CmpNum(dsdt, j - j1, TRUE)) {
                  SBADR = j - j1 + 1;
                  SBSIZE = AcpiGetSize(dsdt, SBADR);

                  //     DBG("found Scope(\\_SB) address = 0x%08x size = 0x%08x\n", SBADR, SBSIZE);
                  if ((SBSIZE != 0) && (SBSIZE < length)) {  //if zero or too large then search more
                    //if found
                    k = SBADR - 6;

                    if ((SBADR + SBSIZE) > i + 4) {  //Yes - it is outer
                      SBFound = TRUE;
                      break;  //SB found
                    }  //else not an outer scope
                  }
                }
              }
            } //else not a scope

            if (SBFound) {
              InsertScore(dsdt, j, 0);
              DBG("score inserted in acpi_cpu_score %a\n", acpi_cpu_score);
              break;
            }
            j = k - 3;    //if found then search again from found
          } //while j
          //--------
        }
      }


      for (j = 0; j < 4; j++) {
        CHAR8   c = dsdt[offset + j],
                c1 = dsdt[offset + j + 1];

        if(c == '\\') {
          offset += 5;
          if (c1 == 0x2E) {
            offset++;
          } else if (c1 == 0x2F) {
            c1 = dsdt[offset + j + 2];
            offset += 2 + (c1 - 2) * 4;
          }
          c = dsdt[offset + j];
        }

        if (!(IS_UPPER(c) || IS_DIGIT(c) || c == '_')) {
          add_name = FALSE;
          DBG("Invalid character found in ProcessorOP 0x%x!\n", c);
          break;
        }
      }

      if (add_name) {
        acpi_cpu_name[acpi_cpu_count] = AllocateZeroPool(5);
        CopyMem(acpi_cpu_name[acpi_cpu_count], dsdt+offset, 4);
        i = offset + 5;

                //if (acpi_cpu_count == 0)
                //    acpi_cpu_p_blk = dsdt[i] | (dsdt[i+1] << 8);

        if (acpi_cpu_count == 0) {
            DBG("Found ACPI CPU: %a ", acpi_cpu_name[acpi_cpu_count]);
        } else {
            DBG("| %a ", acpi_cpu_name[acpi_cpu_count]);
        }

        if (++acpi_cpu_count == 32) {
          break;
        }
      }
    }
  }

  DBG(", within the score: %a\n", acpi_cpu_score);

  if (!acpi_cpu_count) {
    for (i = 0; i < 15; i++) {
      acpi_cpu_name[i] = AllocateZeroPool(5);
      AsciiSPrint(acpi_cpu_name[i], 5, "CPU%1x", i);
    }
  }

  return;
}


//                start => move data start address
//                offset => data move how many byte
//                len => initial length of the buffer
// return final length of the buffer
// we suppose that buffer allocation is more then len+offset
UINT32
MoveData (
  UINT32    start,
  UINT8     *buffer,
  UINT32    len,
  INT32     offset
) {
  UINT32    i;

  if (offset < 0) {
    for (i = start; i < len+offset; i++) {
      buffer[i] = buffer[i-offset];
    }
  } else  if (offset > 0) { // data move to back
    for (i = len-1; i >= start; i--) {
      buffer[i+offset] = buffer[i];
    }
  }

  return len + offset;
}

UINT32
AcpiGetSize (
  UINT8     *Buffer,
  UINT32    adr
) {
  UINT32    temp;

  temp = Buffer[adr] & 0xF0; //keep bits 0x30 to check if this is valid size field

  if(temp <= 0x30)  {       // 0
    temp = Buffer[adr];
  } else if(temp == 0x40) {   // 4
    temp =  (Buffer[adr]   - 0x40)  << 0|
             Buffer[adr+1]          << 4;
  } else if(temp == 0x80) {   // 8
    temp = (Buffer[adr]   - 0x80)  <<  0|
            Buffer[adr+1]          <<  4|
            Buffer[adr+2]          << 12;
  } else if(temp == 0xC0) {   // C
    temp = (Buffer[adr]   - 0xC0) <<  0|
            Buffer[adr+1]         <<  4|
            Buffer[adr+2]         << 12|
            Buffer[adr+3]         << 20;
  } else {
  //  DBG("wrong pointer to size field at %x\n", adr);
    return 0;
  }

  return temp;
}
/*
//return 1 if new size is two bytes else 0
UINT32
write_offset (
  UINT32    adr,
  UINT8     *buffer,
  UINT32    len,
  INT32     offset
) {
  UINT32 i, shift = 0, size = offset + 1;

  if (size >= 0x3F) {
    for (i=len; i>adr; i--) {
      buffer[i+1] = buffer[i];
    }

    shift = 1;
    size += 1;
  }

  aml_write_size(size, (CHAR8 *)buffer, adr);
  return shift;
}


  adr - a place to write new size. Size of some object.
  buffer - the binary aml codes array
  len - its length
  sizeoffset - how much the object increased in size
  return address shift from original  +/- n from outers
 When we increase the object size there is a chance that new size field +1
 so out devices should also be corrected +1 and this may lead to new shift
*/
//Slice - I excluded check (oldsize <= 0x0fffff && size > 0x0fffff)
//because I think size of DSDT will never be 1Mb

INT32
WriteSize (
  UINT32  adr,
  UINT8   *buffer,
  UINT32  len,
  INT32   sizeoffset
) {
  UINT32  size, oldsize;
  INT32   offset = 0;

  oldsize = AcpiGetSize(buffer, adr);
  if (!oldsize) {
    return 0; //wrong address, will not write here
  }

  size = oldsize + sizeoffset;
  // data move to back
  if ((oldsize <= 0x3f) && (size > 0x0fff)) {
    offset = 2;
  } else if (((oldsize <= 0x3f) && (size > 0x3f)) || ((oldsize <= 0x0fff) && (size > 0x0fff))) {
    offset = 1;
  } else if (((size <= 0x3f) && (oldsize > 0x3f)) || ((size <= 0x0fff) && (oldsize > 0x0fff))) {
    // data move to front
    offset = -1;
  } else if ((oldsize > 0x0fff) && (size <= 0x3f)) {
    offset = -2;
  }

  len = MoveData(adr, buffer, len, offset);
  size += offset;
  aml_write_size(size, (CHAR8 *)buffer, adr); //reuse existing codes

  return offset;
}

INT32
FindName (
  UINT8     *dsdt,
  INT32     len,
  CHAR8     *name
) {
  INT32   i;

  for (i = 0; i < len - 5; i++) {
    if (
      (dsdt[i] == 0x08) && (dsdt[i+1] == name[0]) &&
      (dsdt[i+2] == name[1]) && (dsdt[i+3] == name[2]) &&
      (dsdt[i+4] == name[3])
    ) {
      return i+1;
    }
  }

  return 0;
}

BOOLEAN
GetName (
  UINT8       *dsdt,
  INT32       adr,
  CHAR8       *name,
  OUT INTN    *shift
) {
  INT32 i, j = (dsdt[adr] == 0x5C)?1:0; //now we accept \NAME

  if (!name) {
    return FALSE;
  }

  for (i = adr + j; i < adr + j + 4; i++) {
    if (
      (dsdt[i] < 0x2F) ||
      ((dsdt[i] > 0x39) && (dsdt[i] < 0x41)) ||
      ((dsdt[i] > 0x5A) && (dsdt[i] != 0x5F))
    ) {
      return FALSE;
    }
    name[i - adr - j] = dsdt[i];
  }

  name[4] = 0;
  if (shift) {
    *shift = j;
  }

  return TRUE;
}

// if (CmpAdr(dsdt, j, NetworkADR1))
// Name (_ADR, 0x90000)
BOOLEAN
CmpAdr (
  UINT8     *dsdt,
  UINT32    j,
  UINT32    PciAdr
) {
  // Name (_ADR, 0x001f0001)
  return (BOOLEAN)
  (
    (dsdt[j + 4] == 0x08) &&
    (dsdt[j + 5] == 0x5F) &&
    (dsdt[j + 6] == 0x41) &&
    (dsdt[j + 7] == 0x44) &&
    (dsdt[j + 8] == 0x52) &&
    (
      //--------------------
      (
      (dsdt[j +  9] == 0x0C) &&
      (dsdt[j + 10] == ((PciAdr & 0x000000ff) >> 0)) &&
      (dsdt[j + 11] == ((PciAdr & 0x0000ff00) >> 8)) &&
      (dsdt[j + 12] == ((PciAdr & 0x00ff0000) >> 16)) &&
      (dsdt[j + 13] == ((PciAdr & 0xff000000) >> 24))
      ) ||
      //--------------------
      (
        (dsdt[j +  9] == 0x0B) &&
        (dsdt[j + 10] == ((PciAdr & 0x000000ff) >> 0)) &&
        (dsdt[j + 11] == ((PciAdr & 0x0000ff00) >> 8)) &&
        (PciAdr < 0x10000)
      ) ||
      //-----------------------
      (
        (dsdt[j +  9] == 0x0A) &&
        (dsdt[j + 10] == (PciAdr & 0x000000ff)) &&
        (PciAdr < 0x100)
      ) ||
      //-----------------
      ((dsdt[j +  9] == 0x00) && (PciAdr == 0)) ||
      //------------------
      ((dsdt[j +  9] == 0x01) && (PciAdr == 1))
    )
  );
}

BOOLEAN
CmpPNP (
  UINT8   *dsdt,
  UINT32  j,
  UINT16  PNP
) {
  // Name (_HID, EisaId ("PNP0C0F")) for PNP=0x0C0F BigEndian
  if (PNP == 0) {
    return (BOOLEAN)
    (
      (dsdt[j + 0] == 0x08) &&
      (dsdt[j + 1] == 0x5F) &&
      (dsdt[j + 2] == 0x48) &&
      (dsdt[j + 3] == 0x49) &&
      (dsdt[j + 4] == 0x44) &&
      (dsdt[j + 5] == 0x0B) &&
      (dsdt[j + 6] == 0x41) &&
      (dsdt[j + 7] == 0xD0)
    );
  }
  return (BOOLEAN)
  (
    (dsdt[j + 0] == 0x08) &&
    (dsdt[j + 1] == 0x5F) &&
    (dsdt[j + 2] == 0x48) &&
    (dsdt[j + 3] == 0x49) &&
    (dsdt[j + 4] == 0x44) &&
    (dsdt[j + 5] == 0x0C) &&
    (dsdt[j + 6] == 0x41) &&
    (dsdt[j + 7] == 0xD0) &&
    (dsdt[j + 8] == ((PNP & 0xff00) >> 8)) &&
    (dsdt[j + 9] == ((PNP & 0x00ff) >> 0))
  );
}

INT32
CmpDev (
  UINT8     *dsdt,
  UINT32    i,
  UINT8     *Name
) {
  if (
    (dsdt[i+0] == Name[0]) && (dsdt[i+1] == Name[1]) &&
    (dsdt[i+2] == Name[2]) && (dsdt[i+3] == Name[3]) &&
    (
      ((dsdt[i-2] == 0x82) && (dsdt[i-3] == 0x5B) && (dsdt[i-1] < 0x40)) ||
      ((dsdt[i-3] == 0x82) && (dsdt[i-4] == 0x5B) && ((dsdt[i-2] & 0xF0) == 0x40)) ||
      ((dsdt[i-4] == 0x82) && (dsdt[i-5] == 0x5B) && ((dsdt[i-3] & 0xF0) == 0x80))
    )
  ) {
    if (dsdt[i-5] == 0x5B) {
      return i - 3;
    } else if (dsdt[i-4] == 0x5B){
      return i - 2;
    } else {
      return i - 1;
    }
  }

  return 0;
}

//the procedure can find BIN array UNSIGNED CHAR8 sizeof N inside part of large array "dsdt" size of len
// return position or -1 if not found
INT32
FindBin (
  UINT8     *dsdt,
  UINT32    len,
  UINT8     *bin,
  UINT32    N
) {
  UINT32    i, j;
  BOOLEAN   eq;

  for (i=0; i<len-N; i++) {
    eq = TRUE;
    for (j=0; j<N; j++) {
      if (dsdt[i+j] != bin[j]) {
        eq = FALSE;
        break;
      }
    }
    if (eq) {
      return (INT32)i;
    }
  }
  return -1;
}

//if (!FindMethod(dsdt, len, "DTGP"))
// return address of size field. Assume size not more then 0x0FFF = 4095 bytes
//assuming only short methods
UINT32
FindMethod (
  UINT8     *dsdt,
  UINT32    len,
  CHAR8     *Name
) {
  UINT32  i;

  for (i = 0; i<len-7; i++) {
    if (
      ((dsdt[i] == 0x14) || (dsdt[i+1] == 0x14) || (dsdt[i-1] == 0x14)) &&
      (dsdt[i+3] == Name[0]) && (dsdt[i+4] == Name[1]) &&
      (dsdt[i+5] == Name[2]) && (dsdt[i+6] == Name[3])
    ){
      if (dsdt[i-1] == 0x14) {
        return i;
      }

      return (dsdt[i+1] == 0x14)?(i+2):(i+1); //pointer to size field
    }
  }

  return 0;
}

//this procedure corrects size of outer method. Embedded methods is not proposed
// adr - a place of changes
// shift - a size of changes

UINT32
CorrectOuterMethod (
  UINT8   *dsdt,
  UINT32  len,
  UINT32  adr,
  INT32   shift
) {
  INT32     i,  k, offset = 0;
  UINT32    size = 0;
  //INTN    NameShift;
  CHAR8     Name[5];

  if (shift == 0) {
    return len;
  }

  i = adr; //usually adr = @5B - 1 = sizefield - 3
  while (i-- > 0x20) {  //find method that previous to adr
    k = i + 1;

    if ((dsdt[i] == 0x14) && !CmpNum(dsdt, i, FALSE)) { //method candidate
      size = AcpiGetSize(dsdt, k);

      if (!size) {
        continue;
      }

      if (
        ((size <= 0x3F) && !GetName(dsdt, k+1, &Name[0], NULL)) ||
        ((size > 0x3F) && (size <= 0xFFF) && !GetName(dsdt, k+2, &Name[0], NULL)) ||
        ((size > 0xFFF) && !GetName(dsdt, k+3, &Name[0], NULL))
      ) {
        DBG("method found, size=0x%x but name is not\n", size);
        continue;
      }

      if ((k+size) > adr+4) {  //Yes - it is outer
        DBG("found outer method %a begin=%x end=%x\n", Name, k, k+size);
        offset = WriteSize(k, dsdt, len, shift);  //size corrected to sizeoffset at address j
        //shift += offset;
        len += offset;
      }  //else not an outer method
      break;
    }
  }

  return len;
}

//return final length of dsdt
UINT32
CorrectOuters (
  UINT8     *dsdt,
  UINT32    len,
  UINT32    adr,
  INT32     shift
) {
  INT32     i, j, k, offset = 0;
  UINT32    SBSIZE = 0, SBADR = 0, size = 0;
  BOOLEAN   SBFound = FALSE;

  if (shift == 0) {
    return len;
  }

  i = adr; //usually adr = @5B - 1 = sizefield - 3

  while (i > 0x20) {  //find devices that previous to adr
    //check device
    k = i + 2;
    if (
      (dsdt[i] == 0x5B) &&
      (dsdt[i+1] == 0x82) &&
      !CmpNum(dsdt, i, TRUE)
    ) { //device candidate
      size = AcpiGetSize(dsdt, k);
      if (size) {
        if ((k+size) > adr+4) {  //Yes - it is outer
              //DBG("found outer device begin=%x end=%x\n", k, k+size);
          offset = WriteSize(k, dsdt, len, shift);  //size corrected to sizeoffset at address j
          shift += offset;
          len += offset;
        }  //else not an outer device
      } //else wrong size field - not a device
    } //else not a device

    // check scope
    // a problem 45 43 4F 4E 08   10 84 10 05 5F 53 42 5F
    SBSIZE = 0;

    if (dsdt[i] == '_' && dsdt[i+1] == 'S' && dsdt[i+2] == 'B' && dsdt[i+3] == '_') {
      for (j=0; j<10; j++) {
        if (dsdt[i-j] != 0x10) {
          continue;
        }

        if (!CmpNum(dsdt, i-j, TRUE)) {
          SBADR = i-j+1;
          SBSIZE = AcpiGetSize(dsdt, SBADR);

            //DBG("found Scope(\\_SB) address = 0x%08x size = 0x%08x\n", SBADR, SBSIZE);

          if ((SBSIZE != 0) && (SBSIZE < len)) {  //if zero or too large then search more
            //if found
            k = SBADR - 6;

            if ((SBADR + SBSIZE) > adr+4) {  //Yes - it is outer
              //DBG("found outer scope begin=%x end=%x\n", SBADR, SBADR+SBSIZE);
              offset = WriteSize(SBADR, dsdt, len, shift);
              shift += offset;
              len += offset;
              SBFound = TRUE;
              break;  //SB found
            }  //else not an outer scope
          }
        }
      }
    } //else not a scope

    if (SBFound) {
      break;
    }

    i = k - 3;    //if found then search again from found
  }

  return len;
}

//ReplaceName(dsdt, len, "AZAL", "HDEF");
INTN
ReplaceName (
  UINT8     *dsdt,
  UINT32    len,
  /* CONST*/ CHAR8    *OldName,
  /* CONST*/ CHAR8    *NewName
) {
  UINTN   i;
  INTN    j = 0;

  for (i = 0; i < len-4; i++) {
    if (
      (dsdt[i+0] == NewName[0]) && (dsdt[i+1] == NewName[1]) &&
      (dsdt[i+2] == NewName[2]) && (dsdt[i+3] == NewName[3])
    ) {
      if (OldName) {
        DBG("NewName %a already present, renaming impossibble\n", NewName);
      } else {
        DBG("name %a present at %x\n", NewName, i);
      }
      return -1;
    }
  }

  if (!OldName) {
    return 0;
  }

  for (i = 0; i < len - 4; i++) {
    if (
      (dsdt[i+0] == OldName[0]) && (dsdt[i+1] == OldName[1]) &&
      (dsdt[i+2] == OldName[2]) && (dsdt[i+3] == OldName[3])
    ) {
      DBG("Name %a present at 0x%x, renaming to %a\n", OldName, i, NewName);
      dsdt[i+0] = NewName[0];
      dsdt[i+1] = NewName[1];
      dsdt[i+2] = NewName[2];
      dsdt[i+3] = NewName[3];
      j++;
    }
  }

  return j; //number of replacement
}

//the procedure search nearest "Device" code before given address
//should restrict the search by 6 bytes... OK, 10, .. until dsdt begin
//hmmm? will check device name
UINT32
DevFind (
  UINT8     *dsdt,
  UINT32    address
) {
  UINT32    k = address;
  INT32     size = 0;

  while (k > 30) {
    k--;
    if (dsdt[k] == 0x82 && dsdt[k-1] == 0x5B) {
      size = AcpiGetSize(dsdt, k+1);
      if (!size) {
        continue;
      }

      if ((k + size + 1) > address) {
        return (k+1); //pointer to size
      } //else continue
    }
  }

  DBG("Device definition before adr=%x not found\n", address);

  return 0; //impossible value for fool proof
}


BOOLEAN
AddProperties (
  AML_CHUNK     *pack,
  UINT32        Dev
) {
  INT32     i;
  BOOLEAN   Injected = FALSE;

  if (gSettings.NrAddProperties == 0xFFFE) {
    return FALSE; // not do this for Arbitrary properties?
  }

  for (i = 0; i < gSettings.NrAddProperties; i++) {
    if (gSettings.AddProperties[i].Device != Dev) {
      continue;
    }

    Injected = TRUE;

    aml_add_string(pack, gSettings.AddProperties[i].Key);
    aml_add_byte_buffer (
      pack, gSettings.AddProperties[i].Value,
      (UINT32)gSettings.AddProperties[i].ValueLen
    );
  }

  return Injected;
}

//len = DeleteDevice("AZAL", dsdt, len);
UINT32
DeleteDevice (
/*CONST*/ CHAR8   *Name,
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32    i, j;
  INT32     size = 0, sizeoffset;

  DBG(" deleting device %a\n", Name);

  for (i=20; i<len; i++) {
    j = CmpDev(dsdt, i, (UINT8*)Name);

    if (j != 0) {
      size = AcpiGetSize(dsdt, j);
      if (!size) {
        continue;
      }

      sizeoffset = - 2 - size;
      len = MoveData(j-2, dsdt, len, sizeoffset);
      //to correct outers we have to calculate offset
      len = CorrectOuters(dsdt, len, j-3, sizeoffset);
      break;
    }
  }

  return len;
}

UINT32
GetPciDevice (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32    i, PCIADR = 0, PCISIZE = 0;

  for (i=20; i<len; i++) {
    // Find Device PCI0   // PNP0A03
    if (CmpPNP(dsdt, i, 0x0A03)) {
      PCIADR = DevFind(dsdt, i);
      if (!PCIADR) {
        continue;
      }

      PCISIZE = AcpiGetSize(dsdt, PCIADR);
      if (PCISIZE) {
        break;
      }
    } // End find
  }
  if (!PCISIZE) {
    for (i=20; i<len; i++) {
      // Find Device PCIE   // PNP0A08
      if (CmpPNP(dsdt, i, 0x0A08)) {
        PCIADR = DevFind(dsdt, i);
        if (!PCIADR) {
          continue;
        }

        PCISIZE = AcpiGetSize(dsdt, PCIADR);
        if (PCISIZE) {
          break;
        }
      } // End find
    }
  }

  if (PCISIZE) {
    return PCIADR;
  }

  return 0;
}

// Find PCIRootUID and all need Fix Device
UINTN
FindPciRoot (
  UINT8     *dsdt,
  UINT32    len
) {
  UINTN     j, root = 0;
  UINT32    PCIADR, PCISIZE = 0;

  //initialising
  NetworkName   = FALSE;
  DisplayName1  = FALSE;
  DisplayName2  = FALSE;
  //FirewireName  = FALSE;
  ArptName      = FALSE;
  //XhciName      = FALSE;

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  //sample
  /*
   5B 82 8A F1 05 50 43 49 30           Device (PCI0) {
   08 5F 48 49 44 0C 41 D0 0A 08        Name (_HID, EisaId ("PNP0A08"))
   08 5F 43 49 44 0C 41 D0 0A 03        Name (_CID, EisaId ("PNP0A03"))
   08 5F 41 44 52 00                    Name (_ADR, Zero)
   14 09 5E 42 4E 30 30 00 A4 00        Method (^BN00, 0, NotSerialized) {Return (Zero)}
   14 0B 5F 42 42 4E 00 A4 42 4E 30 30  Method (_BBN, 0, NotSerialized) {Return (BN00 ())}
   08 5F 55 49 44 00                    Name (_UID, Zero)
   14 16 5F 50 52 54 00                 Method (_PRT, 0, NotSerialized)
  */

  if (PCISIZE > 0) {
    // find PCIRootUID
    for (j = PCIADR; j < PCIADR + 64; j++) {
      if ((dsdt[j] == '_') && (dsdt[j+1] == 'U') && (dsdt[j+2] == 'I') && (dsdt[j+3] == 'D')) {
        // Slice - I want to set root to zero instead of keeping original value
        if (dsdt[j+4] == 0x0A) {
          dsdt[j+5] = 0;  //AML_BYTE_PREFIX followed by a number
        } else {
          dsdt[j+4] = 0;  //any other will be considered as ONE or WRONG, replace to ZERO
        }

        DBG("Found PCIROOTUID = %d\n", root);
        break;
      }
    }
  } else {
    DBG("Warning! PCI root is not found!");
  }

  return root;
}

UINT32
FixAny (
  UINT8     *dsdt,
  UINT32    len,
  UINT8     *ToFind,
  UINT32    LenTF,
  UINT8     *ToReplace,
  UINT32    LenTR
) {
  INT32     sizeoffset, adr;
  UINT32    i;
  BOOLEAN   found = FALSE;

  if (!ToFind || !LenTF || !LenTR) {
    MsgLog(" invalid patches!\n");
    return len;
  }

  MsgLog(" pattern %02x%02x%02x%02x,", ToFind[0], ToFind[1], ToFind[2], ToFind[3]);

  if ((LenTF + sizeof(EFI_ACPI_DESCRIPTION_HEADER)) > len) {
    MsgLog(" the patch is too large!\n");
    return len;
  }

  sizeoffset = LenTR - LenTF;
  for (i = 20; i < len; ) {
    adr = FindBin(dsdt + i, len - i, ToFind, LenTF);
    if (adr < 0) {
      if (found) {
        //MsgLog(" ]\n");
        DBG(" ]");
        MsgLog("\n");
      } else {
        MsgLog(" bin not found / already patched!\n");
      }
      return len;
    }

    if (!found) {
      //MsgLog(" patched at: [");
      MsgLog(" patched");
      DBG(" at: [");
    }

    DBG(" (%x)", adr);
    found = TRUE;

    len = MoveData(adr + i, dsdt, len, sizeoffset);
    if ((LenTR > 0) && (ToReplace != NULL)) {
      CopyMem(dsdt + adr + i, ToReplace, LenTR);
    }

    len = CorrectOuterMethod(dsdt, len, adr + i - 2, sizeoffset);
    len = CorrectOuters(dsdt, len, adr + i - 3, sizeoffset);
    i += adr + LenTR;
  }

  return len;
}

UINT32
AddPNLF (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32    i, adr  = 0; //, j, size;

  DBG("Start PNLF Fix\n");

  if (FindBin(dsdt, len, (UINT8*)app2, 10) >= 0) {
    return len; //the device already exists
  }

  //search  PWRB PNP0C0C
  for (i=0x20; i<len-6; i++) {
    if (CmpPNP(dsdt, i, 0x0C0C)) {
      DBG("found PWRB at %x\n", i);
      adr = DevFind(dsdt, i);
      break;
    }
  }

  if (!adr) {
    //search battery
    DBG("not found PWRB, look BAT0\n");
    for (i=0x20; i<len-6; i++) {
      if (CmpPNP(dsdt, i, 0x0C0A)) {
        adr = DevFind(dsdt, i);
        DBG("found BAT0 at %x\n", i);
        break;
      }
    }
  }

  if (!adr) {
    return len;
  }

  i = adr - 2;
  len = MoveData(i, dsdt, len, sizeof(pnlf));
  CopyMem(dsdt+i, pnlf, sizeof(pnlf));
  len = CorrectOuters(dsdt, len, adr-3, sizeof(pnlf));

  return len;
}

UINT32
FIXDisplay (
  UINT8     *dsdt,
  UINT32    len,
  INT32     VCard
) {
  UINT32      i = 0, j, k, FakeID = 0, FakeVen = 0,
              PCIADR = 0, PCISIZE = 0, Size,
              devadr = 0, devsize = 0, devadr1 = 0, devsize1 = 0;
  INT32       sizeoffset = 0;
  CHAR8       *display;
  BOOLEAN     DISPLAYFIX = FALSE, NonUsable = FALSE, DsmFound = FALSE,
              NeedHDMI = (gSettings.FixDsdt & FIX_HDMI);
              //NeedHDMI = ((gSettings.FixDsdt & FIX_NEW_WAY) && (gSettings.FixDsdt & FIX_HDMI));
  AML_CHUNK   *root = NULL, *gfx0, *peg0, *met, *met2, *pack;

  DisplayName1 = FALSE;

  if (!DisplayADR1[VCard]) return len;

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return len; //what is the bad DSDT ?!
  }

  DBG("Start Display%d Fix\n", VCard);
  root = aml_create_node(NULL);

  //search DisplayADR1[0]
  for (j=0x20; j<len-10; j++) {
    if (CmpAdr(dsdt, j, DisplayADR1[VCard])) { //for example 0x00020000=2,0
      devadr = DevFind(dsdt, j);  //PEG0@2,0
      if (!devadr) {
        continue;
      }

      devsize = AcpiGetSize(dsdt, devadr); //sizeof PEG0  0x35
      if (devsize) {
        DisplayName1 = TRUE;
        break;
      }
    } // End Display1
  }

  //what if PEG0 is not found?
  if (devadr) {
    for (j=devadr; j<devadr+devsize; j++) { //search card inside PEG0@0
      if (CmpAdr(dsdt, j, DisplayADR2[VCard])) { //else DISPLAYFIX==false
        devadr1 = DevFind(dsdt, j); //found PEGP
        if (!devadr1) {
          continue;
        }

        devsize1 = AcpiGetSize(dsdt, devadr1);
        if (devsize1) {
          DBG("Found internal video device %x @%x\n", DisplayADR2[VCard], devadr1);
          DISPLAYFIX = TRUE;
          break;
        }
      }
    }

    if (!DISPLAYFIX) {
      for (j=devadr; j<devadr+devsize; j++) { //search card inside PEGP@0
        if (CmpAdr(dsdt, j, 0xFFFF)) {  //Special case? want to change to 0
          devadr1 = DevFind(dsdt, j); //found PEGP
          if (!devadr1) {
            continue;
          }
          devsize1 = AcpiGetSize(dsdt, devadr1); //13
          if (devsize1) {
            if (gSettings.ReuseFFFF) {
              dsdt[j+10] = 0;
              dsdt[j+11] = 0;
              DBG("Found internal video device FFFF@%x, ReUse as 0\n", devadr1);
            } else {
              NonUsable = TRUE;
              DBG("Found internal video device FFFF@%x, unusable\n", devadr1);
            }

            DISPLAYFIX = TRUE;
            break;
          }
        }
      }
    }

    i = 0;
    if (DISPLAYFIX) { // device on bridge found
      i = devadr1;
    } else if (DisplayADR2[VCard] == 0xFFFE) { //builtin
      i = devadr;
      DISPLAYFIX = TRUE;
      devadr1 = devadr;
      DBG(" builtin display\n");
    }

    if (i != 0) {
      Size = AcpiGetSize(dsdt, i);
      k = FindMethod(dsdt + i, Size, "_DSM");

      if (k != 0) {
        k += i;
        if (
          (((gSettings.DropOEM_DSM & DEV_ATI)   != 0) && (DisplayVendor[VCard] == 0x1002)) ||
          (((gSettings.DropOEM_DSM & DEV_NVIDIA)!= 0) && (DisplayVendor[VCard] == 0x10DE)) ||
          (((gSettings.DropOEM_DSM & DEV_INTEL) != 0) && (DisplayVendor[VCard] == 0x8086))
        ) {
          Size = AcpiGetSize(dsdt, k);
          sizeoffset = - 1 - Size;
          len = MoveData(k - 1, dsdt, len, sizeoffset); //kill _DSM
          len = CorrectOuters(dsdt, len, k - 2, sizeoffset);
          DBG("_DSM in display already exists, dropped\n");
        } else {
          DBG("_DSM already exists, patch display will not be applied\n");
          //        return len;
          DisplayADR1[VCard] = 0;  //xxx
          DsmFound = TRUE;
        }
      }
    }
  }

  if (!DisplayName1) {
    peg0 = aml_add_device(root, "PEG0");
    aml_add_name(peg0, "_ADR");
    aml_add_dword(peg0, DisplayADR1[VCard]);
    DBG("add device PEG0\n");
  } else {
    peg0 = root;
  }

  if (!DISPLAYFIX) {   //bridge or builtin not found
    gfx0 = aml_add_device(peg0, "GFX0");
    DBG("add device GFX0\n");
    aml_add_name(gfx0, "_ADR");

    if (DisplayADR2[VCard] > 0x3F) {
      aml_add_dword(gfx0, DisplayADR2[VCard]);
    } else {
      aml_add_byte(gfx0, (UINT8)DisplayADR2[VCard]);
    }
  } else {
    gfx0 = peg0;
  }

  if (
    DsmFound ||
    (
      !NeedHDMI &&
      (
        ((DisplayVendor[VCard] == 0x8086) && (gSettings.InjectIntel  || !gSettings.FakeIntel)) ||
        ((DisplayVendor[VCard] == 0x10DE) && (gSettings.InjectNVidia || !gSettings.FakeNVidia)) ||
        ((DisplayVendor[VCard] == 0x1002) && (gSettings.InjectATI    || !gSettings.FakeATI))
      )
    )
  ) {
    DBG("Skip Method(_DSM) for %04x card\n", DisplayVendor[VCard]);
    goto Skip_DSM;
  }

  DBG("Creating Method(_DSM) for %04x card\n", DisplayVendor[VCard]);
  met = aml_add_method(gfx0, "_DSM", 4);
  met2 = aml_add_store(met);
  pack = aml_add_package(met2);

  if (NeedHDMI) {
    aml_add_string(pack, "hda-gfx");
    aml_add_string_buffer(pack, (gSettings.UseIntelHDMI && DisplayVendor[VCard] !=  0x8086) ? "onboard-2" : "onboard-1");
  }

  switch (DisplayVendor[VCard]) {
    case 0x8086:
      if (gSettings.FakeIntel) {
        FakeID = gSettings.FakeIntel >> 16;
        aml_add_string(pack, "device-id");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeID, 4);
        FakeVen = gSettings.FakeIntel & 0xFFFF;
        aml_add_string(pack, "vendor-id");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeVen, 4);
      }
      break;

    case 0x10DE:
      if (gSettings.FakeNVidia) {
        FakeID = gSettings.FakeNVidia >> 16;
        aml_add_string(pack, "device-id");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeID, 4);
        FakeVen = gSettings.FakeNVidia & 0xFFFF;
        aml_add_string(pack, "vendor-id");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeVen, 4);
      }
      break;

    case 0x1002:
      if (gSettings.FakeATI) {
        FakeID = gSettings.FakeATI >> 16;
        aml_add_string(pack, "device-id");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeID, 4);
        aml_add_string(pack, "ATY,DeviceID");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeID, 2);
        FakeVen = gSettings.FakeATI & 0xFFFF;
        aml_add_string(pack, "vendor-id");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeVen, 4);
        aml_add_string(pack, "ATY,VendorID");
        aml_add_byte_buffer(pack, (CHAR8*)&FakeVen, 2);
      }/* else {
        aml_add_string(pack, "ATY,VendorID");
        aml_add_byte_buffer(pack, VenATI, 2);
      }*/
      break;
  }

  aml_add_local0(met);
  aml_add_buffer(met, dtgp_1, sizeof(dtgp_1));

Skip_DSM:

  //add _sun
  switch (DisplayVendor[VCard]) {
    case 0x10DE:
    case 0x1002:
      Size = AcpiGetSize(dsdt, i);
      j = (DisplayVendor[VCard] == 0x1002) ? 0 : 1;
      k = FindMethod(dsdt + i, Size, "_SUN");

      if (k == 0) {
        k = FindName(dsdt + i, Size, "_SUN");
        if (k == 0) {
          aml_add_name(gfx0, "_SUN");
          aml_add_dword(gfx0, SlotDevices[j].SlotID);
        } else {
          //we have name sun, set the number
          if (dsdt[k + 4] == 0x0A) {
            dsdt[k + 5] = SlotDevices[j].SlotID;
          }
        }
      } else {
        DBG("Warning: Method(_SUN) found for %04x card\n", DisplayVendor[VCard]);
      }
      break;
  }

  if (!NonUsable) {
    //now insert video
    DBG("now inserting Video device\n");
    aml_calculate_size(root);
    display = AllocateZeroPool(root->Size);
    sizeoffset = root->Size;
    aml_write_node(root, display, 0);
    aml_destroy_node(root);

    if (DisplayName1) {   //bridge is present
      // move data to back for add Display
      DBG("... into existing bridge\n");

      if (!DISPLAYFIX || (DisplayADR2[VCard] == 0xFFFE)) {   //subdevice absent
        devsize = AcpiGetSize(dsdt, devadr);
        if (!devsize) {
          DBG("BUG! Address of existing PEG0 is lost %x\n", devadr);
          FreePool(display);
          return len;
        }

        i = devadr + devsize;
        len = MoveData(i, dsdt, len, sizeoffset);
        CopyMem(dsdt+i, display, sizeoffset);
        j = WriteSize(devadr, dsdt, len, sizeoffset); //correct bridge size
        sizeoffset += j;
        len += j;
        len = CorrectOuters(dsdt, len, devadr-3, sizeoffset);
      } else {
        devsize1 = AcpiGetSize(dsdt, devadr1);
        if (!devsize1) {
          FreePool(display);
          return len;
        }

        i = devadr1 + devsize1;
        len = MoveData(i, dsdt, len, sizeoffset);
        CopyMem(dsdt+i, display, sizeoffset);
        j = WriteSize(devadr1, dsdt, len, sizeoffset);
        sizeoffset += j;
        len += j;
        len = CorrectOuters(dsdt, len, devadr1-3, sizeoffset);
      }
    } else { //insert PEG0 into PCI0 at the end
      //PCI corrected so search again
      DBG("... into created bridge\n");
      PCIADR = GetPciDevice(dsdt, len);
      if (PCIADR) {
        PCISIZE = AcpiGetSize(dsdt, PCIADR);
      }

      if (!PCISIZE) {
        return len; //what is the bad DSDT ?!
      }

      i = PCIADR + PCISIZE;
      //devadr = i + 2;  //skip 5B 82
      len = MoveData(i, dsdt, len, sizeoffset);
      CopyMem(dsdt + i, display, sizeoffset);
      // Fix PCI0 size
      k = WriteSize(PCIADR, dsdt, len, sizeoffset);
      sizeoffset += k;
      len += k;
      //devadr += k;
      k = CorrectOuters(dsdt, len, PCIADR-3, sizeoffset);
      //devadr += k - len;
      len = k;
    }
    FreePool(display);
  }

  return len;
}

UINT32
AddHDMI (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32      i, j, k, PCIADR = 0, PCISIZE = 0, Size,
              devadr = 0, BridgeSize = 0, devadr1 = 0; //, devsize1=0;
  INT32       sizeoffset = 0;
  CHAR8       *hdmi = NULL, data2[] = {0xe0,0x00,0x56,0x28};
  BOOLEAN     BridgeFound = FALSE, HdauFound = FALSE;
  AML_CHUNK   *brd = NULL, *root = NULL, *met, *met2, *pack;

  if (!HDMIADR1) {
    return len;
  }

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return len; //what is the bad DSDT ?!
  }

  DBG("Start HDMI%d Fix\n");

  // Device Address
  for (i=0x20; i<len-10; i++) {
    if (CmpAdr(dsdt, i, HDMIADR1)) {
      devadr = DevFind(dsdt, i);
      if (!devadr) {
        continue;
      }

      BridgeSize = AcpiGetSize(dsdt, devadr);
      if (!BridgeSize) {
        continue;
      }

      BridgeFound = TRUE;
      if (HDMIADR2 != 0xFFFE){
        for (k = devadr + 9; k < devadr + BridgeSize; k++) {
          if (CmpAdr(dsdt, k, HDMIADR2)) {
            devadr1 = DevFind(dsdt, k);
            if (!devadr1) {
              continue;
            }

            device_name[11] = AllocateZeroPool(5);
            CopyMem(device_name[11], dsdt+k, 4);
            DBG("found HDMI device [0x%08x:%x] at %x and Name is %a\n",
                HDMIADR1, HDMIADR2, devadr1, device_name[11]);
            ReplaceName(dsdt + devadr, BridgeSize, device_name[11], "HDAU");
            HdauFound = TRUE;
            break;
          }
        }
        if (!HdauFound) {
          DBG("have no HDMI device while HDMIADR2=%x\n", HDMIADR2);
          devadr1 = devadr;
        }
      } else {
        devadr1 = devadr;
      }
      break;
    } // End if devadr1 find
  }

  if (BridgeFound) { // bridge or device
    if (HdauFound) {
      i = devadr1;
      Size = AcpiGetSize(dsdt, i);
      k = FindMethod(dsdt + i, Size, "_DSM");

      if (k != 0) {
        k += i;
        if ((gSettings.DropOEM_DSM & DEV_HDMI) != 0) {
          Size = AcpiGetSize(dsdt, k);
          sizeoffset = - 1 - Size;
          len = MoveData(k - 1, dsdt, len, sizeoffset);
          len = CorrectOuters(dsdt, len, k - 2, sizeoffset);
          DBG("_DSM in HDAU already exists, dropped\n");
        } else {
          DBG("_DSM already exists, patch HDAU will not be applied\n");
          return len;
        }
      }
    }
    root = aml_create_node(NULL);
    //what to do if no HDMI bridge?
  } else {
    brd = aml_create_node(NULL);
    root = aml_add_device(brd, "HDM0");
    aml_add_name(root, "_ADR");
    aml_add_dword(root, HDMIADR1);
    DBG("Created  bridge device with ADR=0x%x\n", HDMIADR1);
  }

  DBG("HDMIADR1=%x HDMIADR2=%x\n", HDMIADR1, HDMIADR2);

  if (!HdauFound && (HDMIADR2 != 0xFFFE)) { //there is no HDMI device at dsdt, creating new one
    AML_CHUNK   *dev = aml_add_device(root, "HDAU");

    aml_add_name(dev, "_ADR");
    if (HDMIADR2) {
      if (HDMIADR2 > 0x3F) {
        aml_add_dword(dev, HDMIADR2);
      } else {
        aml_add_byte(dev, (UINT8)HDMIADR2);
      }
    } else {
      aml_add_byte(dev, 0x01);
    }

    met = aml_add_method(dev, "_DSM", 4);

    //something here is not liked by apple...
    /*
    k = FindMethod(dsdt + i, Size, "_SUN");
    if (k == 0) {
      k = FindName(dsdt + i, Size, "_SUN");
      if (k == 0) {
        aml_add_name(dev, "_SUN");
        aml_add_dword(dev, SlotDevices[4].SlotID);
      } else {
        //we have name sun, set the number
        if (dsdt[k + 4] == 0x0A) {
          dsdt[k + 5] = SlotDevices[4].SlotID;
        }
      }
    }
    */
  } else {
    //HDAU device already present
    met = aml_add_method(root, "_DSM", 4);
  }


  met2 = aml_add_store(met);
  pack = aml_add_package(met2);
  if (!gSettings.NoDefaultProperties) {
    aml_add_string(pack, "hda-gfx");
    if (gSettings.UseIntelHDMI) {
      aml_add_string_buffer(pack, "onboard-2");
    } else {
      aml_add_string_buffer(pack, "onboard-1");
    }
  }

  if (!AddProperties(pack, DEV_HDMI)) {
    DBG(" - with default properties\n");
    aml_add_string(pack, "layout-id");
    aml_add_byte_buffer(pack, (CHAR8*)&GfxlayoutId[0], 4);

    aml_add_string(pack, "PinConfigurations");
    aml_add_byte_buffer(pack, data2, sizeof(data2));
  }

  aml_add_local0(met2);
  aml_add_buffer(met, dtgp_1, sizeof(dtgp_1));
  // finish Method(_DSM,4,NotSerialized)

  aml_calculate_size(root);
  hdmi = AllocateZeroPool(root->Size);
  sizeoffset = root->Size;
  aml_write_node(root, hdmi, 0);
  aml_destroy_node(root);

  //insert HDAU
  if (BridgeFound) { // bridge or lan
    k = devadr1;
  } else { //this is impossible
    k = PCIADR;
  }

  Size = AcpiGetSize(dsdt, k);
  if (Size > 0) {
    i = k + Size;
    len = MoveData(i, dsdt, len, sizeoffset);
    CopyMem(dsdt + i, hdmi, sizeoffset);
    j = WriteSize(k, dsdt, len, sizeoffset);
    sizeoffset += j;
    len += j;
    len = CorrectOuters(dsdt, len, k-3, sizeoffset);
  }

  if (hdmi) {
    FreePool(hdmi);
  }

  return len;
}

//Network -------------------------------------------------------------

UINT32
FIXNetwork (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32      i, k, NetworkADR = 0, BridgeSize, Size, BrdADR = 0,
              PCIADR, PCISIZE = 0, FakeID = 0, FakeVen = 0;
  INT32       sizeoffset;
  AML_CHUNK   *met, *met2, *brd, *root, *pack, *dev;
  CHAR8       *network, NameCard[32];
  UINTN       Len = ARRAY_SIZE(NameCard);

  if (!NetworkADR1) {
    return len;
  }

  DBG("Start NetWork Fix\n");

  if (gSettings.FakeLAN) {
    FakeID = gSettings.FakeLAN >> 16;
    FakeVen = gSettings.FakeLAN & 0xFFFF;
    AsciiSPrint(NameCard, Len, "pci%x,%x\0", FakeVen, FakeID);
    AsciiStrCpyS(NameCard, Len, AsciiStrToLower(NameCard));
    //Netmodel = get_net_model((FakeVen << 16) + FakeID);
  }

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return len; //what is the bad DSDT ?!
  }

  NetworkName = FALSE;

  // Network Address
  for (i = 0x24; i < len - 10; i++) {
    if (CmpAdr(dsdt, i, NetworkADR1)) { //0x001C0004
      BrdADR = DevFind(dsdt, i);
      if (!BrdADR) {
        continue;
      }

      BridgeSize = AcpiGetSize(dsdt, BrdADR);
      if (!BridgeSize) {
        continue;
      }

      if (NetworkADR2 != 0xFFFE){  //0
        for (k = BrdADR + 9; k < BrdADR + BridgeSize; k++) {
          if (CmpAdr(dsdt, k, NetworkADR2)) {
            NetworkADR = DevFind(dsdt, k);
            if (!NetworkADR) {
              continue;
            }

            device_name[1] = AllocateZeroPool(5);
            CopyMem(device_name[1], dsdt+k, 4);
            DBG("found NetWork device [0x%08x:%x] at %x and Name is %a\n",
                NetworkADR1, NetworkADR2, NetworkADR, device_name[1]);
            //renaming disabled until better way will found
            //ReplaceName(dsdt + BrdADR, BridgeSize, device_name[1], "GIGE");
            NetworkName = TRUE;
            break;
          }
        }

        if (!NetworkName) {
          DBG("have no Network device while NetworkADR2=%x\n", NetworkADR2);
          //in this case NetworkADR point to bridge
          NetworkADR = BrdADR;
        }
      } else {
        NetworkADR = BrdADR;
      }
      break;
    } // End if NetworkADR find
  }

  if (BrdADR) { // bridge or device
    i = NetworkADR;
    Size = AcpiGetSize(dsdt, i);
    k = FindMethod(dsdt + i, Size, "_DSM");

    if (k != 0) {
      k += i;
      if ((gSettings.DropOEM_DSM & DEV_LAN) != 0) {
        Size = AcpiGetSize(dsdt, k);
        sizeoffset = - 1 - Size;
        len = MoveData(k - 1, dsdt, len, sizeoffset);
        len = CorrectOuters(dsdt, len, k - 2, sizeoffset);
        DBG("_DSM in LAN already exists, dropped\n");
      } else {
        DBG("_DSM already exists, patch LAN will not be applied\n");
        return len;
      }
    }

    root = aml_create_node(NULL);
  } else {
  //what to do if no LAN bridge?
    i = PCIADR;
    brd = aml_create_node(NULL);
    root = aml_add_device(brd, "LAN0");
    aml_add_name(root, "_ADR");
    aml_add_dword(root, NetworkADR1);
    DBG("Created  bridge device with ADR=0x%x\n", NetworkADR1);
  }

  DBG("NetworkADR1=%x NetworkADR2=%x\n", NetworkADR1, NetworkADR2);
  dev = root;

  if (!NetworkName && (NetworkADR2 != 0xFFFE)) { //there is no network device at dsdt, creating new one
    dev = aml_add_device(root, "GIGE");
    aml_add_name(dev, "_ADR");
    if (NetworkADR2) {
      if (NetworkADR2> 0x3F) {
        aml_add_dword(dev, NetworkADR2);
      } else {
        aml_add_byte(dev, (UINT8)NetworkADR2);
      }
    } else {
      aml_add_byte(dev, 0x00);
    }
  }

  Size = AcpiGetSize(dsdt, i);
  k = FindMethod(dsdt + i, Size, "_SUN");
  if (k == 0) {
    k = FindName(dsdt + i, Size, "_SUN");
    if (k == 0) {
      aml_add_name(dev, "_SUN");
      aml_add_dword(dev, SlotDevices[5].SlotID);
    } else {
      //we have name sun, set the number
      if (dsdt[k + 4] == 0x0A) {
        dsdt[k + 5] = SlotDevices[5].SlotID;
      }
    }
  }

  // add Method(_DSM,4,NotSerialized) for network
  if (gSettings.FakeLAN || !gSettings.NoDefaultProperties) {
    met = aml_add_method(dev, "_DSM", 4);
    met2 = aml_add_store(met);
    pack = aml_add_package(met2);

    aml_add_string(pack, "built-in");
    aml_add_byte_buffer(pack, dataBuiltin, sizeof(dataBuiltin));
    aml_add_string(pack, "model");
    //aml_add_string_buffer(pack, Netmodel);
    aml_add_string_buffer(pack, S_NETMODEL);
    aml_add_string(pack, "device_type");
    aml_add_string_buffer(pack, "Ethernet");

    if (gSettings.FakeLAN) {
      //    aml_add_string(pack, "model");
      //    aml_add_string_buffer(pack, "Apple LAN card");
      aml_add_string(pack, "device-id");
      aml_add_byte_buffer(pack, (CHAR8 *)&FakeID, 4);
      aml_add_string(pack, "vendor-id");
      aml_add_byte_buffer(pack, (CHAR8 *)&FakeVen, 4);
      aml_add_string(pack, "name");
      aml_add_string_buffer(pack, &NameCard[0]);
      aml_add_string(pack, "compatible");
      aml_add_string_buffer(pack, &NameCard[0]);
    }

    // Could we just comment this part? (Until remember what was the purposes?)
    if (
      !AddProperties(pack, DEV_LAN) &&
      !gSettings.FakeLAN &&
      !gSettings.NoDefaultProperties
    ) {
      aml_add_string(pack, "empty");
      aml_add_byte(pack, 0);
    }

    aml_add_local0(met2);
    aml_add_buffer(met, dtgp_1, sizeof(dtgp_1));
  }

  // finish Method(_DSM,4,NotSerialized)
  aml_calculate_size(root);
  network = AllocateZeroPool(root->Size);

  if (!network) {
    return len;
  }

  sizeoffset = root->Size;
  DBG(" - _DSM created, size=%x\n", sizeoffset);
  aml_write_node(root, network, 0);
  aml_destroy_node(root);

  if (NetworkADR) { // bridge or lan
    i = NetworkADR;
  } else { //this is impossible
    i = PCIADR;
  }

  Size = AcpiGetSize(dsdt, i);
  // move data to back for add patch
  k = i + Size;
  len = MoveData(k, dsdt, len, sizeoffset);
  CopyMem(dsdt+k, network, sizeoffset);
  // Fix Device network size
  k = WriteSize(i, dsdt, len, sizeoffset);
  sizeoffset += k;
  len += k;
  len = CorrectOuters(dsdt, len, i-3, sizeoffset);
  FreePool(network);

  return len;
}

//Airport--------------------------------------------------

CHAR8 dataBCM[]  = {0x12, 0x43, 0x00, 0x00};
CHAR8 data1ATH[] = {0x2a, 0x00, 0x00, 0x00};
CHAR8 data2ATH[] = {0x8F, 0x00, 0x00, 0x00};
CHAR8 data3ATH[] = {0x6B, 0x10, 0x00, 0x00};

UINT32
FIXAirport (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32      i, k, PCIADR, PCISIZE = 0, FakeID = 0, FakeVen = 0,
              ArptADR = 0, BridgeSize, Size, BrdADR = 0;
  INT32       sizeoffset;
  AML_CHUNK   *met, *met2, *brd, *root, *pack, *dev;
  CHAR8       *network, NameCard[32];
  UINTN       Len = ARRAY_SIZE(NameCard);

  if (!ArptADR1) {
   return len; // no device - no patch
  }

  if (gSettings.FakeWIFI) {
    FakeID = gSettings.FakeWIFI >> 16;
    FakeVen = gSettings.FakeWIFI & 0xFFFF;
    AsciiSPrint(NameCard, Len, "pci%x,%x\0", FakeVen, FakeID);
    AsciiStrCpyS(NameCard, Len, AsciiStrToLower(NameCard));
  }

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return len; //what is the bad DSDT ?!
  }

  DBG("Start Airport Fix\n");

  ArptName = FALSE;

  for (i=0x20; i<len-10; i++) {
    // AirPort Address
    if (CmpAdr(dsdt, i, ArptADR1)) {
      BrdADR = DevFind(dsdt, i);
      if (!BrdADR) {
        continue;
      }

      BridgeSize = AcpiGetSize(dsdt, BrdADR);
      if(!BridgeSize) {
        continue;
      }

      if (ArptADR2 != 0xFFFE){
        for (k = BrdADR + 9; k < BrdADR + BridgeSize; k++) {
          if (CmpAdr(dsdt, k, ArptADR2)) {
            ArptADR = DevFind(dsdt, k);

            if (!ArptADR) {
              continue;
            }

            device_name[9] = AllocateZeroPool(5);
            CopyMem(device_name[9], dsdt+k, 4);

            DBG("found Airport device [%08x:%x] at %x And Name is %a\n",
                ArptADR1, ArptADR2, ArptADR, device_name[9]);

            ReplaceName(dsdt + BrdADR, BridgeSize, device_name[9], "ARPT");
            ArptName = TRUE;
            break;
          }
        }
      }
      break;
    } // End ArptADR2
  }

  if (!ArptName) {
    ArptADR = BrdADR;
  }

  if (BrdADR) { // bridge or device
    i = ArptADR;
    Size = AcpiGetSize(dsdt, i);

    k = FindMethod(dsdt + i, Size, "_DSM");
    if (k != 0) {
      k += i;
      if ((gSettings.DropOEM_DSM & DEV_WIFI) != 0) {
        Size = AcpiGetSize(dsdt, k);
        sizeoffset = - 1 - Size;
        len = MoveData(k - 1, dsdt, len, sizeoffset);
        len = CorrectOuters(dsdt, len, k - 2, sizeoffset);
        DBG("_DSM in ARPT already exists, dropped\n");
      } else {
        DBG("_DSM already exists, patch ARPT will not be applied\n");
        return len;
      }
    }

    root = aml_create_node(NULL);
  } else { //what to do if no Arpt bridge?
    brd = aml_create_node(NULL);
    root = aml_add_device(brd, "ARP0");
    aml_add_name(root, "_ADR");
    aml_add_dword(root, ArptADR1);
    DBG("Created  bridge device with ADR=0x%x\n", ArptADR1);
  }

  dev = root;
  if (!ArptName && (ArptADR2 != 0xFFFE)) {//there is no Airport device at dsdt, creating new one
    dev = aml_add_device(root, "ARPT");
    aml_add_name(dev, "_ADR");
    if (ArptADR2) {
      if (ArptADR2> 0x3F) {
        aml_add_dword(dev, ArptADR2);
      } else {
        aml_add_byte(dev, (UINT8)ArptADR2);
      }
    } else {
      aml_add_byte(dev, 0x00);
    }
  }

  Size = AcpiGetSize(dsdt, i);
  k = FindMethod(dsdt + i, Size, "_SUN");

  if (k == 0) {
    k = FindName(dsdt + i, Size, "_SUN");
    if (k == 0) {
      aml_add_name(dev, "_SUN");
      aml_add_dword(dev, SlotDevices[6].SlotID);
    } else {
      //we have name sun, set the number
      if (dsdt[k + 4] == 0x0A) {
        dsdt[k + 5] = SlotDevices[6].SlotID;
      }
    }
  } else {
    DBG("Warning: Method(_SUN) found for airport\n");
  }

  // add Method(_DSM,4,NotSerialized) for network
  if (gSettings.FakeWIFI || !gSettings.NoDefaultProperties) {
    met = aml_add_method(dev, "_DSM", 4);
    met2 = aml_add_store(met);
    pack = aml_add_package(met2);

    if (!gSettings.NoDefaultProperties) {
      aml_add_string(pack, "built-in");
      aml_add_byte_buffer(pack, dataBuiltin, sizeof(dataBuiltin));
      aml_add_string(pack, "model");
      aml_add_string_buffer(pack, "Apple WiFi card");
      aml_add_string(pack, "subsystem-id");
      aml_add_byte_buffer(pack, data2ATH, 4);
      aml_add_string(pack, "subsystem-vendor-id");
      aml_add_byte_buffer(pack, data3ATH, 4);

      /*
        if (ArptBCM) {
          if (!gSettings.FakeWIFI) {
            aml_add_string(pack, "model");
            aml_add_string_buffer(pack, "Dell Wireless 1395");
            aml_add_string(pack, "name");
            aml_add_string_buffer(pack, "pci14e4,4312");
            aml_add_string(pack, "device-id");
            aml_add_byte_buffer(pack, dataBCM, 4);
          }
        } else if (ArptAtheros) {
          aml_add_string(pack, "model");
          aml_add_string_buffer(pack, "Atheros AR9285 WiFi card");
          if (!gSettings.FakeWIFI && ArptDID != 0x30) {
            aml_add_string(pack, "name");
            aml_add_string_buffer(pack, "pci168c,2a");
            aml_add_string(pack, "device-id");
            aml_add_byte_buffer(pack, data1ATH, 4);
          }
          aml_add_string(pack, "subsystem-id");
          aml_add_byte_buffer(pack, data2ATH, 4);
          aml_add_string(pack, "subsystem-vendor-id");
          aml_add_byte_buffer(pack, data3ATH, 4);
        }
      */

      aml_add_string(pack, "device_type");
      aml_add_string_buffer(pack, "Airport");
      //aml_add_string(pack, "AAPL,slot-name");
      //aml_add_string_buffer(pack, "AirPort");
    }

    if (gSettings.FakeWIFI) {
      aml_add_string(pack, "device-id");
      aml_add_byte_buffer(pack, (CHAR8 *)&FakeID, 4);
      aml_add_string(pack, "vendor-id");
      aml_add_byte_buffer(pack, (CHAR8 *)&FakeVen, 4);
      aml_add_string(pack, "name");
      aml_add_string_buffer(pack, (CHAR8 *)&NameCard[0]);
      aml_add_string(pack, "compatible");
      aml_add_string_buffer(pack, (CHAR8 *)&NameCard[0]);
    }

    if (
      !AddProperties(pack, DEV_WIFI) &&
      !gSettings.NoDefaultProperties &&
      !gSettings.FakeWIFI
    ) {
      aml_add_string(pack, "empty");
      aml_add_byte(pack, 0);
    }

    aml_add_local0(met);
    aml_add_buffer(met, dtgp_1, sizeof(dtgp_1));
  }
  // finish Method(_DSM,4,NotSerialized)

  aml_calculate_size(root);
  network = AllocateZeroPool(root->Size);
  sizeoffset = root->Size;
  aml_write_node(root, network, 0);
  aml_destroy_node(root);

  DBG("AirportADR=%x add patch size=%x\n", ArptADR, sizeoffset);

  if (ArptADR) { // bridge or WiFi
    i = ArptADR;
  } else { //this is impossible
    i = PCIADR;
  }

  Size = AcpiGetSize(dsdt, i);
  DBG("adr %x size of arpt=%x\n", i, Size);
  // move data to back for add patch
  k = i + Size;
  len = MoveData(k, dsdt, len, sizeoffset);
  CopyMem(dsdt+k, network, sizeoffset);
  // Fix Device size
  k = WriteSize(i, dsdt, len, sizeoffset);
  sizeoffset += k;
  len += k;
  len = CorrectOuters(dsdt, len, i-3, sizeoffset);
  FreePool(network);

  return len;
}

UINT32
AddMCHC (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32      i, k = 0, PCIADR, PCISIZE = 0; //, Size;
  INT32       sizeoffset;
  AML_CHUNK   *root, *device; //*met, *met2; *pack;
  CHAR8       *mchc;

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    //DBG("wrong PCI0 address, patch MCHC will not be applied\n");
    return len;
  }

  //Find Device MCHC by name
  for (i=0x20; i<len-10; i++) {
    k = CmpDev(dsdt, i, (UINT8*)"MCHC");
    if (k != 0) {
      DBG("device name (MCHC) found at %x, don't add!\n", k);
      //break;
      return len;
    }
  }

  DBG("Start Add MCHC\n");
  root = aml_create_node(NULL);

  device = aml_add_device(root, "MCHC");
  aml_add_name(device, "_ADR");
  aml_add_byte(device, 0x00);

  aml_calculate_size(root);
  mchc = AllocateZeroPool(root->Size);
  sizeoffset = root->Size;
  aml_write_node(root, mchc, 0);
  aml_destroy_node(root);
  // always add on PCIX back
  PCISIZE = AcpiGetSize(dsdt, PCIADR);
  len = MoveData(PCIADR + PCISIZE, dsdt, len, sizeoffset);
  CopyMem(dsdt + PCIADR + PCISIZE, mchc, sizeoffset);
  // Fix PCIX size
  k = WriteSize(PCIADR, dsdt, len, sizeoffset);
  sizeoffset += k;
  len += k;
  len = CorrectOuters(dsdt, len, PCIADR-3, sizeoffset);
  FreePool(mchc);

  return len;
}



UINT32
AddIMEI (
  UINT8     *dsdt,
  UINT32    len
) {
  UINT32      i, k = 0, PCIADR, PCISIZE = 0, FakeID, FakeVen;
  INT32       sizeoffset;
  AML_CHUNK   *root, *device, *met, *met2, *pack;
  CHAR8       *imei;

  if (gSettings.FakeIMEI) {
    FakeID = gSettings.FakeIMEI >> 16;
    FakeVen = gSettings.FakeIMEI & 0xFFFF;
  }

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    //DBG("wrong PCI0 address, patch IMEI will not be applied\n");
    return len;
  }

  // Find Device IMEI
  if (IMEIADR1) {
    for (i=0x20; i<len-10; i++) {
      if (CmpAdr(dsdt, i, IMEIADR1)) {
        k = DevFind(dsdt, i);
        if (k) {
          DBG("device (IMEI) found at %x, don't add!\n", k);
          //break;
          return len;
        }
      }
    }
  }

  //Find Device IMEI by name
  for (i=0x20; i<len-10; i++) {
    k = CmpDev(dsdt, i, (UINT8*)"IMEI");
    if (k != 0) {
      DBG("device name (IMEI) found at %x, don't add!\n", k);
      return len;
    }
  }

  DBG("Start Add IMEI\n");
  root = aml_create_node(NULL);
  device = aml_add_device(root, "IMEI");
  aml_add_name(device, "_ADR");
  aml_add_dword(device, IMEIADR1);

  // add Method(_DSM,4,NotSerialized)
  if (gSettings.FakeIMEI) {
    met = aml_add_method(device, "_DSM", 4);
    met2 = aml_add_store(met);
    pack = aml_add_package(met2);

    aml_add_string(pack, "device-id");
    aml_add_byte_buffer(pack, (CHAR8*)&FakeID, 4);
    aml_add_string(pack, "vendor-id");
    aml_add_byte_buffer(pack, (CHAR8*)&FakeVen, 4);

    aml_add_local0(met2);
    aml_add_buffer(met, dtgp_1, sizeof(dtgp_1));
    //finish Method(_DSM,4,NotSerialized)
  }

  aml_calculate_size(root);
  imei = AllocateZeroPool(root->Size);
  sizeoffset = root->Size;
  aml_write_node(root, imei, 0);
  aml_destroy_node(root);

  // always add on PCIX back
  len = MoveData(PCIADR+PCISIZE, dsdt, len, sizeoffset);
  CopyMem(dsdt+PCIADR+PCISIZE, imei, sizeoffset);
  // Fix PCIX size
  k = WriteSize(PCIADR, dsdt, len, sizeoffset);
  sizeoffset += k;
  len += k;
  len = CorrectOuters(dsdt, len, PCIADR-3, sizeoffset);

  FreePool(imei);

  return len;
}

UINT32
AddHDEF (
  UINT8     *dsdt,
  UINT32    len,
  CHAR8     *OSVersion
) {
  UINT32        i, k, PCIADR, PCISIZE = 0, HDAADR = 0, Size;
  INT32         sizeoffset;
  AML_CHUNK     *root, *met, *met2, *device, *pack;
  CHAR8         *hdef;

  PCIADR = GetPciDevice(dsdt, len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize(dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return len; //what is the bad DSDT ?!
  }

  DBG("Start HDA Fix\n");
  //len = DeleteDevice("AZAL", dsdt, len);

  // HDA Address
  for (i=0x20; i<len-10; i++) {
    if (
      (HDAADR1 != 0x00000000) &&
      HDAFIX &&
      CmpAdr(dsdt, i, HDAADR1)
    ) {
      HDAADR = DevFind(dsdt, i);
      if (!HDAADR) {
        continue;
      }

      //BridgeSize = AcpiGetSize(dsdt, HDAADR);
      device_name[4] = AllocateZeroPool(5);
      CopyMem(device_name[4], dsdt+i, 4);

      DBG("found HDA device NAME(_ADR,0x%08x) And Name is %a\n",
          HDAADR1, device_name[4]);

      ReplaceName(dsdt, len, device_name[4], "HDEF");
      HDAFIX = FALSE;
      break;
    } // End HDA
  }

  if (HDAADR) { // bridge or device
    i = HDAADR;
    Size = AcpiGetSize(dsdt, i);
    k = FindMethod(dsdt + i, Size, "_DSM");

    if (k != 0) {
      k += i;
      if ((gSettings.DropOEM_DSM & DEV_HDA) != 0) {
        Size = AcpiGetSize(dsdt, k);
        sizeoffset = - 1 - Size;
        len = MoveData(k - 1, dsdt, len, sizeoffset);
        len = CorrectOuters(dsdt, len, k - 2, sizeoffset);
        DBG("_DSM in HDA already exists, dropped\n");
      } else {
        DBG("_DSM already exists, patch HDA will not be applied\n");
        return len;
      }
    }
  }

  root = aml_create_node(NULL);
  if (HDAFIX) {
    DBG("Start Add Device HDEF\n");
    device = aml_add_device(root, "HDEF");
    aml_add_name(device, "_ADR");
    aml_add_dword(device, HDAADR1);

    // add Method(_DSM,4,NotSerialized)
    met = aml_add_method(device, "_DSM", 4);
  } else {
    met = aml_add_method(root, "_DSM", 4);
  }

  met2 = aml_add_store(met);
  pack = aml_add_package(met2);

  if (gSettings.UseIntelHDMI) {
    aml_add_string(pack, "hda-gfx");
    aml_add_string_buffer(pack, "onboard-1");
  }

  if (!AddProperties(pack, DEV_HDA)) {
    if (
      (gSettings.HDALayoutId > 0) ||
      //((OSVersion != NULL) && AsciiOSVersionToUint64(OSVersion) < AsciiOSVersionToUint64("10.8"))
      OSX_LT(OSVersion, "10.8")
    ) {
      aml_add_string(pack, "layout-id");
      aml_add_byte_buffer(pack, (CHAR8*)&HDAlayoutId, 4);
    }

    aml_add_string(pack, "MaximumBootBeepVolume");
    aml_add_byte_buffer(pack, (CHAR8*)&dataBuiltin1[0], 1);

    aml_add_string(pack, "PinConfigurations");
    aml_add_byte_buffer(pack, 0, 0);//data, sizeof(data));
  }

  aml_add_local0(met2);
  aml_add_buffer(met, dtgp_1, sizeof(dtgp_1));
  // finish Method(_DSM,4,NotSerialized)

  aml_calculate_size(root);
  hdef = AllocateZeroPool(root->Size);
  sizeoffset = root->Size;
  aml_write_node(root, hdef, 0);
  aml_destroy_node(root);

  if (!HDAFIX) { // bridge or device
    i = HDAADR;
  } else {
    i = PCIADR;
  }

  Size = AcpiGetSize(dsdt, i);
  // move data to back for add patch
  k = i + Size;
  len = MoveData(k, dsdt, len, sizeoffset);
  CopyMem(dsdt + k, hdef, sizeoffset);
  // Fix Device size
  k = WriteSize(i, dsdt, len, sizeoffset);
  sizeoffset += k;
  len += k;
  len = CorrectOuters(dsdt, len, i-3, sizeoffset);
  FreePool(hdef);

  return len;
}

VOID
FixBiosDsdt (
  UINT8                                       *temp,
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *fadt,
  CHAR8                                       *OSVersion
) {
  UINT32    DsdtLen;

  if (!temp) {
    return;
  }

  //Reenter requires ZERO values
  HDAFIX = TRUE;
  GFXHDAFIX = TRUE;
  //USBIDFIX = TRUE;

  DsdtLen = ((EFI_ACPI_DESCRIPTION_HEADER*)temp)->Length;
  if ((DsdtLen < 20) || (DsdtLen > 400000)) { //fool proof (some ASUS dsdt > 300kb?)
    DBG("DSDT length out of range\n");
    return;
  }

  //DBG("========= Auto patch DSDT Starting ========\n");
  DbgHeader("FixBiosDsdt");

  // First check hardware address: GetPciADR(DevicePath, &NetworkADR1, &NetworkADR2);
  CheckHardware();

  //arbitrary fixes
  if (gSettings.PatchDsdtNum > 0) {
    UINTN   i;

    MsgLog("Patching DSDT:\n");

    for (i = 0; i < gSettings.PatchDsdtNum; i++) {
      if (!gSettings.PatchDsdtFind[i] || !gSettings.LenToFind[i]) {
        continue;
      }

      MsgLog(" - [%02d]:", i);

      DsdtLen = FixAny (
                  temp, DsdtLen,
                  gSettings.PatchDsdtFind[i], gSettings.LenToFind[i],
                  gSettings.PatchDsdtReplace[i], gSettings.LenToReplace[i]
                );
    }
  }

  // find ACPI CPU name and hardware address
  FindCPU(temp, DsdtLen);

  if (!FindMethod(temp, DsdtLen, "DTGP")) {
    CopyMem((CHAR8 *)temp+DsdtLen, dtgp, sizeof(dtgp));
    DsdtLen += sizeof(dtgp);
    ((EFI_ACPI_DESCRIPTION_HEADER*)temp)->Length = DsdtLen;
  }

  gSettings.PCIRootUID = (UINT16)FindPciRoot(temp, DsdtLen);

  // Fix Display
  if (
    (gSettings.FixDsdt & FIX_DISPLAY) ||
    (gSettings.FixDsdt & FIX_INTELGFX)
  ) {
    INT32   i;

    for (i=0; i<4; i++) {
      if (DisplayADR1[i]) {
        if (
          ((DisplayVendor[i] != 0x8086) && (gSettings.FixDsdt & FIX_DISPLAY)) ||
          ((DisplayVendor[i] == 0x8086) && (gSettings.FixDsdt & FIX_INTELGFX))
        ) {
          DsdtLen = FIXDisplay(temp, DsdtLen, i);
          DBG("patch Display #%d of Vendor=0x%4x in DSDT new way\n", i, DisplayVendor[i]);
        }
      }
    }
  }

  // Fix Network
  if (NetworkADR1 && (gSettings.FixDsdt & FIX_LAN)) {
    //DBG("patch LAN in DSDT \n");
    DsdtLen = FIXNetwork(temp, DsdtLen);
  }

  // Fix Airport
  if (ArptADR1 && (gSettings.FixDsdt & FIX_WIFI)) {
    //DBG("patch Airport in DSDT \n");
    DsdtLen = FIXAirport(temp, DsdtLen);
  }

  // HDA HDEF
  if (HDAFIX  && (gSettings.FixDsdt & FIX_HDA)) {
    DBG("patch HDEF in DSDT \n");
    DsdtLen = AddHDEF(temp, DsdtLen, OSVersion);
  }

  //Always add MCHC for PM
  if ((gCPUStructure.Family == 0x06)  && (gSettings.FixDsdt & FIX_MCHC)) {
    //DBG("patch MCHC in DSDT \n");
    DsdtLen = AddMCHC(temp, DsdtLen);
  }

  //add IMEI
  if ((gSettings.FixDsdt & FIX_MCHC) || (gSettings.FixDsdt & FIX_IMEI)) {
    DsdtLen = AddIMEI(temp, DsdtLen);
  }

  //Add HDMI device
  if (
    gSettings.FixDsdt & FIX_HDMI
  ) {
    DsdtLen = AddHDMI(temp, DsdtLen);
  }

  if (
    gSettings.FixDsdt & FIX_PNLF
  ) {
    DsdtLen = AddPNLF(temp, DsdtLen);
  }

  // Finish DSDT patch and resize DSDT Length
  temp[4] = (DsdtLen & 0x000000FF);
  temp[5] = (UINT8)((DsdtLen & 0x0000FF00) >>  8);
  temp[6] = (UINT8)((DsdtLen & 0x00FF0000) >> 16);
  temp[7] = (UINT8)((DsdtLen & 0xFF000000) >> 24);

  CopyMem((UINT8*)((EFI_ACPI_DESCRIPTION_HEADER*)temp)->OemId, (UINT8*)BiosVendor, 6);
  //DBG("orgBiosDsdtLen = 0x%08x\n", orgBiosDsdtLen);
  ((EFI_ACPI_DESCRIPTION_HEADER*)temp)->Checksum = 0;
  ((EFI_ACPI_DESCRIPTION_HEADER*)temp)->Checksum = (UINT8)(256-Checksum8(temp, DsdtLen));

  //DBG("========= Auto patch DSDT Finished ========\n");
  //PauseForKey(L"waiting for key press...\n");
}
