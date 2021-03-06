/**
 *  Module for work with runtime (RT, NVRAM) vars,
 *  determining default boot volume (Startup disk)
 *  and (kid of) persistent RT support with nvram.plist on CloverEFI.
 *  dmazar, 2012
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_NVRAM
#define DEBUG_NVRAM -1
#endif
#else
#ifdef DEBUG_NVRAM
#undef DEBUG_NVRAM
#endif
#define DEBUG_NVRAM DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_NVRAM, __VA_ARGS__)

EFI_DEVICE_PATH_PROTOCOL    // always contains original efi-boot-device-data
                            *gEfiBootDeviceData,
                            // if gEfiBootDeviceData starts with MemoryMapped node,
                            // then gBootCampHD = "BootCampHD" var, otherwise == NULL
                            *gBootCampHD,
                            // contains only volume dev path from gEfiBootDeviceData or gBootCampHD
                            *gEfiBootVolume;

                            // contains file path from gEfiBootDeviceData or gBootCampHD (if exists)
CHAR16                      *gEfiBootLoaderPath;

                            // contains GPT GUID from gEfiBootDeviceData or gBootCampHD (if exists)
EFI_GUID                    *gEfiBootDeviceGuid;

CONST NVRAM_DATA   gNvramData[] = {
  { kNvSystemID,                L"system-id",               &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvMLB,                     L"MLB",                     &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvHWMLB,                   L"HW_MLB",                  &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvROM,                     L"ROM",                     &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvHWROM,                   L"HW_ROM",                  &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvFirmwareFeatures,        L"FirmwareFeatures",        &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvFirmwareFeaturesMask,    L"FirmwareFeaturesMask",    &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvSBoardID,                L"HW_BID",                  &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  { kNvSystemSerialNumber,      L"SSN",                     &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  //{ kNvBlackMode,               L"BlackMode",               &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },
  //{ kNvUIScale,                 L"UIScale",                 &gEfiAppleNvramGuid, NVRAM_ATTR_RT_BS, 1 },

  { kNvBootArgs,                L"boot-args",               &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 0 },
  { kNvPlatformUUID,            L"platform-uuid",           &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvBacklightLevel,          L"backlight-level",         &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvCsrActiveConfig,         L"csr-active-config",       &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvBootercfg,               L"bootercfg",               &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvBootercfgOnce,           L"bootercfg-once",          &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvBootSwitchVar,           L"boot-switch-vars",        &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvRecoveryBootMode,        L"recovery-boot-mode",      &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },

  { kNvCloverConfig,            L"Clover.Config",           &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvCloverTheme,             L"Clover.Theme",            &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 },
  { kNvCloverNoEarlyProgress,   L"Clover.NoEarlyProgress",  &gEfiAppleBootGuid,  NVRAM_ATTR_RT_BS_NV, 1 }
};

#if 0
//
/  Print all fakesmc variables, i.e. SMC keys
//

VOID
GetSmcKeys () {
  EFI_STATUS    Status;
  CHAR16        *Name;
  EFI_GUID      Guid;
  UINT8         *Data;
  UINTN         Index, NameSize, NewNameSize, DataSize;
  BOOLEAN       Found = FALSE;

  NameSize = sizeof (CHAR16);
  Name     = AllocateZeroPool (NameSize);

  if (Name == NULL) {
    return;
  }

  DBG ("Dump SMC keys from NVRAM:\n");

  while (TRUE) {
    NewNameSize = NameSize;
    Status = gRT->GetNextVariableName (&NewNameSize, Name, &Guid);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      Name = ReallocatePool (NameSize, NewNameSize, Name);
      if (Name == NULL) {
        return; //if something wrong then just do nothing
      }

      Status = gRT->GetNextVariableName (&NewNameSize, Name, &Guid);
      NameSize = NewNameSize;
    }

    if (EFI_ERROR (Status)) {
      break;  //no more variables
    }

    if (StriStr (Name, L"fakesmc") == NULL) {
      continue; //the variable is not interesting for us
    }

    Data = GetNvramVariable (Name, &Guid, NULL, &DataSize);
    if (Data) {
      Found = TRUE;
      DBG (" - %s:", Name);
      for (Index = 0; Index < DataSize; Index++) {
        DBG ("%02x ", *((UINT8 *)Data + Index));
      }

      DBG ("\n");
      FreePool (Data);
    }
  }

  if (!Found) {
    DBG (" - none\n");
  }

  FreePool (Name);
}
#endif

/** Reads and returns value of NVRAM variable. */
VOID *
GetNvramVariable (
  IN  CHAR16      *VariableName,
  IN  EFI_GUID    *VendorGuid,
  OUT UINT32      *Attributes    OPTIONAL,
  OUT UINTN       *DataSize      OPTIONAL
) {
  EFI_STATUS    Status;
  VOID    *Data = NULL;

  // Pass in a zero size buffer to find the required buffer size.
  //
  UINTN   IntDataSize = 0;

  *DataSize = 0;

  Status = gRT->GetVariable (VariableName, VendorGuid, Attributes, &IntDataSize, NULL);
  if (IntDataSize == 0) {
    return NULL;
  }

  if (Status == EFI_BUFFER_TOO_SMALL) {
    //
    // Allocate the buffer to return
    //
    Data = AllocateZeroPool (IntDataSize + 1);
    if (Data != NULL) {
      //
      // Read variable into the allocated buffer.
      //
      Status = gRT->GetVariable (VariableName, VendorGuid, Attributes, &IntDataSize, Data);
      if (EFI_ERROR (Status)) {
        FreePool (Data);
        IntDataSize = 0;
        Data = NULL;
      }
    }
  }

  if (DataSize != NULL) {
    *DataSize = IntDataSize;
  }

  return Data;
}

/** Sets NVRAM variable. Does nothing if variable with the same data and attributes already exists. */
EFI_STATUS
SetNvramVariable (
  IN  CHAR16      *VariableName,
  IN  EFI_GUID    *VendorGuid,
  IN  UINT32      Attributes,
  IN  UINTN       DataSize,
  IN  VOID        *Data
) {
  //EFI_STATUS Status;
  VOID      *OldData;
  UINTN     OldDataSize = 0;
  UINT32    OldAttributes = 0;

  //DBG ("SetNvramVariable (%s, guid, 0x%x, %d):", VariableName, Attributes, DataSize);
  OldData = GetNvramVariable (VariableName, VendorGuid, &OldAttributes, &OldDataSize);
  if (OldData != NULL) {
    // var already exists - check if it equal to new value
    //DBG (" exists (0x%x, %d)", OldAttributes, OldDataSize);
    if (
      (OldAttributes == Attributes) &&
      (OldDataSize == DataSize) &&
      (CompareMem (OldData, Data, DataSize) == 0)
    ) {
      // it's the same - do nothing
      //DBG (", equal -> not writing again.\n");
      FreePool (OldData);
      return EFI_SUCCESS;
    }
    //DBG (", not equal");

    FreePool (OldData);

    // not the same - delete previous one if attributes are different
    if (OldAttributes != Attributes) {
      DeleteNvramVariable (VariableName, VendorGuid);
      //DBG (", diff. attr: deleting old (%r)", Status);
    }
  }

  //DBG (" -> writing new (%r)\n", Status);
  //return Status;

  return gRT->SetVariable (VariableName, VendorGuid, Attributes, DataSize, Data);
}

/** Sets NVRAM variable. Does nothing if variable with the same name already exists. */
EFI_STATUS
AddNvramVariable (
  IN  CHAR16      *VariableName,
  IN  EFI_GUID    *VendorGuid,
  IN  UINT32      Attributes,
  IN  UINTN       DataSize,
  IN  VOID        *Data
) {
  VOID    *OldData;

  //DBG ("SetNvramVariable (%s, guid, 0x%x, %d):", VariableName, Attributes, DataSize);
  OldData = GetNvramVariable (VariableName, VendorGuid, NULL, NULL);
  if (OldData == NULL) {
    // set new value
    //DBG (" -> writing new (%r)\n", Status);
    return gRT->SetVariable (VariableName, VendorGuid, Attributes, DataSize, Data);
  }

  FreePool (OldData);
  return EFI_ABORTED;
}

/** Sets NVRAM variable. Or delete if State == FALSE. */
EFI_STATUS
SetOrDeleteNvramVariable (
  IN  CHAR16      *VariableName,
  IN  EFI_GUID    *VendorGuid,
  IN  UINT32      Attributes,
  IN  UINTN       DataSize,
  IN  VOID        *Data,
  IN  BOOLEAN     State
) {
  if (!State) {
    DeleteNvramVariable (VariableName, VendorGuid);
    return EFI_ABORTED;
  }

  return SetNvramVariable (VariableName, VendorGuid, Attributes, DataSize, Data);
}

/** Deletes NVRAM variable. */
EFI_STATUS
DeleteNvramVariable (
  IN  CHAR16      *VariableName,
  IN  EFI_GUID    *VendorGuid
) {
  EFI_STATUS    Status;

  // Delete: attributes and data size = 0
  Status = gRT->SetVariable (VariableName, VendorGuid, 0, 0, NULL);
  //DBG ("DeleteNvramVariable (%s, guid = %r\n):", VariableName, Status);

  return Status;
}

EFI_STATUS
ResetNvram () {
  EFI_STATUS    Status;
  UINTN         Index, NvramDataCount = ARRAY_SIZE (gNvramData);

  for (Index = 0; Index < NvramDataCount; Index++) {
    if (gNvramData[Index].Reset) {
      Status = DeleteNvramVariable (gNvramData[Index].VariableName, gNvramData[Index].Guid);
    }
  }

  return Status;
}

/** Searches for GPT HDD dev path node and return pointer to partition GUID or NULL. */
EFI_GUID *
FindGPTPartitionGuidInDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
) {
  HARDDRIVE_DEVICE_PATH   *HDDDevPath;
  EFI_GUID                *Guid = NULL;

  if (DevicePath == NULL) {
    return NULL;
  }

  while (
    !IsDevicePathEndType (DevicePath) &&
    !((DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) && (DevicePathSubType (DevicePath) == MEDIA_HARDDRIVE_DP))
  ) {
    DevicePath = NextDevicePathNode (DevicePath);
  }

  if (
    (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) &&
    (DevicePathSubType (DevicePath) == MEDIA_HARDDRIVE_DP)
  ) {
    HDDDevPath = (HARDDRIVE_DEVICE_PATH *)DevicePath;

    if (HDDDevPath->SignatureType == SIGNATURE_TYPE_GUID) {
      Guid = (EFI_GUID *)HDDDevPath->Signature;
    }
  }

  return Guid;
}

/** Returns TRUE if dev paths are equal. Ignores some differences. */
BOOLEAN
BootVolumeDevicePathEqual (
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath1,
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath2
) {
  BOOLEAN             Equal, ForceEqualNodes;
  UINT8               Type1, SubType1, Type2, SubType2;
  UINTN               Len1, Len2;
  SATA_DEVICE_PATH    *SataNode1, *SataNode2;

  //DBG ("   BootVolumeDevicePathEqual:\n    %s\n    %s\n", FileDevicePathToStr (DevicePath1), FileDevicePathToStr (DevicePath2));
  //DBG ("    N1: (Type, Subtype, Len) N2: (Type, Subtype, Len)\n");

  Equal = FALSE;
  while (TRUE) {
    Type1    = DevicePathType (DevicePath1);
    SubType1 = DevicePathSubType (DevicePath1);
    Len1     = DevicePathNodeLength (DevicePath1);

    Type2    = DevicePathType (DevicePath2);
    SubType2 = DevicePathSubType (DevicePath2);
    Len2     = DevicePathNodeLength (DevicePath2);

    ForceEqualNodes = FALSE;

    //DBG ("    N1: (%d, %d, %d)", Type1, SubType1, Len1);
    //DBG ("    N2: (%d, %d, %d)", Type2, SubType2, Len2);

    /*
     DBG ("%s\n", DevicePathToStr (DevicePath1));
     DBG ("%s\n", DevicePathToStr (DevicePath2));
     */

    //
    // Some eSata device can have path:
    //  PciRoot (0x0)/Pci (0x1C,0x5)/Pci (0x0,0x0)/VenHw (CF31FAC5-C24E-11D2-85F3-00A0C93EC93B,80)
    // while OSX can set it as
    //  PciRoot (0x0)/Pci (0x1C,0x5)/Pci (0x0,0x0)/Sata (0x0,0x0,0x0)
    // we'll assume VenHw and Sata nodes to be equal to cover that
    //
    if ((Type1 == MESSAGING_DEVICE_PATH) && (SubType1 == MSG_SATA_DP)) {
      if (
        ((Type2 == HARDWARE_DEVICE_PATH) && (SubType2 == HW_VENDOR_DP)) ||
        ((Type2 == MESSAGING_DEVICE_PATH) && (SubType2 == MSG_VENDOR_DP))
      ) { //no it is UART?
        ForceEqualNodes = TRUE;
      }
    } else if (
      (Type2 == MESSAGING_DEVICE_PATH) &&
      (SubType2 == MSG_SATA_DP) &&
      (
        ((Type1 == HARDWARE_DEVICE_PATH) && (SubType1 == HW_VENDOR_DP)) ||
        ((Type1 == MESSAGING_DEVICE_PATH) && (SubType1 == MSG_VENDOR_DP))
      )
    ) {
      ForceEqualNodes = TRUE;
    }

    //
    // UEFI can see it as PcieRoot, while OSX could generate PciRoot
    // we'll assume Acpi dev path nodes to be equal to cover that
    //
    if ((Type1 == ACPI_DEVICE_PATH) && (Type2 == ACPI_DEVICE_PATH)) {
      ForceEqualNodes = TRUE;
    }

    if (ForceEqualNodes) {
      // assume equal nodes
      //DBG (" - forcing equal nodes\n");
      DevicePath1 = NextDevicePathNode (DevicePath1);
      DevicePath2 = NextDevicePathNode (DevicePath2);
      continue;
    }

    if ((Type1 != Type2) || (SubType1 != SubType2) || (Len1 != Len2)) {
      // Not equal
      //DBG (" - not equal\n");
      break;
    }

    //
    // Same type/subtype/len ...
    //
    if (IsDevicePathEnd (DevicePath1)) {
      // END node - they are the same
      Equal = TRUE;
      //DBG (" - END = equal\n");
      break;
    }

    //
    // Do mem compare of nodes or special compare for selected types/subtypes
    //
    if ((Type1 == MESSAGING_DEVICE_PATH) && (SubType1 == MSG_SATA_DP)) {
      //
      // Ignore
      //
      SataNode1 = (SATA_DEVICE_PATH *)DevicePath1;
      SataNode2 = (SATA_DEVICE_PATH *)DevicePath2;

      if (SataNode1->HBAPortNumber != SataNode2->HBAPortNumber) {
        // not equal
        //DBG (" - not equal SataNode.HBAPortNumber\n");
        break;
      }

      if (SataNode1->Lun != SataNode2->Lun) {
        // not equal
        //DBG (" - not equal SataNode.Lun\n");
        break;
      }

      //DBG (" - forcing equal nodes");
    } else if (CompareMem (DevicePath1, DevicePath2, DevicePathNodeLength (DevicePath1)) != 0) {
      // Not equal
      //DBG (" - not equal\n");
      break;
    }

    //DBG ("\n");

    //
    // Advance to next node
    //
    DevicePath1 = NextDevicePathNode (DevicePath1);
    DevicePath2 = NextDevicePathNode (DevicePath2);
  }

  return Equal;
}

/** Finds and returns pointer to specified DevPath node in DevicePath or NULL.
 *  If SubType == 0 then it is ignored.
 */

EFI_DEVICE_PATH_PROTOCOL *
FindDevicePathNodeWithType (
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
  IN  UINT8                       Type,
  IN  UINT8                       SubType OPTIONAL
) {
  while (!IsDevicePathEnd (DevicePath)) {
    if (
      (DevicePathType (DevicePath) == Type) &&
      ((SubType == 0) || (DevicePathSubType (DevicePath) == SubType))
    ) {
      return DevicePath;
    }

    DevicePath = NextDevicePathNode (DevicePath);
  }

  //
  // Not found
  //
  return NULL;
}

/** Returns TRUE if dev paths contain the same MEDIA_DEVICE_PATH. */
BOOLEAN
BootVolumeMediaDevicePathNodesEqual (
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath1,
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath2
) {
    DevicePath1 = FindDevicePathNodeWithType (DevicePath1, MEDIA_DEVICE_PATH, 0);
    if (DevicePath1 == NULL) {
      return FALSE;
    }

    DevicePath2 = FindDevicePathNodeWithType (DevicePath2, MEDIA_DEVICE_PATH, 0);
    if (DevicePath2 == NULL) {
      return FALSE;
    }

    return (
      (DevicePathNodeLength (DevicePath1) == DevicePathNodeLength (DevicePath1)) &&
      (CompareMem (DevicePath1, DevicePath2, DevicePathNodeLength (DevicePath1)) == 0)
    );
}

/** Reads gEfiAppleBootGuid:efi-boot-device-data and BootCampHD NVRAM variables and parses them
 *  into gEfiBootVolume, gEfiBootLoaderPath and gEfiBootDeviceGuid.
 *  Vars after this call:
 *   gEfiBootDeviceData - original efi-boot-device-data
 *   gBootCampHD - if gEfiBootDeviceData starts with MemoryMapped node, then BootCampHD variable (device path), NULL otherwise
 *   gEfiBootVolume - volume device path (from efi-boot-device-data or BootCampHD)
 *   gEfiBootLoaderPath - file path (from efi-boot-device-data or BootCampHD) or NULL
 *   gEfiBootDeviceGuid - GPT volume GUID if gEfiBootVolume or NULL
 */
EFI_STATUS
GetEfiBootDeviceFromNvram () {
  UINTN                   Size = 0;
  EFI_GUID                *Guid;
  FILEPATH_DEVICE_PATH    *FileDevPath;

  DbgHeader ("GetEfiBootDeviceFromNvram");
  //DBG ("GetEfiBootDeviceFromNvram:");

  if (gEfiBootDeviceData != NULL) {
    //DBG (" - [!] already parsed\n");
    return EFI_SUCCESS;
  }

  gEfiBootDeviceData = GetNvramVariable (L"efi-boot-next-data", &gEfiAppleBootGuid, NULL, &Size);

  if (gEfiBootDeviceData != NULL) {
    //DBG ("Got efi-boot-next-data size=%d\n", Size);
    if (IsDevicePathValid (gEfiBootDeviceData, Size)) {
      //DBG (" - efi-boot-next-data: %s\n", FileDevicePathToStr (gEfiBootDeviceData));
    } else {
      //DBG (" - device path for efi-boot-next-data is invalid\n");
      FreePool (gEfiBootDeviceData);
      gEfiBootDeviceData = NULL;
    }
  }

  if (gEfiBootDeviceData == NULL) {
    gEfiBootDeviceData = GetNvramVariable (L"efi-boot-device-data", &gEfiAppleBootGuid, NULL, &Size);
    if (gEfiBootDeviceData != NULL) {
      //DBG ("Got efi-boot-device-data size=%d\n", Size);
      if (!IsDevicePathValid (gEfiBootDeviceData, Size)) {
        //DBG (" - device path for efi-boot-device-data is invalid\n");
        FreePool (gEfiBootDeviceData);
        gEfiBootDeviceData = NULL;
      }
    }
  }

  if (gEfiBootDeviceData == NULL) {
    //DBG (" - [!] efi-boot-device-data not found\n");
    return EFI_NOT_FOUND;
  }

  //DBG ("\n");
  DBG (" - efi-boot-device-data: %s\n", FileDevicePathToStr (gEfiBootDeviceData));

  gEfiBootVolume = gEfiBootDeviceData;

  //
  // if gEfiBootDeviceData starts with MemoryMapped node,
  // then Startup Disk sets BootCampHD to Win disk dev path.
  //
  if (
    (DevicePathType (gEfiBootDeviceData) == HARDWARE_DEVICE_PATH) &&
    (DevicePathSubType (gEfiBootDeviceData) == HW_MEMMAP_DP)
  ) {
    gBootCampHD = GetNvramVariable (L"BootCampHD", &gEfiAppleBootGuid, NULL, &Size);
    gEfiBootVolume = gBootCampHD;

    if (gBootCampHD == NULL) {
      //DBG ("  - [!] Error: BootCampHD not found\n");
      return EFI_NOT_FOUND;
    }

    if (!IsDevicePathValid (gBootCampHD, Size)) {
      //DBG (" Error: BootCampHD device path is invalid\n");
      FreePool (gBootCampHD);
      gEfiBootVolume = gBootCampHD = NULL;
      return EFI_NOT_FOUND;
    }

    DBG ("  - BootCampHD: %s\n", FileDevicePathToStr (gBootCampHD));
  }

  //
  // if gEfiBootVolume contains FilePathNode, then split them into gEfiBootVolume dev path and gEfiBootLoaderPath
  //
  gEfiBootLoaderPath = NULL;

  FileDevPath = (FILEPATH_DEVICE_PATH *)FindDevicePathNodeWithType (gEfiBootVolume, MEDIA_DEVICE_PATH, MEDIA_FILEPATH_DP);
  if (FileDevPath != NULL) {
    gEfiBootLoaderPath = AllocateCopyPool (StrSize (FileDevPath->PathName), FileDevPath->PathName);
    // copy DevPath and write end of path node after in place of file path node
    gEfiBootVolume = DuplicateDevicePath (gEfiBootVolume);
    FileDevPath = (FILEPATH_DEVICE_PATH *)FindDevicePathNodeWithType (gEfiBootVolume, MEDIA_DEVICE_PATH, MEDIA_FILEPATH_DP);
    SetDevicePathEndNode (FileDevPath);
    // gEfiBootVolume now contains only Volume path
  }

  DBG ("  - Volume: '%s'\n", FileDevicePathToStr (gEfiBootVolume));
  DBG ("  - LoaderPath: '%s'\n", gEfiBootLoaderPath);

  //
  // if this is GPT disk, extract GUID
  // gEfiBootDeviceGuid can be used as a flag for GPT disk then
  //
  Guid = FindGPTPartitionGuidInDevicePath (gEfiBootVolume);
  if (Guid != NULL) {
    gEfiBootDeviceGuid = AllocatePool (sizeof (EFI_GUID));

    if (gEfiBootDeviceGuid != NULL) {
      CopyMem (gEfiBootDeviceGuid, Guid, sizeof (EFI_GUID));
      DBG ("  - Guid = %g\n", gEfiBootDeviceGuid);
    }
  }

  return EFI_SUCCESS;
}

/** Performs detailed search for Startup Disk or last Clover boot volume
 *  by looking for gEfiAppleBootGuid:efi-boot-device-data and BootCampHD RT vars.
 *  Returns MainMenu entry index or -1 if not found.
 */
INTN
FindStartupDiskVolume (
  REFIT_MENU_SCREEN   *MenuScreen
) {
  UINTN             Index;
  LOADER_ENTRY      *LoaderEntry;
  REFIT_VOLUME      *Volume, *DiskVolume;
  BOOLEAN           IsPartitionVolume;
  CHAR16            *LoaderPath, *EfiBootVolumeStr;

  //DBG ("FindStartupDiskVolume ...\n");

  //
  // search RT vars for efi-boot-device-data
  // and try to find that volume
  //
  GetEfiBootDeviceFromNvram ();

  if (gEfiBootVolume == NULL) {
    //DBG (" - [!] EfiBootVolume not found\n");
    return -1;
  }

  DbgHeader ("FindStartupDiskVolume");
  //DBG ("FindStartupDiskVolume searching ...\n");

  //
  // Check if gEfiBootVolume is disk or partition volume
  //
  EfiBootVolumeStr  = FileDevicePathToStr (gEfiBootVolume);
  IsPartitionVolume = NULL != FindDevicePathNodeWithType (gEfiBootVolume, MEDIA_DEVICE_PATH, 0);
  DBG ("  - Volume: %s = %s\n", IsPartitionVolume ? L"partition" : L"disk", EfiBootVolumeStr);

  //
  // 1. gEfiBootVolume + gEfiBootLoaderPath
  // PciRoot (0x0)/.../Sata (...)/HD (...)/\EFI\BOOT\XXX.EFI - set by Clover
  //
  if (gEfiBootLoaderPath != NULL) {
    DBG ("   - searching for that partition and loader '%s'\n", gEfiBootLoaderPath);
    for (Index = 0; ((Index < MenuScreen->EntryCount) && (MenuScreen->Entries[Index]->Row == 0)); ++Index) {
      if (MenuScreen->Entries[Index]->Tag == TAG_LOADER) {
        LoaderEntry = (LOADER_ENTRY *)MenuScreen->Entries[Index];
        Volume = LoaderEntry->Volume;
        LoaderPath = LoaderEntry->LoaderPath;

        if ((Volume != NULL) && BootVolumeDevicePathEqual (gEfiBootVolume, Volume->DevicePath)) {
          //DBG ("  checking '%s'\n", DevicePathToStr (Volume->DevicePath));
          //DBG ("   '%s'\n", LoaderPath);
          // case insensitive cmp
          if ((LoaderPath != NULL) && (StriCmp (gEfiBootLoaderPath, LoaderPath) == 0)) {
            // that's the one
            DBG ("    - found entry %d. '%s', Volume '%s', '%s'\n", Index, LoaderEntry->me.Title, Volume->VolName, LoaderPath);
            return Index;
          }
        }
      }
    }

    DBG ("    - [!] not found\n");

    //
    // search again, but compare only Media dev path nodes
    // (in case of some dev path differences we do not cover)
    //
    DBG ("   - searching again, but comparing Media dev path nodes\n");

    for (Index = 0; ((Index < MenuScreen->EntryCount) && (MenuScreen->Entries[Index]->Row == 0)); ++Index) {
      if (MenuScreen->Entries[Index]->Tag == TAG_LOADER) {
        LoaderEntry = (LOADER_ENTRY *)MenuScreen->Entries[Index];
        Volume = LoaderEntry->Volume;
        LoaderPath = LoaderEntry->LoaderPath;

        if ((Volume != NULL) && BootVolumeMediaDevicePathNodesEqual (gEfiBootVolume, Volume->DevicePath)) {
          //DBG ("  checking '%s'\n", DevicePathToStr (Volume->DevicePath));
          //DBG ("   '%s'\n", LoaderPath);
          // case insensitive cmp
          if ((LoaderPath != NULL) && (StriCmp (gEfiBootLoaderPath, LoaderPath) == 0)) {
            // that's the one
            DBG ("   - found entry %d. '%s', Volume '%s', '%s'\n", Index, LoaderEntry->me.Title, Volume->VolName, LoaderPath);
            return Index;
          }
        }
      }
    }

    DBG ("    - [!] not found\n");
  }

  //
  // 2. gEfiBootVolume - partition volume
  // PciRoot (0x0)/.../Sata (...)/HD (...) - set by Clover or OSX
  //
  if (IsPartitionVolume) {
    DBG ("   - searching for that partition\n");
    for (Index = 0; ((Index < MenuScreen->EntryCount) && (MenuScreen->Entries[Index]->Row == 0)); ++Index) {
      Volume = NULL;

      if (MenuScreen->Entries[Index]->Tag == TAG_LOADER) {
        LoaderEntry = (LOADER_ENTRY *)MenuScreen->Entries[Index];
        Volume = LoaderEntry->Volume;
      }

      if ((Volume != NULL) && BootVolumeDevicePathEqual (gEfiBootVolume, Volume->DevicePath)) {
        DBG ("    - found entry %d. '%s', Volume '%s'\n", Index, MenuScreen->Entries[Index]->Title, Volume->VolName);
        return Index;
      }
    }

    DBG ("    - [!] not found\n");

    //
    // search again, but compare only Media dev path nodes
    //
    DBG ("   - searching again, but comparing Media dev path nodes\n");

    for (Index = 0; ((Index < MenuScreen->EntryCount) && (MenuScreen->Entries[Index]->Row == 0)); ++Index) {
      Volume = NULL;

      if (MenuScreen->Entries[Index]->Tag == TAG_LOADER) {
        LoaderEntry = (LOADER_ENTRY *)MenuScreen->Entries[Index];
        Volume = LoaderEntry->Volume;
      }
      if ((Volume != NULL) && BootVolumeMediaDevicePathNodesEqual (gEfiBootVolume, Volume->DevicePath)) {
        DBG ("    - found entry %d. '%s', Volume '%s'\n", Index, MenuScreen->Entries[Index]->Title, Volume->VolName);
        return Index;
      }
    }

    DBG ("    - [!] not found\n");
    return -1;
  }

  //
  // 3. gEfiBootVolume - disk volume
  // PciRoot (0x0)/.../Sata (...) - set by OSX for Win boot
  //
  // 3.1 First find disk volume in Volumes[]
  //
  DiskVolume = NULL;
  DBG ("   - searching for that disk\n");

  for (Index = 0; Index < gVolumesCount; ++Index) {
    Volume = gVolumes[Index];

    if (BootVolumeDevicePathEqual (gEfiBootVolume, Volume->DevicePath)) {
      // that's the one
      DiskVolume = Volume;
      DBG ("    - found disk as volume %d. '%s'\n", Index, Volume->VolName);
      break;
    }
  }

  if (DiskVolume == NULL) {
    DBG ("    - [!] not found\n");
    return -1;
  }

  //
  // 3.2 DiskVolume
  // search for first entry with win loader or win partition on that disk
  //
  DBG ("   - searching for first entry with win loader or win partition on that disk\n");
  for (Index = 0; ((Index < MenuScreen->EntryCount) && (MenuScreen->Entries[Index]->Row == 0)); ++Index) {
    if (MenuScreen->Entries[Index]->Tag == TAG_LOADER) {
      LoaderEntry = (LOADER_ENTRY *)MenuScreen->Entries[Index];
      Volume = LoaderEntry->Volume;

      if ((Volume != NULL) && (Volume->WholeDiskBlockIO == DiskVolume->BlockIO)) {
        // check for Win
        //DBG ("  checking loader entry %d. %s\n", Index, LoaderEntry->me.Title);
        //DBG ("   %s\n", DevicePathToStr (Volume->DevicePath));
        //DBG ("   LoaderPath = %s\n", LoaderEntry->LoaderPath);
        //DBG ("   LoaderType = %d\n", LoaderEntry->LoaderType);
        if (LoaderEntry->LoaderType == OSTYPE_WINEFI) {
          // that's the one - win loader entry
          DBG ("    - found loader entry %d. '%s', Volume '%s', '%s'\n", Index, LoaderEntry->me.Title, Volume->VolName, LoaderEntry->LoaderPath);
          return Index;
        }
      }
    }
  }

  DBG ("    - [!] not found\n");

  //
  // 3.3 DiskVolume, but no Win entry
  // PciRoot (0x0)/.../Sata (...)
  // just find first menu entry on that disk?
  //

  DBG ("   - searching for any entry from disk '%s'\n", DiskVolume->VolName);

  for (Index = 0; ((Index < MenuScreen->EntryCount) && (MenuScreen->Entries[Index]->Row == 0)); ++Index) {
    if (MenuScreen->Entries[Index]->Tag == TAG_LOADER) {
      LoaderEntry = (LOADER_ENTRY *)MenuScreen->Entries[Index];
      Volume = LoaderEntry->Volume;

      if ((Volume != NULL) && (Volume->WholeDiskBlockIO == DiskVolume->BlockIO)) {
        // that's the one
        DBG ("    - found loader entry %d. '%s', Volume '%s', '%s'\n", Index, LoaderEntry->me.Title, Volume->VolName, LoaderEntry->LoaderPath);

        return Index;
      }
    }
  }

  DBG ("    - [!] not found\n");

  return -1;
}

/** Sets efi-boot-device-data RT var to currently selected Volume and LoadePath. */
EFI_STATUS
SetStartupDiskVolume (
  IN  REFIT_VOLUME    *Volume,
  IN  CHAR16          *LoaderPath
) {
  EFI_STATUS                  Status;
  EFI_DEVICE_PATH_PROTOCOL    *DevPath, *FileDevPath;
  EFI_GUID                    *Guid;
  CHAR8                       *EfiBootDevice, *EfiBootDeviceTpl;
  UINTN                       Size;
  UINT32                      Attributes;

  DBG ("SetStartupDiskVolume:\n");
  DBG ("  * Volume: '%s'\n",     Volume->VolName);
  DBG ("  * LoaderPath: '%s'\n", LoaderPath);

  //
  // construct dev path for Volume/LoaderPath
  //
  DevPath = Volume->DevicePath;
  if (LoaderPath != NULL) {
    FileDevPath = FileDevicePath (NULL, LoaderPath);
    DevPath = AppendDevicePathNode (DevPath, FileDevPath);
  }

  DBG ("  * DevPath: %s\n", Volume->VolName, FileDevicePathToStr (DevPath));

  Guid = FindGPTPartitionGuidInDevicePath (Volume->DevicePath);
  DBG ("  * GUID = %g\n", Guid);

  Attributes = NVRAM_ATTR_RT_BS_NV;

  //
  // set efi-boot-device-data to volume dev path
  //
  Status = SetNvramVariable (L"efi-boot-device-data", &gEfiAppleBootGuid, Attributes, GetDevicePathSize (DevPath), DevPath);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // set efi-boot-device to XML string
  // (probably not needed at all)
  //
  if (Guid != NULL) {
    EfiBootDeviceTpl =
      "<array><dict>"
      "<key>IOMatch</key>"
      "<dict>"
      "<key>IOProviderClass</key><string>IOMedia</string>"
      "<key>IOPropertyMatch</key>"
      "<dict><key>UUID</key><string>%g</string></dict>"
      "</dict>"
      "</dict></array>";

    Size = AsciiStrLen (EfiBootDeviceTpl) + 36;
    EfiBootDevice = AllocateZeroPool (AsciiStrLen (EfiBootDeviceTpl) + 36);
    AsciiSPrint (EfiBootDevice, Size, EfiBootDeviceTpl, Guid);
    Size = AsciiStrLen (EfiBootDevice);
    DBG ("  * efi-boot-device: %a\n", EfiBootDevice);

    Status = SetNvramVariable (L"efi-boot-device", &gEfiAppleBootGuid, Attributes, Size, EfiBootDevice);

    FreePool (EfiBootDevice);
  }

  return Status;
}

/** Deletes Startup disk vars: efi-boot-device, efi-boot-device-data, BootCampHD. */
VOID
RemoveStartupDiskVolume () {
  //EFI_STATUS Status;

  //DBG ("RemoveStartupDiskVolume:\n");

  /*Status =*/ DeleteNvramVariable (L"efi-boot-device", &gEfiAppleBootGuid);
  //DBG ("  * efi-boot-device = %r\n", Status);

  /*Status =*/ DeleteNvramVariable (L"efi-boot-device-data", &gEfiAppleBootGuid);
  //DBG ("  * efi-boot-device-data = %r\n", Status);

  /*Status =*/ DeleteNvramVariable (L"BootCampHD", &gEfiAppleBootGuid);
  //DBG ("  * BootCampHD = %r\n", Status);
  //DBG ("Removed efi-boot-device-data variable: %r\n", Status);
}

VOID
SyncBootArgsFromNvram () {
  CHAR8   *TmpString, *Arg = NULL, *tBootArgs;
  UINTN   iNVRAM = 0, iBootArgs = 0, Index = 0, Index2, Len, i;

  DbgHeader ("SyncBootArgsFromNvram");

  tBootArgs = gSettings.BootArgs;
  AsciiTrimSpaces (&tBootArgs);

  iBootArgs = AsciiStrLen (gSettings.BootArgs);

  if (iBootArgs >= AVALUE_MAX_SIZE) {
    return;
  }

  TmpString = GetNvramVariable (L"boot-args", &gEfiAppleBootGuid, NULL, &iNVRAM);

  AsciiTrimSpaces (&TmpString);

  if (!iNVRAM || !TmpString || ((iNVRAM = AsciiStrLen (TmpString)) == 0)) {
    return;
  }

  DBG ("Setting BootArgs: %a\n", gSettings.BootArgs);
  DBG ("Found boot-args in NVRAM: %a, size=%d\n", TmpString, iNVRAM);

  // Save system boot-args to be used later by boot.efi.
  if (AsciiStrCmp (gSettings.BootArgs, TmpString) != 0) {
    gSettings.BootArgsNVRAM = AllocateCopyPool (AsciiStrSize (TmpString), TmpString);
  }

  CONSTRAIN_MAX (iNVRAM, AVALUE_MAX_SIZE - 1 - iBootArgs);

  Arg = AllocateZeroPool (iNVRAM + 1);

  while ((Index < iNVRAM) && (TmpString[Index] != 0x0)) {
    Index2 = 0;

    if (TmpString[Index] != 0x22) {
      //DBG ("search space Index=%d\n", Index);
      while ((Index < iNVRAM) && (TmpString[Index] != 0x20) && (TmpString[Index] != 0x0)) {
        Arg[Index2++] = TmpString[Index++];
      }

      DBG ("...found arg: %a\n", Arg);
    } else {
      Index++;
      //DBG ("search quote Index=%d\n", Index);
      while ((Index < iNVRAM) && (TmpString[Index] != 0x22) && (TmpString[Index] != 0x0)) {
        Arg[Index2++] = TmpString[Index++];
      }

      if (TmpString[Index] == 0x22) {
        Index++;
      }

      DBG ("...found quoted arg:\n", Arg);
    }

    while (TmpString[Index] == 0x20) {
      Index++;
    }

    // For the moment only arg -s must be ignored
    //if (AsciiStrCmp (Arg, "-s") == 0) {
    //  DBG ("...ignoring arg:%a\n", Arg);
    //  continue;
    //}

    if (AsciiStrStr (gSettings.BootArgs, Arg) == NULL) {
      Len = AsciiStrLen (gSettings.BootArgs);
      CONSTRAIN_MAX (Len, AVALUE_MAX_SIZE - 1);

      if ((Len + Index2) >= AVALUE_MAX_SIZE) {
        DBG ("boot-args overflow... bytes=%d+%d\n", Len, Index2);
        break;
      }

      gSettings.BootArgs[Len++] = 0x20;

      for (i = 0; ((i < Index2) && (Arg[i] != 0x0)); i++) {
        gSettings.BootArgs[Len++] = Arg[i];
      }

      DBG (" - added\n");
    } else {
      DBG (" - skip\n");
    }
  }

  DBG ("Final BootArgs: %a\n", gSettings.BootArgs);

  FreePool (TmpString);
  FreePool (Arg);
}
