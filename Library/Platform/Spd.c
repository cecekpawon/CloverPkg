/*
 * spd.c - serial presence detect memory information
 implementation for reading memory spd

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 *
 * Originally restored from pcefi10.5 by netkas
 * Dynamic mem detection original impl. by Rekursor
 * System profiler fix and other fixes by Mozodojo.
 * Slice 2011  remade for UEFI
 * XMP detection - apianti
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_SPD
#define DEBUG_SPD -1
#endif
#else
#ifdef DEBUG_SPD
#undef DEBUG_SPD
#endif
#define DEBUG_SPD DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_SPD, __VA_ARGS__)

#define SPD_MEMORY_TYPE                       2  /* (Fundamental) memory type */
#define SPD_NUM_ROWS                          3  /* Number of row address bits */
#define SPD_NUM_COLUMNS                       4  /* Number of column address bits */
#define SPD_NUM_DIMM_BANKS                    5  /* Number of module rows (banks) */
#define SPD_NUM_BANKS_PER_SDRAM               17 /* SDRAM device attributes, number of banks on SDRAM device */

/* SPD_MEMORY_TYPE values. */
#define SPD_MEMORY_TYPE_SDRAM_DDR             7
#define SPD_MEMORY_TYPE_SDRAM_DDR2            8
#define SPD_MEMORY_TYPE_SDRAM_DDR3            0xb
#define SPD_MEMORY_TYPE_SDRAM_DDR4            0xc

#define SPD_DDR3_MEMORY_BANK                  0x75
#define SPD_DDR3_MEMORY_CODE                  0x76

#define SPD_DDR4_MANUFACTURER_ID_BANK         0x140 /* Manufacturer's JEDEC ID code (bytes 140-141) */
#define SPD_DDR4_MANUFACTURER_ID_CODE         0x141 /* Manufacturer's JEDEC ID code (bytes 140-141) */

// Intel SMB reg offsets
#define SMBHSTSTS                             0
#define SMBHSTCNT                             2
#define SMBHSTCMD                             3
#define SMBHSTADD                             4
#define SMBHSTDAT                             5
#define SMBHSTDAT1                            6
#define SBMBLKDAT                             7

// MCP and nForce SMB reg offsets
#define SMBHPRTCL_NV                          0 /* protocol, PEC */
#define SMBHSTSTS_NV                          1 /* status */
#define SMBHSTADD_NV                          2 /* address */
#define SMBHSTCMD_NV                          3 /* command */
#define SMBHSTDAT_NV                          4 /* 32 data registers */

// XMP memory profile
#define SPD_XMP_SIG1                          176
#define SPD_XMP_SIG1_VALUE                    0x0C
#define SPD_XMP_SIG2                          177
#define SPD_XMP_SIG2_VALUE                    0x4A
#define SPD_XMP_PROFILES                      178
#define SPD_XMP_VERSION                       179
#define SPD_XMP_PROF1_DIVISOR                 180
#define SPD_XMP_PROF1_DIVIDEND                181
#define SPD_XMP_PROF2_DIVISOR                 182
#define SPD_XMP_PROF2_DIVIDEND                183
#define SPD_XMP_PROF1_RATIO                   186
#define SPD_XMP_PROF2_RATIO                   221

#define SPD_XMP20_SIG1                        0x180
#define SPD_XMP20_SIG2                        0x181
#define SPD_XMP20_PROFILES                    0x182
#define SPD_XMP20_VERSION                     0x183
/* 0x189 */
#define SPD_XMP20_PROF1_MINCYCLE              0x18C
#define SPD_XMP20_PROF1_FINEADJUST            0x1AF
/* 0x1B8 */
#define SPD_XMP20_PROF2_MINCYCLE              0x1BB
#define SPD_XMP20_PROF2_FINEADJUST            0x1DE
/* 0x1E7 */
#define MAX_SPD_SIZE                          512  /* end of DDR4 XMP 2.0 */

UINT16 SpdIndexesDDR[] = {
  /* 3 */   SPD_NUM_ROWS,  /* ModuleSize */
  /* 4 */   SPD_NUM_COLUMNS,
  /* 5 */   SPD_NUM_DIMM_BANKS,
  /* 17 */  SPD_NUM_BANKS_PER_SDRAM,
  9, /* Frequency */
  64, /* Manufacturer */
  95, 96, 97, 98, /* UIS */
  0
};

UINT16 SpdIndexesDDR3[] = {
  4, 7, 8, /* ModuleSize */
  10, 11, 12, /* Frequency */
  /* 0x75, 0x76 */ SPD_DDR3_MEMORY_BANK, SPD_DDR3_MEMORY_CODE, /* Manufacturer */
  122, 123, 124, 125, /* UIS */
  /* XMP */
  SPD_XMP_SIG1,
  SPD_XMP_SIG2,
  SPD_XMP_PROFILES,
  SPD_XMP_VERSION,
  SPD_XMP_PROF1_DIVISOR,
  SPD_XMP_PROF1_DIVIDEND,
  SPD_XMP_PROF2_DIVISOR,
  SPD_XMP_PROF2_DIVIDEND,
  SPD_XMP_PROF1_RATIO,
  SPD_XMP_PROF2_RATIO,
  0
};

UINT16 SpdIndexesDDR4[] = {
  4, 6, 12, 13, /* ModuleSize */
  18, 125, /* Frequency */
  SPD_DDR4_MANUFACTURER_ID_BANK, SPD_DDR4_MANUFACTURER_ID_CODE, /* Manufacturer */
  325, 326, 327, 328, /* UIS */
  /* XMP 2.0 */
  SPD_XMP20_SIG1,
  SPD_XMP20_SIG2,
  SPD_XMP20_PROFILES,
  SPD_XMP20_VERSION,
  SPD_XMP20_PROF1_MINCYCLE,
  SPD_XMP20_PROF1_FINEADJUST,
  SPD_XMP20_PROF2_MINCYCLE,
  SPD_XMP20_PROF2_FINEADJUST,
  0
};

BOOLEAN     SmbIntel;
UINT8       SmbPage;

/** Read one byte from i2c, used for reading SPD */

UINT8
SmbReadByte (
  UINT32    Base,
  UINT8     Adr,
  UINT16    Cmd
) {
  //INTN    l1, h1, l2, h2;
  //UINT8   p;
  UINT64    t, t1, t2;
  UINT8     Page, c;

  if (SmbIntel) {
    IoWrite8 (Base + SMBHSTSTS, 0x1f);       // reset SMBus Controller (set busy)
    IoWrite8 (Base + SMBHSTDAT, 0xff);

    t1 = AsmReadTsc (); //rdtsc (l1, h1);

    while (IoRead8 (Base + SMBHSTSTS) & 0x01) {   // wait until host is not busy
      t2 = AsmReadTsc (); //rdtsc (l2, h2);
      t = DivU64x64Remainder (
            (t2 - t1),
            DivU64x32 (gSettings.CPUStructure.TSCFrequency, 1000),
            0
          );

      if (t > 5) {
        DBG ("host is busy for too long for byte %2X:%d!\n", Adr, Cmd);
        return 0xFF;                  // break
      }
    }

    Page = (Cmd >> 8) & 1;
    if (Page != SmbPage) {
      // p = 0xFF;
      IoWrite8 (Base + SMBHSTCMD, 0x00);
      IoWrite8 (Base + SMBHSTADD, 0x6C + (Page << 1)); // Set SPD Page Address
      IoWrite8 (Base + SMBHSTCNT, 0x40); // Start + Quick Write
      // status goes from 0x41 (Busy) -> 0x42 (Completed)

      SmbPage = Page;

      t1 = AsmReadTsc ();

      while (!((c=IoRead8 (Base + SMBHSTSTS)) & 0x02)) {  // wait until command finished
        t2 = AsmReadTsc ();
        t = DivU64x64Remainder ((t2 - t1), DivU64x32 (gSettings.CPUStructure.TSCFrequency, 1000), 0);

        /*
         if (c != p) {
         DBG ("%02d %02X spd page change status %2X\n", t, Cmd, c);
         p = c;
         }
        */

        if (c & 4) {
          DBG ("spd page change error for byte %2X:%d!\n", Adr, Cmd);
          break;
        }

        if (t > 5) {
          DBG ("spd page change taking too long for byte %2X:%d!\n", Adr, Cmd);
          break;                  // break after 5ms
        }
      }

      return SmbReadByte (Base, Adr, Cmd);
    }

    // p = 0xFF;
    IoWrite8 (Base + SMBHSTCMD, (UINT8)(Cmd & 0xFF)); // SMBus uses 8 bit commands
    IoWrite8 (Base + SMBHSTADD, (Adr << 1) | 0x01); // read from spd
    IoWrite8 (Base + SMBHSTCNT, 0x48); // Start + Byte Data Read
    // status goes from 0x41 (Busy) -> 0x42 (Completed) or 0x44 (Error)

    t1 = AsmReadTsc ();

    while (!((c=IoRead8 (Base + SMBHSTSTS)) & 0x02)) {  // wait until command finished
      t2 = AsmReadTsc ();
      t = DivU64x64Remainder ((t2 - t1), DivU64x32 (gSettings.CPUStructure.TSCFrequency, 1000), 0);

      /*
       if (c != p) {
       DBG ("%02d %02X spd read status %2X\n", t, Cmd, c);
       p = c;
       }
      */

      if (c & 4) {
        // This alway happens when trying to read the memory type (Cmd 2) of an empty Slot
        // DBG ("spd byte read error for byte %2X:%d!\n", Adr, Cmd);
        break;
      }

      if (t > 5) {
        // if (Cmd != 2)
        DBG ("spd byte read taking too long for byte %2X:%d!\n", Adr, Cmd);
        break;                  // break after 5ms
      }
    }

    return IoRead8 (Base + SMBHSTDAT);
  } else {
    IoWrite8 (Base + SMBHSTSTS_NV, 0x1f);      // reset SMBus Controller
    IoWrite8 (Base + SMBHSTDAT_NV, 0xff);

    t1 = AsmReadTsc (); //rdtsc (l1, h1);
    while (IoRead8 (Base + SMBHSTSTS_NV) & 0x01) {    // wait until read
      t2 = AsmReadTsc (); //rdtsc (l2, h2);
      t = DivU64x64Remainder (
            (t2 - t1),
            DivU64x32 (gSettings.CPUStructure.TSCFrequency, 1000),
            0
          );

      if (t > 5) {
        return 0xFF;                  // break
      }
    }

    IoWrite8 (Base + SMBHSTSTS_NV, 0x00); // clear status register
    IoWrite16 (Base + SMBHSTCMD_NV, Cmd);
    IoWrite8 (Base + SMBHSTADD_NV, (Adr << 1) | 0x01);
    IoWrite8 (Base + SMBHPRTCL_NV, 0x07);

    t1 = AsmReadTsc ();

    while (!( IoRead8 (Base + SMBHSTSTS_NV) & 0x9F)) {   // wait till command finished
      t2 = AsmReadTsc ();
      t = DivU64x64Remainder (
            (t2 - t1),
            DivU64x32 (gSettings.CPUStructure.TSCFrequency, 1000),
            0
          );

      if (t > 5)
        break; // break after 5ms
    }

    return IoRead8 (Base + SMBHSTDAT_NV);
  }
}

/* SPD i2c read optimization: prefetch only what we need, read non prefetcheable bytes on the fly */
#define READ_SPD(Spd, Base, Slot, x) Spd[x] = SmbReadByte (Base, 0x50 + Slot, x)
#define LOG_INDENT "        "

#define SMST(a) ((UINT8)((Spd[a] & 0xf0) >> 4))
#define SLST(a) ((UINT8)(Spd[a] & 0x0f))

/** Read from spd * used * values only */
VOID
InitSPD (
  UINT16    *SpdIndexes,
  UINT8     *Spd,
  UINT32    Base,
  UINT8     Slot
) {
  UINT16  i;

  for (i = 0; SpdIndexes[i]; i++) {
    READ_SPD (Spd, Base, Slot, SpdIndexes[i]);
  }

  #if 0
    DBG ("Reading entire spd data\n");
    for (i = 0; i < 512; i++) {
      UINT8 b = SmbReadByte (Base, 0x50 + Slot, i);
      DBG ("%02X", b);
    }
    DBG (".\n");
  #endif
}

// Get Vendor Name from spd, 3 cases handled DDR3, DDR4 and DDR2,
// have different formats, always return a valid ptr.
CHAR8 *
GetVendorName (
  RAM_SLOT_INFO   *Slot,
  UINT8           *Spd,
  UINT32          Base,
  UINT8           SlotNum
) {
  return "Generic";
}

/** Get Default Memory Module Speed (no overclocking handled) */
UINT16
GetDDRSpeedMhz (
  UINT8     *Spd
) {
  UINT16    Frequency = 0, // default freq for unknown types //shit! DDR1 = 533
            XmpFrequency1 = 0, XmpFrequency2 = 0;
  UINT8     XmpVersion = 0, XmpProfiles = 0;

  switch (Spd[SPD_MEMORY_TYPE]) {
    case SPD_MEMORY_TYPE_SDRAM_DDR4: {
        UINT16  MinCycle = Spd[18];
        INT8    FineAdjust = Spd[125];

        Frequency = (UINT16)(2000000 / (MinCycle * 125 + FineAdjust));

        // Check if module supports XMP
        if (
          (Spd[SPD_XMP20_SIG1] == SPD_XMP_SIG1_VALUE) &&
          (Spd[SPD_XMP20_SIG2] == SPD_XMP_SIG2_VALUE)
        ) {
          XmpVersion = Spd[SPD_XMP20_VERSION];
          XmpProfiles = Spd[SPD_XMP20_PROFILES] & 3;

          if ((XmpProfiles & 1) == 1) {
            // Check the first profile
            MinCycle = Spd[SPD_XMP20_PROF1_MINCYCLE];
            FineAdjust = Spd[SPD_XMP20_PROF1_FINEADJUST];
            XmpFrequency1 = (UINT16)(2000000 / (MinCycle * 125 + FineAdjust));
            DBG ("%a XMP Profile1: %d * 125 %d ns\n", LOG_INDENT, MinCycle, FineAdjust);
          }

          if ((XmpProfiles & 2) == 2) {
            // Check the second profile
            MinCycle = Spd[SPD_XMP20_PROF2_MINCYCLE];
            FineAdjust = Spd[SPD_XMP20_PROF2_FINEADJUST];
            XmpFrequency2 = (UINT16)(2000000 / (MinCycle * 125 + FineAdjust));
            DBG ("%a XMP Profile2: %d * 125 %d ns\n", LOG_INDENT, MinCycle, FineAdjust);
          }
        }
      }
      break;

    case SPD_MEMORY_TYPE_SDRAM_DDR3: {
        // This should be multiples of MTB converted to MHz- apianti
        UINT16    divisor = Spd[10],
                  dividend = Spd[11],
                  ratio = Spd[12];

        Frequency = (
                      ((dividend != 0) && (divisor != 0) && (ratio != 0))
                        ? ((2000 * dividend) / (divisor * ratio))
                        : 0
                    );

        // Check if module supports XMP
        if (
          (Spd[SPD_XMP_SIG1] == SPD_XMP_SIG1_VALUE) &&
          (Spd[SPD_XMP_SIG2] == SPD_XMP_SIG2_VALUE)
        ) {
          XmpVersion = Spd[SPD_XMP_VERSION];
          XmpProfiles = Spd[SPD_XMP_PROFILES] & 3;

          if ((XmpProfiles & 1) == 1) {
            // Check the first profile
            divisor = Spd[SPD_XMP_PROF1_DIVISOR];
            dividend = Spd[SPD_XMP_PROF1_DIVIDEND];
            ratio = Spd[SPD_XMP_PROF1_RATIO];
            XmpFrequency1 = (
                              ((dividend != 0) && (divisor != 0) && (ratio != 0))
                                ? ((2000 * dividend) / (divisor * ratio))
                                : 0
                            );
            DBG ("%a XMP Profile1: %d * %d/%dns\n", LOG_INDENT, ratio, divisor, dividend);
          }

          if ((XmpProfiles & 2) == 2) {
            // Check the second profile
            divisor = Spd[SPD_XMP_PROF2_DIVISOR];
            dividend = Spd[SPD_XMP_PROF2_DIVIDEND];
            ratio = Spd[SPD_XMP_PROF2_RATIO];
            XmpFrequency2 = (
                              ((dividend != 0) && (divisor != 0) && (ratio != 0))
                                ? ((2000 * dividend) / (divisor * ratio))
                                : 0
                            );
            DBG ("%a XMP Profile2: %d * %d/%dns\n", LOG_INDENT, ratio, divisor, dividend);
          }
        }
      }
      break;

    case SPD_MEMORY_TYPE_SDRAM_DDR2:
    case SPD_MEMORY_TYPE_SDRAM_DDR:
      switch (Spd[9]) {
        case 0x50:
          return 400;
        case 0x3d:
          return 533;
        case 0x30:
          return 667;
        case 0x25:
        default:
          return 800;
        case 0x1E:
          return 1066;
      }
      break;

    default:
      break;
  }

  if (XmpProfiles) {
    MsgLog ("%a Found module with XMP version %d.%d\n", LOG_INDENT, (XmpVersion >> 4) & 0xF, XmpVersion & 0xF);

    switch (gSettings.XMPDetection) {
      case 0:
        // Detect the better XMP profile
        if (XmpFrequency1 >= XmpFrequency2) {
          if (XmpFrequency1 >= Frequency) {
            DBG ("%a Using XMP Profile1 instead of standard Frequency %dMHz\n", LOG_INDENT, Frequency);
            Frequency = XmpFrequency1;
          }
        } else if (XmpFrequency2 >= Frequency) {
          DBG ("%a Using XMP Profile2 instead of standard Frequency %dMHz\n", LOG_INDENT, Frequency);
          Frequency = XmpFrequency2;
        }
        break;

      case 1:
        // Use first profile if present
        if ((XmpProfiles & 1) == 1) {
          Frequency = XmpFrequency1;
          DBG ("%a Using XMP Profile1 instead of standard Frequency %dMHz\n", LOG_INDENT, Frequency);
        } else {
          DBG ("%a Not using XMP Profile1 because it is not present\n", LOG_INDENT);
        }
        break;

      case 2:
        // Use second profile
        if ((XmpProfiles & 2) == 2) {
          Frequency = XmpFrequency2;
          DBG ("%a Using XMP Profile2 instead of standard Frequency %dMHz\n", LOG_INDENT, Frequency);
        } else {
          DBG ("%a Not using XMP Profile2 because it is not present\n", LOG_INDENT);
        }
        break;

      default:
        break;
    }
  } else {
    // Print out XMP not detected
    switch (gSettings.XMPDetection) {
      case 0:
        DBG ("%a Not using XMP because it is not present\n", LOG_INDENT);
        break;

      case 1:
      case 2:
        DBG ("%a Not using XMP Profile%d because it is not present\n", LOG_INDENT, gSettings.XMPDetection);
        break;

      default:
        break;
    }
  }

  return Frequency;
}

/** Get DDR3 or DDR2 serial number, 0 most of the times, always return a valid ptr */
CHAR8 *
GetDDRSerial (
  UINT8  *Spd
) {
  UINTN   Len = 17;
  CHAR8   *AsciiSerial = AllocateZeroPool (Len);

  switch (Spd[SPD_MEMORY_TYPE]) {
    case SPD_MEMORY_TYPE_SDRAM_DDR4:
      AsciiSPrint (
        AsciiSerial, Len, "%2X%2X%2X%2X%2X%2X%2X%2X",
        SMST (325) /* & 0x7 */,
        SLST (325),
        SMST (326),
        SLST (326),
        SMST (327),
        SLST (327),
        SMST (328),
        SLST (328)
      );
      break;

    case SPD_MEMORY_TYPE_SDRAM_DDR3:
      AsciiSPrint (
        AsciiSerial, Len, "%2X%2X%2X%2X%2X%2X%2X%2X",
        SMST (122) /* & 0x7 */,
        SLST (122),
        SMST (123),
        SLST (123),
        SMST (124),
        SLST (124),
        SMST (125),
        SLST (125)
      );
      break;

    case SPD_MEMORY_TYPE_SDRAM_DDR2:
    case SPD_MEMORY_TYPE_SDRAM_DDR:
      AsciiSPrint (
        AsciiSerial, Len, "%2X%2X%2X%2X%2X%2X%2X%2X",
        SMST (95) /* & 0x7 */,
        SLST (95),
        SMST (96),
        SLST (96),
        SMST (97),
        SLST (97),
        SMST (98),
        SLST (98)
      );
      break;

    default:
      AsciiStrCpyS (AsciiSerial, Len, "0000000000000000");
      break;
  }

  return AsciiSerial;
}

/** Get DDR2 or DDR3 or DDR4 Part Number, always return a valid ptr */
CHAR8 *
GetDDRPartNum (
  UINT8   *Spd,
  UINT32  Base,
  UINT8   Slot
) {
  UINT16  i, Start=0, Index = 0;
  CHAR8   c, *AsciiPartNo = AllocateZeroPool (32);

  switch (Spd[SPD_MEMORY_TYPE]) {
    case SPD_MEMORY_TYPE_SDRAM_DDR4:
      Start = 329;
      break;

    case SPD_MEMORY_TYPE_SDRAM_DDR3:
      Start = 128;
      break;

    case SPD_MEMORY_TYPE_SDRAM_DDR2:
    case SPD_MEMORY_TYPE_SDRAM_DDR:
      Start = 73;
      break;

    default:
      break;
  }

  for (i = Start; i < Start + 20; i++) {
    READ_SPD (Spd, Base, Slot, (UINT16)i); // only read once the corresponding model part (ddr3 or ddr2)
    c = Spd[i];

    if (IS_ALFA (c) || IS_DIGIT (c) || IS_PUNCT (c)) { // It seems that System Profiler likes only letters and digits...
      AsciiPartNo[Index++] = c;
    } else if (c < 0x20) {
      break;
    }
  }

  return AsciiPartNo;
}

/** Read from smbus the SPD content and interpret it for detecting memory attributes */
STATIC
VOID
ReadSmb (
  EFI_PCI_IO_PROTOCOL   *PciIo,
  UINT16                Vid,
  UINT16                Did
) {
  //EFI_STATUS  Status;
  //RAM_SLOT_INFO  *Slot;
  //BOOLEAN     fullBanks;
  //UINT16      Vid, Did;
  UINT16      Speed, Command;
  UINT32      Base, Mmio, HostC;
  UINT8       *SpdBuf, i, SpdType;

  SmbPage = 0; // valid pages are 0 and 1; assume the first page (page 0) is already selected
  //Vid = gPci->Hdr.VendorId;
  //Did = gPci->Hdr.DeviceId;

  /*Status = */PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            0x04,
                            1,
                            &Command
                          );

  Command |= 1;
  /*Status = */PciIo->Pci.Write (
                             PciIo,
                             EfiPciIoWidthUint16,
                             0x04,
                             1,
                             &Command
                           );

  DBG ("SMBus CmdReg: 0x%x\n", Command);

  /*Status = */PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint32,
                            0x10,
                            1,
                            &Mmio
                          );
  if (Vid == 0x8086) {
    /*Status = */PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0x20,
                              1,
                              &Base
                            );

    Base &= 0xFFFE;
    SmbIntel = TRUE;
  } else {
    /*Status = */PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0x24, // ioBase offset 0x24 on MCP
                              1,
                              &Base
                            );
    Base &= 0xFFFC;
    SmbIntel = FALSE;
  }

 /*Status = */PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint32,
                            0x40,
                            1,
                            &HostC
                          );

  MsgLog (
    "Scanning SMBus [%04x:%04x], Mmio: 0x%x, ioport: 0x%x, HostC: 0x%x\n",
    Vid, Did, Mmio, Base, HostC
  );

  // needed at least for laptops
  //fullBanks = (gDMI->MemoryModules == gDMI->CntMemorySlots);

  SpdBuf = AllocateZeroPool (MAX_SPD_SIZE);

  // Search MAX_RAM_SLOTS Slots
  //==>
  /*
    TotalSlotsCount = (UINT8)TotalCount; // TotalCount from SMBIOS.
    if (!TotalSlotsCount) {
      TotalSlotsCount = MAX_RAM_SLOTS;
    }
  */

  //TotalSlotsCount = 8; //MAX_RAM_SLOTS;  -- spd can read only 8 Slots

  MsgLog ("Slots to scan: %d (max)\n", MAX_RAM_SLOTS);

  for (i = 0; i <  MAX_RAM_SLOTS; i++) {
    //<==
    ZeroMem (SpdBuf, MAX_SPD_SIZE);
    READ_SPD (SpdBuf, Base, i, SPD_MEMORY_TYPE);

    if (SpdBuf[SPD_MEMORY_TYPE] == 0) {
      // First 0x40 bytes of DDR4 spd second page is 0. Maybe we need to change page, so do that and retry.
      //DBG (" - [%02d]: Got invalid type 0 @0x%x. Will set page and retry.\n", i, 0x50 + i);
      SmbPage = 0xFF; // force page to be set
      READ_SPD (SpdBuf, Base, i, SPD_MEMORY_TYPE);
    }

    SpdType = SpdBuf[SPD_MEMORY_TYPE];

    if (SpdType == 0xFF) {
      DBG (" - [%02d]: Empty\n", i);
      continue;
    }

    // Copy spd data into buffer

    switch (SpdType)  {
      case SPD_MEMORY_TYPE_SDRAM_DDR:
        InitSPD (SpdIndexesDDR, SpdBuf, Base, i);

        gSettings.RAM.SPD[i].Type = MemoryTypeDdr;
        gSettings.RAM.SPD[i].ModuleSize =  (
                                              (
                                               (1 << ((SpdBuf[SPD_NUM_ROWS] & 0x0f) + (SpdBuf[SPD_NUM_COLUMNS] & 0x0f) - 17)) *
                                               ((SpdBuf[SPD_NUM_DIMM_BANKS] & 0x7) + 1) * SpdBuf[SPD_NUM_BANKS_PER_SDRAM]
                                              ) / 3
                                            ) * 2;
        break;

      case SPD_MEMORY_TYPE_SDRAM_DDR2:
        InitSPD (SpdIndexesDDR, SpdBuf, Base, i);

        gSettings.RAM.SPD[i].Type = MemoryTypeDdr2;
        gSettings.RAM.SPD[i].ModuleSize =  (
                                              (1 << ((SpdBuf[SPD_NUM_ROWS] & 0x0f) + (SpdBuf[SPD_NUM_COLUMNS] & 0x0f) - 17)) *
                                              ((SpdBuf[SPD_NUM_DIMM_BANKS] & 0x7) + 1) * SpdBuf[SPD_NUM_BANKS_PER_SDRAM]
                                            );
        break;

      case SPD_MEMORY_TYPE_SDRAM_DDR3:
        InitSPD (SpdIndexesDDR3, SpdBuf, Base, i);

        gSettings.RAM.SPD[i].Type = MemoryTypeDdr3;
        gSettings.RAM.SPD[i].ModuleSize = ((SpdBuf[4] & 0x0f) + 28) + ((SpdBuf[8] & 0x7)  + 3);
        gSettings.RAM.SPD[i].ModuleSize -= (SpdBuf[7] & 0x7) + 25;
        gSettings.RAM.SPD[i].ModuleSize = ((1 << gSettings.RAM.SPD[i].ModuleSize) * (((SpdBuf[7] >> 3) & 0x1f) + 1));
        break;

      case SPD_MEMORY_TYPE_SDRAM_DDR4:
        InitSPD (SpdIndexesDDR4, SpdBuf, Base, i);

        gSettings.RAM.SPD[i].Type = MemoryTypeDdr4;

        gSettings.RAM.SPD[i].ModuleSize =
          (1 << ((SpdBuf[4] & 0x0f) + 8 /* Mb */ - 3 /* MB */)) // SDRAM Capacity
          * (1 << ((SpdBuf[13] & 0x07) + 3)) // Primary Bus Width
          / (1 << ((SpdBuf[12] & 0x07) + 2)) // SDRAM Width
          * (((SpdBuf[12] >> 3) & 0x07) + 1) // Logical Ranks per DIMM
          * (((SpdBuf[6] & 0x03) == 2) ? (((SpdBuf[6] >> 4) & 0x07) + 1) : 1);

        /*
         Total = SDRAM Capacity / 8 * Primary Bus Width / SDRAM Width * Logical Ranks per DIMM
         where:
         : SDRAM Capacity = SPD byte 4 bits 3~0
         : Primary Bus Width = SPD byte 13 bits 2~0
         : SDRAM Width = SPD byte 12 bits 2~0
         : Logical Ranks per DIMM =
         for SDP, DDP, QDP: = SPD byte 12 bits 5~3
         for 3DS: = SPD byte 12 bits 5~3
         times SPD byte 6 bits 6~4 (Die Count)

         SDRAM Capacity

         0  0000 = 256 Mb
         1  0001 = 512 Mb
         2  0010 = 1 Gb
         3  0011 = 2 Gb
         4  0100 = 4 Gb
         5  0101 = 8 Gb
         6  0110 = 16 Gb
         7  0111 = 32 Gb

         Primary Bus Width

         000 = 8 bits
         001 = 16 bits
         010 = 32 bits
         011 = 64 bits

         SDRAM Device Width

         000 = 4 bits
         001 = 8 bits
         010 = 16 bits
         011 = 32 bits

         Logical Ranks per DIMM for SDP, DDP, QDP

         000 = 1 Package Rank
         001 = 2 Package Ranks
         010 = 3 Package Ranks
         011 = 4 Package Ranks

         Die Count for 3DS

         000 = Single die 001 = 2 die
         010 = 3 die
         011 = 4 die
         100 = 5 die
         101 = 6 die
         110 = 7 die
         111 = 8 die
         */
        break;

      default:
        gSettings.RAM.SPD[i].ModuleSize = 0;
        break;
    }

    if (gSettings.RAM.SPD[i].ModuleSize == 0) {
      DBG (" - [%02d]: Invalid\n", i);
      continue;
    }

    MsgLog (" - [%02d]: Type %d @0x%x\n", i, SpdType, 0x50 + i);

    //SpdType = (Slot->spd[SPD_MEMORY_TYPE] < ((UINT8)12) ? Slot->spd[SPD_MEMORY_TYPE] : 0);
    //gRAM Type = spd_mem_to_smbios[SpdType];
    gSettings.RAM.SPD[i].PartNo = GetDDRPartNum (SpdBuf, Base, i);
    gSettings.RAM.SPD[i].Vendor = GetVendorName (&(gSettings.RAM.SPD[i]), SpdBuf, Base, i);
    gSettings.RAM.SPD[i].SerialNo = GetDDRSerial (SpdBuf);
    //XXX - when we can FreePool allocated for these buffers?
    // determine spd Speed
    Speed = GetDDRSpeedMhz (SpdBuf);

    DBG ("%a DDR Speed %dMHz\n", LOG_INDENT, Speed);

    if (gSettings.RAM.SPD[i].Frequency < Speed) {
      gSettings.RAM.SPD[i].Frequency = Speed;
    }

    #if 0
    // pci memory controller if available, is more reliable
    if (gSettings.RAM.Frequency > 0) {
      UINT32 freq = (UINT32)DivU64x32 (gSettings.RAM.Frequency, 500000);
      // now round off special cases
      UINT32 fmod100 = freq %100;
      switch (fmod100) {
        case  1:  freq--;   break;
        case 32:  freq++;   break;
        case 65:  freq++;   break;
        case 98:  freq+=2;  break;
        case 99:  freq++;   break;
      }

      gSettings.RAM.SPD[i].Frequency = freq;
      DBG ("%a RAM Speed %dMHz\n", LOG_INDENT, freq);
    }
    #endif

    MsgLog (
      "%a Type %d %dMB %dMHz Vendor=%a PartNo=%a SerialNo=%a\n",
      LOG_INDENT,
      (INT32)gSettings.RAM.SPD[i].Type,
      gSettings.RAM.SPD[i].ModuleSize,
      gSettings.RAM.SPD[i].Frequency,
      gSettings.RAM.SPD[i].Vendor,
      gSettings.RAM.SPD[i].PartNo,
      gSettings.RAM.SPD[i].SerialNo
    );

    gSettings.RAM.SPD[i].InUse = TRUE;
    ++(gSettings.RAM.SPDInUse);
  } // for

  if (SmbPage != 0) {
    READ_SPD (SpdBuf, Base, 0, 0); // force first page when we're done
  }
}

VOID
ScanSPD () {
  EFI_STATUS            Status;
  EFI_HANDLE            *HandleBuffer = NULL;
  EFI_PCI_IO_PROTOCOL   *PciIo = NULL;
  UINTN                 HandleCount, Index;
  PCI_TYPE00            Pci;

  DbgHeader ("ScanSPD");

  // Scan PCI handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );

  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < HandleCount; ++Index) {
      Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
      if (!EFI_ERROR (Status)) {
        // Read PCI BUS
        //PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        Status = PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              sizeof (Pci) / sizeof (UINT32),
                              &Pci
                            );
        //SmBus controller has class = 0x0c0500
        if (
          (Pci.Hdr.ClassCode[2] == 0x0c) &&
          (Pci.Hdr.ClassCode[1] == 5) &&
          (Pci.Hdr.ClassCode[0] == 0) &&
          ((Pci.Hdr.VendorId == 0x8086) || (Pci.Hdr.VendorId == 0x10DE))
        ) {
          DBG (
            "SMBus device : %04x %04x class=%02x%02x%02x status=%r\n",
            Pci.Hdr.VendorId,
            Pci.Hdr.DeviceId,
            Pci.Hdr.ClassCode[2],
            Pci.Hdr.ClassCode[1],
            Pci.Hdr.ClassCode[0],
            Status
          );

          ReadSmb (PciIo, Pci.Hdr.VendorId, Pci.Hdr.DeviceId);
        }
      }
    }
  }
}
