/*
 * refit/lib.c
 * General library functions
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_LIB
#define DEBUG_LIB -1
#endif
#else
#ifdef DEBUG_LIB
#undef DEBUG_LIB
#endif
#define DEBUG_LIB DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_LIB, __VA_ARGS__)

// variables

#define MAX_FILE_SIZE   (1024 * 1024 * 1024)

EFI_HANDLE              SelfImageHandle;
EFI_HANDLE              SelfDeviceHandle;
EFI_LOADED_IMAGE        *SelfLoadedImage;
EFI_FILE                *SelfRootDir;
EFI_FILE                *SelfDir;
CHAR16                  *SelfDirPath;
EFI_DEVICE_PATH         *SelfDevicePath;
EFI_DEVICE_PATH         *SelfFullDevicePath;
EFI_FILE                *ThemeDir = NULL;
CHAR16                  *ThemePath;
BOOLEAN                 gThemeChanged = FALSE;
BOOLEAN                 gBootChanged = FALSE;
BOOLEAN                 gThemeOptionsChanged = FALSE;

EFI_FILE                *OEMDir;
CHAR16                  *OEMPath = DIR_CLOVER;
//EFI_FILE                *OemThemeDir = NULL;

REFIT_VOLUME            *SelfVolume = NULL;
REFIT_VOLUME            **Volumes = NULL;
UINTN                   VolumesCount = 0;
//
// Unicode collation protocol interface
//
EFI_UNICODE_COLLATION_PROTOCOL    *mUnicodeCollation = NULL;

#define                 MAX_ELEMENT_COUNT 8

REFIT_VOLUME_GUID       VolumesGUID[256];
UINTN                   VolumesGUIDCount = 0;

BOOLEAN
MetaiMatch (
  IN CHAR16   *String,
  IN CHAR16   *Pattern
);

/**
 This function converts an input device structure to a Unicode string.

 @param DevPath                  A pointer to the device path structure.

 @return A new allocated Unicode string that represents the device path.

 **/

CHAR16 *
EFIAPI
DevicePathToStr (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevPath
) {
  return ConvertDevicePathToText (DevPath, TRUE, TRUE);
}

BOOLEAN
IsEmbeddedTheme () {
  if (!GlobalConfig.Theme || (StriCmp (GlobalConfig.Theme, CONFIG_THEME_EMBEDDED) == 0)) {
    ThemeDir = NULL;
  }

  return (ThemeDir == NULL);
}

/*
VOID
CreateList (
  OUT VOID    ***ListPtr,
  OUT UINTN   *ElementCount,
  IN  UINTN   InitialElementCount
) {
  UINTN   AllocateCount;
  INTN    SubMenuCount = GetSubMenuCount ();

  *ListPtr = NULL;

  *ElementCount = InitialElementCount;
  if (*ElementCount > 0) {
    AllocateCount = (*ElementCount + (SubMenuCount - 1)) & ~(SubMenuCount - 1);   // next multiple of 8
    *ListPtr = AllocatePool (sizeof (VOID *) * AllocateCount);
  }
}
*/

VOID
AddListElement (
  IN OUT  VOID    ***ListPtr,
  IN OUT  UINTN   *ElementCount,
  IN      VOID    *NewElement
) {
  UINTN   AllocateCount;

  if ((*ElementCount & MAX_ELEMENT_COUNT) == 0) {
      AllocateCount = *ElementCount + MAX_ELEMENT_COUNT + 1;
    if (*ElementCount == 0) {
      *ListPtr = AllocatePool (sizeof (VOID *) * AllocateCount);
    } else {
      *ListPtr =  EfiReallocatePool ((VOID *)*ListPtr, sizeof (VOID *) * (*ElementCount), sizeof (VOID *) * AllocateCount);
    }
  }

  (*ListPtr)[*ElementCount] = NewElement;
  (*ElementCount)++;
}


VOID
FreeList (
  IN OUT VOID   ***ListPtr,
  IN OUT INTN   *ElementCount
) {
  INTN i;

  if ((*ElementCount > 0) && (**ListPtr != NULL)) {
    for (i = 0; i < *ElementCount; i++) {
      // TODO: call a user-provided routine for each element here
      if ((*ListPtr)[i] != NULL) {
        FreePool ((*ListPtr)[i]);
      }
    }

    FreePool (*ListPtr);
  }
}

//
// volume functions
//

STATIC
VOID
ScanVolumeBootCode (
  IN OUT  REFIT_VOLUME  *Volume,
  OUT     BOOLEAN       *Bootable
) {
  EFI_STATUS              Status;
  UINT8                   *SectorBuffer;
  UINTN                   i;
  //MBR_PARTITION_INFO      *MbrTable;
  //BOOLEAN                 MbrTableFound;
  UINTN         BlockSize = 0;
  CHAR16        VolumeName[255];
  CHAR8         Tmp[64];
  UINT32        VCrc32;
  //CHAR16      *kind = NULL;

  Volume->HasBootCode = FALSE;
  Volume->LegacyOS->IconName = NULL;
  Volume->LegacyOS->Name = NULL;
  //Volume->BootType = BOOTING_BY_MBR; //default value
  Volume->BootType = BOOTING_BY_EFI;

  *Bootable = FALSE;

  if ((Volume->BlockIO == NULL) || (!Volume->BlockIO->Media->MediaPresent)) {
    return;
  }

  ZeroMem ((CHAR8 *)&Tmp[0], 64);
  BlockSize = Volume->BlockIO->Media->BlockSize;

  if (BlockSize > 2048) {
    return;   // our buffer is too small... the bred of thieve of cable
  }

  SectorBuffer = AllocateAlignedPages (EFI_SIZE_TO_PAGES (2048), 16); //align to 16 byte?! Poher
  ZeroMem ((CHAR8 *)&SectorBuffer[0], 2048);

  // look at the boot sector (this is used for both hard disks and El Torito images!)
  Status = Volume->BlockIO->ReadBlocks (
                              Volume->BlockIO,
                              Volume->BlockIO->Media->MediaId,
                              Volume->BlockIOOffset /* start lba */,
                              2048,
                              SectorBuffer
                            );

  if (!EFI_ERROR (Status) && (SectorBuffer[1] != 0)) {
    // calc crc checksum of first 2 sectors - it's used later for legacy boot BIOS drive num detection
    // note: possible future issues with AF 4K disks
    *Bootable = TRUE;
    Volume->HasBootCode = TRUE; //we assume that all CD are bootable

    VCrc32 = GetCrc32 (SectorBuffer, 512 * 2);
    Volume->DriveCRC32 = VCrc32;

    if (Volume->DiskKind == DISK_KIND_OPTICAL) { //CDROM
      CHAR8   *p = (CHAR8 *)&SectorBuffer[8];

      while (*p == 0x20) {
        p++;
      }

      for (i = 0; i < 30 && (*p >= 0x20) && (*p <= 'z'); i++, p++) {
        Tmp[i] = *p;
      }

      Tmp[i] = 0;
      while ((i>0) && (Tmp[--i] == 0x20)) {}
      Tmp[i + 1] = 0;

      //  if (*p != 0) {
      AsciiStrToUnicodeStrS ((CHAR8 *)&Tmp[0], VolumeName, ARRAY_SIZE (VolumeName));
      //  }

      DBG ("Detected name %s\n", VolumeName);

      Volume->VolName = PoolPrint (L"%s", VolumeName);

      for (i = 8; i < 2000; i++) { //vendor search
        if (SectorBuffer[i] == 'A') {
          if (AsciiStrStr ((CHAR8 *)&SectorBuffer[i], "APPLE")) {
            //StrCpy (Volume->VolName, VolumeName);

            DBG ("        Found AppleDVD\n");

            Volume->LegacyOS->Type = OSTYPE_DARWIN;
            Volume->BootType = BOOTING_BY_CD;
            Volume->LegacyOS->IconName = L"mac";
            break;
          }

        } else if (SectorBuffer[i] == 'M') {
          if (AsciiStrStr ((CHAR8 *)&SectorBuffer[i], "MICROSOFT")) {
            //StrCpy (Volume->VolName, VolumeName);

            DBG ("        Found Windows DVD\n");

            Volume->LegacyOS->Type = OSTYPE_WIN;
            Volume->BootType = BOOTING_BY_CD;
            Volume->LegacyOS->IconName = L"win";
            break;
          }

        } else if (SectorBuffer[i] == 'L') {
          if (AsciiStrStr ((CHAR8 *)&SectorBuffer[i], "LINUX")) {
            //Volume->DevicePath = DuplicateDevicePath (DevicePath);
            //StrCpy (Volume->VolName, VolumeName);

            DBG ("        Found Linux DVD\n");

            Volume->LegacyOS->Type = OSTYPE_LIN;
            Volume->BootType = BOOTING_BY_CD;
            Volume->LegacyOS->IconName = L"linux";
            break;
          }
        }
      }

    } else { //HDD
      // detect specific boot codes
      if (
        (CompareMem (SectorBuffer + 2, "LILO", 4) == 0) ||
        (CompareMem (SectorBuffer + 6, "LILO", 4) == 0) ||
        (CompareMem (SectorBuffer + 3, "SYSLINUX", 8) == 0) ||
        (FindMem (SectorBuffer, 2048, "ISOLINUX", 8) >= 0)
      ) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"linux";
        Volume->LegacyOS->Name = OSTYPE_LINUX_STR;
        Volume->LegacyOS->Type = OSTYPE_LIN;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (FindMem (SectorBuffer, 512, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0) {   // GRUB
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"grub,linux";
        Volume->LegacyOS->Name = OSTYPE_LINUX_STR;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (
          (
            (*((UINT32 *)(SectorBuffer)) == 0x4d0062e9) &&
            (*((UINT16 *)(SectorBuffer + 510)) == 0xaa55)
          ) ||
          (FindMem (SectorBuffer, 2048, "BOOT      ", 10) >= 0)
        ) { //reboot Clover
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"clover";
        Volume->LegacyOS->Name = L"Clover";
        Volume->LegacyOS->Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        //DBG ("Detected Clover FAT32 bootCode\n");
      } else if (
          (
            (*((UINT32 *)(SectorBuffer + 502)) == 0) &&
            (*((UINT32 *)(SectorBuffer + 506)) == 50000) &&
            (*((UINT16 *)(SectorBuffer + 510)) == 0xaa55)
          ) ||
          (FindMem (SectorBuffer, 2048, "Starting the BTX loader", 23) >= 0)
        ) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"freebsd";
        Volume->LegacyOS->Name = L"FreeBSD";
        Volume->LegacyOS->Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (
          (FindMem (SectorBuffer, 512, "!Loading", 8) >= 0) ||
          (FindMem (SectorBuffer, 2048, "/cdboot\0/CDBOOT\0", 16) >= 0)
        ) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"openbsd";
        Volume->LegacyOS->Name = L"OpenBSD";
        Volume->LegacyOS->Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (
          (FindMem (SectorBuffer, 512, "Not a bootxx image", 18) >= 0) ||
          (*((UINT32 *)(SectorBuffer + 1028)) == 0x7886b6d1)
        ) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"netbsd";
        Volume->LegacyOS->Name = L"NetBSD";
        Volume->LegacyOS->Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (FindMem (SectorBuffer, 2048, "NTLDR", 5) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"win";
        Volume->LegacyOS->Name = OSTYPE_WINDOWS_STR;
        Volume->LegacyOS->Type = OSTYPE_WIN;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (FindMem (SectorBuffer, 2048, "BOOTMGR", 7) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"vista,win";
        Volume->LegacyOS->Name = OSTYPE_WINDOWS_STR;
        Volume->LegacyOS->Type = OSTYPE_WIN;
        Volume->BootType = BOOTING_BY_PBR;
      } else if (
          (FindMem (SectorBuffer, 512, "CPUBOOT SYS", 11) >= 0) ||
          (FindMem (SectorBuffer, 512, "KERNEL  SYS", 11) >= 0)
        ) {
        Volume->HasBootCode = TRUE;
        Volume->LegacyOS->IconName = L"freedos";
        Volume->LegacyOS->Name = L"FreeDOS";
        Volume->LegacyOS->Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
      }
    }

    // NOTE: If you add an operating system with a name that starts with 'W' or 'L', you
    //  need to fix AddLegacyEntry in main.c.

#if REFIT_DEBUG > 0
    DBG ("         Result of bootCode detection: %sbootable %s (%s)\n",
        Volume->HasBootCode ? L"" : L"non-",
        Volume->LegacyOS->Name ? Volume->LegacyOS->Name: L"unknown",
        Volume->LegacyOS->IconName ? Volume->LegacyOS->IconName: L"legacy");
#endif

    if (FindMem (SectorBuffer, 512, "Non-system disk", 15) >= 0) { // dummy FAT boot sector
      Volume->HasBootCode = FALSE;
    }
  }

  gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer, 1);
}

STATIC
EFI_STATUS
ReadGPT (
  IN OUT REFIT_VOLUME           *Volume
) {
  EFI_STATUS                        Status = EFI_SUCCESS;
  EFI_DISK_IO_PROTOCOL              *DiskIo;
  EFI_PARTITION_TABLE_HEADER        *PartHdr;
  EFI_PARTITION_ENTRY               *PartEntry, *Entry;
  UINT32                            MediaId, BlockSize, Index;

  Status = gBS->HandleProtocol (
                  Volume->DeviceHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **) &(DiskIo)
                );

  if (!EFI_ERROR (Status)) {
    BlockSize = Volume->BlockIO->Media->BlockSize;
    MediaId = Volume->BlockIO->Media->MediaId;
    PartHdr = AllocateZeroPool (BlockSize);

    Status = DiskIo->ReadDisk (
                       DiskIo,
                       MediaId,
                       MultU64x32 (PRIMARY_PART_HEADER_LBA, BlockSize),
                       BlockSize,
                       PartHdr
                     );

    if (!EFI_ERROR (Status)) {
      if (
        (PartHdr->Header.Signature != EFI_PTAB_HEADER_ID) ||
        (PartHdr->MyLBA != PRIMARY_PART_HEADER_LBA) ||
        (PartHdr->SizeOfPartitionEntry < sizeof (EFI_PARTITION_ENTRY))
      ) {
        DBG ("Invalid EFI partition table header\n");
        FreePool (PartHdr);
        return EFI_LOAD_ERROR;
      }

      DBG ("Read patition entries:\n");

      PartEntry = AllocatePool (PartHdr->NumberOfPartitionEntries * PartHdr->SizeOfPartitionEntry);
      if (PartEntry == NULL) {
        return EFI_BUFFER_TOO_SMALL;
      }

      DBG ("BlockSize                :%d\n", BlockSize);
      DBG ("PartitionEntryLBA        :%x\n", PartHdr->PartitionEntryLBA);
      DBG ("NumberOfPartitionEntries :%d\n", PartHdr->NumberOfPartitionEntries);
      DBG ("SizeOfPartitionEntry     :%d\n", PartHdr->SizeOfPartitionEntry);

      Status = DiskIo->ReadDisk (
                         DiskIo,
                         MediaId,
                         MultU64x32 (PartHdr->PartitionEntryLBA, BlockSize),
                         PartHdr->NumberOfPartitionEntries * (PartHdr->SizeOfPartitionEntry),
                         PartEntry
                       );

      if (EFI_ERROR (Status)) {
        DBG (" Partition Entry ReadDisk error\n");
        return EFI_DEVICE_ERROR;
      }

      for (Index = 0; Index < PartHdr->NumberOfPartitionEntries; Index++) {
        Entry = (EFI_PARTITION_ENTRY *) ((UINT8 *) PartEntry + Index * PartHdr->SizeOfPartitionEntry);
        if ((Entry->StartingLBA == 0) && (Entry->EndingLBA == 0)) {
          break;
        }

        DBG (" Partition            :%d\n", Index);
        DBG (" PartitionTypeGUID    :%g\n", Entry->PartitionTypeGUID);
        DBG (" UniquePartitionGUID  :%g\n", Entry->UniquePartitionGUID);
        DBG (" StartingLBA          :%d\n", Entry->StartingLBA);
        DBG (" EndingLBA            :%d\n", Entry->EndingLBA);

        if (VolumesGUIDCount < ARRAY_SIZE (VolumesGUID)) {
          CopyGuid (&VolumesGUID[VolumesGUIDCount].PartitionTypeGUID, &Entry->PartitionTypeGUID);
          CopyGuid (&VolumesGUID[VolumesGUIDCount++].UniquePartitionGUID, &Entry->UniquePartitionGUID);
        }
      }
    }
  }

  return Status;
}

//at start we have only Volume->DeviceHandle
STATIC
EFI_STATUS
ScanVolume (
  IN OUT REFIT_VOLUME           *Volume
) {
  EFI_STATUS                    Status;
  EFI_DEVICE_PATH               *DevicePath, *NextDevicePath,
                                *DiskDevicePath, *RemainingDevicePath = NULL;
  HARDDRIVE_DEVICE_PATH         *HdPath     = NULL;
  EFI_HANDLE                    WholeDiskHandle;
  UINTN                         PartialLength = 0, DevicePathSize;
  //  UINTN                     BufferSize = 255;
  EFI_FILE_SYSTEM_VOLUME_LABEL  *VolumeInfo;
  EFI_FILE_SYSTEM_INFO          *FileSystemInfoPtr;
  EFI_FILE_INFO                 *RootInfo = NULL;
  BOOLEAN                       Bootable;
  //EFI_INPUT_KEY               Key;
  CHAR16                        *TmpName;

  // get device path
  DiskDevicePath = DevicePathFromHandle (Volume->DeviceHandle);
  DevicePathSize = GetDevicePathSize (DiskDevicePath);
  Volume->DevicePath = AllocateAlignedPages (EFI_SIZE_TO_PAGES (DevicePathSize), 64);
  CopyMem (Volume->DevicePath, DiskDevicePath, DevicePathSize);
  Volume->DevicePathString = FileDevicePathToStr (Volume->DevicePath);

  //Volume->DevicePath = DuplicateDevicePath (DevicePathFromHandle (Volume->DeviceHandle));
  if (Volume->DevicePath != NULL) {
    MsgLog (" %s\n", FileDevicePathToStr (Volume->DevicePath));
  }

  Volume->DiskKind = DISK_KIND_INTERNAL;  // default

  // get block i/o
  Status = gBS->HandleProtocol (
                  Volume->DeviceHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **) &(Volume->BlockIO)
                );

  if (EFI_ERROR (Status)) {
    Volume->BlockIO = NULL;
    DBG ("         Warning: Can't get BlockIO protocol.\n");
    //WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
    //gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    return Status;
  }

  Bootable = FALSE;
  if (Volume->BlockIO->Media->BlockSize == 2048) {
    DBG ("         Found optical drive\n");
    Volume->DiskKind = DISK_KIND_OPTICAL;
    Volume->BlockIOOffset = 0x10; // offset already applied for FS but not for blockio
    ScanVolumeBootCode (Volume, &Bootable);
  } else {
    //DBG ("         Found HD drive\n");
    Volume->BlockIOOffset = 0;
    // scan for bootCode and MBR table
    ScanVolumeBootCode (Volume, &Bootable);

    //DBG ("         ScanVolumeBootCode success\n");

    // detect device type
    DevicePath = DuplicateDevicePath (Volume->DevicePath);
    while ((DevicePath != NULL) && !IsDevicePathEndType (DevicePath)) {
      NextDevicePath = NextDevicePathNode (DevicePath);

      if (
        (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) &&
        ((DevicePathSubType (DevicePath) == MSG_SATA_DP) || (DevicePathSubType (DevicePath) == MSG_ATAPI_DP))
      ) {
        //DBG ("         HDD volume\n");
        Volume->DiskKind = DISK_KIND_INTERNAL;
        break;
      }

      if (
        (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) &&
        ((DevicePathSubType (DevicePath) == MSG_USB_DP) || (DevicePathSubType (DevicePath) == MSG_USB_CLASS_DP))
      ) {
        //DBG ("         USB volume\n");
        Volume->DiskKind = DISK_KIND_EXTERNAL;
        break;
      }

      // FIREWIRE Devices
      if (
        (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) &&
        ((DevicePathSubType (DevicePath) == MSG_1394_DP) || (DevicePathSubType (DevicePath) == MSG_FIBRECHANNEL_DP))
      ) {
        //DBG ("         FireWire volume\n");
        Volume->DiskKind = DISK_KIND_FIREWIRE;
        break;
      }

      // CD-ROM Devices
      if (
        (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MEDIA_CDROM_DP)
      ) {
        //DBG ("         CD-ROM volume\n");
        Volume->DiskKind = DISK_KIND_OPTICAL;    //it's impossible
        break;
      }

      // VENDOR Specific Path
      if (
        (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MEDIA_VENDOR_DP)
      ) {
        //DBG ("         Vendor volume\n");
        Volume->DiskKind = DISK_KIND_NODISK;
        break;
      }

      // LEGACY CD-ROM
      if (
        (DevicePathType (DevicePath) == BBS_DEVICE_PATH) &&
        ((DevicePathSubType (DevicePath) == BBS_BBS_DP) || (DevicePathSubType (DevicePath) == BBS_TYPE_CDROM))
      ) {
        //DBG ("         Legacy CD-ROM volume\n");
        Volume->DiskKind = DISK_KIND_OPTICAL;
        break;
      }

      // LEGACY HARDDISK
      if (
        (DevicePathType (DevicePath) == BBS_DEVICE_PATH) &&
        ((DevicePathSubType (DevicePath) == BBS_BBS_DP) || (DevicePathSubType (DevicePath) == BBS_TYPE_HARDDRIVE))
      ) {
        //DBG ("         Legacy HDD volume\n");
        Volume->DiskKind = DISK_KIND_INTERNAL;
        break;
      }

      DevicePath = NextDevicePath;
    }
  }

  DevicePath = DuplicateDevicePath (Volume->DevicePath);
  RemainingDevicePath = DevicePath; //initial value

  //
  // find the partition device path node
  //
  while (DevicePath && !IsDevicePathEnd (DevicePath)) {
    if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) {
      if (DevicePathSubType (DevicePath) == MEDIA_HARDDRIVE_DP) {
        HdPath = (HARDDRIVE_DEVICE_PATH *)DevicePath;
        //break;
      }

      // @savvas: Check that vendor-assigned GUID defines APFS Container Partition
      else if (
        (DevicePathSubType (DevicePath) == MEDIA_VENDOR_DP) &&
        (CompareGuid ((EFI_GUID *)((UINT8 *)DevicePath + 0x04), &gAppleVenMediaGuid))
      ) {
        CopyGuid (&Volume->VenMediaGUID, (EFI_GUID *)((UINT8 *)DevicePath + 0x14));
        //DBG (" ---> VenMediaGUID :%g\n", &Volume->VenMediaGUID);
      }
    }

    DevicePath = NextDevicePathNode (DevicePath);
  }

  //  DBG ("DevicePath scanned\n");

  if (HdPath) {
    //Print (L"Partition found %s\n", DevicePathToStr ((EFI_DEVICE_PATH *)HdPath));

    PartialLength = (UINTN)((UINT8 *)HdPath - (UINT8 *)(RemainingDevicePath));

    if (PartialLength > 0x1000) {
      PartialLength = sizeof (EFI_DEVICE_PATH); //something wrong here but I don't want to be freezed
      //return EFI_SUCCESS;
    }

    DiskDevicePath = (EFI_DEVICE_PATH *)AllocatePool (PartialLength + sizeof (EFI_DEVICE_PATH));
    CopyMem (DiskDevicePath, Volume->DevicePath, PartialLength);
    CopyMem ((UINT8 *)DiskDevicePath + PartialLength, DevicePath, sizeof (EFI_DEVICE_PATH)); //EndDevicePath

    //Print (L"WholeDevicePath  %s\n", DevicePathToStr (DiskDevicePath));
    //DBG ("WholeDevicePath  %s\n", DevicePathToStr (DiskDevicePath));

    RemainingDevicePath = DiskDevicePath;

    Status = gBS->LocateDevicePath (&gEfiDevicePathProtocolGuid, &RemainingDevicePath, &WholeDiskHandle);
    if (EFI_ERROR (Status)) {
      DBG ("Can't find WholeDevicePath: %r\n", Status);
    } else {
      Volume->WholeDiskDeviceHandle = WholeDiskHandle;
      Volume->WholeDiskDevicePath = DuplicateDevicePath (RemainingDevicePath);

      // look at the BlockIO protocol
      Status = gBS->HandleProtocol (
                      WholeDiskHandle,
                      &gEfiBlockIoProtocolGuid,
                      (VOID **)&Volume->WholeDiskBlockIO
                    );

      if (!EFI_ERROR (Status)) {
        //DBG ("WholeDiskBlockIO %x BlockSize=%d\n", Volume->WholeDiskBlockIO, Volume->WholeDiskBlockIO->Media->BlockSize);

        // check the media block size
        if (Volume->WholeDiskBlockIO->Media->BlockSize == 2048) {
          Volume->DiskKind = DISK_KIND_OPTICAL;
        }

      } else {
        Volume->WholeDiskBlockIO = NULL;
        DBG ("no WholeDiskBlockIO: %r\n", Status);
        //CheckError (Status, L"from HandleProtocol");
      }
    }

    FreePool (DiskDevicePath);
  } /*else {
    DBG ("HD path is not found\n"); //master volume!
  }*/

  //  if (GlobalConfig.FastBoot) {
  //    return EFI_SUCCESS;
  //  }

  if (!Bootable) {

    //if (Volume->HasBootCode) {
    //  DBG ("  Volume considered non-bootable, but boot code is present\n");
    //  //WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
    //  //gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    //}

    Volume->HasBootCode = FALSE;
  }

  // open the root directory of the volume
  Volume->RootDir = EfiLibOpenRoot (Volume->DeviceHandle);
  //  DBG ("Volume->RootDir OK\n");
  if (Volume->RootDir == NULL) {
    //Print (L"Error: Can't open volume.\n");
    // TODO: signal that we had an error
    //Slice - there is LegacyBoot volume
    //properties are set before
    //DBG ("LegacyBoot volume\n");

    if (HdPath) {
      TmpName = (CHAR16 *)AllocateZeroPool (60);
      UnicodeSPrint (TmpName, 60, L"Legacy HD%d", HdPath->PartitionNumber);
      Volume->VolName = EfiStrDuplicate (TmpName);
      FreePool (TmpName);
    } else if (!Volume->VolName) {
      Volume->VolName =  L"Whole Disc Boot";
    }

    if (!Volume->LegacyOS->IconName) {
      Volume->LegacyOS->IconName = L"legacy";
    }

    return EFI_SUCCESS;
  }

  // get volume name
  if (!Volume->VolName) {
    FileSystemInfoPtr = EfiLibFileSystemInfo (Volume->RootDir);
    if (FileSystemInfoPtr) {
      //DBG ("  Volume name from FileSystem: '%s'\n", FileSystemInfoPtr->VolumeLabel);
      Volume->VolName = EfiStrDuplicate (FileSystemInfoPtr->VolumeLabel);
      FreePool (FileSystemInfoPtr);
    }
  }

  if (!Volume->VolName) {
    VolumeInfo = EfiLibFileSystemVolumeLabelInfo (Volume->RootDir);
    if (VolumeInfo) {
      //DBG ("  Volume name from VolumeLabel: '%s'\n", VolumeInfo->VolumeLabel);
      Volume->VolName = EfiStrDuplicate (VolumeInfo->VolumeLabel);
      FreePool (VolumeInfo);
    }
  }

  if (!Volume->VolName) {
    RootInfo = EfiLibFileInfo (Volume->RootDir);
    if (RootInfo) {
      //DBG ("  Volume name from RootFile: '%s'\n", RootInfo->FileName);
      Volume->VolName = EfiStrDuplicate (RootInfo->FileName);
      FreePool (RootInfo);
    }
  }

  if (
      (Volume->VolName == NULL) ||
      (Volume->VolName[0] == 0) ||
      ((Volume->VolName[0] == L'\\') && (Volume->VolName[1] == 0)) ||
      ((Volume->VolName[0] == L'/') && (Volume->VolName[1] == 0))
  ) {
    VOID  *Instance;

    if (!EFI_ERROR (gBS->HandleProtocol (
                          Volume->DeviceHandle,
                          &gEfiPartTypeSystemPartGuid,
                          &Instance)
                   )
    ) {
      Volume->VolName = L"EFI";
    }
  }

  if (!Volume->VolName) {
    //DBG ("Create unknown name\n");
    //WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
    //gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if (HdPath) {
      TmpName = (CHAR16 *)AllocateZeroPool (128);
      UnicodeSPrint (TmpName, 128, L"Unknown HD%d", HdPath->PartitionNumber);
      Volume->VolName = EfiStrDuplicate (TmpName);
      FreePool (TmpName);
      // NOTE: this is normal for Apple's VenMedia device paths
    } else {
      Volume->VolName = L"Unknown HD";
    }
  }

  //NOTE: Sothor - We will find icon names later once we have found boot.efi on the volume
  //here we set Volume->IconName (tiger,leo,snow,lion,cougar, etc)
  //Status = GetOSVersion (Volume);

  return EFI_SUCCESS;
}

STATIC
VOID
ScanExtendedPartition (
  REFIT_VOLUME          *WholeDiskVolume,
  MBR_PARTITION_INFO    *MbrEntry
) {
  EFI_STATUS              Status;
  REFIT_VOLUME            *Volume;
  UINT32                  ExtBase, ExtCurrent, NextExtCurrent;
  UINTN                   i, LogicalPartitionIndex = 4;
  UINT8                   *SectorBuffer;
  BOOLEAN                 Bootable;
  MBR_PARTITION_INFO      *EMbrTable;

  ExtBase = MbrEntry->StartLBA;
  SectorBuffer = AllocateAlignedPages (EFI_SIZE_TO_PAGES (512), WholeDiskVolume->BlockIO->Media->IoAlign);

  for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
    // read current EMBR
    Status = WholeDiskVolume->BlockIO->ReadBlocks (
                                          WholeDiskVolume->BlockIO,
                                          WholeDiskVolume->BlockIO->Media->MediaId,
                                          ExtCurrent, 512, SectorBuffer
                                        );

    if (EFI_ERROR (Status)) {
      break;
    }

    if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55) {
      break;
    }

    EMbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);

    // scan logical partitions in this EMBR
    NextExtCurrent = 0;
    for (i = 0; i < 4; i++) {
      if (
        ((EMbrTable[i].Flags != 0x00) && (EMbrTable[i].Flags != 0x80)) ||
        (EMbrTable[i].StartLBA == 0) ||
        (EMbrTable[i].Size == 0)
      ) {
        break;
      }

      if (IS_EXTENDED_PART_TYPE (EMbrTable[i].Type)) {
        // set next ExtCurrent
        NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
        break;
      } else {
        // found a logical partition
        Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
        Volume->DiskKind = WholeDiskVolume->DiskKind;
        Volume->IsMbrPartition = TRUE;
        Volume->MbrPartitionIndex = LogicalPartitionIndex++;
        Volume->VolName = PoolPrint (L"Partition %d", Volume->MbrPartitionIndex + 1);
        Volume->BlockIO = WholeDiskVolume->BlockIO;
        Volume->BlockIOOffset = ExtCurrent + EMbrTable[i].StartLBA;
        Volume->WholeDiskBlockIO = WholeDiskVolume->BlockIO;
        Volume->WholeDiskDeviceHandle = WholeDiskVolume->DeviceHandle;

        Bootable = FALSE;
        ScanVolumeBootCode (Volume, &Bootable);

        if (!Bootable) {
          Volume->HasBootCode = FALSE;
        }

        AddListElement ((VOID ***)&Volumes, &VolumesCount, Volume);
      }
    }
  }

  gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer, 1);
}

VOID
ScanVolumes () {
  EFI_STATUS                  Status;
  EFI_HANDLE                  *Handles = NULL;
  //EFI_DEVICE_PATH_PROTOCOL  *VolumeDevicePath;
  EFI_GUID                    *Guid; //for debug only
  //EFI_INPUT_KEY Key;
  UINTN                       HandleCount = 0, HandleIndex,
                              PartitionIndex, i, SectorSum,
                              VolumeIndex, VolumeIndex2;
  REFIT_VOLUME                *Volume, *WholeDiskVolume;
  MBR_PARTITION_INFO          *MbrTable;
  UINT8                       *SectorBuffer1, *SectorBuffer2;
  INT32                       HVi, Index;

  //DBG ("Scanning volumes...\n");
  DbgHeader ("ScanVolumes");

  // get all BlockIo handles
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                );

  if (Status == EFI_NOT_FOUND) {
    return;
  }

  MsgLog ("Found %d volumes with blockIO:\n", HandleCount);

  VolumesGUIDCount = 0;
  ZeroMem (&VolumesGUID, ARRAY_SIZE (VolumesGUID) * sizeof (REFIT_VOLUME_GUID));

  // first pass: collect information about all handles
  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
    Volume->LegacyOS = AllocateZeroPool (sizeof (LEGACY_OS));
    Volume->DeviceHandle = Handles[HandleIndex];
    Volume->BlessedPath = NULL;
    Volume->PartitionType = PARTITION_TYPE_UNKNOWN;

    if (Volume->DeviceHandle == SelfDeviceHandle) {
      SelfVolume = Volume;
    }

    MsgLog (" - [%02d]: Volume:", HandleIndex);

    Volume->Hidden = FALSE; // default to not hidden

    CopyGuid (&Volume->VenMediaGUID, &gEfiPartTypeUnusedGuid);

    Status = ScanVolume (Volume);
    if (!EFI_ERROR (Status)) {
      if (ReadGPT (Volume) == EFI_LOAD_ERROR) { /////////////
        Guid = FindGPTPartitionGuidInDevicePath (Volume->DevicePath);
        for (Index = 0; Index < VolumesGUIDCount; Index++) {
          if (CompareGuid (&VolumesGUID[Index].UniquePartitionGUID, Guid)) {
            CopyGuid (&Volume->PartitionTypeGUID, &VolumesGUID[Index].PartitionTypeGUID);
            //DBG (" ---> PartitionTypeGUID :%g\n", &Volume->PartitionTypeGUID);
          }
        }
      }

      AddListElement ((VOID ***)&Volumes, &VolumesCount, Volume);
      for (HVi = 0; HVi < gSettings.HVCount; HVi++) {
        if (
          StriStr (Volume->DevicePathString, gSettings.HVHideStrings[HVi]) ||
          ((Volume->VolName != NULL) && StriStr (Volume->VolName, gSettings.HVHideStrings[HVi]))
        ) {
          Volume->Hidden = TRUE;
          MsgLog ("         hiding this volume\n");
          break;
        }
      }

      if (!Volume->LegacyOS->IconName) {
        Volume->LegacyOS->IconName = L"legacy";
      }

      //DBG ("  Volume '%s', LegacyOS '%s', LegacyIcon (s) '%s', GUID = %g\n",
      //Volume->VolName, Volume->LegacyOS->Na -me ? Volume->LegacyOS->Name : L"", Volume->LegacyOS->IconName, Guid);

      if (SelfVolume == Volume) {
        MsgLog ("         This is SelfVolume !!\n");
      }

    } else {
      DBG ("         wrong volume Nr%d?!\n", HandleIndex);
      FreePool (Volume);
    }
  }

  FreePool (Handles);

  //DBG ("Found %d volumes\n", VolumesCount);

  // second pass: relate partitions and whole disk devices
  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];

    // check MBR partition table for extended partitions
    if (
      (Volume->BlockIO != NULL) &&
      (Volume->WholeDiskBlockIO != NULL) &&
      (Volume->BlockIO == Volume->WholeDiskBlockIO) &&
      (Volume->BlockIOOffset == 0) &&
      (Volume->MbrPartitionTable != NULL)
    ) {
      DBG ("         Volume %d has MBR\n", VolumeIndex);
      MbrTable = Volume->MbrPartitionTable;
      for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
        if (IS_EXTENDED_PART_TYPE (MbrTable[PartitionIndex].Type)) {
          ScanExtendedPartition (Volume, MbrTable + PartitionIndex);
        }
      }
    }

    // search for corresponding whole disk volume entry
    WholeDiskVolume = NULL;
    if (
      (Volume->BlockIO != NULL) &&
      (Volume->WholeDiskBlockIO != NULL) &&
      (Volume->BlockIO != Volume->WholeDiskBlockIO)
    ) {
      for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
        if (
          (Volumes[VolumeIndex2]->BlockIO == Volume->WholeDiskBlockIO) &&
          (Volumes[VolumeIndex2]->BlockIOOffset == 0)
        ) {
          WholeDiskVolume = Volumes[VolumeIndex2];
        }
      }
    }

    if (
      (WholeDiskVolume != NULL) &&
      (WholeDiskVolume->MbrPartitionTable != NULL)
    ) {
      // check if this volume is one of the partitions in the table
      MbrTable = WholeDiskVolume->MbrPartitionTable;
      SectorBuffer1 = AllocateAlignedPages (EFI_SIZE_TO_PAGES (512), 16);
      SectorBuffer2 = AllocateAlignedPages (EFI_SIZE_TO_PAGES (512), 16);

      for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
        // check size
        if ((UINT64)(MbrTable[PartitionIndex].Size) != (Volume->BlockIO->Media->LastBlock + 1)) {
          continue;
        }

        // compare boot sector read through offset vs. directly
        Status = Volume->BlockIO->ReadBlocks (
                                    Volume->BlockIO,
                                    Volume->BlockIO->Media->MediaId,
                                    Volume->BlockIOOffset,
                                    512,
                                    SectorBuffer1
                                  );

        if (EFI_ERROR (Status)) {
          break;
        }

        Status = Volume->WholeDiskBlockIO->ReadBlocks (
                                              Volume->WholeDiskBlockIO,
                                              Volume->WholeDiskBlockIO->Media->MediaId,
                                              MbrTable[PartitionIndex].StartLBA,
                                              512,
                                              SectorBuffer2
                                            );

        if (EFI_ERROR (Status)) {
          break;
        }

        if (CompareMem (SectorBuffer1, SectorBuffer2, 512) != 0) {
          continue;
        }

        SectorSum = 0;

        for (i = 0; i < 512; i++) {
          SectorSum += SectorBuffer1[i];
        }

        if (SectorSum < 1000) {
          continue;
        }

        // TODO: mark entry as non-bootable if it is an extended partition

        // now we're reasonably sure the association is correct...
        Volume->IsMbrPartition = TRUE;
        Volume->MbrPartitionTable = MbrTable;
        Volume->MbrPartitionIndex = PartitionIndex;

        if (Volume->VolName == NULL) {
          Volume->VolName = PoolPrint (L"Partition %d", PartitionIndex + 1);
        }
        break;
      }

      gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer1, 1);
      gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer2, 1);
    }
  }
}

STATIC
VOID
UninitVolumes () {
  REFIT_VOLUME    *Volume;
  UINTN           VolumeIndex;

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];

    if (Volume->RootDir != NULL) {
      Volume->RootDir->Close (Volume->RootDir);
      Volume->RootDir = NULL;
    }

    Volume->DeviceHandle = NULL;
    Volume->BlockIO = NULL;
    Volume->WholeDiskBlockIO = NULL;
    Volume->WholeDiskDeviceHandle = NULL;
    FreePool (Volume);
  }

  if (Volumes != NULL) {
    FreePool (Volumes);
    Volumes = NULL;
  }

  VolumesCount = 0;
}

VOID
ReinitVolumes () {
  EFI_STATUS              Status;
  REFIT_VOLUME            *Volume;
  UINTN                   VolumeIndex, VolumesFound = 0;
  EFI_DEVICE_PATH         *RemainingDevicePath;
  EFI_HANDLE              DeviceHandle, WholeDiskHandle;

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];

    if (!Volume) {
      continue;
    }

    DBG ("Volume %d at reinit found:\n", VolumeIndex);
    DBG ("Volume->DevicePath=%s\n", FileDevicePathToStr (Volume->DevicePath));

    VolumesFound++;

    if (Volume->DevicePath != NULL) {
      // get the handle for that path
      RemainingDevicePath = Volume->DevicePath;
      Status = gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid, &RemainingDevicePath, &DeviceHandle);

      if (!EFI_ERROR (Status)) {
        Volume->DeviceHandle = DeviceHandle;

        // get the root directory
        Volume->RootDir = EfiLibOpenRoot (Volume->DeviceHandle);
      } /*else {
        CheckError (Status, L"from LocateDevicePath");
      }*/
    }

    if (Volume->WholeDiskDevicePath != NULL) {
      // get the handle for that path
      RemainingDevicePath = DuplicateDevicePath (Volume->WholeDiskDevicePath);
      Status = gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid, &RemainingDevicePath, &WholeDiskHandle);

      if (!EFI_ERROR (Status)) {
        Volume->WholeDiskBlockIO = WholeDiskHandle;

        // get the BlockIO protocol
        Status = gBS->HandleProtocol (
                        WholeDiskHandle,
                        &gEfiBlockIoProtocolGuid,
                        (VOID **)&Volume->WholeDiskBlockIO
                      );

        if (EFI_ERROR (Status)) {
          Volume->WholeDiskBlockIO = NULL;
          CheckError (Status, L"from HandleProtocol");
        }
      } /*else {
        CheckError (Status, L"from LocateDevicePath");
      }*/
    }
  }

  VolumesCount = VolumesFound;
}

REFIT_VOLUME *
FindVolumeByName (
  IN CHAR16 *VolName
) {
  REFIT_VOLUME    *Volume;
  UINTN           VolumeIndex;

  if (!VolName) {
    return NULL;
  }

  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];
    if (!Volume) {
      continue;
    }

    if (Volume->VolName && StrCmp (Volume->VolName,VolName) == 0) {
      return Volume;
    }
  }

  return NULL;
}

STATIC
EFI_STATUS
FinishInitRefitLib () {
  EFI_STATUS      Status;

  if (SelfRootDir == NULL) {
    SelfRootDir = EfiLibOpenRoot (SelfLoadedImage->DeviceHandle);
    if (SelfRootDir != NULL) {
      SelfDeviceHandle = SelfLoadedImage->DeviceHandle;
    } else {
      return EFI_LOAD_ERROR;
    }
  }

  /*Status  = */  SelfRootDir->Open (SelfRootDir, &ThemeDir, ThemePath,    EFI_FILE_MODE_READ, 0);
  /*Status  = */  SelfRootDir->Open (SelfRootDir, &OEMDir,   OEMPath,      EFI_FILE_MODE_READ, 0);
  Status    =     SelfRootDir->Open (SelfRootDir, &SelfDir,  SelfDirPath,  EFI_FILE_MODE_READ, 0);

  CheckFatalError (Status, L"while opening our installation directory");

  return Status;
}

EFI_STATUS
InitRefitLib (
  IN EFI_HANDLE     ImageHandle
) {
  EFI_STATUS                  Status;
  CHAR16                      *FilePathAsString;
  UINTN                       i, DevicePathSize;
  EFI_DEVICE_PATH_PROTOCOL    *TmpDevicePath;

  SelfImageHandle = ImageHandle;

  Status = gBS->HandleProtocol (
                  SelfImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&SelfLoadedImage
                );

  if (CheckFatalError (Status, L"while getting a LoadedImageProtocol handle")) {
    return Status;
  }

  SelfDeviceHandle = SelfLoadedImage->DeviceHandle;
  TmpDevicePath = DevicePathFromHandle (SelfDeviceHandle);
  DevicePathSize = GetDevicePathSize (TmpDevicePath);
  SelfDevicePath = AllocateAlignedPages (EFI_SIZE_TO_PAGES (DevicePathSize), 64);
  CopyMem (SelfDevicePath, TmpDevicePath, DevicePathSize);

  //DBG ("SelfDevicePath=%s @%x\n", FileDevicePathToStr (SelfDevicePath), SelfDeviceHandle);

  // find the current directory
  FilePathAsString = FileDevicePathToStr (SelfLoadedImage->FilePath);

  if (FilePathAsString != NULL) {
    SelfFullDevicePath = FileDevicePath (SelfDeviceHandle, FilePathAsString);
    for (i = StrLen (FilePathAsString); i > 0 && FilePathAsString[i] != '\\'; i--);
    if (i > 0) {
      FilePathAsString[i] = 0;
    } else {
      FilePathAsString[0] = L'\\';
      FilePathAsString[1] = 0;
    }
  } else {
    FilePathAsString = AllocateCopyPool (StrSize (L"\\"), L"\\");
  }

  SelfDirPath = FilePathAsString;

  //DBG ("SelfDirPath = %s\n", SelfDirPath);

  return FinishInitRefitLib ();
}

VOID
UninitRefitLib () {
  // called before running external programs to close open file handles

  if (SelfDir != NULL) {
    SelfDir->Close (SelfDir);
    SelfDir = NULL;
  }

  if (OEMDir != NULL) {
    OEMDir->Close (OEMDir);
    OEMDir = NULL;
  }

  if (ThemeDir != NULL) {
    ThemeDir->Close (ThemeDir);
    ThemeDir = NULL;
  }

  if (SelfRootDir != NULL) {
    SelfRootDir->Close (SelfRootDir);
    SelfRootDir = NULL;
  }

  UninitVolumes ();
}

EFI_STATUS
ReinitRefitLib () {
  // called after reconnect drivers to re-open file handles
  EFI_STATUS                  Status;
  EFI_HANDLE                  NewSelfHandle;
  EFI_DEVICE_PATH_PROTOCOL    *TmpDevicePath;

  //DbgHeader ("ReinitRefitLib");

  if (!SelfDevicePath) {
    return EFI_NOT_FOUND;
  }

  TmpDevicePath = DuplicateDevicePath (SelfDevicePath);

  DBG ("reinit: self device path=%s\n", FileDevicePathToStr (TmpDevicePath));

  if (TmpDevicePath == NULL) {
    return EFI_NOT_FOUND;
  }

  NewSelfHandle = NULL;
  Status = gBS->LocateDevicePath (
                  &gEfiSimpleFileSystemProtocolGuid,
                  &TmpDevicePath,
                  &NewSelfHandle
                );

  CheckError (Status, L"while reopening our self handle");
  //DBG ("new SelfHandle=%x\n", NewSelfHandle);

  SelfRootDir = EfiLibOpenRoot (NewSelfHandle);

  if (SelfRootDir == NULL) {
    DBG ("SelfRootDir can't be reopened\n");
    return EFI_NOT_FOUND;
  }

  SelfDeviceHandle = NewSelfHandle;

  /*Status  = */  SelfRootDir->Open (SelfRootDir, &ThemeDir, ThemePath,    EFI_FILE_MODE_READ, 0);
  /*Status  = */  SelfRootDir->Open (SelfRootDir, &OEMDir,   OEMPath,      EFI_FILE_MODE_READ, 0);
  Status    =     SelfRootDir->Open (SelfRootDir, &SelfDir,  SelfDirPath,  EFI_FILE_MODE_READ, 0);

  CheckFatalError (Status, L"while reopening our installation directory");

  return Status;
}

//
// file and dir functions
//

BOOLEAN
FileExists (
  IN EFI_FILE   *Root,
  IN CHAR16     *RelativePath
) {
  EFI_STATUS  Status;
  EFI_FILE    *TestFile;

  Status = Root->Open (Root, &TestFile, RelativePath, EFI_FILE_MODE_READ, 0);
  if (Status == EFI_SUCCESS) {
    TestFile->Close (TestFile);
  }

  return (Status == EFI_SUCCESS);
}

BOOLEAN
DeleteFile (
  IN EFI_FILE   *Root,
  IN CHAR16     *RelativePath
) {
  EFI_STATUS      Status;
  EFI_FILE        *File;
  EFI_FILE_INFO   *FileInfo;

  //DBG ("DeleteFile: %s\n", RelativePath);
  // open file for read/write to see if it exists, need write for delete
  Status = Root->Open (Root, &File, RelativePath, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  //DBG (" Open: %r\n", Status);

  if (Status == EFI_SUCCESS) {
    // exists - check if it is a file
    FileInfo = EfiLibFileInfo (File);

    if (FileInfo == NULL) {
      // error
      //DBG (" FileInfo is NULL\n");
      File->Close (File);
      return FALSE;
    }

    //DBG (" FileInfo attr: %x\n", FileInfo->Attribute);

    if ((FileInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY) {
      // it's directory - return error
      //DBG (" File is DIR\n");
      FreePool (FileInfo);
      File->Close (File);
      return FALSE;
    }

    FreePool (FileInfo);
    // it's a file - delete it
    //DBG (" File is file\n");
    Status = File->Delete (File);
    //DBG (" Delete: %r\n", Status);

    return (Status == EFI_SUCCESS);
  }

  return FALSE;
}

EFI_STATUS
DirNextEntry (
  IN      EFI_FILE        *Directory,
  IN OUT  EFI_FILE_INFO   **DirEntry,
  IN      UINTN           FilterMode
) {
  EFI_STATUS  Status;
  VOID        *Buffer;
  UINTN       LastBufferSize, BufferSize;
  INTN        IterCount;

  for (;;) {
    // free pointer from last call
    if (*DirEntry != NULL) {
      FreePool (*DirEntry);
      *DirEntry = NULL;
    }

    // read next directory entry
    LastBufferSize = BufferSize = AVALUE_MAX_SIZE;
    Buffer = AllocateZeroPool (BufferSize);

    for (IterCount = 0; ; IterCount++) {
      Status = Directory->Read (Directory, &BufferSize, Buffer);

      if ((Status != EFI_BUFFER_TOO_SMALL) || (IterCount >= 4)) {
        break;
      }

      if (BufferSize <= LastBufferSize) {
        DBG ("FS Driver requests bad buffer size %d (was %d), using %d instead\n",
          BufferSize, LastBufferSize, LastBufferSize * 2);

        BufferSize = LastBufferSize * 2;

#if REFIT_DEBUG > 0
      } else {
        DBG ("Reallocating buffer from %d to %d\n", LastBufferSize, BufferSize);
#endif
      }

      Buffer = EfiReallocatePool (Buffer, LastBufferSize, BufferSize);
      LastBufferSize = BufferSize;
    }

    if (EFI_ERROR (Status)) {
      FreePool (Buffer);
      break;
    }

    // check for end of listing
    if (BufferSize == 0) {  // end of directory listing
      FreePool (Buffer);
      break;
    }

    // entry is ready to be returned
    *DirEntry = (EFI_FILE_INFO *)Buffer;
    if (*DirEntry) {
      // filter results
      if (FilterMode == 1) {    // only return directories
        if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY)) {
          break;
        }
      } else if (FilterMode == 2) {   // only return files
        if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0) {
          break;
        }
      } else {  // no filter or unknown filter -> return everything
        break;
      }
    }
  }
  return Status;
}

VOID
DirIterOpen (
  IN  EFI_FILE        *BaseDir,
  IN  CHAR16          *RelativePath OPTIONAL,
  OUT REFIT_DIR_ITER  *DirIter
) {
  if (RelativePath == NULL) {
    DirIter->LastStatus = EFI_SUCCESS;
    DirIter->DirHandle = BaseDir;
    DirIter->CloseDirHandle = FALSE;
  } else {
    DirIter->LastStatus = BaseDir->Open (BaseDir, &(DirIter->DirHandle), RelativePath, EFI_FILE_MODE_READ, 0);
    DirIter->CloseDirHandle = EFI_ERROR (DirIter->LastStatus) ? FALSE : TRUE;
  }
  DirIter->LastFileInfo = NULL;
}

BOOLEAN
DirIterNext (
  IN OUT  REFIT_DIR_ITER  *DirIter,
  IN      UINTN           FilterMode,
  IN      CHAR16          *FilePattern OPTIONAL,
  OUT     EFI_FILE_INFO   **DirEntry
) {
  if (DirIter->LastFileInfo != NULL) {
    FreePool (DirIter->LastFileInfo);
    DirIter->LastFileInfo = NULL;
  }

  if (EFI_ERROR (DirIter->LastStatus)) {
    return FALSE;   // stop iteration
  }

  for (;;) {
    DirIter->LastStatus = DirNextEntry (DirIter->DirHandle, &(DirIter->LastFileInfo), FilterMode);
    if (EFI_ERROR (DirIter->LastStatus)) {
      return FALSE;// end of listing
    }

    if (DirIter->LastFileInfo == NULL) {
      return FALSE;
    }

    if (FilePattern != NULL) {
      if ((DirIter->LastFileInfo->Attribute & EFI_FILE_DIRECTORY)) {
        break;
      }

      if (MetaiMatch (DirIter->LastFileInfo->FileName, FilePattern)) {
        break;
      }
      // else continue loop
    } else {
      break;
    }
  }

  *DirEntry = DirIter->LastFileInfo;

  return TRUE;
}

EFI_STATUS
DirIterClose (
  IN OUT  REFIT_DIR_ITER    *DirIter
) {
  if (DirIter->LastFileInfo != NULL) {
    FreePool (DirIter->LastFileInfo);
    DirIter->LastFileInfo = NULL;
  }

  if (DirIter->CloseDirHandle) {
    DirIter->DirHandle->Close (DirIter->DirHandle);
  }

  return DirIter->LastStatus;
}

//
// Basic file operations
//

EFI_STATUS
LoadFile (
  IN  EFI_FILE_HANDLE   BaseDir,
  IN  CHAR16            *FileName,
  OUT UINT8             **FileData,
  OUT UINTN             *FileDataLength
) {
  EFI_STATUS        Status;
  EFI_FILE_HANDLE   FileHandle;
  EFI_FILE_INFO     *FileInfo;
  UINT64            ReadSize;
  UINTN             BufferSize;
  UINT8             *Buffer;

  Status = BaseDir->Open (BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  FileInfo = EfiLibFileInfo (FileHandle);

  if (FileInfo == NULL) {
    FileHandle->Close (FileHandle);
    return EFI_NOT_FOUND;
  }

  ReadSize = FileInfo->FileSize;

  if (ReadSize > MAX_FILE_SIZE) {
    ReadSize = MAX_FILE_SIZE;
  }

  FreePool (FileInfo);

  BufferSize = (UINTN)ReadSize;   // was limited to 1 GB above, so this is safe
  Buffer = (UINT8 *)AllocateZeroPool (BufferSize);
  if (Buffer == NULL) {
    FileHandle->Close (FileHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = FileHandle->Read (FileHandle, &BufferSize, Buffer);
  FileHandle->Close (FileHandle);

  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  *FileData = Buffer;
  *FileDataLength = BufferSize;

  return EFI_SUCCESS;
}

//there is assumed only one ESP partition. What if there are two HDD gpt formatted?
EFI_STATUS
FindESP (
  OUT EFI_FILE_HANDLE   *RootDir
) {
  EFI_STATUS    Status;
  UINTN         HandleCount = 0;
  EFI_HANDLE    *Handles;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPartTypeSystemPartGuid, NULL, &HandleCount, &Handles);
  if (!EFI_ERROR (Status) && (HandleCount > 0)) {
    *RootDir = EfiLibOpenRoot (Handles[0]);

    if (*RootDir == NULL) {
      Status = EFI_NOT_FOUND;
    }

    FreePool (Handles);
  }
  return Status;
}

//if (NULL, ...) then save to EFI partition
EFI_STATUS
SaveFile (
  IN EFI_FILE_HANDLE    BaseDir OPTIONAL,
  IN CHAR16             *FileName,
  IN UINT8              *FileData,
  IN UINTN              FileDataLength
) {
  EFI_STATUS        Status;
  EFI_FILE_HANDLE   FileHandle;
  UINTN             BufferSize;
  BOOLEAN           CreateNew = TRUE;

  if (BaseDir == NULL) {
    Status = FindESP (&BaseDir);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  // Delete existing file if it exists
  Status = BaseDir->Open (
                      BaseDir, &FileHandle, FileName,
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0
                    );

  if (!EFI_ERROR (Status)) {
    Status = FileHandle->Delete (FileHandle);

    if (Status == EFI_WARN_DELETE_FAILURE) {
      //This is READ_ONLY file system
      CreateNew = FALSE; // will write into existing file
    }
  }

  if (CreateNew) {
    // Write new file
    Status = BaseDir->Open (
                        BaseDir, &FileHandle, FileName,
                        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
                      );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    //to write into existing file we must sure it size larger then our data
    EFI_FILE_INFO *Info = EfiLibFileInfo (FileHandle);
    if (Info && Info->FileSize < FileDataLength) {
      return EFI_NOT_FOUND;
    }
  }

  if (!FileHandle) {
    return EFI_NOT_FOUND;
  }

  BufferSize = FileDataLength;
  Status = FileHandle->Write (FileHandle, &BufferSize, FileData);
  FileHandle->Close (FileHandle);

  return Status;
}

EFI_STATUS
MkDir (
  IN EFI_FILE_HANDLE    BaseDir OPTIONAL,
  IN CHAR16             *DirName
) {
  EFI_STATUS        Status;
  EFI_FILE_HANDLE   FileHandle;

  //DBG ("Looking up dir assets (%s):", DirName);

  if (BaseDir == NULL) {
    Status = FindESP (&BaseDir);

    if (EFI_ERROR (Status)) {
      //DBG (" %r\n", Status);
      return Status;
    }
  }

  Status = BaseDir->Open (
                      BaseDir, &FileHandle, DirName,
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, EFI_FILE_DIRECTORY
                    );

  if (EFI_ERROR (Status)) {
    // Write new dir
    //DBG ("%r, attempt to create one:", Status);
    Status = BaseDir->Open (
                        BaseDir, &FileHandle, DirName,
                        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, EFI_FILE_DIRECTORY
                      );
  }

  //DBG (" %r\n", Status);
  return Status;
}

//
// file name manipulation
//
BOOLEAN
MetaiMatch (
  IN CHAR16   *String,
  IN CHAR16   *Pattern
) {
  if (!mUnicodeCollation) {
    // quick fix for driver loading on UEFIs without UnicodeCollation
    //return FALSE;
    return TRUE;
  }

  return mUnicodeCollation->MetaiMatch (mUnicodeCollation, String, Pattern);
}

EFI_STATUS
InitializeUnicodeCollationProtocol () {
  EFI_STATUS  Status;

  if (mUnicodeCollation != NULL) {
    return EFI_SUCCESS;
  }

  //
  // BUGBUG: Proper impelmentation is to locate all Unicode Collation Protocol
  // instances first and then select one which support English language.
  // Current implementation just pick the first instance.
  //
  Status = gBS->LocateProtocol (
                  &gEfiUnicodeCollation2ProtocolGuid,
                  NULL,
                  (VOID **)&mUnicodeCollation
                );

  if (EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (
                    &gEfiUnicodeCollationProtocolGuid,
                    NULL,
                    (VOID **)&mUnicodeCollation
                  );
  }

  return Status;
}

CHAR16 *
Basename (
  IN CHAR16   *Path
) {
  CHAR16  *FileName = Path;

  if (Path != NULL) {
    UINTN   i, len = StrLen (Path);

    for (i = len; i > 0; i--) {
      if (Path[i - 1] == '\\' || Path[i - 1] == '/') {
        FileName = Path + i;
        break;
      }
    }
  }

  return FileName;
}

CHAR16 *
Dirname (
  IN CHAR16   *Path
) {
  CHAR16  *FileName = Path;

  if (Path != NULL) {
    UINTN   i, len = StrLen (Path);

    for (i = len; i >= 0; i--) {
      if ((FileName[i] == '\\') || (FileName[i] == '/')) {
        FileName[i] = 0;
        break;
      }
    }
  }

  return FileName;
}

CHAR16 *
ReplaceExtension (
  IN CHAR16    *Path,
  IN CHAR16    *Extension
) {
  CHAR16  *NewExtension = Extension ? EfiStrDuplicate (Extension) : L"";
  UINTN   i,
          len = StrSize (Path),
          slen = len + StrSize (NewExtension) + 1;

  Path = ReallocatePool (StrSize (Path), slen, Path);

  for (i = len; i >= 0; i--) {
    if (Path[i] == '.') {
      Path[i] = 0;
      break;
    }

    if ((Path[i] == '\\') || (Path[i] == '/')) {
      break;
    }
  }

  StrCatS (Path, slen, NewExtension);

  return Path;
}

CHAR16 *
FindExtension (
  IN CHAR16   *FileName
) {
  UINTN   i, len = StrLen (FileName);

  for (i = len; i >= 0; i--) {
    if (FileName[i] == '.') {
      return FileName + i + 1;
    }

    if ((FileName[i] == '/') || (FileName[i] == '\\')) {
      break;
    }
  }

  return FileName;
}

CHAR16 *
RemoveExtension (
  IN CHAR16    *Path
) {
  return ReplaceExtension (Path, NULL);
}

//
// memory string search
//

INTN
FindMem (
  IN VOID     *Buffer,
  IN UINTN    BufferLength,
  IN VOID     *SearchString,
  IN UINTN    SearchStringLength
) {
  UINT8   *BufferPtr;
  UINTN   Offset;

  BufferPtr = Buffer;
  BufferLength -= SearchStringLength;
  for (Offset = 0; Offset < BufferLength; Offset++, BufferPtr++) {
    if (CompareMem (BufferPtr, SearchString, SearchStringLength) == 0) {
      return (INTN)Offset;
    }
  }

  return -1;
}

CHAR8 *
SearchString (
  IN  CHAR8     *Source,
  IN  UINT64    SourceSize,
  IN  CHAR8     *Search,
  IN  UINTN     SearchSize
) {
  CHAR8   *End = Source + SourceSize;

  while (Source < End) {
    if (CompareMem (Source, Search, SearchSize) == 0) {
      return Source;
    } else {
      Source++;
    }
  }

  return NULL;
}

//
// Aptio UEFI returns File DevPath as 2 nodes (dir, file)
// and DevicePathToStr connects them with /, but we need '\\'
CHAR16 *
FileDevicePathToStr (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevPath
) {
  CHAR16    *FilePath, *Res = NULL;
  UINTN     Len, i = 0;
  BOOLEAN   Found = FALSE;

  FilePath = DevicePathToStr (DevPath);

  if (FilePath != NULL) {
    Len = StrSize (FilePath) + 1;
    Res = AllocatePool (Len);

    while (*FilePath != L'\0') {
      if (*FilePath == L'\\') {
        if (Found) {
          FilePath++;
          continue;
        }

        Res[i++] = *FilePath;
        Found = TRUE;
        FilePath++;
        continue;
      }

      Found = FALSE;

      Res[i++] = (*FilePath == L'/') ? L'\\' : *FilePath;
      FilePath++;
    }

    if (i <= Len) {
      Res[i] = '\0';
    }
  }

  return Res;
}

CHAR16 *
FileDevicePathFileToStr (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevPath
) {
  EFI_DEVICE_PATH_PROTOCOL  *Node;

  if (DevPath == NULL) {
    return NULL;
  }

  Node = (EFI_DEVICE_PATH_PROTOCOL *)DevPath;
  while (!IsDevicePathEnd (Node)) {
    if (
      (Node->Type == MEDIA_DEVICE_PATH) &&
      (Node->SubType == MEDIA_FILEPATH_DP)
    ) {
      return FileDevicePathToStr (Node);
    }

    Node = NextDevicePathNode (Node);
  }

  return NULL;
}

BOOLEAN
DumpVariable (
  CHAR16      *Name,
  EFI_GUID    *Guid,
  INTN        DevicePathAt
) {
  UINTN       DataSize = 0, i;
  UINT8       *Data = NULL;
  EFI_STATUS  Status;

  Status = gRT->GetVariable (Name, Guid, NULL, &DataSize, Data);

  if (Status == EFI_BUFFER_TOO_SMALL) {
    Data = AllocateZeroPool (DataSize);
    Status = gRT->GetVariable (Name, Guid, NULL, &DataSize, Data);
    if (EFI_ERROR (Status)) {
      DBG ("Can't get %s, size=%d\n", Name, DataSize);
      FreePool (Data);
      Data = NULL;
    } else {
      DBG ("%s var size=%d\n", Name, DataSize);
      for (i = 0; i < DataSize; i++) {
        DBG ("%02x ", Data[i]);
      }

      DBG ("\n");

      if (DevicePathAt >= 0) {
        DBG ("%s: %s\n", Name, FileDevicePathToStr ((EFI_DEVICE_PATH_PROTOCOL *)&Data[DevicePathAt]));
      }
    }
  }

  if (Data) {
    FreePool (Data);
    return TRUE;
  }

  return FALSE;
}

VOID
DbgHeader (
  CHAR8   *str
) {
  DebugLog (1, ":: %a\n", str);
  /*
  UINTN    len = 60 - AsciiStrSize (str);

  if (len <= 3) {
    DebugLog (1, "=== [ %a ] ===\n", str);
    return;
  } else {
    CHAR8   *fill = AllocateZeroPool (len);

    SetMem (fill, len, '=');
    fill[len] = '\0';
    DebugLog (1, "=== [ %a ] %a\n", str, fill);
    FreePool (fill);
  }
  */
}

BOOLEAN
BootArgsExists (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
) {
  CHAR16    *TmpOption = NULL;
  UINTN     Len = 0, i = 0, y = 0;
  BOOLEAN   Found = FALSE;

  if ((LoadOptions == NULL) || (LoadOption == NULL)) {
    return Found;
  }

  Len = StrLen (LoadOptions);

  if (!Len || !StrLen (LoadOption)) {
    return Found;
  }

  TmpOption = AllocatePool (Len);

  while ((i < Len) && (LoadOptions[i] != 0x0)) {
    ZeroMem (TmpOption, Len + 1);
    y = 0;

    while ((i < Len) && (LoadOptions[i] != 0x20) && (LoadOptions[i] != 0x0)) {
      TmpOption[y++] = LoadOptions[i++];
    }

    while (LoadOptions[i] == 0x20) {
      i++;
    }

    if (StriCmp (TmpOption, LoadOption) == 0) {
      Found = TRUE;
      break;
    }
  }

  FreePool (TmpOption);

  return Found;
}

//
//  BmLib.c
//

/**

  Find the first instance of this Protocol
  in the system and return it's interface.

  @param ProtocolGuid    Provides the protocol to search for
  @param Interface       On return, a pointer to the first interface
                         that matches ProtocolGuid

  @retval  EFI_SUCCESS      A protocol instance matching ProtocolGuid was found
  @retval  EFI_NOT_FOUND    No protocol instances were found that match ProtocolGuid

**/
EFI_STATUS
EfiLibLocateProtocol (
  IN  EFI_GUID    *ProtocolGuid,
  OUT VOID        **Interface
) {
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  ProtocolGuid,
                  NULL,
                  (VOID **)Interface
                );

  return Status;
}

/**

  Function opens and returns a file handle to the root directory of a volume.

  @param DeviceHandle    A handle for a device

  @return A valid file handle or NULL is returned

**/
EFI_FILE_HANDLE
EfiLibOpenRoot (
  IN EFI_HANDLE   DeviceHandle
) {
  EFI_STATUS                        Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_HANDLE                   File;

  File = NULL;

  //
  // File the file system interface to the device
  //
  Status = gBS->HandleProtocol (
                  DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&Volume
                );

  //
  // Open the root directory of the volume
  //
  if (!EFI_ERROR (Status)) {
    Status = Volume->OpenVolume (Volume, &File);
  }

  return EFI_ERROR (Status) ? NULL : File;
}

/**

  Function gets the file system information from an open file descriptor,
  and stores it in a buffer allocated from pool.

  @param FHand           The file handle.

  @return                A pointer to a buffer with file information.
  @retval                NULL is returned if failed to get Volume Label Info.

**/
EFI_FILE_SYSTEM_VOLUME_LABEL *
EfiLibFileSystemVolumeLabelInfo (
  IN EFI_FILE_HANDLE    FHand
) {
  EFI_STATUS                    Status;
  UINTN                         Size = 0;
  EFI_FILE_SYSTEM_VOLUME_LABEL  *VolumeInfo = NULL;

  Status = FHand->GetInfo (FHand, &gEfiFileSystemVolumeLabelInfoIdGuid, &Size, VolumeInfo);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // inc size by 2 because some drivers (HFSPlus.efi) do not count 0 at the end of file name
    Size += 2;
    VolumeInfo = AllocateZeroPool (Size);
    Status = FHand->GetInfo (FHand, &gEfiFileSystemVolumeLabelInfoIdGuid, &Size, VolumeInfo);
    // Check to make sure this isn't actually EFI_FILE_SYSTEM_INFO
    if (!EFI_ERROR (Status)) {
       EFI_FILE_SYSTEM_INFO *FSInfo = (EFI_FILE_SYSTEM_INFO *)VolumeInfo;
       if (FSInfo->Size == (UINT64)Size) {
          // Allocate a new volume label
          VolumeInfo = (EFI_FILE_SYSTEM_VOLUME_LABEL *)EfiStrDuplicate (FSInfo->VolumeLabel);
          FreePool (FSInfo);
       }
    }

    if (!EFI_ERROR (Status)) {
      return VolumeInfo;
    }

    FreePool (VolumeInfo);
  }

  return NULL;
}

/**

  Function gets the file information from an open file descriptor, and stores it
  in a buffer allocated from pool.

  @param FHand           File Handle.

  @return                A pointer to a buffer with file information or NULL is returned

**/
EFI_FILE_INFO *
EfiLibFileInfo (
  IN EFI_FILE_HANDLE    FHand
) {
  EFI_STATUS      Status;
  EFI_FILE_INFO   *FileInfo = NULL;
  UINTN           Size = 0;

  Status = FHand->GetInfo (FHand, &gEfiFileInfoGuid, &Size, FileInfo);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // inc size by 2 because some drivers (HFSPlus.efi) do not count 0 at the end of file name
    Size += 2;
    FileInfo = AllocateZeroPool (Size);
    Status = FHand->GetInfo (FHand, &gEfiFileInfoGuid, &Size, FileInfo);
  }

  return EFI_ERROR (Status) ? NULL : FileInfo;
}

EFI_FILE_SYSTEM_INFO *
EfiLibFileSystemInfo (
  IN EFI_FILE_HANDLE    FHand
) {
  EFI_STATUS              Status;
  UINTN                   Size = 0;
  EFI_FILE_SYSTEM_INFO    *FileSystemInfo = NULL;

  Status = FHand->GetInfo (FHand, &gEfiFileSystemInfoGuid, &Size, FileSystemInfo);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // inc size by 2 because some drivers (HFSPlus.efi) do not count 0 at the end of file name
    Size += 2;
    FileSystemInfo = AllocateZeroPool (Size);
    Status = FHand->GetInfo (FHand, &gEfiFileSystemInfoGuid, &Size, FileSystemInfo);
  }

  return EFI_ERROR (Status) ? NULL : FileSystemInfo;
}

EFI_DEVICE_PATH_PROTOCOL *
EfiLibPathInfo (
  IN EFI_FILE_HANDLE    FHand,
  IN EFI_GUID           *InformationType
) {
  EFI_STATUS                  Status;
  UINTN                       Size = 0;
  EFI_DEVICE_PATH_PROTOCOL    *PathInfo = NULL;

  Status = FHand->GetInfo (FHand, InformationType, &Size, PathInfo);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // inc size by 2 because some drivers (HFSPlus.efi) do not count 0 at the end of file name
    Size += 2;
    PathInfo = AllocateZeroPool (Size);
    Status = FHand->GetInfo (FHand, InformationType, &Size, PathInfo);
  }

  return EFI_ERROR (Status) ? NULL : PathInfo;
}

/**
  Function is used to determine the number of device path instances
  that exist in a device path.

  @param DevicePath      A pointer to a device path data structure.

  @return This function counts and returns the number of device path instances
          in DevicePath.
**/

UINTN
EfiDevicePathInstanceCount (
  IN EFI_DEVICE_PATH_PROTOCOL      *DevicePath
) {
  UINTN   Count = 0, Size;

  while (GetNextDevicePathInstance (&DevicePath, &Size) != NULL) {
    Count += 1;
  }

  return Count;
}

INTN
TimeCompare (
  IN EFI_TIME   *Time1,
  IN EFI_TIME   *Time2
) {
 INTN Comparison;

 if (Time1 == NULL) {
   return (Time2 == NULL) ? 0 : -1;
 } else if (Time2 == NULL) {
   return 1;
 }

 Comparison = Time1->Year - Time2->Year;
 if (Comparison == 0) {
   Comparison = Time1->Month - Time2->Month;
   if (Comparison == 0) {
     Comparison = Time1->Day - Time2->Day;
     if (Comparison == 0) {
       Comparison = Time1->Hour - Time2->Hour;
       if (Comparison == 0) {
         Comparison = Time1->Minute - Time2->Minute;
         if (Comparison == 0) {
           Comparison = Time1->Second - Time2->Second;
           if (Comparison == 0) {
             Comparison = Time1->Nanosecond - Time2->Nanosecond;
           }
         }
       }
     }
   }
 }

 return Comparison;
}

/** Returns TRUE is Str is ascii Guid in format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
BOOLEAN
IsValidGuidAsciiString (
  IN CHAR8    *Str
) {
  UINTN   Index;

  if ((Str == NULL) || (AsciiStrLen (Str) != 36)) {
    return FALSE;
  }

  for (Index = 0; Index < 36; Index++, Str++) {
    if ((Index == 8) || (Index == 13) || (Index == 18) || (Index == 23)) {
      if (*Str != '-') {
        return FALSE;
      }
    } else {
      if (!(
            (*Str >= '0' && *Str <= '9') ||
            (TO_UPPER (*Str) >= 'A' && TO_UPPER (*Str) <= 'F')
           )
      ) {
        return FALSE;
      }
    }
  }

  return TRUE;
}

//
// IO
//

// input - tsc
// output - milliseconds
// the caller is responsible for t1 > t0
UINT64
TimeDiff (
  UINT64  t0,
  UINT64  t1
) {
  return DivU64x64Remainder ((t1 - t0), DivU64x32 (gCPUStructure.TSCFrequency, 1000), 0);
}

//set Timeout in ms
EFI_STATUS
WaitFor2EventWithTsc (
  IN  EFI_EVENT   Event1,
  IN  EFI_EVENT   Event2,
  IN  UINT64      Timeout OPTIONAL
) {
  EFI_STATUS      Status;
  UINTN           Index;
  EFI_EVENT       WaitList[2];
  UINT64          t0, t1,
                  Delta = DivU64x64Remainder (MultU64x64 (Timeout, gCPUStructure.TSCFrequency), 1000, NULL);

  if (Timeout != 0) {
    t0 = AsmReadTsc ();

    do {
      Status = gBS->CheckEvent (Event1);
      if (!EFI_ERROR (Status)) {
        break;
      }

      if (Event2) {
        Status = gBS->CheckEvent (Event2);
        if (!EFI_ERROR (Status)) {
          break;
        }
      }

      // Let's try to relax processor a bit
      CpuPause ();
      Status = EFI_TIMEOUT;
      t1 = AsmReadTsc ();
    } while ((t1 - t0) < Delta);
  } else {
    WaitList[0] = Event1;
    WaitList[1] = Event2;
    Status = gBS->WaitForEvent (2, WaitList, &Index);
  }

  return Status;
}

// TimeoutDefault for a wait in seconds
// return EFI_TIMEOUT if no inputs
EFI_STATUS
WaitForInputEventPoll (
  REFIT_MENU_SCREEN   *Screen,
  UINTN               TimeoutDefault
) {
  EFI_STATUS    Status = EFI_SUCCESS;
  UINTN         TimeoutRemain = TimeoutDefault * 100;

  while (TimeoutRemain != 0) {
    //Status = WaitForSingleEvent (gST->ConIn->WaitForKey, ONE_MSECOND * 10);
    Status = WaitFor2EventWithTsc (gST->ConIn->WaitForKey, NULL, 10);

    if (Status != EFI_TIMEOUT) {
      break;
    }

    UpdateAnime (Screen, &(Screen->FilmPlace));

    /*
    if ((INTN)gItemID < Screen->EntryCount) {
      UpdateAnime (Screen->Entries[gItemID]->SubScreen, &(Screen->Entries[gItemID]->Place));
    }
    */

    TimeoutRemain--;
  }

  return Status;
}

EFI_STATUS
SetupBooterLog () {
  EFI_STATUS    Status = EFI_SUCCESS;

  Status = LogDataHub (
              &gEfiMiscSubClassGuid,
              PoolPrint (L"%a", DATAHUB_LOG),
              AllocateZeroPool (MEM_LOG_INITIAL_SIZE),
              (UINT32)MEM_LOG_INITIAL_SIZE
            );

  return Status;
}

// Made msgbuf and msgCursor private to this source
// so we need a different way of saving the msg log - apianti
EFI_STATUS
SaveBooterLog (
  IN EFI_FILE_HANDLE    BaseDir OPTIONAL,
  IN CHAR16             *FileName
) {
  CHAR8         *MemLogBuffer = GetMemLogBuffer ();
  UINTN         MemLogLen = GetMemLogLen ();

  if ((MemLogBuffer == NULL) || (MemLogLen == 0)) {
    return EFI_NOT_FOUND;
  }

  return SaveFile (BaseDir, FileName, (UINT8 *)MemLogBuffer, MemLogLen);
}

STATIC
VOID
DestroyGeneratedACPI () {
  REFIT_DIR_ITER      DirIter;
  EFI_FILE_INFO       *DirEntry;
  CHAR16              *AcpiPath = AllocateZeroPool (SVALUE_MAX_SIZE);
  UINTN               PathIndex, PathCount = ARRAY_SIZE (SupportedOsType);

  //DbgHeader ("DestroyGeneratedAML");

  for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
    AcpiPath = PoolPrint (DIR_ACPI_PATCHED L"\\%s", SupportedOsType[PathIndex]);

    DirIterOpen (SelfRootDir, AcpiPath, &DirIter);

    while (DirIterNext (&DirIter, 2, L"*.aml", &DirEntry)) {
      if (
        (DirEntry->FileName[0] != L'.') &&
        (
          (StriStr (DirEntry->FileName, DSDT_CLOVER_PREFIX) != NULL) ||
          (StriStr (DirEntry->FileName, SSDT_CLOVER_PREFIX) != NULL)
        )
      ) {
        DeleteFile (SelfRootDir, PoolPrint (L"%s\\%s", AcpiPath, DirEntry->FileName));
      }
    }

    DirIterClose (&DirIter);

    FreePool (AcpiPath);
  }
}

VOID
ResetClover () {
  ClearScreen (&BlueBackgroundPixel);
  gBS->Stall (1 * 1000000);
  ClearScreen (&GreenBackgroundPixel);
  gBS->Stall (1 * 1000000);
  ClearScreen (&RedBackgroundPixel);
  gBS->Stall (1 * 1000000);

  DestroyGeneratedACPI ();
  ResetNvram ();
}

// EOF
