/*
 *  Hibernate.c
 *
 *  Created by dmazar, 01.2014.
 *
 *  Hibernate support.
 *
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_HIBERNATE
#define DEBUG_HIBERNATE -1
#endif
#else
#ifdef DEBUG_HIBERNATE
#undef DEBUG_HIBERNATE
#endif
#define DEBUG_HIBERNATE DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_HIBERNATE, __VA_ARGS__)

#include <Library/Common/Hibernate.h>

INT32
MacToPosix (
  UINT32  MacTime
) {
  // Mac time is 1904 year based
  return MacTime ?  MacTime - 2082844800 : 0;
}

EFI_BLOCK_READ  OrigBlockIoRead = NULL;
UINT64          gSleepImageOffset = 0;
UINT32          gSleepTime = 0;

/** BlockIo->Read () override. */
EFI_STATUS
EFIAPI
OurBlockIoRead (
  IN EFI_BLOCK_IO_PROTOCOL    *This,
  IN UINT32                   MediaId,
  IN EFI_LBA                  Lba,
  IN UINTN                    BufferSize,
  OUT VOID                    *Buffer
) {
  EFI_STATUS    Status = OrigBlockIoRead (This, MediaId, Lba, BufferSize, Buffer);

  // Enter special processing only when gSleepImageOffset == 0, to avoid recursion when Boot/Log=true
  if (
    (gSleepImageOffset == 0) &&
    (Status == EFI_SUCCESS) &&
    (BufferSize >= sizeof (IOHibernateImageHeader))
  ) { //sizeof (IOHibernateImageHeaderMin)==96
    UINT32    BlockSize = (This->Media != NULL) ? This->Media->BlockSize : 512;

    DBG (" OurBlockIoRead: Lba=%lx, Offset=%lx (BlockSize=%d)\n", Lba, MultU64x32 (Lba, BlockSize), BlockSize);

    IOHibernateImageHeader *Header = (IOHibernateImageHeader *)Buffer;

    // Mark that we are executing, to avoid entering above phrase again, and don't add DBGs outside this scope, to avoid recursion
    gSleepImageOffset = (UINT64)-1;

    //DBG (" sig lion: %x\n", Header->signature);
    // DBG (" sig swap: %x\n", SwapBytes32 (Header->signature));

    if (Header->signature == kIOHibernateHeaderSignature) {
      gSleepImageOffset = MultU64x32 (Lba, BlockSize);

      DBG (" got sleep image offset\n");

      //save sleep time as lvs1974 suggested
      gSleepTime = (Header->signature == kIOHibernateHeaderSignature) ? Header->sleepTime : 0;

      // return invalid parameter in case of success in order to prevent driver from caching our buffer
      return EFI_INVALID_PARAMETER;
    } else {
      DBG (" no valid sleep image offset was found\n");
      gSleepImageOffset = 0;
    }
  }

  return Status;
}

/** Get slep image location (volume and name) */
VOID
GetSleepImageLocation (
  IN REFIT_VOLUME   *Volume,
  REFIT_VOLUME      **SleepImageVolume,
  CHAR16            **SleepImageName
) {
  EFI_STATUS      Status;
  UINT8           *PrefBuffer = NULL;
  UINTN           PrefBufferLen = 0;
  TagPtr          PrefDict, Dict, Dict2, Prop;
  CHAR16          *PrefName = L"\\Library\\Preferences\\SystemConfiguration\\com.apple.PowerManagement.plist",
                  *ImageName = NULL;
  REFIT_VOLUME    *ImageVolume = Volume;

  // find sleep image entry from plist
  Status = LoadFile (Volume->RootDir, PrefName, &PrefBuffer, &PrefBufferLen);
  DBG ("    read prefs %s status=%r\n", PrefName, Status);

  if (!EFI_ERROR (Status)) {
    Status = ParseXML ((CHAR8 *)PrefBuffer, 0, &PrefDict);
    if (!EFI_ERROR (Status)) {
      Dict = GetProperty (PrefDict, "Custom Profile");
      if (Dict) {
        Dict2 = GetProperty (Dict, "AC Power");
        if (Dict2) {
          Prop = GetProperty (Dict2, "Hibernate File");
          if ((Prop != NULL) && (Prop->type == kTagTypeString)) {
            CHAR16    *p;

            if (AsciiStrStr (Prop->string, "/Volumes/")) {
              CHAR8     *VolNameStart = NULL, *VolNameEnd = NULL;
              CHAR16    *VolName = NULL;
              UINTN     VolNameSize = 0;

              // Extract Volumes Name
              VolNameStart = AsciiStrStr (Prop->string + 1, "/") + 1;
              if (VolNameStart) {
                VolNameEnd = AsciiStrStr (VolNameStart, "/");
                if (VolNameEnd) {
                  VolNameSize = (VolNameEnd - VolNameStart + 1) * sizeof (CHAR16);
                  if (VolNameSize > 0) {
                    VolName = AllocateZeroPool (VolNameSize);
                  }
                }
              }

              if (VolName) {
                AsciiStrToUnicodeStrS (VolNameStart, VolName, VolNameSize);
                ImageVolume = FindVolumeByName (VolName);

                if (ImageVolume) {
                  ImageName = PoolPrint (L"%a", VolNameEnd);
                } else {
                  ImageVolume = Volume;
                }

                FreePool (VolName);
              }
            } else if (AsciiStrStr (Prop->string, "/var") && !AsciiStrStr (Prop->string, "private")) {
              ImageName = PoolPrint (L"\\private%a", Prop->string);
            } else {
              ImageName = PoolPrint (L"%a", Prop->string);
            }

            p = ImageName;
            while (*p) {
              if (*p == L'/') {
                *p = L'\\';
              }
              p++;
            }

            DBG ("    SleepImage name from pref: ImageVolume = '%s', ImageName = '%s'\n", ImageVolume->VolName, ImageName);
          }
        }
      }
    }
  }

  if (!ImageName) {
    ImageName = PoolPrint (L"\\private\\var\\vm\\sleepimage");
    DBG ("    using default sleep image name = %s\n", ImageName);
  }

  if (PrefBuffer) {
    FreePool (PrefBuffer); //allocated by LoadFile
  }

  *SleepImageVolume = ImageVolume;
  *SleepImageName = ImageName;
}

/** Returns byte offset of sleepimage on the whole disk or 0 if not found or error.
 *
 * To avoid messing with HFS+ format, we'll use the trick with overriding
 * BlockIo->Read () of the disk and then read first bytes of the sleepimage
 * through file system driver. And then we'll detect block delivered by BlockIo
 * and calculate position from there.
 * It's for hack after all :)
 */
UINT64
GetSleepImagePosition (
  IN REFIT_VOLUME     *Volume,
  REFIT_VOLUME        **SleepImageVolume
) {
  EFI_STATUS          Status;
  EFI_FILE            *File;
  VOID                *Buffer;
  UINTN               BufferSize = 512;
  CHAR16              *ImageName;
  REFIT_VOLUME        *ImageVolume;

  if (!Volume) {
    DBG ("    no volume to get sleepimage\n");
    return 0;
  }

  if (Volume->WholeDiskBlockIO == NULL) {
    DBG ("    no disk BlockIo\n");
    return 0;
  }

  // If IsSleepImageValidBySignature () was used, then we already have that offset
  if (Volume->SleepImageOffset != 0) {
    if (SleepImageVolume != NULL) {
      // Update caller's SleepImageVolume when requested
      GetSleepImageLocation (Volume, SleepImageVolume, &ImageName);
    }

    DBG ("    returning previously calculated offset: %lx\n", Volume->SleepImageOffset);
    return Volume->SleepImageOffset;
  }

  // Get sleepimage name and volume
  GetSleepImageLocation (Volume,&ImageVolume,&ImageName);

  // Open sleepimage
  Status = ImageVolume->RootDir->Open (ImageVolume->RootDir, &File, ImageName, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    DBG ("    sleepimage not found -> %r\n", Status);
    return 0;
  }

  // We want to read the first 512 bytes from sleepimage
  Buffer = AllocatePool (BufferSize);
  if (Buffer == NULL) {
    DBG ("    could not allocate buffer for sleepimage\n");
    return 0;
  }

  DBG ("    Reading first %d bytes of sleepimage ...\n", BufferSize);

  // Override disk BlockIo
  OrigBlockIoRead = ImageVolume->WholeDiskBlockIO->ReadBlocks;
  ImageVolume->WholeDiskBlockIO->ReadBlocks = OurBlockIoRead;
  gSleepImageOffset = 0; //used as temporary global variable to pass our value
  Status = File->Read (File, &BufferSize, Buffer);

  // Restore original disk BlockIo
  ImageVolume->WholeDiskBlockIO->ReadBlocks = OrigBlockIoRead;

  // OurBlockIoRead always returns invalid parameter in order to avoid driver caching, so that is a good value
  if (Status == EFI_INVALID_PARAMETER) {
    Status = EFI_SUCCESS;
  }

  DBG ("    Reading completed -> %r\n", Status);

  // Close sleepimage
  File->Close (File);

  // We don't use the buffer, as actual signature checking is being done by OurBlockIoRead
  if (Buffer) {
    FreePool (Buffer);
  }

  if (EFI_ERROR (Status)) {
    DBG ("     can not read sleepimage -> %r\n", Status);
    return 0;
  }

  // We store SleepImageOffset, in case our BlockIoRead does not execute again on next read due to driver caching.
  if (gSleepImageOffset != 0) {
    DBG ("     sleepimage offset acquired successfully: %lx\n", gSleepImageOffset);
    ImageVolume->SleepImageOffset = gSleepImageOffset;
  } else {
    DBG ("     sleepimage offset could not be acquired\n");
  }

  if (SleepImageVolume != NULL) {
    // Update caller's SleepImageVolume when requested
    *SleepImageVolume = ImageVolume;
  }

  return gSleepImageOffset;
}

/** Returns TRUE if /private/var/vm/sleepimage exists
 *  and it's modification time is close to volume modification time).
 */
BOOLEAN
IsSleepImageValidBySleepTime (
  IN REFIT_VOLUME     *Volume
) {
  EFI_STATUS                Status;
  VOID                      *Buffer;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;
  HFSPlusVolumeHeaderMin    *HFSHeader;
  UINT32                    HFSVolumeModifyDate;
  INTN                      TimeDiff, Pages; // = 1;
  //EFI_TIME                ImageModifyTime;
  //EFI_TIME                *TimePtr;
  //EFI_TIME                HFSVolumeModifyTime;

  DBG ("     gSleepTime: %d\n", gSleepTime);
  //fsw_efi_decode_time (&ImageModifyTime, gSleepTime);
  //TimePtr = &ImageModifyTime;
  //DBG (" in EFI: %d-%d-%d %d:%d:%d\n", TimePtr->Year, TimePtr->Month, TimePtr->Day, TimePtr->Hour, TimePtr->Minute, TimePtr->Second);

  //
  // Get HFS+ volume nodification time
  //
  // use 4KB aligned page to avoid possible issues with BlockIo buffer alignment
  BlockIo = Volume->BlockIO;
  Pages = EFI_SIZE_TO_PAGES (BlockIo->Media->BlockSize);
  Buffer = AllocatePages (Pages);
  if (Buffer == NULL) {
    return FALSE;
  }

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, 2, BlockIo->Media->BlockSize, Buffer);
  if (EFI_ERROR (Status)) {
    DBG ("     can not read HFS+ header -> %r\n", Status);
    FreePages (Buffer, Pages);
    return FALSE;
  }

  HFSHeader = (HFSPlusVolumeHeaderMin *)Buffer;
  HFSVolumeModifyDate = SwapBytes32 (HFSHeader->modifyDate);
  HFSVolumeModifyDate = MacToPosix (HFSVolumeModifyDate);

  DBG ("     HFS+ volume modifyDate: %d\n", HFSVolumeModifyDate);

  //fsw_efi_decode_time (&HFSVolumeModifyTime, MacToPosix (HFSVolumeModifyDate));
  //TimePtr = &HFSVolumeModifyTime;
  //DBG (" in EFI: %d-%d-%d %d:%d:%d\n", TimePtr->Year, TimePtr->Month, TimePtr->Day, TimePtr->Hour, TimePtr->Minute, TimePtr->Second);
  FreePages (Buffer, Pages);

  //
  // Check that sleepimage is not more then 5 secs older then volume modification date
  // Idea is from Chameleon
  //
  TimeDiff = HFSVolumeModifyDate - (INTN)gSleepTime;

  DBG ("     image older then volume: %d sec\n", TimeDiff);

  if (TimeDiff > 5 /*|| TimeDiff < -5 */) {
    //Slice - if image newer then volume it should be OK
    DBG ("     image too old\n");
    return FALSE;
  }

  return TRUE;
}

/** Returns TRUE if /private/var/vm/sleepimage exists
 *  and it's signature is kIOHibernateHeaderSignature.
 */
BOOLEAN
IsSleepImageValidBySignature (
  IN REFIT_VOLUME     *Volume
) {
  // We'll have to detect offset here also in case driver caches
  // some data and stops us from detecting offset later.
  // So, make first call to GetSleepImagePosition () now.
  DBG ("    Check sleep image 'by signature':\n");
  return (GetSleepImagePosition (Volume, NULL) != 0);
}

/** Returns TRUE if given OSX on given volume is hibernated. */
BOOLEAN
IsDarwinHibernated (
  IN REFIT_VOLUME     *Volume
) {
  EFI_STATUS          Status;
  UINTN               Size = 0;
  UINT8               *Data = NULL;

  //if sleep image is good but OSX was not hibernated.
  //or we choose "cancel hibernate wake" then it must be canceled
  if (gSettings.NeverHibernate) {
    DBG ("        hibernated: set as never\n");
    return FALSE;
  }

  DBG ("        Check if volume Is Hibernated:\n");

  // CloverEFI or UEFI with EmuVariable
  if (IsSleepImageValidBySignature (Volume)) {
    if ((gSleepTime == 0) || IsSleepImageValidBySleepTime (Volume)) {
      DBG ("        hibernated: yes\n");
    } else {
      DBG ("        hibernated: no - time\n");
      return FALSE;
    }
  } else {
    DBG ("        hibernated: no - sign\n");
    return FALSE;
  }

  DBG ("        UEFI with NVRAM: ");
  Status = gRT->GetVariable (L"Boot0082", &gEfiGlobalVariableGuid, NULL, &Size, Data);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    DBG ("yes\n");
    return TRUE;
  }

  DBG ("Boot0082 no\n");
  return FALSE;
}

/** Prepares nvram vars needed for boot.efi to wake from hibernation:
 *  boot-switch-vars and boot-image.
 *
 * Normally those vars should be set by kernel
 * boot-switch-vars: structure with image encription key
 * boot-image: device path like Acpi (PNP0A03,0)/Pci (1f,2)/Sata (2,0)/File (56b99e000)
 *  where Acpi (PNP0A03,0)/Pci (1f,2)/Sata (2,0) points to the disk containing sleepimage
 *  and File (56b99e000) contains hex position (in bytes) of the beginning of the sleepimage
 *
 * Since boot-switch-vars is not present in CloverEFI or with EmuVar driver (no real NVRAM) but also not on UEFI hack
 * (not written by the kernel for some reason), and since boot-image is also not present in CloverEFI
 * and on UEFI hack device path as set by kernel can be different in some bytes from the device path
 * reported by UEFI, we'll compute and set both vars here.
 *
 * That's the only way for CloverEFI and should be OK for UEFI hack also.
 */

BOOLEAN
PrepareHibernation (
  IN REFIT_VOLUME     *Volume
) {
  EFI_STATUS                  Status;
  UINT64                      SleepImageOffset;
  CHAR16                      OffsetHexStr[17];
  EFI_DEVICE_PATH_PROTOCOL    *BootImageDevPath;
  UINTN                       Size;
  VOID                        *Value;
  UINT8                       *VarData = NULL;
  REFIT_VOLUME                *SleepImageVolume;

  DBG ("PrepareHibernation:\n");

  // Find sleep image offset
  SleepImageOffset = GetSleepImagePosition (Volume, &SleepImageVolume);

  DBG (" SleepImageOffset: %lx\n", SleepImageOffset);

  if ((SleepImageOffset == 0) || (SleepImageVolume == NULL)) {
    DBG (" sleepimage offset not found\n");
    return FALSE;
  }

  // Set boot-image var
  UnicodeSPrint (OffsetHexStr, sizeof (OffsetHexStr), L"%lx", SleepImageOffset);
  BootImageDevPath = FileDevicePath (SleepImageVolume->WholeDiskDeviceHandle, OffsetHexStr);
  //DBG (" boot-image device path:\n");
  Size = GetDevicePathSize (BootImageDevPath);
  VarData = (UINT8 *)BootImageDevPath;
  //PrintBytes (VarData, Size);
  DBG ("boot-image before: %s\n", FileDevicePathToStr (BootImageDevPath));
  //VarData[6] = 8;

  //VarData[24] = 0xFF;
  //VarData[25] = 0xFF;
  //DBG ("boot-image corrected: %s\n", FileDevicePathToStr (BootImageDevPath));

  Status = gRT->SetVariable (
                  L"boot-image", &gEfiAppleBootGuid,
                  NVRAM_ATTR_RT_BS_NV,
                  Size , BootImageDevPath
                );

  if (EFI_ERROR (Status)) {
    DBG (" can not write boot-image -> %r\n", Status);
    return FALSE;
  }

  // now we should delete boot0082 to do hibernate only once
  //Status = DeleteBootOption (0x82);
  //if (EFI_ERROR (Status)) {
  //  DBG ("Options 0082 was not deleted: %r\n", Status);
  //}

  // if boot-switch-vars exists (NVRAM working), then use it
  Status = GetVariable2 (L"boot-switch-vars", &gEfiAppleBootGuid, &Value, &Size);
  if (!EFI_ERROR (Status)) {
    // leave it as is
    DBG (" boot-switch-vars present\n");
    return TRUE;
  }

  // if IOHibernateRTCVariables exists (NVRAM working), then copy it to boot-switch-vars
  // else (no NVRAM) set boot-switch-vars to dummy one
  Status = GetVariable2 (L"IOHibernateRTCVariables", &gEfiAppleBootGuid, &Value, &Size);
  if (!EFI_ERROR (Status)) {
    DBG (" IOHibernateRTCVariables found - will be used as boot-switch-vars\n");
    // delete IOHibernateRTCVariables
    Status = gRT->SetVariable (
                    L"IOHibernateRTCVariables", &gEfiAppleBootGuid,
                    NVRAM_ATTR_RT_BS_NV,
                    0, NULL
                  );

    if (!EFI_ERROR (Status)) {
      Status = gRT->SetVariable (
                      L"boot-switch-vars", &gEfiAppleBootGuid,
                      NVRAM_ATTR_RT_BS_NV,
                      Size, Value
                    );

      if (EFI_ERROR (Status)) {
        DBG (" can not write boot-switch-vars -> %r\n", Status);
      }
    } else {
      DBG (" can not write IOHibernateRTCVariables -> %r\n", Status);
    }
  }

  return !EFI_ERROR (Status);
}
