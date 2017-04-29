/*
 *  Copyright 2009 Jasmin Fazlic All rights reserved.
 */
/*
 *  Cleaned and merged by iNDi
 */

#ifndef __LIBSAIO_DEVICE_INJECT_H
#define __LIBSAIO_DEVICE_INJECT_H

/* No more used
#define DP_ADD_TEMP_VAL(dev, val) devprop_add_value (dev, (CHAR8 *)val[0], (UINT8 *)val[1], (UINT32)AsciiStrLen (val[1]))
#define DP_ADD_TEMP_VAL_DATA(dev, val) devprop_add_value (dev, (CHAR8 *)val.name, (UINT8 *)val.data, val.size)
 */
#define MAX_PCI_DEV_PATHS   16

typedef struct PCI_DT {
  EFI_HANDLE        DeviceHandle;
  UINT8             *regs;

  union {
    struct {
      UINT32          :2;
      UINT32      reg :6;
      UINT32      func:3;
      UINT32      dev :5;
      UINT32      bus :8;
      UINT32          :7;
      UINT32      eb  :1;
    } bits;
    UINT32        addr;
  } dev;

  UINT16          vendor_id;
  UINT16          device_id;

  union {
    struct {
      UINT16      vendor_id;
      UINT16      device_id;
    } subsys;
    UINT32        subsys_id;
  } subsys_id;

  UINT8           revision;
  UINT8           subclass;
  UINT16          class_id;
  UINT8           class_code[3];
  EFI_HANDLE      *handle;

  struct PCI_DT   *parent;
  struct PCI_DT   *children;
  struct PCI_DT   *next;
} PCI_DT;

/* Option ROM header */
typedef struct {
  UINT16    signature;          // 0xAA55
  UINT8     rom_size;           //in 512 bytes blocks
  UINT8     jump;               //0xE9 for ATI and Intel, 0xEB for NVidia
  UINT8     entry_point[4];     //offset to
  UINT8     reserved[16];
  UINT16    pci_header_offset;  //@0x18
  UINT16    expansion_header_offset;
} OPTION_ROM_HEADER;

/* Option ROM PCI Data Structure */
typedef struct {
  UINT32    signature;    // 0x52494350 'PCIR'
  UINT16    vendor_id;
  UINT16    device_id;
  UINT16    vital_product_data_offset;
  UINT16    structure_length;
  UINT8     structure_revision;
  UINT8     class_code[3];
  UINT16    image_length;  //same as rom_size for NVidia and ATI, 0x80 for Intel
  UINT16    image_revision;
  UINT8     code_type;
  UINT8     indicator;
  UINT16    reserved;
} OPTION_ROM_PCI_HEADER;

typedef struct ACPIDevPath {
  UINT8   type;     // = 2 ACPI device-path
  UINT8   subtype;  // = 1 ACPI Device-path
  UINT16  length;   // = 0x0c
  UINT32  _HID;     // = 0xD041030A ?
  UINT32  _UID;     // = 0x00000000 PCI ROOT
} ACPIDevPath;

typedef struct PCIDevPath {
  UINT8   type;     // = 1 Hardware device-path
  UINT8   subtype;  // = 1 PCI
  UINT16  length;   // = 6
  UINT8   function; // pci func number
  UINT8   device;   // pci dev number
} PCIDevPath;

typedef struct DevicePathEnd {
  UINT8   type;     // = 0x7f
  UINT8   subtype;  // = 0xff
  UINT16  length;   // = 4;
} DevicePathEnd;

typedef struct DevPropDevice {
          UINT32          length;
          UINT16          numentries;
          UINT16          WHAT2;                            // 0x0000 ?
          ACPIDevPath     acpi_dev_path;                    // = 0x02010c00 0xd041030a
          PCIDevPath      pci_dev_path[MAX_PCI_DEV_PATHS];  // = 0x01010600 func dev
          DevicePathEnd   path_end;                         // = 0x7fff0400
          UINT8           *data;
          UINT8           num_pci_devpaths;
  struct  DevPropString   *string;
} DevPropDevice;

typedef struct DevPropString {
  UINT32          length;
  UINT32          WHAT2;     // 0x01000000 ?
  UINT16          numentries;
  UINT16          WHAT3;     // 0x0000     ?
  DevPropDevice   **entries;
} DevPropString;

CHAR8 *
GetPciDevPath (
  PCI_DT    *PciDt
);

UINT32
PciConfigRead32 (
  PCI_DT    *PciDt,
  UINT8     Reg
);

DevPropString *
DevpropCreateString ();

DevPropDevice *
DevpropAddDevicePci (
  DevPropString   *StringBuf,
  PCI_DT          *PciDt
);

BOOLEAN
DevpropAddValue (
  DevPropDevice   *Device,
  CHAR8           *Nm,
  UINT8           *Vl,
  UINTN           Len
);

CHAR8 *
DevpropGenerateString (
  DevPropString     *StringBuf
);

VOID
DevpropFreeString (
  DevPropString   *StringBuf
);

//extern PCI_DT       *nvdevice;
extern DevPropString  *gDevPropString;
//extern UINT8          *stringdata;
extern UINT32         gDevPropStringLength;

#endif /* !__LIBSAIO_DEVICE_INJECT_H */
