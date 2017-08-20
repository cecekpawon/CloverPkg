/*
 * ATI Graphics Card Enabler, part of the Chameleon Boot Loader Project
 *
 * Copyright 2010 by Islam M. Ahmed Zaid. All rights reserved.
 *
 */

#include <Library/Platform/Platform.h>
//#include <Library/Platform/Ati.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_ATI
#define DEBUG_ATI -1
#endif
#else
#ifdef DEBUG_ATI
#undef DEBUG_ATI
#endif
#define DEBUG_ATI DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_ATI, __VA_ARGS__)

#define S_ATIMODEL "ATI/AMD Graphics"

#define OFFSET_TO_GET_ATOMBIOS_STRINGS_START 0x6e
//#define DATVAL(x)     { kPtr, sizeof(x), (UINT8 *)x }
#define STRVAL(x)     { kStr, sizeof(x)-1, (UINT8 *)x }
//#define BYTVAL(x)     { kCst, 1, (UINT8 *)(UINTN)x }
#define WRDVAL(x)     { kCst, 2, (UINT8 *)(UINTN)x }
#define DWRVAL(x)     { kCst, 4, (UINT8 *)(UINTN)x }
//QWRVAL would work only in 64 bit
//#define QWRVAL(x)     { kCst, 8, (UINT8 *)(UINTN)x }
#define NULVAL        { kNul, 0, (UINT8 *)NULL }

#define RADEON_CRTC_GEN_CNTL    0x0050
#define RADEON_CRTC_EN          (1 << 25)
#define RADEON_CRTC2_GEN_CNTL   0x03f8
#define R600_CONFIG_MEMSIZE     0x5428

/* Typedefs ENUMS */
typedef enum {
  kNul,
  kStr,
  kPtr,
  kCst
} ATI_TYPE;

typedef struct {
        DevPropDevice   *device;
        //ATI_CARD_INFO   *info;
        PCI_DT          *pci_dev;
        UINT8           *fb;
        UINT8           *mmio;
        UINT8           *io;
        UINT8           *rom;
        UINT32          rom_size;
        UINT64          vram_size;
  CONST CHAR8           *cfg_name;
        UINT8           ports;
        UINT32          flags;
        BOOLEAN         posted;
} ATI_CARD;

#define MKFLAG(n)   (1 << n)
#define FLAGTRUE    MKFLAG(0)
//#define EVERGREEN   MKFLAG(1)
#define FLAGMOBILE  MKFLAG(2)
#define FLAGOLD     MKFLAG(3)
#define FLAGNOTFAKE MKFLAG(4)

typedef struct {
  ATI_TYPE  type;
  UINT32    size;
  UINT8     *data;
} ATI_VAL;

typedef struct {
  UINT32    flags;
  BOOLEAN   all_ports;
  CHAR8     *name;
  BOOLEAN   (*get_value) (ATI_VAL *Val, INTN Index);
  ATI_VAL   default_val;
} ATI_DEV_PROP;

BOOLEAN GetBootDisplayVal       (ATI_VAL *Val, INTN Index);
BOOLEAN GetVramVal              (ATI_VAL *Val, INTN Index);
BOOLEAN GetEdidVal              (ATI_VAL *Val, INTN Index);
BOOLEAN GetDisplayType          (ATI_VAL *Val, INTN Index);
BOOLEAN GetNameVal              (ATI_VAL *Val, INTN Index);
BOOLEAN GetNameParentVal        (ATI_VAL *Val, INTN Index);
BOOLEAN GetModelVal             (ATI_VAL *Val, INTN Index);
BOOLEAN GetConnTypeVal          (ATI_VAL *Val, INTN Index);
BOOLEAN GetVramSizeVal          (ATI_VAL *Val, INTN Index);
BOOLEAN GetBinImageVal          (ATI_VAL *Val, INTN Index);
BOOLEAN GetBinImageOwr          (ATI_VAL *Val, INTN Index);
BOOLEAN GetRomRevisionVal       (ATI_VAL *Val, INTN Index);
BOOLEAN GetDeviceIdVal          (ATI_VAL *Val, INTN Index);
BOOLEAN GetMclkVal              (ATI_VAL *Val, INTN Index);
BOOLEAN GetSclkVal              (ATI_VAL *Val, INTN Index);
BOOLEAN GetRefclkVal            (ATI_VAL *Val, INTN Index);
BOOLEAN GetPlatformInfoVal      (ATI_VAL *Val, INTN Index);
BOOLEAN GetVramTotalSizeVal     (ATI_VAL *Val, INTN Index);
BOOLEAN GetDualLinkVal          (ATI_VAL *Val, INTN Index);
BOOLEAN GetNamePciVal           (ATI_VAL *Val, INTN Index);

//STATIC ATI_VAL  ATYModel;
STATIC  ATI_VAL   ATYName;
STATIC  ATI_VAL   ATYNameParent;
        ATI_CARD  *Card;

ATI_DEV_PROP  ATIDevPropList[] = {
  { FLAGTRUE,    FALSE,  "@0,AAPL,boot-display",             GetBootDisplayVal,    NULVAL },
  //{ FLAGTRUE,  FALSE,  "@0,ATY,EFIDisplay",                NULL,                 STRVAL ("TMDSA") },

  //{ FLAGTRUE,  TRUE,   "@0,AAPL,vram-memory",              GetVramVal,           NULVAL },
  { FLAGTRUE,    TRUE,   "AAPL00,override-no-connect",       GetEdidVal,           NULVAL },
  { FLAGNOTFAKE, TRUE,   "@0,compatible",                    GetNameVal,           NULVAL },
  { FLAGTRUE,    TRUE,   "@0,connector-type",                GetConnTypeVal,       NULVAL },
  { FLAGTRUE,    TRUE,   "@0,device_type",                   NULL,                 STRVAL ("display") },
  //{ FLAGTRUE,  FALSE,  "@0,display-connect-flags",         NULL,                 DWRVAL (0) },

  //some set of properties for mobile radeons
  { FLAGMOBILE,  FALSE,  "@0,display-link-component-bits",   NULL,                 DWRVAL (6) },
  { FLAGMOBILE,  FALSE,  "@0,display-pixel-component-bits",  NULL,                 DWRVAL (6) },
  { FLAGMOBILE,  FALSE,  "@0,display-dither-support",        NULL,                 DWRVAL (0) },
  { FLAGMOBILE,  FALSE,  "@0,backlight-control",             NULL,                 DWRVAL (1) },
  { FLAGTRUE,    FALSE,  "AAPL00,Dither",                    NULL,                 DWRVAL (0) },

  //{ FLAGTRUE,  TRUE,   "@0,display-type",                  NULL,                 STRVAL ("NONE") },
  { FLAGTRUE,    TRUE,   "@0,name",                          GetNameVal,           NULVAL },
  { FLAGTRUE,    TRUE,   "@0,VRAM,memsize",                  GetVramSizeVal,       NULVAL },
  //{ FLAGTRUE,  TRUE,   "@0,ATY,memsize",                   GetVramSizeVal,       NULVAL },

  { FLAGTRUE,    FALSE,  "AAPL,aux-power-connected",         NULL,                 DWRVAL (1) },
  { FLAGTRUE,    FALSE,  "AAPL00,DualLink",                  GetDualLinkVal,       NULVAL  },
  { FLAGMOBILE,  FALSE,  "AAPL,HasPanel",                    NULL,                 DWRVAL (1) },
  { FLAGMOBILE,  FALSE,  "AAPL,HasLid",                      NULL,                 DWRVAL (1) },
  { FLAGMOBILE,  FALSE,  "AAPL,backlight-control",           NULL,                 DWRVAL (1) },
  { FLAGTRUE,    FALSE,  "AAPL,overwrite_binimage",          GetBinImageOwr,       NULVAL },
  { FLAGTRUE,    FALSE,  "ATY,bin_image",                    GetBinImageVal,       NULVAL },
  { FLAGTRUE,    FALSE,  "ATY,Copyright",                    NULL,                 STRVAL ("Copyright AMD Inc. All Rights Reserved. 2005-2011") },
  { FLAGTRUE,    FALSE,  "ATY,EFIVersion",                   NULL,                 STRVAL ("01.00.3180") },
  { FLAGTRUE,    FALSE,  "ATY,Card#",                        GetRomRevisionVal,    NULVAL },
  //{ FLAGTRUE,  FALSE,  "ATY,Rom#",                         NULL,                 STRVAL ("www.amd.com") },
  { FLAGNOTFAKE, FALSE,  "ATY,VendorID",                     NULL,                 WRDVAL (0x1002) },
  { FLAGNOTFAKE, FALSE,  "ATY,DeviceID",                     GetDeviceIdVal,       NULVAL },

  //{ FLAGTRUE,  FALSE,  "ATY,MCLK",                         GetMclkVal,           NULVAL },
  //{ FLAGTRUE,  FALSE,  "ATY,SCLK",                         GetSclkVal,           NULVAL },
  { FLAGTRUE,    FALSE,  "ATY,RefCLK",                       GetRefclkVal,         DWRVAL (0x0a8c) },

  { FLAGTRUE,    FALSE,  "ATY,PlatformInfo",                 GetPlatformInfoVal,   NULVAL },
  { FLAGOLD,     FALSE,  "compatible",                       GetNamePciVal,        NULVAL },
  { FLAGTRUE,    FALSE,  "name",                             GetNameParentVal,     NULVAL },
  { FLAGTRUE,    FALSE,  "device_type",                      GetNameParentVal,     NULVAL },
  { FLAGTRUE,    FALSE,  "model",                            GetModelVal,          STRVAL (S_ATIMODEL)},
  //{ FLAGTRUE,  FALSE,  "VRAM,totalsize",                   GetVramTotalSizeVal,  NULVAL },

  { FLAGTRUE,    FALSE,  NULL,                               NULL,                 NULVAL }
};

#define DEF_ATI_CFGNAME   "Eulemur"

KERNEL_AND_KEXT_PATCHES   *CurrentPatches;
CHAR8                     *DarwinOSVersion = NULL;

VOID
GetAtiModel (
  OUT GFX_PROPERTIES  *Gfx,
  IN  UINT32          DevId
) {
  AsciiStrCpyS (Gfx->Model, ARRAY_SIZE (Gfx->Model), S_ATIMODEL);
}

BOOLEAN
GetBootDisplayVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = 1;

  if (!Card->posted) {
    return FALSE;
  }

  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
GetDualLinkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = gSettings.DualLink;

  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
GetVramVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetEdidVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  if (!gSettings.InjectEDID || !gSettings.CustomEDID) {
    return FALSE;
  }

  Val->type = kPtr;
  Val->size = 128;
  Val->data = AllocateCopyPool (Val->size, gSettings.CustomEDID);

  return TRUE;
}

STATIC CONST CHAR8 *dtyp[] = { "LCD", "CRT", "DVI", "NONE" };
STATIC UINT32 dti = 0;

BOOLEAN
GetDisplayType (
  ATI_VAL   *Val,
  INTN      Index
) {
  dti++;

  if (dti > 3) {
    dti = 0;
  }

  Val->type = kStr;
  Val->size = 4;
  Val->data = (UINT8 *)dtyp[dti];

  return TRUE;
}

BOOLEAN
GetNameVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = ATYName.type;
  Val->size = ATYName.size;
  Val->data = ATYName.data;

  return TRUE;
}

BOOLEAN
GetNameParentVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = ATYNameParent.type;
  Val->size = ATYNameParent.size;
  Val->data = ATYNameParent.data;

  return TRUE;
}

STATIC CHAR8 pciName[15];

BOOLEAN
GetNamePciVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINTN   Len = ARRAY_SIZE (pciName);

  if (!gSettings.FakeATI) { //(!Card->info->model_name || !gSettings.FakeATI)
    return FALSE;
  }

  AsciiSPrint (pciName, Len, "pci1002,%x", gSettings.FakeATI >> 16);
  AsciiStrCpyS (pciName, Len, AsciiStrToLower (pciName));

  Val->type = kStr;
  Val->size = 13;
  Val->data = (UINT8 *)&pciName[0];

  return TRUE;
}

BOOLEAN
GetModelVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = kStr;
  Val->size = (UINT32)AsciiStrLen (S_ATIMODEL);
  Val->data = (UINT8 *)S_ATIMODEL;

  return TRUE;
}

//TODO - get connectors from ATIConnectorsPatch
BOOLEAN
GetConnTypeVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  //Connector types:
  //0x10:  VGA
  //0x04:  DL DVI-I
  //0x800: HDMI
  //0x400: DisplayPort
  //0x02:  LVDS

  UINTN   Len = 16;

  if ((CurrentPatches == NULL) || (CurrentPatches->KPATIConnectorsDataLen == 0)) {
    return FALSE;
  }

  if (OSX_GE (DarwinOSVersion, DARWIN_OS_VER_STR_SIERRA)) {
    Len = 24;
  }

  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&CurrentPatches->KPATIConnectorsPatch[Index * Len];

  return TRUE;
}

BOOLEAN
GetVramSizeVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  INTN     i = -1;
  UINT64   MemSize;

  i++;
  MemSize = LShiftU64 ((UINT64)Card->vram_size, 32);

  if (i == 0) {
    MemSize = MemSize | (UINT64)Card->vram_size;
  }

  Val->type = kCst;
  Val->size = 8;
  Val->data = (UINT8 *)&MemSize;

  return TRUE;
}

BOOLEAN
GetBinImageVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  if (!Card->rom) {
    return FALSE;
  }

  Val->type = kPtr;
  Val->size = Card->rom_size;
  Val->data = Card->rom;

  return TRUE;
}

BOOLEAN
GetBinImageOwr (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = 1;

  if (!gSettings.LoadVBios) {
    return FALSE;
  }

  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
GetRomRevisionVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  CHAR8   *CRev = "109-B77101-00";
  UINT8   *Rev;

  if (!Card->rom) {
    Val->type = kPtr;
    Val->size = 13;
    Val->data = AllocateZeroPool (Val->size);
    if (!Val->data)
      return FALSE;

    CopyMem (Val->data, CRev, Val->size);

    return TRUE;
  }

  Rev = Card->rom + *(UINT8 *)(Card->rom + OFFSET_TO_GET_ATOMBIOS_STRINGS_START);

  Val->type = kPtr;
  Val->size = (UINT32)AsciiStrLen ((CHAR8 *)Rev);

  if ((Val->size < 3) || (Val->size > 30)) { //fool proof. Real Value 13
    Rev = (UINT8 *)CRev;
    Val->size = 13;
  }

  Val->data = AllocateZeroPool (Val->size);

  if (!Val->data) {
    return FALSE;
  }

  CopyMem (Val->data, Rev, Val->size);

  return TRUE;
}

BOOLEAN
GetDeviceIdVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = kCst;
  Val->size = 2;
  Val->data = (UINT8 *)&Card->pci_dev->device_id;

  return TRUE;
}

BOOLEAN
GetMclkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetSclkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetRefclkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetPlatformInfoVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->data = AllocateZeroPool (0x80);

  if (!Val->data) {
    return FALSE;
  }

  Val->type   = kPtr;
  Val->size   = 0x80;
  Val->data[0]  = 1;

  return TRUE;
}

BOOLEAN
GetVramTotalSizeVal (
  ATI_VAL   *Val,
  INTN      index
) {
  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&Card->vram_size;

  return TRUE;
}

VOID
FreeVal (
  ATI_VAL   *Val
) {
  if (Val->type == kPtr) {
    FreePool (Val->data);
  }

  ZeroMem (Val, sizeof (ATI_VAL));
}

VOID
DevpropAddList (
  ATI_DEV_PROP    DevPropList[]
) {
  INTN      i, PNum;
  ATI_VAL   *Val = AllocateZeroPool (sizeof (ATI_VAL));

  for (i = 0; DevPropList[i].name != NULL; i++) {
    if ((DevPropList[i].flags == FLAGTRUE) || BIT_ISSET (DevPropList[i].flags, Card->flags)) {
      if (DevPropList[i].get_value && DevPropList[i].get_value (Val, 0)) {
        DevpropAddValue (Card->device, DevPropList[i].name, Val->data, Val->size);
        FreeVal (Val);

        if (DevPropList[i].all_ports) {
          for (PNum = 1; PNum < Card->ports; PNum++) {
            if (DevPropList[i].get_value (Val, PNum)) {
              DevPropList[i].name[1] = (CHAR8)(0x30 + PNum); // convert to ascii
              DevpropAddValue (Card->device, DevPropList[i].name, Val->data, Val->size);
              FreeVal (Val);
            }
          }

          DevPropList[i].name[1] = 0x30; // write back our "@0," for a next possible card
        }
      } else {
        if (DevPropList[i].default_val.type != kNul) {
          DevpropAddValue (
            Card->device, DevPropList[i].name,
            DevPropList[i].default_val.type == kCst
              ? (UINT8 *)&(DevPropList[i].default_val.data)
              : DevPropList[i].default_val.data,
            DevPropList[i].default_val.size
          );
        }

        if (DevPropList[i].all_ports) {
          for (PNum = 1; PNum < Card->ports; PNum++) {
            if (DevPropList[i].default_val.type != kNul) {
              DevPropList[i].name[1] = (CHAR8)(0x30 + PNum); // convert to ascii
              DevpropAddValue (
                Card->device, DevPropList[i].name,
                DevPropList[i].default_val.type == kCst
                  ? (UINT8 *)&(DevPropList[i].default_val.data)
                  : DevPropList[i].default_val.data,
                DevPropList[i].default_val.size
              );
            }
          }

          DevPropList[i].name[1] = 0x30; // write back our "@0," for a next possible card
        }
      }
    }
  }

  FreePool (Val);
}

BOOLEAN
ValidateROM (
  OPTION_ROM_HEADER   *RomHeader,
  PCI_DT              *Dev
) {
  OPTION_ROM_PCI_HEADER   *RomPciHeader;

  if (RomHeader->signature != 0xaa55) {
    DBG ("invalid ROM signature %x\n", RomHeader->signature);
    return FALSE;
  }

  RomPciHeader = (OPTION_ROM_PCI_HEADER *)((UINT8 *)RomHeader + RomHeader->pci_header_offset);

  if (RomPciHeader->signature != 0x52494350) {
    DBG ("invalid ROM header %x\n", RomPciHeader->signature);
    return FALSE;
  }

  if (
    (RomPciHeader->vendor_id != Dev->vendor_id) ||
    (RomPciHeader->device_id != Dev->device_id)
  ) {
    DBG ("invalid ROM vendor=%x deviceID=%d\n", RomPciHeader->vendor_id, RomPciHeader->device_id);
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
LoadVbiosFile (
  UINT16  VendorId,
  UINT16  DeviceId
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  UINTN         BufferLen = 0;
  CHAR16        FileName[64];
  UINT8         *Buffer = 0;

  //if we are here then TRUE
  //if (!gSettings.LoadVBios) {
  //  return FALSE;
  //}

  UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\%04x_%04x.rom", DIR_ROM, VendorId, DeviceId);
  if (FileExists (gSelfRootDir, FileName)) {
    DBG ("Found generic VBIOS ROM file (%04x_%04x.rom)\n", VendorId, DeviceId);
    Status = LoadFile (gSelfRootDir, FileName, &Buffer, &BufferLen);
  }

  if (EFI_ERROR (Status) || (BufferLen == 0)) {
    DBG ("ATI ROM not found \n");
    Card->rom_size = 0;
    Card->rom = 0;
    return FALSE;
  }

  DBG ("Loaded ROM len=%d\n", BufferLen);

  Card->rom_size = (UINT32)BufferLen;
  Card->rom = AllocateZeroPool (BufferLen);

  if (!Card->rom) {
    return FALSE;
  }

  CopyMem (Card->rom, Buffer, BufferLen);
  //  read (fd, (CHAR8 *)Card->rom, Card->rom_size);

  if (!ValidateROM ((OPTION_ROM_HEADER *)Card->rom, Card->pci_dev)) {
    DBG ("ValidateROM fails\n");
    Card->rom_size = 0;
    Card->rom = 0;
    return FALSE;
  }

  BufferLen = ((OPTION_ROM_HEADER *)Card->rom)->rom_size;
  Card->rom_size = (UINT32)(BufferLen << 9);
  DBG ("Calculated ROM len=%d\n", Card->rom_size);
  //  close (fd);
  FreePool (Buffer);

  return TRUE;
}

VOID
GetVramSize () {
  Card->vram_size = 128 << 20; //default 128Mb, this is minimum for OS
  if (gSettings.VRAM != 0) {
    Card->vram_size = gSettings.VRAM;
    DBG ("Set VRAM from config=%dMb\n", (INTN)RShiftU64 (Card->vram_size, 20));
    //WRITEREG32 (Card->mmio, RADEON_CONFIG_MEMSIZE, Card->vram_size);
  }

  gSettings.VRAM = Card->vram_size;
  DBG ("ATI: GetVramSize returned 0x%x\n", Card->vram_size);
}

BOOLEAN
ReadVBios (
  BOOLEAN   FromPci
) {
  OPTION_ROM_HEADER   *RomAddr;

  if (FromPci) {
    RomAddr = (OPTION_ROM_HEADER *)(UINTN)(PciConfigRead32 (Card->pci_dev, PCI_EXPANSION_ROM_BASE) & ~0x7ff);
    DBG (" @0x%x\n", RomAddr);
  } else {
    RomAddr = (OPTION_ROM_HEADER *)(UINTN)0xc0000;
  }

  if (!ValidateROM (RomAddr, Card->pci_dev)) {
    DBG ("There is no ROM @0x%x\n", RomAddr);
    return FALSE;
  }

  Card->rom_size = (UINT32)(RomAddr->rom_size) << 9;

  if (!Card->rom_size) {
    DBG ("invalid ROM size =0\n");
    return FALSE;
  }

  Card->rom = AllocateZeroPool (Card->rom_size);

  if (!Card->rom) {
    return FALSE;
  }

  CopyMem (Card->rom, (VOID *)RomAddr, Card->rom_size);

  return TRUE;
}

BOOLEAN
RadeonCardPosted () {
  UINT32  Reg = REG32 (Card->mmio, RADEON_CRTC_GEN_CNTL) | REG32 (Card->mmio, RADEON_CRTC2_GEN_CNTL);

  if (BIT_ISSET (Reg, RADEON_CRTC_EN)) {
    return TRUE;
  }

  // then check MEM_SIZE, in case something turned the crtcs off
  Reg = REG32 (Card->mmio, R600_CONFIG_MEMSIZE);

  if (Reg) {
    return TRUE;
  }

  return FALSE;
}

STATIC
BOOLEAN
InitCard (
  PCI_DT    *Dev
) {
  BOOLEAN     LoadVBios = gSettings.LoadVBios;
  CHAR8       *Name, *NameParent, *CfgName;
  UINTN       j, ExpansionRom = 0; // i,
  INTN        NameLen = 0, PortsNum = 0;

  Card = AllocateZeroPool (sizeof (ATI_CARD));

  if (!Card) {
    return FALSE;
  }

  Card->pci_dev = Dev;

  for (j = 0; j < gSettings.NGFX; j++) {
    if (
      (gSettings.Graphics[j].Vendor == GfxAti) &&
      (gSettings.Graphics[j].DeviceID == Dev->device_id)
    ) {
      //      model = gSettings.Graphics[j].Model;
      PortsNum = gSettings.Graphics[j].Ports;
      LoadVBios = gSettings.Graphics[j].LoadVBios;
      break;
    }
  }

  Card->fb    = (UINT8 *)(UINTN)(PciConfigRead32 (Dev, PCI_BASE_ADDRESS_0) & ~0x0f);
  Card->mmio  = (UINT8 *)(UINTN)(PciConfigRead32 (Dev, PCI_BASE_ADDRESS_2) & ~0x0f);
  Card->io    = (UINT8 *)(UINTN)(PciConfigRead32 (Dev, PCI_BASE_ADDRESS_4) & ~0x03);
  Dev->regs = Card->mmio;
  ExpansionRom = PciConfigRead32 (Dev, PCI_EXPANSION_ROM_BASE); //0x30 as Chimera

  DBG ("Framebuffer @0x%08X  MMIO @0x%08X  I/O Port @0x%08X ROM Addr @0x%08X\n",
      Card->fb, Card->mmio, Card->io, ExpansionRom);

  Card->posted = RadeonCardPosted ();

  DBG ("ATI card %a\n", Card->posted ? "POSTed" : "non-POSTed");

  GetVramSize ();

  if (LoadVBios) {
    LoadVbiosFile (Dev->vendor_id, Dev->device_id);
    if (!Card->rom) {
      DBG ("reading VBIOS from %a", Card->posted ? "legacy space" : "PCI ROM");
      if (Card->posted) { // && ExpansionRom != 0)
        ReadVBios (FALSE);
      } /*else {
        ReadDisabledVbios ();
      }*/
      DBG ("\n");
    } else {
      DBG ("VideoBIOS read from file\n");
    }
  }

  if (gSettings.Mobile) {
    DBG ("ATI Mobile Radeon\n");
    Card->flags |= FLAGMOBILE;
  }

  Card->flags |= FLAGNOTFAKE;

  NameLen = StrLen (gSettings.FBName);
  if (NameLen > 2) {  //fool proof: cfg_name is 3 character or more.
    CfgName = AllocateZeroPool (NameLen);
    UnicodeStrToAsciiStrS ((CHAR16 *)&gSettings.FBName[0], CfgName, NameLen);
    DBG ("Users config name %a\n", CfgName);
    Card->cfg_name = CfgName;
  } else {
    Card->cfg_name = DEF_ATI_CFGNAME;
  }

  if (gSettings.VideoPorts != 0) {
    PortsNum = gSettings.VideoPorts;
    DBG (" use N ports setting from config.plist: %d\n", PortsNum);
  }

  if (PortsNum > 0) {
    Card->ports = (UINT8)PortsNum; // use it.
    DBG ("(AtiPorts) Nr of ports set to: %d\n", Card->ports);
  }

  if (Card->ports == 0) {
    Card->ports = 2; //real minimum
    DBG ("Nr of ports set to min: %d\n", Card->ports);
  }

  //
  Name = AllocateZeroPool (24);
  AsciiSPrint (Name, 24, "ATY,%a", Card->cfg_name);
  ATYName.type = kStr;
  ATYName.size = (UINT32)AsciiStrLen (Name);
  ATYName.data = (UINT8 *)Name;

  NameParent = AllocateZeroPool (24);
  AsciiSPrint (NameParent, 24, "ATY,%aParent", Card->cfg_name);
  ATYNameParent.type = kStr;
  ATYNameParent.size = (UINT32)AsciiStrLen (NameParent);
  ATYNameParent.data = (UINT8 *)NameParent;

  //how can we free pool when we leave the procedure? Make all pointers global?
  return TRUE;
}

BOOLEAN
SetupAtiDevprop (
  LOADER_ENTRY  *Entry,
  PCI_DT        *Dev
) {
  CHAR8     Compatible[64], *DevPath;
  UINT32    FakeID = 0;
  INT32     i;
  BOOLEAN   Injected = FALSE;

  if (!InitCard (Dev)) {
    return FALSE;
  }

  CurrentPatches = Entry->KernelAndKextPatches;
  DarwinOSVersion = AllocateCopyPool (AsciiStrSize (Entry->OSVersion), Entry->OSVersion);

  // -------------------------------------------------
  // Find a better way to do this (in device_inject.c)
  if (!gDevPropString) {
    gDevPropString = DevpropCreateString ();
  }

  DevPath = GetPciDevPath (Dev);
  //Card->device = devprop_add_device (gDevPropString, DevPath);
  Card->device = DevpropAddDevicePci (gDevPropString, Dev);

  if (!Card->device) {
    return FALSE;
  }

  // -------------------------------------------------

  if (gSettings.FakeATI) {
    UINTN   Len = ARRAY_SIZE (Compatible);

    Card->flags &= ~FLAGNOTFAKE;
    Card->flags |= FLAGOLD;

    FakeID = gSettings.FakeATI >> 16;
    DevpropAddValue (Card->device, "device-id", (UINT8 *)&FakeID, 4);
    DevpropAddValue (Card->device, "ATY,DeviceID", (UINT8 *)&FakeID, 2);
    AsciiSPrint (Compatible, Len, "pci1002,%04x", FakeID);
    AsciiStrCpyS (Compatible, Len, AsciiStrToLower (Compatible));
    DevpropAddValue (Card->device, "@0,compatible", (UINT8 *)&Compatible[0], 12);
    FakeID = gSettings.FakeATI & 0xFFFF;
    DevpropAddValue (Card->device, "vendor-id", (UINT8 *)&FakeID, 4);
    DevpropAddValue (Card->device, "ATY,VendorID", (UINT8 *)&FakeID, 2);
    MsgLog (" - With FakeID: %a\n", Compatible);
  }

  if (!gSettings.NoDefaultProperties) {
    DevpropAddList (ATIDevPropList);
    if (gSettings.UseIntelHDMI) {
      DevpropAddValue (Card->device, "hda-gfx", (UINT8 *)"onboard-2", 10);
    } else {
      DevpropAddValue (Card->device, "hda-gfx", (UINT8 *)"onboard-1", 10);
    }
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_ATI) {
        continue;
      }

      Injected = TRUE;

      DevpropAddValue (
        Card->device,
        gSettings.AddProperties[i].Key,
        (UINT8 *)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }

    if (Injected) {
      DBG (" - custom properties injected\n");
    }
  }

  DBG (
    "ATI %a %dMB (%a) [%04x:%04x] (subsys [%04x:%04x]) | %a\n",
    S_ATIMODEL,
    (UINT32)RShiftU64 (Card->vram_size, 20), Card->cfg_name,
    Dev->vendor_id, Dev->device_id,
    Dev->subsys_id.subsys.vendor_id, Dev->subsys_id.subsys.device_id,
    DevPath
  );

  //FreePool (Card->info); //TODO we can't free constant so this info should be copy of constants
  FreePool (Card);

  return TRUE;
}
