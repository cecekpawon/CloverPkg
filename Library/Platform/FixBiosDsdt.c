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

#define DBG(...) DebugLog (DEBUG_FIX_DSDT, __VA_ARGS__)

#define S_NETMODEL "Generic Ethernet"

//OPER_REGION   *gRegions;

// 0=>Display  1=>network  2=>firewire 3=>LPCB 4=>HDAAudio 5=>RTC 6=>TMR 7=>SBUS 8=>PIC 9=>Airport 10=>XHCI 11=>HDMI
CHAR8  *DeviceName[12];
//CHAR8  *UsbName[10];
//CHAR8  *Netmodel;

CHAR8 DataBuiltin[]  = { 0x00 };
CHAR8 DataBuiltin1[] = { 0x01 };
//UINT8 Slash[]        = { 0x5c, 0 };

BOOLEAN     HDAFIX = TRUE;
BOOLEAN     GFXHDAFIX = TRUE;

BOOLEAN     DisplayName1;
BOOLEAN     DisplayName2;

BOOLEAN     NetworkName;
BOOLEAN     ArptBCM;
BOOLEAN     ArptAtheros;
UINT16      ArptDevID;

UINT32      IMEIADR1;
UINT32      IMEIADR2;

// for read computer data

//UINT32    HDAcodecId = 0;
UINT32      HDAlayoutId = 0;
UINT32      HDAADR1;
UINT32      HDMIADR1;
UINT32      HDMIADR2;

UINT32      DisplayADR1[4];
UINT32      DisplayADR2[4];
UINT32      DisplayVendor[4];
UINT16      DisplayID[4];
UINT32      DisplaySubID[4];
//UINT32    GfxcodecId[2] = {0, 1};
UINT32      GfxlayoutId[2] = { 1, 12 };
PCI_DT      Displaydevice[2];

//UINT32 IMEIADR1;
//UINT32 IMEIADR2;

UINT32      NetworkADR1;
UINT32      NetworkADR2;
BOOLEAN     ArptName;
UINT32      ArptADR1;
UINT32      ArptADR2;

CHAR8       ClassFix[] =  { 0x00, 0x00, 0x03, 0x00 };

UINT8       Dtgp[] = {                                              // Method (DTGP, 5, NotSerialized) ......
   0x14, 0x3F, 0x44, 0x54, 0x47, 0x50, 0x05, 0xA0,
   0x30, 0x93, 0x68, 0x11, 0x13, 0x0A, 0x10, 0xC6,
   0xB7, 0xB5, 0xA0, 0x18, 0x13, 0x1C, 0x44, 0xB0,
   0xC9, 0xFE, 0x69, 0x5E, 0xAF, 0x94, 0x9B, 0xA0,
   0x18, 0x93, 0x69, 0x01, 0xA0, 0x0C, 0x93, 0x6A,
   0x00, 0x70, 0x11, 0x03, 0x01, 0x03, 0x6C, 0xA4,
   0x01, 0xA0, 0x06, 0x93, 0x6A, 0x01, 0xA4, 0x01,
   0x70, 0x11, 0x03, 0x01, 0x00, 0x6C, 0xA4, 0x00
};

CHAR8       Dtgp1[] = {
   0x44, 0x54, 0x47, 0x50, 0x68, 0x69, 0x6A, 0x6B,                  // DTGP (Arg0, Arg1, Arg2, Arg3, RefOf (Local0))
   0x71, 0x60, 0xA4, 0x60                                           // Return (Local0)
};

CHAR8       Pnlf[] = {
  0x5B, 0x82, 0x2D, 0x50, 0x4E, 0x4C, 0x46,                         //Device (PNLF)
  0x08, 0x5F, 0x48, 0x49, 0x44, 0x0C, 0x06, 0x10, 0x00, 0x02,       //  Name (_HID, EisaId ("APP0002"))
  0x08, 0x5F, 0x43, 0x49, 0x44,                                     //  Name (_CID,
  0x0D, 0x62, 0x61, 0x63, 0x6B, 0x6C, 0x69, 0x67, 0x68, 0x74, 0x00, //              "backlight")
  0x08, 0x5F, 0x55, 0x49, 0x44, 0x0A, 0x0A,                         //  Name (_UID, 0x0A)
  0x08, 0x5F, 0x53, 0x54, 0x41, 0x0A, 0x0B                          //  Name (_STA, 0x0B)
};

CHAR8       App2[] = { //Name (_HID, EisaId ("APP0002"))
  0x08, 0x5F, 0x48, 0x49, 0x44, 0x0C, 0x06, 0x10, 0x00, 0x02
};

STATIC
BOOLEAN
CmpNum (
  UINT8     *Dsdt,
  INT32     i,
  BOOLEAN   Sure
) {
  return  (
            (Sure &&  (
                        (Dsdt[i - 1] == 0x0A) ||
                        (Dsdt[i - 2] == 0x0B) ||
                        (Dsdt[i - 4] == 0x0C)
                      )
            ) ||
            (!Sure && (
                        ((Dsdt[i - 1] >= 0x0A) && (Dsdt[i - 1] <= 0x0C)) ||
                        ((Dsdt[i - 2] == 0x0B) || (Dsdt[i - 2] == 0x0C)) ||
                        (Dsdt[i - 4] == 0x0C)
                      )
            )
          );
}

STATIC
VOID
GetPciADR (
  IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  OUT UINT32                    *Addr1,
  OUT UINT32                    *Addr2,
  OUT UINT32                    *Addr3
) {
  UINTN                       PciNodeCount;
  PCI_DEVICE_PATH             *PciNode;
  EFI_DEVICE_PATH_PROTOCOL    *TmpDevicePath = DuplicateDevicePath (DevicePath);

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
  while (TmpDevicePath && !IsDevicePathEndType (TmpDevicePath)) {
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

    TmpDevicePath = NextDevicePathNode (TmpDevicePath);
  }

  return;
}

STATIC
VOID
CheckHardware () {
  EFI_STATUS                  Status;
  EFI_HANDLE                  *HandleBuffer = NULL, Handle;
  EFI_PCI_IO_PROTOCOL         *PciIo;
  PCI_TYPE00                  Pci;
  UINTN                       HandleCount = 0, HandleIndex, Segment,
                              Bus, Device, Function, Display = 0;
  PCI_DT                      PCIdevice;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath = NULL;

   //usb = 0;
  // Scan PCI handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );

  if (!EFI_ERROR (Status)) {
    //DBG ("PciIo handles count=%d\n", HandleCount);
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
      Handle = HandleBuffer[HandleIndex];
      Status = gBS->HandleProtocol (
                      Handle,
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&PciIo
                    );

      if (!EFI_ERROR (Status)) {
        UINT32    DeviceId;

        /* Read PCI BUS */
        PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        Status = PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              sizeof (Pci) / sizeof (UINT32),
                              &Pci
                            );

        DeviceId = Pci.Hdr.DeviceId | (Pci.Hdr.VendorId << 16);

        // add for auto patch dsdt get DSDT Device _ADR
        PCIdevice.DeviceHandle = Handle;
        DevicePath = DevicePathFromHandle (Handle);
        if (DevicePath) {
          //DBG ("Device patch = %s \n", DevicePathToStr (DevicePath));

          //Display ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_DISPLAY) &&
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_VGA)
          ) {

            #if DEBUG_FIX
            UINT32    DAdr1, DAdr2;
            #endif

            //PCI_IO_DEVICE *PciIoDevice;

            GetPciADR (DevicePath, &DisplayADR1[Display], &DisplayADR2[Display], NULL);
            DBG ("VideoCard devID = 0x%x\n", ((Pci.Hdr.DeviceId << 16) | Pci.Hdr.VendorId));

            #if DEBUG_FIX
            DAdr1 = DisplayADR1[Display];
            DAdr2 = DisplayADR2[Display];
            DBG (" - DisplayADR1[%02d]: 0x%x, DisplayADR2[%02d] = 0x%x\n", Display, DAdr1, Display, DAdr2);
            #endif

                 //DAdr2 = DAdr1; //to avoid warning "unused variable" :(
            DisplayVendor[Display] = Pci.Hdr.VendorId;
            DisplayID[Display] = Pci.Hdr.DeviceId;
            DisplaySubID[Display] = (Pci.Device.SubsystemID << 16) | (Pci.Device.SubsystemVendorID << 0);
            // for get Display data
            Displaydevice[Display].DeviceHandle = HandleBuffer[HandleIndex];
            Displaydevice[Display].dev.addr = (UINT32)PCIADDR (Bus, Device, Function);
            Displaydevice[Display].vendor_id = Pci.Hdr.VendorId;
            Displaydevice[Display].device_id = Pci.Hdr.DeviceId;
            Displaydevice[Display].revision = Pci.Hdr.RevisionID;
            Displaydevice[Display].subclass = Pci.Hdr.ClassCode[0];
            Displaydevice[Display].class_id = *((UINT16 *)(Pci.Hdr.ClassCode + 1));
            Displaydevice[Display].subsys_id.subsys.vendor_id = Pci.Device.SubsystemVendorID;
            Displaydevice[Display].subsys_id.subsys.device_id = Pci.Device.SubsystemID;

            //
            // Detect if PCI Express Device
            //
            //
            // Go through the Capability list
            //unused
            /*
            PciIoDevice = PCI_IO_DEVICE_FROM_PCI_IO_THIS (PciIo);
            if (PciIoDevice->IsPciExp) {
              if (Display==0)
                Display1PCIE = TRUE;
              else
                Display2PCIE = TRUE;
            }
            DBG ("Display %d is %aPCIE\n", Display, (PciIoDevice->IsPciExp) ? "" :" not");
            */

            Display++;
          }

          //Network ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_ETHERNET)
          ) {
            GetPciADR (DevicePath, &NetworkADR1, &NetworkADR2, NULL);
            //DBG ("NetworkADR1 = 0x%x, NetworkADR2 = 0x%x\n", NetworkADR1, NetworkADR2);
            //Netmodel = get_net_model (DeviceId);
          }

          //Network WiFi ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_NETWORK) &&
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_NETWORK_OTHER)
          ) {
            GetPciADR (DevicePath, &ArptADR1, &ArptADR2, NULL);
            //DBG ("ArptADR1 = 0x%x, ArptADR2 = 0x%x\n", ArptADR1, ArptADR2);
            //Netmodel = get_arpt_model (DeviceId);
            ArptBCM = (Pci.Hdr.VendorId == 0x14e4);

            if (ArptBCM) {
              DBG ("Found Airport BCM at 0x%x, 0x%x\n", ArptADR1, ArptADR2);
            }

            ArptAtheros = (Pci.Hdr.VendorId == 0x168c);
            ArptDevID = Pci.Hdr.DeviceId;

            if (ArptAtheros) {
              DBG ("Found Airport Atheros at 0x%x, 0x%x, DeviceID = 0x%04x\n", ArptADR1, ArptADR2, ArptDevID);
            }
          }

          //IMEI ADR
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_SCC) &&
            (Pci.Hdr.ClassCode[1] == PCI_SUBCLASS_SCC_OTHER)
          ) {
            GetPciADR (DevicePath, &IMEIADR1, &IMEIADR2, NULL);
          }

          // HDA and HDMI Audio
          if (
            (Pci.Hdr.ClassCode[2] == PCI_CLASS_MEDIA) &&
            ((Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_HDA) ||
            (Pci.Hdr.ClassCode[1] == PCI_CLASS_MEDIA_AUDIO))
          ) {
            UINT32    /*codecId = 0,*/ layoutId = 0;

            if (!IsHDMIAudio (Handle)) {
              if (gSettings.HDALayoutId > 0) {
                // layoutId is specified - use it
                layoutId = (UINT32)gSettings.HDALayoutId;
                DBG ("Audio HDA (addr:0x%x) setting specified layout - id=%d\n", HDAADR1, layoutId);
              }

              HDAFIX = TRUE;
              //HDAcodecId = codecId;
              HDAlayoutId = layoutId;
            } else { //HDMI
              GetPciADR (DevicePath, &HDMIADR1, &HDMIADR2, NULL);
              GFXHDAFIX = TRUE;
            }
          }
        }
        // detected finish
      }
    }
  }
}

STATIC
VOID
InsertScore (
  UINT8     *Dsdt,
  UINT32    Off,
  INTN      Root
) {
  UINT8     NumNames = 0;
  UINT32    Ind = 0, i;
  CHAR8     Buf[31];

  if (Dsdt[Off + Root] == 0x2E) {
    NumNames = 2;
    Off += (UINT32)(Root + 1);
  } else if (Dsdt[Off + Root] == 0x2F) {
    NumNames = Dsdt[Off + Root + 1];
    Off += (UINT32)(Root + 2);
  } else if (Dsdt[Off + Root] != 0x00) {
    NumNames = 1;
    Off += (UINT32)Root;
  }

  if (NumNames > 4) {
    DBG (" strange NumNames=%d\n", NumNames);
    NumNames = 1;
  }

  NumNames *= 4;
  CopyMem (Buf + Ind, Dsdt + Off, NumNames);
  Ind += NumNames;
  // apianti - This generates a memcpy call
  /*
  for (i = 0; i < NumNames; i++) {
    Buf[Ind++] = Dsdt[Off + i];
  }
  */

  i = 0;
  while (i < 127) {
    Buf[Ind++] = AcpiCPUScore[i];
    if (AcpiCPUScore[i] == 0) {
      break;
    }

    i++;
  }

  CopyMem (AcpiCPUScore, Buf, Ind);
  AcpiCPUScore[Ind] = 0;
}

STATIC
VOID
FindCPU (
  UINT8     *Dsdt,
  UINT32    Length
) {
  UINT32      i, k, Size, SBSIZE = 0, SBADR = 0,
              Off, j1;
  BOOLEAN     SBFound = FALSE;

  if (AcpiCPUScore) {
    FreePool (AcpiCPUScore);
  }

  AcpiCPUScore = AllocateZeroPool (128);
  AcpiCPUCount = 0;

  for (i = 0; i < Length - 20; i++) {
    if ((Dsdt[i] == 0x5B) && (Dsdt[i + 1] == 0x83)) { // ProcessorOP
      UINT32    j, Offset = i + 3 + (Dsdt[i + 2] >> 6); // name
      BOOLEAN   AddName = TRUE;

      if (AcpiCPUCount == 0) {         //only first time in the cycle
        CHAR8   C1 = Dsdt[Offset + 1];

        // I want to determine a scope of PR
        //1. if name begin with \\ this is with score
        //2. else find outer device or scope until \\ is found
        //3. add new name everytime is found
        DBG ("first CPU found at %x offset %x\n", i, Offset);

        if (Dsdt[Offset] == '\\') {
          // "\_PR.CPU0"
          j = 1;

          if (C1 == 0x2E) {
            j = 2;
          } else if (C1 == 0x2F) {
            C1 = Dsdt[Offset + 2];
            j = 2 + (C1 - 2) * 4;
          }

          CopyMem (AcpiCPUScore, Dsdt + Offset + j, 4);
          DBG ("slash found\n");
        } else {
          //--------
          j = i - 1; //usually Adr = &5B - 1 = sizefield - 3
          while (j > 0x24) {  //find devices that previous to Adr
            //check device
            k = j + 2;

            if (
              (Dsdt[j] == 0x5B) &&
              (Dsdt[j + 1] == 0x82) &&
              !CmpNum (Dsdt, j, TRUE)
            ) { //device candidate
              DBG ("device candidate at %x\n", j);

              Size = AcpiGetSize (Dsdt, k);

              if (Size) {
                if (k + Size > i + 3) {  //Yes - it is outer
                  Off = j + 3 + (Dsdt[j + 2] >> 6);

                  if (Dsdt[Off] == '\\') {
                    // "\_SB.SCL0"
                    InsertScore (Dsdt, Off, 1);
                    DBG ("AcpiCPUScore calculated as %a\n", AcpiCPUScore);
                    break;
                  } else {
                    InsertScore (Dsdt, Off, 0);
                    DBG ("device inserted in AcpiCPUScore %a\n", AcpiCPUScore);
                  }
                }  //else not an outer device
              } //else wrong size field - not a device
            } //else not a device

            // check scope
            // a problem 45 43 4F 4E 08   10 84 10 05 5F 53 42 5F
            SBSIZE = 0;

            if (
              (Dsdt[j] == '_' && Dsdt[j + 1] == 'S' && Dsdt[j + 2] == 'B' && Dsdt[j + 3] == '_') ||
              (Dsdt[j] == '_' && Dsdt[j + 1] == 'P' && Dsdt[j + 2] == 'R' && Dsdt[j + 3] == '_')
            ) {
              DBG ("score candidate at %x\n", j);

              for (j1 = 0; j1 < 10; j1++) {
                if (Dsdt[j - j1] != 0x10) {
                  continue;
                }

                if (!CmpNum (Dsdt, j - j1, TRUE)) {
                  SBADR = j - j1 + 1;
                  SBSIZE = AcpiGetSize (Dsdt, SBADR);

                  //     DBG ("found Scope (\\_SB) address = 0x%08x size = 0x%08x\n", SBADR, SBSIZE);
                  if ((SBSIZE != 0) && (SBSIZE < Length)) {  //if zero or too large then search more
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
              InsertScore (Dsdt, j, 0);
              DBG ("score inserted in AcpiCPUScore %a\n", AcpiCPUScore);
              break;
            }
            j = k - 3;    //if found then search again from found
          } //while j
          //--------
        }
      }

      for (j = 0; j < 4; j++) {
        CHAR8   C = Dsdt[Offset + j],
                C1 = Dsdt[Offset + j + 1];

        if (C == '\\') {
          Offset += 5;
          if (C1 == 0x2E) {
            Offset++;
          } else if (C1 == 0x2F) {
            C1 = Dsdt[Offset + j + 2];
            Offset += 2 + (C1 - 2) * 4;
          }
          C = Dsdt[Offset + j];
        }

        if (!(IS_UPPER (C) || IS_DIGIT (C) || C == '_')) {
          AddName = FALSE;
          DBG ("Invalid character found in ProcessorOP 0x%x!\n", C);
          break;
        }
      }

      if (AddName) {
        AcpiCPUName[AcpiCPUCount] = AllocateZeroPool (5);
        CopyMem (AcpiCPUName[AcpiCPUCount], Dsdt + Offset, 4);
        i = Offset + 5;

        if (AcpiCPUCount == 0) {
          DBG ("Found ACPI CPU: %a ", AcpiCPUName[AcpiCPUCount]);
        } else {
          DBG ("| %a ", AcpiCPUName[AcpiCPUCount]);
        }

        if (++AcpiCPUCount == 32) {
          break;
        }
      }
    }
  }

  DBG (", within the score: %a\n", AcpiCPUScore);

  if (!AcpiCPUCount) {
    for (i = 0; i < 15; i++) {
      AcpiCPUName[i] = AllocateZeroPool (5);
      AsciiSPrint (AcpiCPUName[i], 5, "CPU%1x", i);
    }
  }

  return;
}

//                start => move data start address
//                offset => data move how many byte
//                len => initial length of the buffer
// return final length of the buffer
// we suppose that buffer allocation is more then len + offset

STATIC
UINT32
MoveData (
  UINT32    Start,
  UINT8     *Buffer,
  UINT32    Len,
  INT32     Offset
) {
  UINT32    i;

  if (Offset < 0) {
    for (i = Start; i < Len + Offset; i++) {
      Buffer[i] = Buffer[i - Offset];
    }
  } else  if (Offset > 0) { // data move to back
    for (i = Len - 1; i >= Start; i--) {
      Buffer[i + Offset] = Buffer[i];
    }
  }

  return Len + Offset;
}

UINT32
AcpiGetSize (
  UINT8     *Buffer,
  UINT32    Adr
) {
  UINT32    Temp;

  Temp = Buffer[Adr] & 0xF0; //keep bits 0x30 to check if this is valid size field

  if (Temp <= 0x30)  {       // 0
    Temp = Buffer[Adr];
  } else if (Temp == 0x40) {   // 4
    Temp =  (Buffer[Adr]   - 0x40)    << 0 |
             Buffer[Adr + 1]          << 4;
  } else if (Temp == 0x80) {   // 8
    Temp = (Buffer[Adr]   - 0x80)     <<  0 |
            Buffer[Adr + 1]           <<  4 |
            Buffer[Adr + 2]           << 12;
  } else if (Temp == 0xC0) {   // C
    Temp = (Buffer[Adr]   - 0xC0)     <<  0 |
            Buffer[Adr + 1]           <<  4 |
            Buffer[Adr + 2]           << 12 |
            Buffer[Adr + 3]           << 20;
  } else {
    //DBG ("wrong pointer to size field at %x\n", Adr);
    return 0;
  }

  return Temp;
}

STATIC
INT32
WriteSize (
  UINT32  Adr,
  UINT8   *Buffer,
  UINT32  Len,
  INT32   SizeOffset
) {
  UINT32  Size, OldSize;
  INT32   Offset = 0;

  OldSize = AcpiGetSize (Buffer, Adr);
  if (!OldSize) {
    return 0; //wrong address, will not write here
  }

  Size = OldSize + SizeOffset;
  // data move to back
  if ((OldSize <= 0x3f) && (Size > 0x0fff)) {
    Offset = 2;
  } else if (((OldSize <= 0x3f) && (Size > 0x3f)) || ((OldSize <= 0x0fff) && (Size > 0x0fff))) {
    Offset = 1;
  } else if (((Size <= 0x3f) && (OldSize > 0x3f)) || ((Size <= 0x0fff) && (OldSize > 0x0fff))) {
    // data move to front
    Offset = -1;
  } else if ((OldSize > 0x0fff) && (Size <= 0x3f)) {
    Offset = -2;
  }

  Len = MoveData (Adr, Buffer, Len, Offset);
  Size += Offset;
  AmlWriteSize (Size, (CHAR8 *)Buffer, Adr); //reuse existing codes

  return Offset;
}

STATIC
INT32
FindName (
  UINT8     *Dsdt,
  INT32     Len,
  CHAR8     *Name
) {
  INT32   i;

  for (i = 0; i < Len - 5; i++) {
    if (
      (Dsdt[i] == 0x08) && (Dsdt[i + 1] == Name[0]) &&
      (Dsdt[i + 2] == Name[1]) && (Dsdt[i + 3] == Name[2]) &&
      (Dsdt[i + 4] == Name[3])
    ) {
      return i + 1;
    }
  }

  return 0;
}

STATIC
BOOLEAN
GetName (
      UINT8   *Dsdt,
      INT32   Adr,
      CHAR8   *Name,
  OUT INTN    *Shift
) {
  INT32 i, j = (Dsdt[Adr] == 0x5C) ? 1 : 0; //now we accept \NAME

  if (!Name) {
    return FALSE;
  }

  for (i = Adr + j; i < Adr + j + 4; i++) {
    if (
      (Dsdt[i] < 0x2F) ||
      ((Dsdt[i] > 0x39) && (Dsdt[i] < 0x41)) ||
      ((Dsdt[i] > 0x5A) && (Dsdt[i] != 0x5F))
    ) {
      return FALSE;
    }
    Name[i - Adr - j] = Dsdt[i];
  }

  Name[4] = 0;
  if (Shift) {
    *Shift = j;
  }

  return TRUE;
}

// if (CmpAdr (Dsdt, j, NetworkADR1))
// Name (_ADR, 0x90000)

STATIC
BOOLEAN
CmpAdr (
  UINT8     *Dsdt,
  UINT32    j,
  UINT32    PciAdr
) {
  // Name (_ADR, 0x001f0001)
  return (BOOLEAN)
  (
    (Dsdt[j + 4] == 0x08) &&
    (Dsdt[j + 5] == 0x5F) &&
    (Dsdt[j + 6] == 0x41) &&
    (Dsdt[j + 7] == 0x44) &&
    (Dsdt[j + 8] == 0x52) &&
    (
      //--------------------
      (
      (Dsdt[j +  9] == 0x0C) &&
      (Dsdt[j + 10] == ((PciAdr & 0x000000ff) >> 0)) &&
      (Dsdt[j + 11] == ((PciAdr & 0x0000ff00) >> 8)) &&
      (Dsdt[j + 12] == ((PciAdr & 0x00ff0000) >> 16)) &&
      (Dsdt[j + 13] == ((PciAdr & 0xff000000) >> 24))
      ) ||
      //--------------------
      (
        (Dsdt[j +  9] == 0x0B) &&
        (Dsdt[j + 10] == ((PciAdr & 0x000000ff) >> 0)) &&
        (Dsdt[j + 11] == ((PciAdr & 0x0000ff00) >> 8)) &&
        (PciAdr < 0x10000)
      ) ||
      //-----------------------
      (
        (Dsdt[j +  9] == 0x0A) &&
        (Dsdt[j + 10] == (PciAdr & 0x000000ff)) &&
        (PciAdr < 0x100)
      ) ||
      //-----------------
      ((Dsdt[j +  9] == 0x00) && (PciAdr == 0)) ||
      //------------------
      ((Dsdt[j +  9] == 0x01) && (PciAdr == 1))
    )
  );
}

STATIC
BOOLEAN
CmpPNP (
  UINT8   *Dsdt,
  UINT32  j,
  UINT16  PNP
) {
  // Name (_HID, EisaId ("PNP0C0F")) for PNP = 0x0C0F BigEndian
  if (PNP == 0) {
    return (BOOLEAN)
    (
      (Dsdt[j + 0] == 0x08) &&
      (Dsdt[j + 1] == 0x5F) &&
      (Dsdt[j + 2] == 0x48) &&
      (Dsdt[j + 3] == 0x49) &&
      (Dsdt[j + 4] == 0x44) &&
      (Dsdt[j + 5] == 0x0B) &&
      (Dsdt[j + 6] == 0x41) &&
      (Dsdt[j + 7] == 0xD0)
    );
  }
  return (BOOLEAN)
  (
    (Dsdt[j + 0] == 0x08) &&
    (Dsdt[j + 1] == 0x5F) &&
    (Dsdt[j + 2] == 0x48) &&
    (Dsdt[j + 3] == 0x49) &&
    (Dsdt[j + 4] == 0x44) &&
    (Dsdt[j + 5] == 0x0C) &&
    (Dsdt[j + 6] == 0x41) &&
    (Dsdt[j + 7] == 0xD0) &&
    (Dsdt[j + 8] == ((PNP & 0xff00) >> 8)) &&
    (Dsdt[j + 9] == ((PNP & 0x00ff) >> 0))
  );
}

STATIC
INT32
CmpDev (
  UINT8     *Dsdt,
  UINT32    i,
  UINT8     *Name
) {
  if (
    (Dsdt[i + 0] == Name[0]) && (Dsdt[i + 1] == Name[1]) &&
    (Dsdt[i + 2] == Name[2]) && (Dsdt[i + 3] == Name[3]) &&
    (
      ((Dsdt[i - 2] == 0x82) && (Dsdt[i - 3] == 0x5B) && (Dsdt[i - 1] < 0x40)) ||
      ((Dsdt[i - 3] == 0x82) && (Dsdt[i - 4] == 0x5B) && ((Dsdt[i - 2] & 0xF0) == 0x40)) ||
      ((Dsdt[i - 4] == 0x82) && (Dsdt[i - 5] == 0x5B) && ((Dsdt[i - 3] & 0xF0) == 0x80))
    )
  ) {
    if (Dsdt[i - 5] == 0x5B) {
      return i - 3;
    } else if (Dsdt[i - 4] == 0x5B){
      return i - 2;
    } else {
      return i - 1;
    }
  }

  return 0;
}

//the procedure can find BIN array UNSIGNED CHAR8 sizeof N inside part of large array "Dsdt" size of len
// return position or -1 if not found

INT32
FindBin (
  UINT8   *Bin,
  UINT32  BinLen,
  UINT8   *Pattern,
  UINT32  PatternLen
) {
  UINT32    i, j;
  BOOLEAN   Eq;

  for (i = 0; i < BinLen - PatternLen; i++) {
    Eq = TRUE;

    for (j = 0; j < PatternLen; j++) {
      if (Bin[i + j] != Pattern[j]) {
        Eq = FALSE;
        break;
      }
    }

    if (Eq) {
      return (INT32)i;
    }
  }

  return -1;
}

//if (!FindMethod (Dsdt, len, "DTGP"))
// return address of size field. Assume size not more then 0x0FFF = 4095 bytes
//assuming only short methods

STATIC
UINT32
FindMethod (
  UINT8     *Dsdt,
  UINT32    Len,
  CHAR8     *Name
) {
  UINT32  i;

  for (i = 0; i < Len - 7; i++) {
    if (
      ((Dsdt[i] == 0x14) || (Dsdt[i + 1] == 0x14) || (Dsdt[i - 1] == 0x14)) &&
      (Dsdt[i + 3] == Name[0]) && (Dsdt[i + 4] == Name[1]) &&
      (Dsdt[i + 5] == Name[2]) && (Dsdt[i + 6] == Name[3])
    ){
      if (Dsdt[i - 1] == 0x14) {
        return i;
      }

      return (Dsdt[i + 1] == 0x14) ? (i + 2) : (i + 1); //pointer to size field
    }
  }

  return 0;
}

//this procedure corrects size of outer method. Embedded methods is not proposed
// Adr - a place of changes
// shift - a size of changes

STATIC
UINT32
CorrectOuterMethod (
  UINT8   *Dsdt,
  UINT32  Len,
  UINT32  Adr,
  INT32   Shift
) {
  INT32     i,  k, Offset = 0;
  UINT32    Size = 0;
  CHAR8     Name[5];

  if (Shift == 0) {
    return Len;
  }

  i = Adr; //usually Adr = @5B - 1 = Sizefield - 3
  while (i-- > 0x20) {  //find method that previous to Adr
    k = i + 1;

    if ((Dsdt[i] == 0x14) && !CmpNum (Dsdt, i, FALSE)) { //method candidate
      Size = AcpiGetSize (Dsdt, k);

      if (!Size) {
        continue;
      }

      if (
        ((Size <= 0x3F) && !GetName (Dsdt, k + 1, &Name[0], NULL)) ||
        ((Size > 0x3F) && (Size <= 0xFFF) && !GetName (Dsdt, k + 2, &Name[0], NULL)) ||
        ((Size > 0xFFF) && !GetName (Dsdt, k + 3, &Name[0], NULL))
      ) {
        DBG ("method found, size = 0x%x but name is not\n", Size);
        continue;
      }

      if ((k + Size) > (Adr + 4)) {  //Yes - it is outer
        DBG ("found outer method %a begin=%x end=%x\n", Name, k, k + Size);
        Offset = WriteSize (k, Dsdt, Len, Shift);  //size corrected to sizeOffset at address j
        //Shift += Offset;
        Len += Offset;
      }  //else not an outer method
      break;
    }
  }

  return Len;
}

//return final length of Dsdt

STATIC
UINT32
CorrectOuters (
  UINT8     *Dsdt,
  UINT32    Len,
  UINT32    Adr,
  INT32     Shift
) {
  INT32     i, j, k, Offset = 0;
  UINT32    SBSIZE = 0, SBADR = 0, Size = 0;
  BOOLEAN   SBFound = FALSE;

  if (Shift == 0) {
    return Len;
  }

  i = Adr; //usually Adr = @5B - 1 = Sizefield - 3

  while (i > 0x20) {  //find devices that previous to Adr
    //check device
    k = i + 2;
    if (
      (Dsdt[i] == 0x5B) &&
      (Dsdt[i + 1] == 0x82) &&
      !CmpNum (Dsdt, i, TRUE)
    ) { //device candidate
      Size = AcpiGetSize (Dsdt, k);
      if (Size) {
        if ((k + Size) > (Adr + 4)) {  //Yes - it is outer
          //DBG ("found outer device begin=%x end=%x\n", k, k + Size);
          Offset = WriteSize (k, Dsdt, Len, Shift);  //Size corrected to SizeOffset at address j
          Shift += Offset;
          Len += Offset;
        }  //else not an outer device
      } //else wrong Size field - not a device
    } //else not a device

    // check scope
    // a problem 45 43 4F 4E 08   10 84 10 05 5F 53 42 5F
    SBSIZE = 0;

    if (
      (Dsdt[i] == '_') &&
      (Dsdt[i + 1] == 'S') &&
      (Dsdt[i + 2] == 'B') &&
      (Dsdt[i + 3] == '_')
    ) {
      for (j = 0; j < 10; j++) {
        if (Dsdt[i - j] != 0x10) {
          continue;
        }

        if (!CmpNum (Dsdt, i - j, TRUE)) {
          SBADR = i - j + 1;
          SBSIZE = AcpiGetSize (Dsdt, SBADR);

            //DBG ("found Scope (\\_SB) address = 0x%08x Size = 0x%08x\n", SBADR, SBSIZE);

          if ((SBSIZE != 0) && (SBSIZE < Len)) {  //if zero or too large then search more
            //if found
            k = SBADR - 6;

            if ((SBADR + SBSIZE) > Adr + 4) {  //Yes - it is outer
              //DBG ("found outer scope begin=%x end=%x\n", SBADR, SBADR + SBSIZE);
              Offset = WriteSize (SBADR, Dsdt, Len, Shift);
              Shift += Offset;
              Len += Offset;
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

  return Len;
}

//ReplaceName (Dsdt, len, "AZAL", "HDEF");

STATIC
INTN
ReplaceName (
  UINT8     *Dsdt,
  UINT32    Len,
  CHAR8     *OldName,
  CHAR8     *NewName
) {
  UINTN   i;
  INTN    j = 0;

  for (i = 0; i < Len - 4; i++) {
    if (
      (Dsdt[i + 0] == NewName[0]) && (Dsdt[i + 1] == NewName[1]) &&
      (Dsdt[i + 2] == NewName[2]) && (Dsdt[i + 3] == NewName[3])
    ) {
      if (OldName) {
        DBG ("NewName %a already present, renaming impossibble\n", NewName);
      } else {
        DBG ("name %a present at %x\n", NewName, i);
      }

      return -1;
    }
  }

  if (!OldName) {
    return 0;
  }

  for (i = 0; i < Len - 4; i++) {
    if (
      (Dsdt[i + 0] == OldName[0]) && (Dsdt[i + 1] == OldName[1]) &&
      (Dsdt[i + 2] == OldName[2]) && (Dsdt[i + 3] == OldName[3])
    ) {
      DBG ("Name %a present at 0x%x, renaming to %a\n", OldName, i, NewName);
      Dsdt[i + 0] = NewName[0];
      Dsdt[i + 1] = NewName[1];
      Dsdt[i + 2] = NewName[2];
      Dsdt[i + 3] = NewName[3];
      j++;
    }
  }

  return j; //number of replacement
}

//the procedure search nearest "Device" code before given address
//should restrict the search by 6 bytes... OK, 10, .. until Dsdt begin
//hmmm? will check device name

STATIC
UINT32
DevFind (
  UINT8     *Dsdt,
  UINT32    Adr
) {
  UINT32    k = Adr;
  INT32     Size = 0;

  while (k > 30) {
    k--;
    if (Dsdt[k] == 0x82 && Dsdt[k - 1] == 0x5B) {
      Size = AcpiGetSize (Dsdt, k + 1);
      if (!Size) {
        continue;
      }

      if ((k + Size + 1) > Adr) {
        return (k + 1); //pointer to Size
      } //else continue
    }
  }

  DBG ("Device definition before Adr=%x not found\n", Adr);

  return 0; //impossible value for fool proof
}

STATIC
BOOLEAN
AddProperties (
  AML_CHUNK     *Pack,
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

    AmlAddString (Pack, gSettings.AddProperties[i].Key);
    AmlAddByteBuffer (
      Pack,
      gSettings.AddProperties[i].Value,
      (UINT32)gSettings.AddProperties[i].ValueLen
    );
  }

  return Injected;
}

/*
//len = DeleteDevice ("AZAL", Dsdt, len);

STATIC
UINT32
DeleteDevice (
  CHAR8     *Name,
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32    i, j;
  INT32     Size = 0, SizeOffset;

  DBG (" deleting device %a\n", Name);

  for (i = 20; i < Len; i++) {
    j = CmpDev (Dsdt, i, (UINT8 *)Name);

    if (j != 0) {
      Size = AcpiGetSize (Dsdt, j);
      if (!Size) {
        continue;
      }

      SizeOffset = - 2 - Size;
      Len = MoveData (j - 2, Dsdt, Len, SizeOffset);
      //to correct outers we have to calculate offset
      Len = CorrectOuters (Dsdt, Len, j - 3, SizeOffset);
      break;
    }
  }

  return Len;
}
*/

STATIC
UINT32
GetPciDevice (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32    i, PCIADR = 0, PCISIZE = 0;

  for (i = 20; i < Len; i++) {
    // Find Device PCI0   // PNP0A03
    if (CmpPNP (Dsdt, i, 0x0A03)) {
      PCIADR = DevFind (Dsdt, i);
      if (!PCIADR) {
        continue;
      }

      PCISIZE = AcpiGetSize (Dsdt, PCIADR);
      if (PCISIZE) {
        break;
      }
    } // End find
  }

  if (!PCISIZE) {
    for (i = 20; i < Len; i++) {
      // Find Device PCIE   // PNP0A08
      if (CmpPNP (Dsdt, i, 0x0A08)) {
        PCIADR = DevFind (Dsdt, i);
        if (!PCIADR) {
          continue;
        }

        PCISIZE = AcpiGetSize (Dsdt, PCIADR);
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

#if 0
// Find PCIRootUID and all need Fix Device

STATIC
UINTN
FindPciRoot (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINTN     j, Root = 0;
  UINT32    PCIADR, PCISIZE = 0;

  //initialising
  NetworkName   = FALSE;
  DisplayName1  = FALSE;
  DisplayName2  = FALSE;
  //FirewireName  = FALSE;
  ArptName      = FALSE;
  //XhciName      = FALSE;

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
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
      if (
        (Dsdt[j] == '_') &&
        (Dsdt[j + 1] == 'U') &&
        (Dsdt[j + 2] == 'I') &&
        (Dsdt[j + 3] == 'D')
      ) {
        // Slice - I want to set Root to zero instead of keeping original value
        if (Dsdt[j + 4] == 0x0A) {
          Dsdt[j + 5] = 0;  //AML_BYTE_PREFIX followed by a number
        } else {
          Dsdt[j + 4] = 0;  //any other will be considered as ONE or WRONG, replace to ZERO
        }

        DBG ("Found PCIROOTUID = %d\n", Root);
        break;
      }
    }
  } else {
    DBG ("Warning! PCI Root is not found!");
  }

  return Root;
}
#endif

UINT32
FixAny (
  UINT8     *Dsdt,
  UINT32    Len,
  UINT8     *ToFind,
  UINT32    LenTF,
  UINT8     *ToReplace,
  UINT32    LenTR
) {
  INT32     SizeOffset, Adr;
  UINT32    i;
  BOOLEAN   Found = FALSE;

  if (!ToFind || !LenTF || !LenTR) {
    MsgLog (" invalid patches!\n");
    return Len;
  }

  //DBG (" pattern %02x%02x%02x%02x,", ToFind[0], ToFind[1], ToFind[2], ToFind[3]);

  if ((LenTF + sizeof (EFI_ACPI_DESCRIPTION_HEADER)) > Len) {
    MsgLog (" the patch is too large!\n");
    return Len;
  }

  SizeOffset = LenTR - LenTF;

  for (i = 20; i < Len; ) {
    Adr = FindBin (Dsdt + i, Len - i, ToFind, LenTF);
    if (Adr < 0) {
      if (Found) {
        DBG (" ]");
        MsgLog ("\n");
      } else {
        MsgLog (" bin not Found / already patched!\n");
      }
      return Len;
    }

    if (!Found) {
      MsgLog (" patched");
      DBG (" at: [");
    }

    DBG (" (%x)", Adr);
    Found = TRUE;

    Len = MoveData (Adr + i, Dsdt, Len, SizeOffset);
    if ((LenTR > 0) && (ToReplace != NULL)) {
      CopyMem (Dsdt + Adr + i, ToReplace, LenTR);
    }

    Len = CorrectOuterMethod (Dsdt, Len, Adr + i - 2, SizeOffset);
    Len = CorrectOuters (Dsdt, Len, Adr + i - 3, SizeOffset);
    i += Adr + LenTR;
  }

  return Len;
}

UINT32
PatchBinACPI (
  UINT8   *Ptr,
  UINT32  Len
) {
  PATCH_DSDT  *PatchDsdt = gSettings.PatchDsdt;
  UINTN       i = 0;

  //if (!gSettings.PatchDsdtNum || !gSettings.PatchDsdt) {
  //  return Len;
  //}

  while (PatchDsdt) {
    if (!PatchDsdt->Disabled) {
      MsgLog (" - [%02d]: (%a)",
        i++,
        PatchDsdt->Comment ? PatchDsdt->Comment : "NoLabel"
      );

      DBG (" (%a -> %a)",
        Bytes2HexStr(PatchDsdt->Find, (UINTN)PatchDsdt->LenToFind),
        Bytes2HexStr(PatchDsdt->Replace, (UINTN)PatchDsdt->LenToReplace)
      );

      Len = FixAny (
              Ptr, Len,
              PatchDsdt->Find, PatchDsdt->LenToFind,
              PatchDsdt->Replace, PatchDsdt->LenToReplace
            );
    }

    PatchDsdt = PatchDsdt->Next;
  }

  return Len;
}

STATIC
UINT32
AddPNLF (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32    i, Adr  = 0;

  DBG ("Start PNLF Fix\n");

  if (FindBin (Dsdt, Len, (UINT8 *)App2, ARRAY_SIZE (App2)) >= 0) {
    return Len; //the device already exists
  }

  //search  PWRB PNP0C0C
  for (i = 0x20; i < Len - 6; i++) {
    if (CmpPNP (Dsdt, i, 0x0C0C)) {
      DBG ("found PWRB at %x\n", i);
      Adr = DevFind (Dsdt, i);
      break;
    }
  }

  if (!Adr) {
    //search battery
    DBG ("not found PWRB, look BAT0\n");
    for (i = 0x20; i < Len - 6; i++) {
      if (CmpPNP (Dsdt, i, 0x0C0A)) {
        Adr = DevFind (Dsdt, i);
        DBG ("found BAT0 at %x\n", i);
        break;
      }
    }
  }

  if (!Adr) {
    return Len;
  }

  i = Adr - 2;
  Len = MoveData (i, Dsdt, Len, ARRAY_SIZE (Pnlf));
  CopyMem (Dsdt + i, Pnlf, ARRAY_SIZE (Pnlf));
  Len = CorrectOuters (Dsdt, Len, Adr - 3, ARRAY_SIZE (Pnlf));

  return Len;
}

STATIC
UINT32
FIXDisplay (
  UINT8     *Dsdt,
  UINT32    Len,
  INT32     VCard
) {
  UINT32      i = 0, j, k, FakeID = 0, FakeVen = 0,
              PCIADR = 0, PCISIZE = 0, Size,
              DevAdr = 0, DevSize = 0, DevAdr1 = 0, DevSize1 = 0;
  INT32       SizeOffset = 0;
  CHAR8       *Display;
  BOOLEAN     DISPLAYFIX = FALSE, NonUsable = FALSE, DsmFound = FALSE,
              NeedHDMI = (gSettings.FixDsdt & FIX_HDMI);
  AML_CHUNK   *Root = NULL, *Gfx0, *Peg0, *Met, *Met2, *Pack;

  DisplayName1 = FALSE;

  if (!DisplayADR1[VCard]) return Len;

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return Len; //what is the bad DSDT ?!
  }

  DBG ("Start Display%d Fix\n", VCard);
  Root = AmlCreateNode (NULL);

  //search DisplayADR1[0]
  for (j = 0x20; j < Len - 10; j++) {
    if (CmpAdr (Dsdt, j, DisplayADR1[VCard])) { //for example 0x00020000 = 2,0
      DevAdr = DevFind (Dsdt, j);  //PEG0@2,0
      if (!DevAdr) {
        continue;
      }

      DevSize = AcpiGetSize (Dsdt, DevAdr); //sizeof PEG0  0x35
      if (DevSize) {
        DisplayName1 = TRUE;
        break;
      }
    } // End Display1
  }

  //what if PEG0 is not found?
  if (DevAdr) {
    for (j = DevAdr; j < DevAdr + DevSize; j++) { //search card inside PEG0@0
      if (CmpAdr (Dsdt, j, DisplayADR2[VCard])) { //else DISPLAYFIX==false
        DevAdr1 = DevFind (Dsdt, j); //found PEGP
        if (!DevAdr1) {
          continue;
        }

        DevSize1 = AcpiGetSize (Dsdt, DevAdr1);
        if (DevSize1) {
          DBG ("Found internal video device %x @%x\n", DisplayADR2[VCard], DevAdr1);
          DISPLAYFIX = TRUE;
          break;
        }
      }
    }

    if (!DISPLAYFIX) {
      for (j = DevAdr; j < DevAdr + DevSize; j++) { //search card inside PEGP@0
        if (CmpAdr (Dsdt, j, 0xFFFF)) {  //Special case? want to change to 0
          DevAdr1 = DevFind (Dsdt, j); //found PEGP
          if (!DevAdr1) {
            continue;
          }
          DevSize1 = AcpiGetSize (Dsdt, DevAdr1); //13
          if (DevSize1) {
            if (gSettings.ReuseFFFF) {
              Dsdt[j + 10] = 0;
              Dsdt[j + 11] = 0;
              DBG ("Found internal video device FFFF@%x, ReUse as 0\n", DevAdr1);
            } else {
              NonUsable = TRUE;
              DBG ("Found internal video device FFFF@%x, unusable\n", DevAdr1);
            }

            DISPLAYFIX = TRUE;
            break;
          }
        }
      }
    }

    i = 0;
    if (DISPLAYFIX) { // device on bridge found
      i = DevAdr1;
    } else if (DisplayADR2[VCard] == 0xFFFE) { //builtin
      i = DevAdr;
      DISPLAYFIX = TRUE;
      DevAdr1 = DevAdr;
      DBG (" builtin display\n");
    }

    if (i != 0) {
      Size = AcpiGetSize (Dsdt, i);
      k = FindMethod (Dsdt + i, Size, "_DSM");

      if (k != 0) {
        k += i;
        if (
          ((DisplayVendor[VCard] == 0x1002) && (gSettings.DropOEM_DSM & DEV_ATI)   != 0) ||
          ((DisplayVendor[VCard] == 0x10DE) && (gSettings.DropOEM_DSM & DEV_NVIDIA)!= 0) ||
          ((DisplayVendor[VCard] == 0x8086) && (gSettings.DropOEM_DSM & DEV_INTEL) != 0)
        ) {
          Size = AcpiGetSize (Dsdt, k);
          SizeOffset = - 1 - Size;
          Len = MoveData (k - 1, Dsdt, Len, SizeOffset); //kill _DSM
          Len = CorrectOuters (Dsdt, Len, k - 2, SizeOffset);
          DBG ("_DSM in display already exists, dropped\n");
        } else {
          DBG ("_DSM already exists, patch display will not be applied\n");
          DisplayADR1[VCard] = 0;  //xxx
          DsmFound = TRUE;
        }
      }
    }
  }

  if (!DisplayName1) {
    Peg0 = AmlAddDevice (Root, "PEG0");
    AmlAddName (Peg0, "_ADR");
    AmlAddDword (Peg0, DisplayADR1[VCard]);
    DBG ("add device PEG0\n");
  } else {
    Peg0 = Root;
  }

  if (!DISPLAYFIX) {   //bridge or builtin not found
    Gfx0 = AmlAddDevice (Peg0, "GFX0");
    DBG ("add device GFX0\n");
    AmlAddName (Gfx0, "_ADR");

    if (DisplayADR2[VCard] > 0x3F) {
      AmlAddDword (Gfx0, DisplayADR2[VCard]);
    } else {
      AmlAddByte (Gfx0, (UINT8)DisplayADR2[VCard]);
    }
  } else {
    Gfx0 = Peg0;
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
    DBG ("Skip Method (_DSM) for %04x card\n", DisplayVendor[VCard]);
    goto Skip_DSM;
  }

  DBG ("Creating Method (_DSM) for %04x card\n", DisplayVendor[VCard]);
  Met = AmlAddMethod (Gfx0, "_DSM", 4);
  Met2 = AmlAddStore (Met);
  Pack = AmlAddPackage (Met2);

  if (NeedHDMI) {
    AmlAddString (Pack, "hda-gfx");
    AmlAddStringBuffer (Pack, (gSettings.UseIntelHDMI && DisplayVendor[VCard] !=  0x8086) ? "onboard-2" : "onboard-1");
  }

  switch (DisplayVendor[VCard]) {
    case 0x8086:
      if (gSettings.FakeIntel) {
        FakeID = gSettings.FakeIntel >> 16;
        AmlAddString (Pack, "device-id");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 4);
        FakeVen = gSettings.FakeIntel & 0xFFFF;
        AmlAddString (Pack, "vendor-id");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 4);
      }
      break;

    case 0x10DE:
      if (gSettings.FakeNVidia) {
        FakeID = gSettings.FakeNVidia >> 16;
        AmlAddString (Pack, "device-id");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 4);
        FakeVen = gSettings.FakeNVidia & 0xFFFF;
        AmlAddString (Pack, "vendor-id");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 4);
      }
      break;

    case 0x1002:
      if (gSettings.FakeATI) {
        FakeID = gSettings.FakeATI >> 16;
        AmlAddString (Pack, "device-id");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 4);
        AmlAddString (Pack, "ATY,DeviceID");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 2);
        FakeVen = gSettings.FakeATI & 0xFFFF;
        AmlAddString (Pack, "vendor-id");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 4);
        AmlAddString (Pack, "ATY,VendorID");
        AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 2);
      }/* else {
        AmlAddString (Pack, "ATY,VendorID");
        AmlAddByteBuffer (Pack, VenATI, 2);
      }*/
      break;
  }

  AmlAddLocal0 (Met);
  AmlAddBuffer (Met, Dtgp1, ARRAY_SIZE (Dtgp1));

  Skip_DSM:

  //add _sun
  switch (DisplayVendor[VCard]) {
    case 0x10DE:
    case 0x1002:
      Size = AcpiGetSize (Dsdt, i);
      j = (DisplayVendor[VCard] == 0x1002) ? 0 : 1;
      k = FindMethod (Dsdt + i, Size, "_SUN");

      if (k == 0) {
        k = FindName (Dsdt + i, Size, "_SUN");
        if (k == 0) {
          AmlAddName (Gfx0, "_SUN");
          AmlAddDword (Gfx0, SlotDevices[j].SlotID);
        } else {
          //we have name sun, set the number
          if (Dsdt[k + 4] == 0x0A) {
            Dsdt[k + 5] = SlotDevices[j].SlotID;
          }
        }
      } else {
        DBG ("Warning: Method (_SUN) found for %04x card\n", DisplayVendor[VCard]);
      }
      break;
  }

  if (!NonUsable) {
    //now insert video
    DBG ("now inserting Video device\n");
    AmlCalculateSize (Root);
    Display = AllocateZeroPool (Root->Size);
    SizeOffset = Root->Size;
    AmlWriteNode (Root, Display, 0);
    AmlDestroyNode (Root);

    if (DisplayName1) {   //bridge is present
      // move data to back for add Display
      DBG ("... into existing bridge\n");

      if (!DISPLAYFIX || (DisplayADR2[VCard] == 0xFFFE)) {   //subdevice absent
        DevSize = AcpiGetSize (Dsdt, DevAdr);
        if (!DevSize) {
          DBG ("BUG! Address of existing PEG0 is lost %x\n", DevAdr);
          FreePool (Display);
          return Len;
        }

        i = DevAdr + DevSize;
        Len = MoveData (i, Dsdt, Len, SizeOffset);
        CopyMem (Dsdt + i, Display, SizeOffset);
        j = WriteSize (DevAdr, Dsdt, Len, SizeOffset); //correct bridge size
        SizeOffset += j;
        Len += j;
        Len = CorrectOuters (Dsdt, Len, DevAdr - 3, SizeOffset);
      } else {
        DevSize1 = AcpiGetSize (Dsdt, DevAdr1);
        if (!DevSize1) {
          FreePool (Display);
          return Len;
        }

        i = DevAdr1 + DevSize1;
        Len = MoveData (i, Dsdt, Len, SizeOffset);
        CopyMem (Dsdt + i, Display, SizeOffset);
        j = WriteSize (DevAdr1, Dsdt, Len, SizeOffset);
        SizeOffset += j;
        Len += j;
        Len = CorrectOuters (Dsdt, Len, DevAdr1 - 3, SizeOffset);
      }
    } else { //insert PEG0 into PCI0 at the end
      //PCI corrected so search again
      DBG ("... into created bridge\n");
      PCIADR = GetPciDevice (Dsdt, Len);
      if (PCIADR) {
        PCISIZE = AcpiGetSize (Dsdt, PCIADR);
      }

      if (!PCISIZE) {
        return Len; //what is the bad DSDT ?!
      }

      i = PCIADR + PCISIZE;
      //DevAdr = i + 2;  //skip 5B 82
      Len = MoveData (i, Dsdt, Len, SizeOffset);
      CopyMem (Dsdt + i, Display, SizeOffset);
      // Fix PCI0 size
      k = WriteSize (PCIADR, Dsdt, Len, SizeOffset);
      SizeOffset += k;
      Len += k;
      //DevAdr += k;
      k = CorrectOuters (Dsdt, Len, PCIADR - 3, SizeOffset);
      //DevAdr += k - Len;
      Len = k;
    }

    FreePool (Display);
  }

  return Len;
}

STATIC
UINT32
AddHDMI (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32      i, j, k, PCIADR = 0, PCISIZE = 0, Size,
              DevAdr = 0, BridgeSize = 0, DevAdr1 = 0;
  INT32       SizeOffset = 0;
  CHAR8       *Hdmi = NULL, Data[] = { 0xe0, 0x00, 0x56, 0x28 };
  BOOLEAN     BridgeFound = FALSE, HdauFound = FALSE;
  AML_CHUNK   *Brd = NULL, *Root = NULL, *Met, *Met2, *Pack;

  if (!HDMIADR1) {
    return Len;
  }

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return Len; //what is the bad DSDT ?!
  }

  DBG ("Start HDMI%d Fix\n");

  // Device Address
  for (i = 0x20; i < Len - 10; i++) {
    if (CmpAdr (Dsdt, i, HDMIADR1)) {
      DevAdr = DevFind (Dsdt, i);
      if (!DevAdr) {
        continue;
      }

      BridgeSize = AcpiGetSize (Dsdt, DevAdr);
      if (!BridgeSize) {
        continue;
      }

      BridgeFound = TRUE;
      if (HDMIADR2 != 0xFFFE){
        for (k = DevAdr + 9; k < DevAdr + BridgeSize; k++) {
          if (CmpAdr (Dsdt, k, HDMIADR2)) {
            DevAdr1 = DevFind (Dsdt, k);
            if (!DevAdr1) {
              continue;
            }

            DeviceName[11] = AllocateZeroPool (5);
            CopyMem (DeviceName[11], Dsdt + k, 4);
            DBG (
              "found HDMI device [0x%08x:%x] at %x and Name is %a\n",
              HDMIADR1, HDMIADR2, DevAdr1, DeviceName[11]
            );
            ReplaceName (Dsdt + DevAdr, BridgeSize, DeviceName[11], "HDAU");
            HdauFound = TRUE;
            break;
          }
        }

        if (!HdauFound) {
          DBG ("have no HDMI device while HDMIADR2=%x\n", HDMIADR2);
          DevAdr1 = DevAdr;
        }
      } else {
        DevAdr1 = DevAdr;
      }
      break;
    } // End if DevAdr1 find
  }

  if (BridgeFound) { // bridge or device
    if (HdauFound) {
      i = DevAdr1;
      Size = AcpiGetSize (Dsdt, i);
      k = FindMethod (Dsdt + i, Size, "_DSM");

      if (k != 0) {
        k += i;
        if ((gSettings.DropOEM_DSM & DEV_HDMI) != 0) {
          Size = AcpiGetSize (Dsdt, k);
          SizeOffset = - 1 - Size;
          Len = MoveData (k - 1, Dsdt, Len, SizeOffset);
          Len = CorrectOuters (Dsdt, Len, k - 2, SizeOffset);
          DBG ("_DSM in HDAU already exists, dropped\n");
        } else {
          DBG ("_DSM already exists, patch HDAU will not be applied\n");
          return Len;
        }
      }
    }
    Root = AmlCreateNode (NULL);
    //what to do if no HDMI bridge?
  } else {
    Brd = AmlCreateNode (NULL);
    Root = AmlAddDevice (Brd, "HDM0");
    AmlAddName (Root, "_ADR");
    AmlAddDword (Root, HDMIADR1);
    DBG ("Created  bridge device with ADR = 0x%x\n", HDMIADR1);
  }

  DBG ("HDMIADR1=%x HDMIADR2=%x\n", HDMIADR1, HDMIADR2);

  if (!HdauFound && (HDMIADR2 != 0xFFFE)) { //there is no HDMI device at Dsdt, creating new one
    AML_CHUNK   *dev = AmlAddDevice (Root, "HDAU");

    AmlAddName (dev, "_ADR");
    if (HDMIADR2) {
      if (HDMIADR2 > 0x3F) {
        AmlAddDword (dev, HDMIADR2);
      } else {
        AmlAddByte (dev, (UINT8)HDMIADR2);
      }
    } else {
      AmlAddByte (dev, 0x01);
    }

    Met = AmlAddMethod (dev, "_DSM", 4);

    //soMething here is not liked by apple...
    /*
    k = FindMethod (Dsdt + i, Size, "_SUN");
    if (k == 0) {
      k = FindName (Dsdt + i, Size, "_SUN");
      if (k == 0) {
        AmlAddName (dev, "_SUN");
        AmlAddDword (dev, SlotDevices[4].SlotID);
      } else {
        //we have name sun, set the number
        if (Dsdt[k + 4] == 0x0A) {
          Dsdt[k + 5] = SlotDevices[4].SlotID;
        }
      }
    }
    */
  } else {
    //HDAU device already present
    Met = AmlAddMethod (Root, "_DSM", 4);
  }

  Met2 = AmlAddStore (Met);
  Pack = AmlAddPackage (Met2);
  if (!gSettings.NoDefaultProperties) {
    AmlAddString (Pack, "hda-gfx");
    if (gSettings.UseIntelHDMI) {
      AmlAddStringBuffer (Pack, "onboard-2");
    } else {
      AmlAddStringBuffer (Pack, "onboard-1");
    }
  }

  if (!AddProperties (Pack, DEV_HDMI)) {
    DBG (" - with default properties\n");
    AmlAddString (Pack, "layout-id");
    AmlAddByteBuffer (Pack, (CHAR8 *)&GfxlayoutId[0], 4);

    AmlAddString (Pack, "PinConfigurations");
    AmlAddByteBuffer (Pack, Data, sizeof (Data));
  }

  AmlAddLocal0 (Met2);
  AmlAddBuffer (Met, Dtgp1, ARRAY_SIZE (Dtgp1));
  // finish Method (_DSM,4,NotSerialized)

  AmlCalculateSize (Root);
  Hdmi = AllocateZeroPool (Root->Size);
  SizeOffset = Root->Size;
  AmlWriteNode (Root, Hdmi, 0);
  AmlDestroyNode (Root);

  //insert HDAU
  if (BridgeFound) { // bridge or lan
    k = DevAdr1;
  } else { //this is impossible
    k = PCIADR;
  }

  Size = AcpiGetSize (Dsdt, k);
  if (Size > 0) {
    i = k + Size;
    Len = MoveData (i, Dsdt, Len, SizeOffset);
    CopyMem (Dsdt + i, Hdmi, SizeOffset);
    j = WriteSize (k, Dsdt, Len, SizeOffset);
    SizeOffset += j;
    Len += j;
    Len = CorrectOuters (Dsdt, Len, k - 3, SizeOffset);
  }

  if (Hdmi) {
    FreePool (Hdmi);
  }

  return Len;
}

//Network -------------------------------------------------------------

STATIC
UINT32
FIXNetwork (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32      i, k, NetworkADR = 0, BridgeSize, Size, BrdADR = 0,
              PCIADR, PCISIZE = 0, FakeID = 0, FakeVen = 0;
  INT32       SizeOffset;
  AML_CHUNK   *Met, *Met2, *Brd, *Root, *Pack, *Dev;
  CHAR8       *Network, NameCard[32];
  UINTN       Length = ARRAY_SIZE (NameCard);

  if (!NetworkADR1) {
    return Len;
  }

  DBG ("Start NetWork Fix\n");

  if (gSettings.FakeLAN) {
    FakeID = gSettings.FakeLAN >> 16;
    FakeVen = gSettings.FakeLAN & 0xFFFF;
    AsciiSPrint (NameCard, Length, "pci%x,%x\0", FakeVen, FakeID);
    AsciiStrCpyS (NameCard, Length, AsciiStrToLower (NameCard));
    //Netmodel = get_net_model ((FakeVen << 16) + FakeID);
  }

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return Len; //what is the bad DSDT ?!
  }

  NetworkName = FALSE;

  // Network Address
  for (i = 0x24; i < Len - 10; i++) {
    if (CmpAdr (Dsdt, i, NetworkADR1)) { //0x001C0004
      BrdADR = DevFind (Dsdt, i);
      if (!BrdADR) {
        continue;
      }

      BridgeSize = AcpiGetSize (Dsdt, BrdADR);
      if (!BridgeSize) {
        continue;
      }

      if (NetworkADR2 != 0xFFFE){  //0
        for (k = BrdADR + 9; k < BrdADR + BridgeSize; k++) {
          if (CmpAdr (Dsdt, k, NetworkADR2)) {
            NetworkADR = DevFind (Dsdt, k);
            if (!NetworkADR) {
              continue;
            }

            DeviceName[1] = AllocateZeroPool (5);
            CopyMem (DeviceName[1], Dsdt + k, 4);
            DBG ("found NetWork device [0x%08x:%x] at %x and Name is %a\n",
                NetworkADR1, NetworkADR2, NetworkADR, DeviceName[1]);
            //renaming disabled until better way will found
            //ReplaceName (Dsdt + BrdADR, BridgeSize, DeviceName[1], "GIGE");
            NetworkName = TRUE;
            break;
          }
        }

        if (!NetworkName) {
          DBG ("have no Network device while NetworkADR2=%x\n", NetworkADR2);
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
    Size = AcpiGetSize (Dsdt, i);
    k = FindMethod (Dsdt + i, Size, "_DSM");

    if (k != 0) {
      k += i;
      if ((gSettings.DropOEM_DSM & DEV_LAN) != 0) {
        Size = AcpiGetSize (Dsdt, k);
        SizeOffset = - 1 - Size;
        Len = MoveData (k - 1, Dsdt, Len, SizeOffset);
        Len = CorrectOuters (Dsdt, Len, k - 2, SizeOffset);
        DBG ("_DSM in LAN already exists, dropped\n");
      } else {
        DBG ("_DSM already exists, patch LAN will not be applied\n");
        return Len;
      }
    }

    Root = AmlCreateNode (NULL);
  } else {
    //what to do if no LAN bridge?
    i = PCIADR;
    Brd = AmlCreateNode (NULL);
    Root = AmlAddDevice (Brd, "LAN0");
    AmlAddName (Root, "_ADR");
    AmlAddDword (Root, NetworkADR1);
    DBG ("Created  bridge device with ADR = 0x%x\n", NetworkADR1);
  }

  DBG ("NetworkADR1 = %x, NetworkADR2 = %x\n", NetworkADR1, NetworkADR2);
  Dev = Root;

  if (!NetworkName && (NetworkADR2 != 0xFFFE)) { //there is no Network Device at Dsdt, creating new one
    Dev = AmlAddDevice (Root, "GIGE");
    AmlAddName (Dev, "_ADR");
    if (NetworkADR2) {
      if (NetworkADR2> 0x3F) {
        AmlAddDword (Dev, NetworkADR2);
      } else {
        AmlAddByte (Dev, (UINT8)NetworkADR2);
      }
    } else {
      AmlAddByte (Dev, 0x00);
    }
  }

  Size = AcpiGetSize (Dsdt, i);
  k = FindMethod (Dsdt + i, Size, "_SUN");
  if (k == 0) {
    k = FindName (Dsdt + i, Size, "_SUN");
    if (k == 0) {
      AmlAddName (Dev, "_SUN");
      AmlAddDword (Dev, SlotDevices[5].SlotID);
    } else {
      //we have name sun, set the number
      if (Dsdt[k + 4] == 0x0A) {
        Dsdt[k + 5] = SlotDevices[5].SlotID;
      }
    }
  }

  // add Method (_DSM,4,NotSerialized) for Network
  if (gSettings.FakeLAN || !gSettings.NoDefaultProperties) {
    Met = AmlAddMethod (Dev, "_DSM", 4);
    Met2 = AmlAddStore (Met);
    Pack = AmlAddPackage (Met2);

    AmlAddString (Pack, "built-in");
    AmlAddByteBuffer (Pack, DataBuiltin, sizeof (DataBuiltin));
    AmlAddString (Pack, "model");
    //AmlAddStringBuffer (Pack, Netmodel);
    AmlAddStringBuffer (Pack, S_NETMODEL);
    AmlAddString (Pack, "device_type");
    AmlAddStringBuffer (Pack, "Ethernet");

    if (gSettings.FakeLAN) {
      //    AmlAddString (Pack, "model");
      //    AmlAddStringBuffer (Pack, "Apple LAN card");
      AmlAddString (Pack, "device-id");
      AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 4);
      AmlAddString (Pack, "vendor-id");
      AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 4);
      AmlAddString (Pack, "name");
      AmlAddStringBuffer (Pack, &NameCard[0]);
      AmlAddString (Pack, "compatible");
      AmlAddStringBuffer (Pack, &NameCard[0]);
    }

    // Could we just comment this part? (Until remember what was the purposes?)
    if (
      !AddProperties (Pack, DEV_LAN) &&
      !gSettings.FakeLAN &&
      !gSettings.NoDefaultProperties
    ) {
      AmlAddString (Pack, "empty");
      AmlAddByte (Pack, 0);
    }

    AmlAddLocal0 (Met2);
    AmlAddBuffer (Met, Dtgp1, ARRAY_SIZE (Dtgp1));
  }

  // finish Method (_DSM,4,NotSerialized)
  AmlCalculateSize (Root);
  Network = AllocateZeroPool (Root->Size);

  if (!Network) {
    return Len;
  }

  SizeOffset = Root->Size;
  DBG (" - _DSM created, size=%d\n", SizeOffset);
  AmlWriteNode (Root, Network, 0);
  AmlDestroyNode (Root);

  if (NetworkADR) { // bridge or lan
    i = NetworkADR;
  } else { //this is impossible
    i = PCIADR;
  }

  Size = AcpiGetSize (Dsdt, i);
  // move data to back for add patch
  k = i + Size;
  Len = MoveData (k, Dsdt, Len, SizeOffset);
  CopyMem (Dsdt + k, Network, SizeOffset);
  // Fix Device Network size
  k = WriteSize (i, Dsdt, Len, SizeOffset);
  SizeOffset += k;
  Len += k;
  Len = CorrectOuters (Dsdt, Len, i - 3, SizeOffset);
  FreePool (Network);

  return Len;
}

//Airport--------------------------------------------------

//CHAR8 DataBCM[]  = {0x12, 0x43, 0x00, 0x00};
//CHAR8 Data1ATH[] = {0x2a, 0x00, 0x00, 0x00};
CHAR8 Data2ATH[] = {0x8F, 0x00, 0x00, 0x00};
CHAR8 Data3ATH[] = {0x6B, 0x10, 0x00, 0x00};

STATIC
UINT32
FIXAirport (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32      i, k, PCIADR, PCISIZE = 0, FakeID = 0, FakeVen = 0,
              ArptADR = 0, BridgeSize, Size, BrdADR = 0;
  INT32       SizeOffset;
  AML_CHUNK   *Met, *Met2, *Brd, *Root, *Pack, *Dev;
  CHAR8       *Network, NameCard[32];
  UINTN       Length = ARRAY_SIZE (NameCard);

  if (!ArptADR1) {
   return Len; // no device - no patch
  }

  if (gSettings.FakeWIFI) {
    FakeID = gSettings.FakeWIFI >> 16;
    FakeVen = gSettings.FakeWIFI & 0xFFFF;
    AsciiSPrint (NameCard, Length, "pci%x,%x\0", FakeVen, FakeID);
    AsciiStrCpyS (NameCard, Length, AsciiStrToLower (NameCard));
  }

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return Len; //what is the bad DSDT ?!
  }

  DBG ("Start Airport Fix\n");

  ArptName = FALSE;

  for (i = 0x20; i < Len - 10; i++) {
    // AirPort Address
    if (CmpAdr (Dsdt, i, ArptADR1)) {
      BrdADR = DevFind (Dsdt, i);
      if (!BrdADR) {
        continue;
      }

      BridgeSize = AcpiGetSize (Dsdt, BrdADR);
      if (!BridgeSize) {
        continue;
      }

      if (ArptADR2 != 0xFFFE){
        for (k = BrdADR + 9; k < BrdADR + BridgeSize; k++) {
          if (CmpAdr (Dsdt, k, ArptADR2)) {
            ArptADR = DevFind (Dsdt, k);

            if (!ArptADR) {
              continue;
            }

            DeviceName[9] = AllocateZeroPool (5);
            CopyMem (DeviceName[9], Dsdt + k, 4);

            DBG (
              "found Airport device [%08x:%x] at %x And Name is %a\n",
              ArptADR1, ArptADR2, ArptADR, DeviceName[9]
            );

            ReplaceName (Dsdt + BrdADR, BridgeSize, DeviceName[9], "ARPT");
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
    Size = AcpiGetSize (Dsdt, i);

    k = FindMethod (Dsdt + i, Size, "_DSM");
    if (k != 0) {
      k += i;
      if ((gSettings.DropOEM_DSM & DEV_WIFI) != 0) {
        Size = AcpiGetSize (Dsdt, k);
        SizeOffset = - 1 - Size;
        Len = MoveData (k - 1, Dsdt, Len, SizeOffset);
        Len = CorrectOuters (Dsdt, Len, k - 2, SizeOffset);
        DBG ("_DSM in ARPT already exists, dropped\n");
      } else {
        DBG ("_DSM already exists, patch ARPT will not be applied\n");
        return Len;
      }
    }

    Root = AmlCreateNode (NULL);
  } else { //what to do if no Arpt bridge?
    Brd = AmlCreateNode (NULL);
    Root = AmlAddDevice (Brd, "ARP0");
    AmlAddName (Root, "_ADR");
    AmlAddDword (Root, ArptADR1);
    DBG ("Created  bridge device with ADR = 0x%x\n", ArptADR1);
  }

  Dev = Root;
  if (!ArptName && (ArptADR2 != 0xFFFE)) {//there is no Airport Device at Dsdt, creating new one
    Dev = AmlAddDevice (Root, "ARPT");
    AmlAddName (Dev, "_ADR");
    if (ArptADR2) {
      if (ArptADR2> 0x3F) {
        AmlAddDword (Dev, ArptADR2);
      } else {
        AmlAddByte (Dev, (UINT8)ArptADR2);
      }
    } else {
      AmlAddByte (Dev, 0x00);
    }
  }

  Size = AcpiGetSize (Dsdt, i);
  k = FindMethod (Dsdt + i, Size, "_SUN");

  if (k == 0) {
    k = FindName (Dsdt + i, Size, "_SUN");
    if (k == 0) {
      AmlAddName (Dev, "_SUN");
      AmlAddDword (Dev, SlotDevices[6].SlotID);
    } else {
      //we have name sun, set the number
      if (Dsdt[k + 4] == 0x0A) {
        Dsdt[k + 5] = SlotDevices[6].SlotID;
      }
    }
  } else {
    DBG ("Warning: Method (_SUN) found for airport\n");
  }

  // add Method (_DSM,4,NotSerialized) for Network
  if (gSettings.FakeWIFI || !gSettings.NoDefaultProperties) {
    Met = AmlAddMethod (Dev, "_DSM", 4);
    Met2 = AmlAddStore (Met);
    Pack = AmlAddPackage (Met2);

    if (!gSettings.NoDefaultProperties) {
      AmlAddString (Pack, "built-in");
      AmlAddByteBuffer (Pack, DataBuiltin, sizeof (DataBuiltin));
      AmlAddString (Pack, "model");
      AmlAddStringBuffer (Pack, "Apple WiFi card");
      AmlAddString (Pack, "subsystem-id");
      AmlAddByteBuffer (Pack, Data2ATH, 4);
      AmlAddString (Pack, "subsystem-vendor-id");
      AmlAddByteBuffer (Pack, Data3ATH, 4);

      /*
        if (ArptBCM) {
          if (!gSettings.FakeWIFI) {
            AmlAddString (Pack, "model");
            AmlAddStringBuffer (Pack, "Dell Wireless 1395");
            AmlAddString (Pack, "name");
            AmlAddStringBuffer (Pack, "pci14e4,4312");
            AmlAddString (Pack, "device-id");
            AmlAddByteBuffer (Pack, DataBCM, 4);
          }
        } else if (ArptAtheros) {
          AmlAddString (Pack, "model");
          AmlAddStringBuffer (Pack, "Atheros AR9285 WiFi card");
          if (!gSettings.FakeWIFI && ArptDevID != 0x30) {
            AmlAddString (Pack, "name");
            AmlAddStringBuffer (Pack, "pci168c,2a");
            AmlAddString (Pack, "device-id");
            AmlAddByteBuffer (Pack, Data1ATH, 4);
          }
          AmlAddString (Pack, "subsystem-id");
          AmlAddByteBuffer (Pack, Data2ATH, 4);
          AmlAddString (Pack, "subsystem-vendor-id");
          AmlAddByteBuffer (Pack, Data3ATH, 4);
        }
      */

      AmlAddString (Pack, "device_type");
      AmlAddStringBuffer (Pack, "Airport");
      //AmlAddString (Pack, "AAPL,slot-name");
      //AmlAddStringBuffer (Pack, "AirPort");
    }

    if (gSettings.FakeWIFI) {
      AmlAddString (Pack, "device-id");
      AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 4);
      AmlAddString (Pack, "vendor-id");
      AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 4);
      AmlAddString (Pack, "name");
      AmlAddStringBuffer (Pack, (CHAR8 *)&NameCard[0]);
      AmlAddString (Pack, "compatible");
      AmlAddStringBuffer (Pack, (CHAR8 *)&NameCard[0]);
    }

    if (
      !AddProperties (Pack, DEV_WIFI) &&
      !gSettings.NoDefaultProperties &&
      !gSettings.FakeWIFI
    ) {
      AmlAddString (Pack, "empty");
      AmlAddByte (Pack, 0);
    }

    AmlAddLocal0 (Met);
    AmlAddBuffer (Met, Dtgp1, ARRAY_SIZE (Dtgp1));
  }
  // finish Method (_DSM,4,NotSerialized)

  AmlCalculateSize (Root);
  Network = AllocateZeroPool (Root->Size);
  SizeOffset = Root->Size;
  AmlWriteNode (Root, Network, 0);
  AmlDestroyNode (Root);

  DBG ("AirportADR=%x add patch size=%x\n", ArptADR, SizeOffset);

  if (ArptADR) { // bridge or WiFi
    i = ArptADR;
  } else { //this is impossible
    i = PCIADR;
  }

  Size = AcpiGetSize (Dsdt, i);
  DBG ("Adr %x size of arpt=%x\n", i, Size);
  // move data to back for add patch
  k = i + Size;
  Len = MoveData (k, Dsdt, Len, SizeOffset);
  CopyMem (Dsdt + k, Network, SizeOffset);
  // Fix Device size
  k = WriteSize (i, Dsdt, Len, SizeOffset);
  SizeOffset += k;
  Len += k;
  Len = CorrectOuters (Dsdt, Len, i - 3, SizeOffset);
  FreePool (Network);

  return Len;
}

STATIC
UINT32
AddMCHC (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32      i, k = 0, PCIADR, PCISIZE = 0; //, Size;
  INT32       SizeOffset;
  AML_CHUNK   *Root, *Device; //*met, *met2; *Pack;
  CHAR8       *Mchc;

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    //DBG ("wrong PCI0 address, patch MCHC will not be applied\n");
    return Len;
  }

  //Find Device MCHC by name
  for (i = 0x20; i < Len - 10; i++) {
    k = CmpDev (Dsdt, i, (UINT8 *)"MCHC");
    if (k != 0) {
      DBG ("device name (MCHC) found at %x, don't add!\n", k);
      //break;
      return Len;
    }
  }

  DBG ("Start Add MCHC\n");
  Root = AmlCreateNode (NULL);

  Device = AmlAddDevice (Root, "MCHC");
  AmlAddName (Device, "_ADR");
  AmlAddByte (Device, 0x00);

  AmlCalculateSize (Root);
  Mchc = AllocateZeroPool (Root->Size);
  SizeOffset = Root->Size;
  AmlWriteNode (Root, Mchc, 0);
  AmlDestroyNode (Root);
  // always add on PCIX back
  PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  Len = MoveData (PCIADR + PCISIZE, Dsdt, Len, SizeOffset);
  CopyMem (Dsdt + PCIADR + PCISIZE, Mchc, SizeOffset);
  // Fix PCIX size
  k = WriteSize (PCIADR, Dsdt, Len, SizeOffset);
  SizeOffset += k;
  Len += k;
  Len = CorrectOuters (Dsdt, Len, PCIADR - 3, SizeOffset);
  FreePool (Mchc);

  return Len;
}

STATIC
UINT32
AddIMEI (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32      i, k = 0, PCIADR, PCISIZE = 0, FakeID, FakeVen;
  INT32       SizeOffset;
  AML_CHUNK   *Root, *Device, *Met, *Met2, *Pack;
  CHAR8       *Imei;

  if (gSettings.FakeIMEI) {
    FakeID = gSettings.FakeIMEI >> 16;
    FakeVen = gSettings.FakeIMEI & 0xFFFF;
  }

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    //DBG ("wrong PCI0 address, patch IMEI will not be applied\n");
    return Len;
  }

  // Find Device IMEI
  if (IMEIADR1) {
    for (i = 0x20; i < Len - 10; i++) {
      if (CmpAdr (Dsdt, i, IMEIADR1)) {
        k = DevFind (Dsdt, i);
        if (k) {
          DBG ("device (IMEI) found at %x, don't add!\n", k);
          //break;
          return Len;
        }
      }
    }
  }

  //Find Device IMEI by name
  for (i = 0x20; i < Len - 10; i++) {
    k = CmpDev (Dsdt, i, (UINT8 *)"IMEI");
    if (k != 0) {
      DBG ("device name (IMEI) found at %x, don't add!\n", k);
      return Len;
    }
  }

  DBG ("Start Add IMEI\n");
  Root = AmlCreateNode (NULL);
  Device = AmlAddDevice (Root, "IMEI");
  AmlAddName (Device, "_ADR");
  AmlAddDword (Device, IMEIADR1);

  // add Method (_DSM,4,NotSerialized)
  if (gSettings.FakeIMEI) {
    Met = AmlAddMethod (Device, "_DSM", 4);
    Met2 = AmlAddStore (Met);
    Pack = AmlAddPackage (Met2);

    AmlAddString (Pack, "device-id");
    AmlAddByteBuffer (Pack, (CHAR8 *)&FakeID, 4);
    AmlAddString (Pack, "vendor-id");
    AmlAddByteBuffer (Pack, (CHAR8 *)&FakeVen, 4);

    AmlAddLocal0 (Met2);
    AmlAddBuffer (Met, Dtgp1, ARRAY_SIZE (Dtgp1));
    //finish Method (_DSM,4,NotSerialized)
  }

  AmlCalculateSize (Root);
  Imei = AllocateZeroPool (Root->Size);
  SizeOffset = Root->Size;
  AmlWriteNode (Root, Imei, 0);
  AmlDestroyNode (Root);

  // always add on PCIX back
  Len = MoveData (PCIADR + PCISIZE, Dsdt, Len, SizeOffset);
  CopyMem (Dsdt + PCIADR + PCISIZE, Imei, SizeOffset);
  // Fix PCIX size
  k = WriteSize (PCIADR, Dsdt, Len, SizeOffset);
  SizeOffset += k;
  Len += k;
  Len = CorrectOuters (Dsdt, Len, PCIADR - 3, SizeOffset);

  FreePool (Imei);

  return Len;
}

STATIC
UINT32
AddHDEF (
  UINT8     *Dsdt,
  UINT32    Len
) {
  UINT32        i, k, PCIADR, PCISIZE = 0, HDAADR = 0, Size;
  INT32         SizeOffset;
  AML_CHUNK     *Root, *Met, *Met2, *Device, *Pack;
  CHAR8         *Hdef;

  PCIADR = GetPciDevice (Dsdt, Len);
  if (PCIADR) {
    PCISIZE = AcpiGetSize (Dsdt, PCIADR);
  }

  if (!PCISIZE) {
    return Len; //what is the bad DSDT ?!
  }

  DBG ("Start HDA Fix\n");
  //Len = DeleteDevice ("AZAL", Dsdt, Len);

  // HDA Address
  for (i = 0x20; i < Len - 10; i++) {
    if (
      (HDAADR1 != 0x00000000) &&
      HDAFIX &&
      CmpAdr (Dsdt, i, HDAADR1)
    ) {
      HDAADR = DevFind (Dsdt, i);
      if (!HDAADR) {
        continue;
      }

      //BridgeSize = AcpiGetSize (Dsdt, HDAADR);
      DeviceName[4] = AllocateZeroPool (5);
      CopyMem (DeviceName[4], Dsdt + i, 4);

      DBG ("found HDA device NAME (_ADR,0x%08x) And Name is %a\n",
          HDAADR1, DeviceName[4]);

      ReplaceName (Dsdt, Len, DeviceName[4], "HDEF");
      HDAFIX = FALSE;
      break;
    } // End HDA
  }

  if (HDAADR) { // bridge or device
    i = HDAADR;
    Size = AcpiGetSize (Dsdt, i);
    k = FindMethod (Dsdt + i, Size, "_DSM");

    if (k != 0) {
      k += i;
      if ((gSettings.DropOEM_DSM & DEV_HDA) != 0) {
        Size = AcpiGetSize (Dsdt, k);
        SizeOffset = - 1 - Size;
        Len = MoveData (k - 1, Dsdt, Len, SizeOffset);
        Len = CorrectOuters (Dsdt, Len, k - 2, SizeOffset);
        DBG ("_DSM in HDA already exists, dropped\n");
      } else {
        DBG ("_DSM already exists, patch HDA will not be applied\n");
        return Len;
      }
    }
  }

  Root = AmlCreateNode (NULL);
  if (HDAFIX) {
    DBG ("Start Add Device HDEF\n");
    Device = AmlAddDevice (Root, "HDEF");
    AmlAddName (Device, "_ADR");
    AmlAddDword (Device, HDAADR1);

    // add Method (_DSM,4,NotSerialized)
    Met = AmlAddMethod (Device, "_DSM", 4);
  } else {
    Met = AmlAddMethod (Root, "_DSM", 4);
  }

  Met2 = AmlAddStore (Met);
  Pack = AmlAddPackage (Met2);

  if (gSettings.UseIntelHDMI) {
    AmlAddString (Pack, "hda-gfx");
    AmlAddStringBuffer (Pack, "onboard-1");
  }

  if (!AddProperties (Pack, DEV_HDA)) {
    AmlAddString (Pack, "MaximumBootBeepVolume");
    AmlAddByteBuffer (Pack, (CHAR8 *)&DataBuiltin1[0], 1);

    AmlAddString (Pack, "PinConfigurations");
    AmlAddByteBuffer (Pack, 0, 0);//data, sizeof (data));
  }

  AmlAddLocal0 (Met2);
  AmlAddBuffer (Met, Dtgp1, ARRAY_SIZE (Dtgp1));
  // finish Method (_DSM,4,NotSerialized)

  AmlCalculateSize (Root);
  Hdef = AllocateZeroPool (Root->Size);
  SizeOffset = Root->Size;
  AmlWriteNode (Root, Hdef, 0);
  AmlDestroyNode (Root);

  i = HDAFIX ? PCIADR : HDAADR; // bridge or device

  Size = AcpiGetSize (Dsdt, i);
  // move data to back for add patch
  k = i + Size;
  Len = MoveData (k, Dsdt, Len, SizeOffset);
  CopyMem (Dsdt + k, Hdef, SizeOffset);
  // Fix Device size
  k = WriteSize (i, Dsdt, Len, SizeOffset);
  SizeOffset += k;
  Len += k;
  Len = CorrectOuters (Dsdt, Len, i - 3, SizeOffset);
  FreePool (Hdef);

  return Len;
}

VOID
FixBiosDsdt (
  UINT8     *Temp,
  BOOLEAN   Patched
) {
  UINT32    DsdtLen;

  if (!Temp) {
    return;
  }

  //Reenter requires ZERO values
  HDAFIX = TRUE;
  GFXHDAFIX = TRUE;
  //USBIDFIX = TRUE;

  DsdtLen = ((EFI_ACPI_DESCRIPTION_HEADER *)Temp)->Length;
  if ((DsdtLen < 20) || (DsdtLen > 400000)) { //fool proof (some ASUS dsdt > 300kb?)
    DBG ("DSDT length out of range\n");
    return;
  }

  //DBG ("========= Auto patch DSDT Starting ========\n");
  DbgHeader ("FixBiosDsdt");

  // First check hardware address: GetPciADR (DevicePath, &NetworkADR1, &NetworkADR2);
  CheckHardware ();

  //arbitrary fixes
  if (!Patched && (gSettings.PatchDsdtNum > 0) && gSettings.PatchDsdt) {
    MsgLog ("Patching DSDT:\n");
    DsdtLen = PatchBinACPI (Temp, DsdtLen);
  }

  // find ACPI CPU name and hardware address
  FindCPU (Temp, DsdtLen);

  if (!gSettings.FixDsdt) {
    //return;
    goto Finish;
  }

  if (!FindMethod (Temp, DsdtLen, "DTGP")) {
    CopyMem ((CHAR8 *)Temp + DsdtLen, Dtgp, ARRAY_SIZE (Dtgp));
    DsdtLen += ARRAY_SIZE (Dtgp);
    ((EFI_ACPI_DESCRIPTION_HEADER *)Temp)->Length = DsdtLen;
  }

  //gSettings.PCIRootUID = (UINT16)FindPciRoot (Temp, DsdtLen);

  // Fix Display
  if (
    (gSettings.FixDsdt & FIX_DISPLAY) ||
    (gSettings.FixDsdt & FIX_INTELGFX)
  ) {
    INT32   i;

    for (i = 0; i < 4; i++) {
      if (DisplayADR1[i]) {
        if (
          ((DisplayVendor[i] != 0x8086) && (gSettings.FixDsdt & FIX_DISPLAY)) ||
          ((DisplayVendor[i] == 0x8086) && (gSettings.FixDsdt & FIX_INTELGFX))
        ) {
          DsdtLen = FIXDisplay (Temp, DsdtLen, i);
          DBG ("patch Display #%d of Vendor = 0x%4x in DSDT new way\n", i, DisplayVendor[i]);
        }
      }
    }
  }

  // Fix Network
  if (NetworkADR1 && (gSettings.FixDsdt & FIX_LAN)) {
    //DBG ("patch LAN in DSDT \n");
    DsdtLen = FIXNetwork (Temp, DsdtLen);
  }

  // Fix Airport
  if (ArptADR1 && (gSettings.FixDsdt & FIX_WIFI)) {
    //DBG ("patch Airport in DSDT \n");
    DsdtLen = FIXAirport (Temp, DsdtLen);
  }

  // HDA HDEF
  if (HDAFIX && (gSettings.FixDsdt & FIX_HDA)) {
    DBG ("patch HDEF in DSDT \n");
    DsdtLen = AddHDEF (Temp, DsdtLen);
  }

  //Always add MCHC for PM
  if ((gCPUStructure.Family == 0x06)  && (gSettings.FixDsdt & FIX_MCHC)) {
    //DBG ("patch MCHC in DSDT \n");
    DsdtLen = AddMCHC (Temp, DsdtLen);
  }

  //add IMEI
  if ((gSettings.FixDsdt & FIX_MCHC) || (gSettings.FixDsdt & FIX_IMEI)) {
    DsdtLen = AddIMEI (Temp, DsdtLen);
  }

  //Add HDMI device
  if (gSettings.FixDsdt & FIX_HDMI) {
    DsdtLen = AddHDMI (Temp, DsdtLen);
  }

  if (gSettings.FixDsdt & FIX_PNLF) {
    DsdtLen = AddPNLF (Temp, DsdtLen);
  }

  Finish:

  // Finish DSDT patch and resize DSDT Length
  Temp[4] = (DsdtLen & 0x000000FF);
  Temp[5] = (UINT8)((DsdtLen & 0x0000FF00) >>  8);
  Temp[6] = (UINT8)((DsdtLen & 0x00FF0000) >> 16);
  Temp[7] = (UINT8)((DsdtLen & 0xFF000000) >> 24);

  CopyMem ((UINT8 *)((EFI_ACPI_DESCRIPTION_HEADER *)Temp)->OemId, (UINT8 *)BiosVendor, 6);
  //DBG ("orgBiosDsdtLen = 0x%08x\n", orgBiosDsdtLen);
  ((EFI_ACPI_DESCRIPTION_HEADER *)Temp)->Checksum = 0;
  ((EFI_ACPI_DESCRIPTION_HEADER *)Temp)->Checksum = (UINT8)(256 - Checksum8 (Temp, DsdtLen));

  //DBG ("========= Auto patch DSDT Finished ========\n");
  //PauseForKey (L"waiting for key press...\n");
}
