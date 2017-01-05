/**

  Various helper functions.

  by dmazar

**/

#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include <Protocol/DiskIo.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/SimpleFileSystem.h>

#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>

//#include "NVRAMDebug.h"
#include "Lib.h"


// DBG_TO: 0=no debug, 1=serial, 2=console
// serial requires
// [PcdsFixedAtBuild]
//  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x07
//  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0xFFFFFFFF
// in package DSC file
#define DBG_TO 0

#if DBG_TO == 2
  #define DBG(...) AsciiPrint(__VA_ARGS__);
#elif DBG_TO == 1
  #define DBG(...) DebugPrint(1, __VA_ARGS__);
#else
  #define DBG(...)
#endif


CHAR16 *EfiMemoryTypeDesc[EfiMaxMemoryType] = {
  L"reserved",
  L"LoaderCode",
  L"LoaderData",
  L"BS_code",
  L"BS_data",
  L"RT_code",
  L"RT_data",
  L"available",
  L"Unusable",
  L"ACPI_recl",
  L"ACPI_NVS",
  L"MemMapIO",
  L"MemPortIO",
  L"PAL_code"
};

CHAR16 *EfiAllocateTypeDesc[MaxAllocateType] = {
  L"AllocateAnyPages",
  L"AllocateMaxAddress",
  L"AllocateAddress"
};

CHAR16 *EfiLocateSearchType[] = {
  L"AllHandles",
  L"ByRegisterNotify",
  L"ByProtocol"
};

VOID EFIAPI
FixMemMap (
  IN UINTN                  MemoryMapSize,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion
) {
  UINTN                   NumEntries, Index, BlockSize, PhysicalEnd;
  EFI_MEMORY_DESCRIPTOR   *Desc;

  //DBG ("FixMemMap: Size=%d, Addr=%p, DescSize=%d\n", MemoryMapSize, MemoryMap, DescriptorSize);

  Desc = MemoryMap;
  NumEntries = MemoryMapSize / DescriptorSize;

  for (Index = 0; Index < NumEntries; Index++) {
    BlockSize = EFI_PAGES_TO_SIZE ((UINTN)Desc->NumberOfPages);
    PhysicalEnd = Desc->PhysicalStart + BlockSize;

    //
    // Some UEFIs end up with "reserved" area with EFI_MEMORY_RUNTIME flag set
    // when Intel HD3000 or HD4000 is used. We will remove that flag here.
    //
    if ((Desc->Attribute & EFI_MEMORY_RUNTIME) != 0 && Desc->Type == EfiReservedMemoryType) {
      //EfiMemoryTypeDesc[Desc->Type], Desc->PhysicalStart, Desc->NumberOfPages, Desc->Attribute);
      Desc->Attribute = Desc->Attribute & (~EFI_MEMORY_RUNTIME);

      /* This one is not working - blocks during DefragmentRuntimeServices ()
      Desc->Type = EfiMemoryMappedIO;
      */

      /* Another possible solution - mark the range as MMIO.
      Desc->Type = EfiRuntimeServicesData;
      */
    }

    //
        // Fix by Slice - fixes sleep/wake on GB boards.
        //
    //    if ((Desc->PhysicalStart >= 0x9e000) && (Desc->PhysicalStart < 0xa0000)) {
    if ((Desc->PhysicalStart < 0xa0000) && (PhysicalEnd >= 0x9e000)) {
      Desc->Type = EfiACPIMemoryNVS;
      Desc->Attribute = 0;
    }


#if 0//DBG_TO
    //
    // Also do some checking
    //
    if ((Desc->Attribute & EFI_MEMORY_RUNTIME) != 0) {
      //
      // block with RT flag.
      // if it is not RT or MMIO, then report to log
      //
      if (Desc->Type != EfiRuntimeServicesCode
        && Desc->Type != EfiRuntimeServicesData
        && Desc->Type != EfiMemoryMappedIO
        && Desc->Type != EfiMemoryMappedIOPortSpace
        )
      {
        DBG (" %s with RT flag: %lx (0x%x) - ???\n", EfiMemoryTypeDesc[Desc->Type], Desc->PhysicalStart, Desc->NumberOfPages);
      }
    } else {
      //
      // block without RT flag.
      // if it is RT or MMIO, then report to log
      //
      if (Desc->Type == EfiRuntimeServicesCode
        || Desc->Type == EfiRuntimeServicesData
        || Desc->Type == EfiMemoryMappedIO
        || Desc->Type == EfiMemoryMappedIOPortSpace
        )
      {
        DBG (" %s without RT flag: %lx (0x%x) - ???\n", EfiMemoryTypeDesc[Desc->Type], Desc->PhysicalStart, Desc->NumberOfPages);
      }
    }

#endif
    Desc = NEXT_MEMORY_DESCRIPTOR (Desc, DescriptorSize);
  }
}

VOID
EFIAPI
ShrinkMemMap (
  IN UINTN                  *MemoryMapSize,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion
) {
  UINTN                   SizeFromDescToEnd;
  UINT64                  Bytes;
  EFI_MEMORY_DESCRIPTOR   *PrevDesc, *Desc;
  BOOLEAN                 CanBeJoined, HasEntriesToRemove;

  PrevDesc = MemoryMap;
  Desc = NEXT_MEMORY_DESCRIPTOR (PrevDesc, DescriptorSize);
  SizeFromDescToEnd = *MemoryMapSize - DescriptorSize;
  *MemoryMapSize = DescriptorSize;
  HasEntriesToRemove = FALSE;
  while (SizeFromDescToEnd > 0) {
    Bytes = (((UINTN) PrevDesc->NumberOfPages) * EFI_PAGE_SIZE);
    CanBeJoined = FALSE;
    if (
      (Desc->Attribute == PrevDesc->Attribute) &&
      (PrevDesc->PhysicalStart + Bytes == Desc->PhysicalStart)
    ) {
      if (
        (Desc->Type == EfiBootServicesCode)
        || (Desc->Type == EfiBootServicesData)
        //|| (Desc->Type == EfiConventionalMemory)
        //|| (Desc->Type == EfiLoaderCode)
        //|| (Desc->Type == EfiLoaderData)
      ) {
        CanBeJoined = (
                  (PrevDesc->Type == EfiBootServicesCode)
                  || (PrevDesc->Type == EfiBootServicesData)
                  //|| (PrevDesc->Type == EfiConventionalMemory)
                  //|| (PrevDesc->Type == EfiLoaderCode)
                  //|| (PrevDesc->Type == EfiLoaderData)
                )
          ;
      }
    }

    if (CanBeJoined) {
      // two entries are the same/similar - join them
      PrevDesc->NumberOfPages += Desc->NumberOfPages;
      HasEntriesToRemove = TRUE;
    } else {
      // can not be joined - we need to move to next
      *MemoryMapSize += DescriptorSize;
      PrevDesc = NEXT_MEMORY_DESCRIPTOR (PrevDesc, DescriptorSize);
      if (HasEntriesToRemove) {
        // have entries between PrevDesc and Desc which are joined to PrevDesc
        // we need to copy [Desc, end of list] to PrevDesc + 1
        CopyMem (PrevDesc, Desc, SizeFromDescToEnd);
        Desc = PrevDesc;
      }
      HasEntriesToRemove = FALSE;
    }
    // move to next
    Desc = NEXT_MEMORY_DESCRIPTOR (Desc, DescriptorSize);
    SizeFromDescToEnd -= DescriptorSize;
  }
}

VOID
EFIAPI
PrintMemMap (
  IN UINTN                  MemoryMapSize,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion
) {
#if DBG_TO
  UINTN                   NumEntries, Index;
  UINT64                  Bytes;
  EFI_MEMORY_DESCRIPTOR   *Desc;

  Desc = MemoryMap;
  NumEntries = MemoryMapSize / DescriptorSize;
  DBG ("MEMMAP: Size=%d, Addr=%p, DescSize=%d, DescVersion: 0x%x\n", MemoryMapSize, MemoryMap, DescriptorSize, DescriptorVersion);
  DBG ("Type       Start            End       VStart               # Pages          Attributes\n");
  for (Index = 0; Index < NumEntries; Index++) {

    Bytes = (((UINTN) Desc->NumberOfPages) * EFI_PAGE_SIZE);

    DBG ("%-12s %lX-%lX %lX  %lX %lX\n",
      EfiMemoryTypeDesc[Desc->Type],
      Desc->PhysicalStart,
      Desc->PhysicalStart + Bytes - 1,
      Desc->VirtualStart,
      Desc->NumberOfPages,
      Desc->Attribute
    );

    Desc = NEXT_MEMORY_DESCRIPTOR (Desc, DescriptorSize);
    //if ((Index + 1) % 16 == 0) {
    //  WaitForKeyPress (L"press a key to continue\n");
    //}
  }
  //WaitForKeyPress (L"End: press a key to continue\n");
#endif
}

VOID
EFIAPI
PrintSystemTable (
  IN EFI_SYSTEM_TABLE  *ST
) {
#if DBG_TO
  UINTN     Index;

  DBG ("SysTable: %p\n", ST);
  DBG ("- FirmwareVendor: %p, %s\n", ST->FirmwareVendor, ST->FirmwareVendor);
  DBG ("- ConsoleInHandle: %p, ConIn: %p\n", ST->ConsoleInHandle, ST->ConIn);
  DBG ("- RuntimeServices: %p, BootServices: %p, ConfigurationTable: %p\n", ST->RuntimeServices, ST->BootServices, ST->ConfigurationTable);
  DBG ("RT:\n");
  DBG ("- GetVariable: %p, SetVariable: %p\n", ST->RuntimeServices->GetVariable, ST->RuntimeServices->SetVariable);

  for (Index = 0; Index < ST->NumberOfTableEntries; Index++) {
    DBG ("ConfTab: %p\n", ST->ConfigurationTable[Index].VendorTable);
  }
#endif
}

VOID
WaitForKeyPress (
  CHAR16 *Message
) {
  EFI_STATUS      Status;
  UINTN           index;
  EFI_INPUT_KEY   key;

  Print (Message);
  do {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);
  } while (Status == EFI_SUCCESS);
  gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &index);
  do {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);
  } while (Status == EFI_SUCCESS);
}

/** Returns file path from FilePathProto in allocated memory. Mem should be released by caler.*/
CHAR16 *
EFIAPI
FileDevicePathToText (
  EFI_DEVICE_PATH_PROTOCOL *FilePathProto
) {
  EFI_STATUS            Status;
  FILEPATH_DEVICE_PATH  *FilePath;
  CHAR16                FilePathText[256], // possible problem: if filepath is bigger
                        *OutFilePathText;
  UINTN                 Size, SizeAll, i;

  FilePathText[0] = L'\0';
  i = 4;
  SizeAll = 0;
  //DBG ("FilePathProto->Type: %d, SubType: %d, Length: %d\n", FilePathProto->Type, FilePathProto->SubType, DevicePathNodeLength (FilePathProto));
  while (FilePathProto != NULL && FilePathProto->Type != END_DEVICE_PATH_TYPE && i > 0) {
    if (FilePathProto->Type == MEDIA_DEVICE_PATH && FilePathProto->SubType == MEDIA_FILEPATH_DP) {
      FilePath = (FILEPATH_DEVICE_PATH *) FilePathProto;
      Size = (DevicePathNodeLength (FilePathProto) - 4) / 2;
      if (SizeAll + Size < 256) {
        if (SizeAll > 0 && FilePathText[SizeAll / 2 - 2] != L'\\') {
          StrCat (FilePathText, L"\\");
        }
        StrCat (FilePathText, FilePath->PathName);
        SizeAll = StrSize (FilePathText);
      }
    }
    FilePathProto = NextDevicePathNode (FilePathProto);
    //DBG ("FilePathProto->Type: %d, SubType: %d, Length: %d\n", FilePathProto->Type, FilePathProto->SubType, DevicePathNodeLength (FilePathProto));
    i--;
    //DBG ("FilePathText: %s\n", FilePathText);
  }

  OutFilePathText = NULL;
  Size = StrSize (FilePathText);
  if (Size > 2) {
    // we are allocating mem here - should be released by caller
    Status = gBS->AllocatePool (EfiBootServicesData, Size, (VOID*)&OutFilePathText);
    if (Status == EFI_SUCCESS) {
      StrCpy (OutFilePathText, FilePathText);
    } else {
      OutFilePathText = NULL;
    }
  }

  return OutFilePathText;
}

/** Helper function that calls GetMemoryMap (), allocates space for mem map and returns it. */
EFI_STATUS
EFIAPI
GetMemoryMapAlloc (
  IN EFI_GET_MEMORY_MAP       GetMemoryMapFunction,
  OUT UINTN                   *MemoryMapSize,
  OUT EFI_MEMORY_DESCRIPTOR   **MemoryMap,
  OUT UINTN                   *MapKey,
  OUT UINTN                   *DescriptorSize,
  OUT UINT32                  *DescriptorVersion
) {
  EFI_STATUS    Status;

  *MemoryMapSize = 0;
  *MemoryMap = NULL;
  Status = GetMemoryMapFunction (MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // OK. Space needed for mem map is in MemoryMapSize
    // Important: next AllocatePool can increase mem map size - we must add some space for this
    *MemoryMapSize += 256;
    *MemoryMap = AllocatePool (*MemoryMapSize);
    Status = GetMemoryMapFunction (MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
    if (EFI_ERROR (Status)) {
      FreePool (*MemoryMap);
    }
  }

  return Status;
}

/** Alloctes Pages from the top of mem, up to address specified in Memory. Returns allocated address in Memory. */
EFI_STATUS
EFIAPI
AllocatePagesFromTop (
  IN EFI_MEMORY_TYPE            MemoryType,
  IN UINTN                      Pages,
  IN OUT EFI_PHYSICAL_ADDRESS   *Memory
) {
  EFI_STATUS              Status;
  UINTN                   MemoryMapSize, MapKey, DescriptorSize;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap, *MemoryMapEnd, *Desc;
  UINT32                  DescriptorVersion;

  Status = GetMemoryMapAlloc (gBS->GetMemoryMap, &MemoryMapSize, &MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);

  /*
    MemoryMapSize = 0;
    MemoryMap = NULL;
    Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      MemoryMapSize += 256; // allocating pool can increase future mem map size
      MemoryMap = AllocatePool (MemoryMapSize);
      Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
      if (EFI_ERROR (Status)) {
        FreePool (*MemoryMap);
      }
    }
  */

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EFI_NOT_FOUND;

  //DBG ("Search for Pages=%x, TopAddr=%lx\n", Pages, *Memory);
  //DBG ("MEMMAP: Size=%d, Addr=%p, DescSize=%d, DescVersion: 0x%x\n", MemoryMapSize, MemoryMap, DescriptorSize, DescriptorVersion);
  //DBG ("Type       Start            End       VStart               # Pages          Attributes\n");
  MemoryMapEnd = NEXT_MEMORY_DESCRIPTOR (MemoryMap, MemoryMapSize);
  Desc = PREV_MEMORY_DESCRIPTOR (MemoryMapEnd, DescriptorSize);

  for ( ; Desc >= MemoryMap; Desc = PREV_MEMORY_DESCRIPTOR (Desc, DescriptorSize)) {
    /*
    DBG ("%-12s %lX-%lX %lX  %lX %lX\n",
      EfiMemoryTypeDesc[Desc->Type],
      Desc->PhysicalStart,
      Desc->PhysicalStart + EFI_PAGES_TO_SIZE (Desc->NumberOfPages) - 1,
      Desc->VirtualStart,
      Desc->NumberOfPages,
      Desc->Attribute
    );
    */
    if (
      (Desc->Type == EfiConventionalMemory) &&                      // free mem
      (Pages <= Desc->NumberOfPages) &&                             // contains enough space
      (Desc->PhysicalStart + EFI_PAGES_TO_SIZE (Pages) <= *Memory)   // contains space below specified Memory
    ) {
      // free block found
      if (Desc->PhysicalStart + EFI_PAGES_TO_SIZE ((UINTN)Desc->NumberOfPages) <= *Memory) {
        // the whole block is unded Memory - allocate from the top of the block
        *Memory = Desc->PhysicalStart + EFI_PAGES_TO_SIZE ((UINTN)Desc->NumberOfPages - Pages);
        //DBG ("found the whole block under top mem, allocating at %lx\n", *Memory);
      } else {
        // the block contains enough pages under Memory, but spans above it - allocate below Memory.
        *Memory = *Memory - EFI_PAGES_TO_SIZE (Pages);
        //DBG ("found the whole block under top mem, allocating at %lx\n", *Memory);
      }

      Status = gBS->AllocatePages (
                      AllocateAddress,
                      MemoryType,
                      Pages,
                      Memory
                    );

      //DBG ("Alloc Pages=%x, Addr=%lx, Status=%r\n", Pages, *Memory, Status);

      break;
    }
  }

  // release mem
  FreePool (MemoryMap);

  return Status;
}
