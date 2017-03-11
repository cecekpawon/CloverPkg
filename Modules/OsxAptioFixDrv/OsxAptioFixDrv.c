/**

 UEFI driver for enabling loading of OSX with/out memory relocation.

 by dmazar

 **/

#ifndef APTIOFIX_VER
#define APTIOFIX_VER 2
#else
#if APTIOFIX_VER > 2 || APTIOFIX_VER < 0
#undef APTIOFIX_VER
#define APTIOFIX_VER 2
#endif
#endif

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>

#if APTIOFIX_VER == 2
#include <Library/Common/Hibernate.h>
#endif

#include <Protocol/LoadedImage.h>

#include <Protocol/AptioFixProtocol.h>

#include "BootFixes.h"
#include "VMem.h"
#include "Lib.h"

#if APTIOFIX_VER == 1
// defines the size of block that will be allocated for kernel image relocation, without RT and MMIO regions
// rehabman - Increase the size for ElCapitan to 128Mb 0x8000
// stinga11 - 0x6000
#define KERNEL_BLOCK_NO_RT_SIZE_PAGES   0x8000
#define MAX_PHYSICAL_ADDRESS            0x100000000
#endif

// TRUE if we are doing hibernate wake
BOOLEAN                       gHibernateWake = FALSE;

// placeholders for storing original Boot and RT Services functions
EFI_ALLOCATE_PAGES            gStoredAllocatePages = NULL;
EFI_GET_MEMORY_MAP            gStoredGetMemoryMap = NULL;
EFI_EXIT_BOOT_SERVICES        gStoredExitBootServices = NULL;
EFI_IMAGE_START               gStartImage = NULL;
EFI_HANDLE_PROTOCOL           gHandleProtocol = NULL;

#if APTIOFIX_VER == 2
EFI_SET_VIRTUAL_ADDRESS_MAP   gStoredSetVirtualAddressMap = NULL;
UINT32                        OrgRTCRC32 = 0;
#endif

// monitoring AlocatePages
EFI_PHYSICAL_ADDRESS          gMinAllocatedAddr = 0;
EFI_PHYSICAL_ADDRESS          gMaxAllocatedAddr = 0;

// relocation base address
EFI_PHYSICAL_ADDRESS          gRelocBase = 0;
#if APTIOFIX_VER == 1
// relocation block size in pages
UINTN                         gRelocSizePages = 0;
#endif

// location of memory allocated by boot.efi for hibernate image
EFI_PHYSICAL_ADDRESS          gHibernateImageAddress = 0;

// last memory map obtained by boot.efi
UINTN                         gLastMemoryMapSize = 0;
EFI_MEMORY_DESCRIPTOR         *gLastMemoryMap = NULL;
UINTN                         gLastDescriptorSize = 0;
UINT32                        gLastDescriptorVersion = 0;

/** Helper function that calls GetMemoryMap () and returns new MapKey.
 * Uses gStoredGetMemoryMap, so can be called only after gStoredGetMemoryMap is set.
 */
EFI_STATUS
GetMemoryMapKey (
  OUT UINTN   *MapKey
) {
  EFI_STATUS              Status;
  UINTN                   MemoryMapSize, DescriptorSize;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap;
  UINT32                  DescriptorVersion;

  Status = GetMemoryMapAlloc (gStoredGetMemoryMap, &MemoryMapSize, &MemoryMap, MapKey, &DescriptorSize, &DescriptorVersion);

  return Status;
}

#if APTIOFIX_VER == 1

/** Helper function that calculates number of RT and MMIO pages from mem map. */
EFI_STATUS
GetNumberOfRTPages (
  OUT UINTN   *NumPages
) {
  EFI_STATUS              Status;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap, *Desc;
  UINTN                   MemoryMapSize, MapKey, DescriptorSize, NumEntries, Index;
  UINT32                  DescriptorVersion;

  Status = GetMemoryMapAlloc (gBS->GetMemoryMap, &MemoryMapSize, &MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Apply some fixes
  //
  FixMemMap (MemoryMapSize, MemoryMap, DescriptorSize, DescriptorVersion);

  //
  // Sum RT and MMIO areas - all that have runtime attribute
  //

  *NumPages = 0;
  Desc = MemoryMap;
  NumEntries = MemoryMapSize / DescriptorSize;

  for (Index = 0; Index < NumEntries; Index++) {
    if ((Desc->Attribute & EFI_MEMORY_RUNTIME) != 0) {
      *NumPages += Desc->NumberOfPages;
    }

    Desc = NEXT_MEMORY_DESCRIPTOR (Desc, DescriptorSize);
  }

  return Status;
}

/** Calculate the size of reloc block.
  * gRelocSizePages = KERNEL_BLOCK_NO_RT_SIZE_PAGES + RT&MMIO pages
  */
EFI_STATUS
CalculateRelocBlockSize () {
  EFI_STATUS    Status;
  UINTN         NumPagesRT;

  // Sum pages needed for RT and MMIO areas
  Status = GetNumberOfRTPages (&NumPagesRT);
  if (EFI_ERROR (Status)) {
    //DBG ("OsxAptioFixDrv: CalculateRelocBlockSize (): GetNumberOfRTPages: %r\n", Status);
    //Print (L"OsxAptioFixDrv: CalculateRelocBlockSize (): GetNumberOfRTPages: %r\n", Status);
    return Status;
  }

  gRelocSizePages = KERNEL_BLOCK_NO_RT_SIZE_PAGES + NumPagesRT;

  return Status;
}

/** Allocate free block on top of mem for kernel image relocation (will be returned to boot.efi for kernel boot image). */
EFI_STATUS
AllocateRelocBlock () {
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Addr;

  // calculate the needed size for reloc block
  CalculateRelocBlockSize ();

  gRelocBase = 0;
  Addr = MAX_PHYSICAL_ADDRESS; // max address
  Status = AllocatePagesFromTop (EfiBootServicesData, gRelocSizePages, &Addr);
  if (Status != EFI_SUCCESS) {
    //DBG ("OsxAptioFixDrv: AllocateRelocBlock (): can not allocate relocation block (0x%x pages below 0x%lx): %r\n",
    //  gRelocSizePages, Addr, Status);
    //Print (L"OsxAptioFixDrv: AllocateRelocBlock (): can not allocate relocation block (0x%x pages below 0x%lx): %r\n",
    //  gRelocSizePages, Addr, Status);
    return Status;
  } else {
    gRelocBase = Addr;
    //DBG ("OsxAptioFixDrv: AllocateRelocBlock (): gRelocBase set to %lx - %lx\n", gRelocBase, gRelocBase + EFI_PAGES_TO_SIZE (gRelocSizePages) - 1);
  }

  // set reloc addr in runtime vars for boot manager
  //Print (L"OsxAptioFixDrv: AllocateRelocBlock (): gRelocBase set to %lx - %lx\n", gRelocBase, gRelocBase + EFI_PAGES_TO_SIZE (gRelocSizePages) - 1);
  /*Status = */gRT->SetVariable (
                      L"OsxAptioFixDrv-RelocBase",
                      &gEfiGlobalVariableGuid,
                      /* EFI_VARIABLE_NON_VOLATILE |*/ EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                      sizeof (gRelocBase),
                      &gRelocBase
                    );

  return Status;
}

/** Releases relocation block. */
EFI_STATUS
FreeRelocBlock () {
  return gBS->FreePages (gRelocBase, gRelocSizePages);
}

#endif

/** gBS->HandleProtocol override:
 * Boot.efi requires EfiGraphicsOutputProtocol on ConOutHandle, but it is not present
 * there on Aptio 2.0. EfiGraphicsOutputProtocol exists on some other handle.
 * If this is the case, we'll intercept that call and return EfiGraphicsOutputProtocol
 * from that other handle.
 */
EFI_STATUS
EFIAPI
MOHandleProtocol (
  IN  EFI_HANDLE    Handle,
  IN  EFI_GUID      *Protocol,
  OUT VOID          **Interface
) {
  EFI_STATUS                      res;
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *GraphicsOutput;

  // special handling if gEfiGraphicsOutputProtocolGuid is requested by boot.efi
  if (CompareGuid (Protocol, &gEfiGraphicsOutputProtocolGuid)) {
    res = gHandleProtocol (Handle, Protocol, Interface);
    if (res != EFI_SUCCESS) {
      // let's find it on some other handle
      res = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&GraphicsOutput);
      if (res == EFI_SUCCESS) {
        // return it
        *Interface = GraphicsOutput;
        //DBG ("->HandleProtocol (%p, %s, %p) = %r (returning from other handle)\n", Handle, GuidStr (Protocol), *Interface, res);

        return res;
      }
    }
  } else {
    res = gHandleProtocol (Handle, Protocol, Interface);
  }

  //DBG ("->HandleProtocol (%p, %s, %p) = %r\n", Handle, GuidStr (Protocol), *Interface, res);

  return res;
}

/** gBS->AllocatePages override:
 * Returns pages from free memory block to boot.efi for kernel boot image.
 */
EFI_STATUS
EFIAPI
MOAllocatePages (
  IN      EFI_ALLOCATE_TYPE       Type,
  IN      EFI_MEMORY_TYPE         MemoryType,
  IN      UINTN                   NumberOfPages,
  IN OUT  EFI_PHYSICAL_ADDRESS    *Memory
) {
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  UpperAddr;

  if ((Type == AllocateAddress) && (MemoryType == EfiLoaderData)) {
    // called from boot.efi

    UpperAddr = *Memory + EFI_PAGES_TO_SIZE (NumberOfPages);

#if APTIOFIX_VER == 1
    // check if the requested mem can be served from reloc block
    // the upper address is compared to the size of the relocation block to achieve Address + gRelocBase for all
    // allocations, so that the entire block can be copied to the proper location on kernel entry
    // Comparing only the number of pages will not only give wrong results as gRelocSizePages is not decreased,
    // but also implies memory is 'stacked', which it is not.
    if (UpperAddr >= EFI_PAGES_TO_SIZE (gRelocSizePages)) {
      // no - exceeds our block - signal error
      Print (L"OsxAptipFixDrv: Error - requested memory exceeds our allocated relocation block\n");
      //Print (L"Requested mem: %lx - %lx, Pages: %x, Size: %lx\n",
      //    *Memory, UpperAddr - 1,
      //    NumberOfPages, EFI_PAGES_TO_SIZE (NumberOfPages)
      //    );
      //Print (L"Reloc block: %lx - %lx, Pages: %x, Size: %lx\n",
      //    gRelocBase, gRelocBase + EFI_PAGES_TO_SIZE (gRelocSizePages) - 1,
      //    gRelocSizePages, EFI_PAGES_TO_SIZE (gRelocSizePages)
      //    );
      //Print (L"Reloc block can handle mem requests: %lx - %lx\n",
      //    0, EFI_PAGES_TO_SIZE (gRelocSizePages) - 1
      //    );
      Print (L"Exiting in 30 secs ...\n");
      gBS->Stall (30 * 1000000);

      return EFI_OUT_OF_RESOURCES;
    }
#endif

    // store min and max mem - can be used later to determine start and end of kernel boot image
    if ((gMinAllocatedAddr == 0) || (*Memory < gMinAllocatedAddr)) {
      gMinAllocatedAddr = *Memory;
    }

    if (UpperAddr > gMaxAllocatedAddr) {
      gMaxAllocatedAddr = UpperAddr;
    }

#if APTIOFIX_VER == 1
    // give it from our allocated block
    *Memory += gRelocBase;
    //Status = gStoredAllocatePages (Type, MemoryType, NumberOfPages, Memory);
    //FromRelocBlock = TRUE;
    Status = EFI_SUCCESS;
#else
    Status = gStoredAllocatePages (Type, MemoryType, NumberOfPages, Memory);
  } else if (
    gHibernateWake &&
    (Type == AllocateAnyPages) &&
    (MemoryType == EfiLoaderData)
  ) {
    // called from boot.efi during hibernate wake
    // first such allocation is for hibernate image
    Status = gStoredAllocatePages (Type, MemoryType, NumberOfPages, Memory);
    if ((gHibernateImageAddress == 0) && (Status == EFI_SUCCESS)) {
      gHibernateImageAddress = *Memory;
    }
#endif
  } else {
    // default page allocation
    Status = gStoredAllocatePages (Type, MemoryType, NumberOfPages, Memory);
  }

  //DBG ("AllocatePages (%s, %s, %x, %lx/%lx) = %r %c\n",
  //  EfiAllocateTypeDesc[Type], EfiMemoryTypeDesc[MemoryType], NumberOfPages, MemoryIn, *Memory, Status, FromRelocBlock ? L'+' : L' ');
  return Status;
}

/** gBS->GetMemoryMap override:
 * Returns shrinked memory map. I think kernel can handle up to 128 entries.
 */
EFI_STATUS
EFIAPI
MOGetMemoryMap (
  IN OUT  UINTN                   *MemoryMapSize,
  IN OUT  EFI_MEMORY_DESCRIPTOR   *MemoryMap,
     OUT  UINTN                   *MapKey,
     OUT  UINTN                   *DescriptorSize,
     OUT  UINT32                  *DescriptorVersion
) {
  EFI_STATUS    Status;

  Status = gStoredGetMemoryMap (MemoryMapSize, MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
  //PrintMemMap (*MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);
  if (Status == EFI_SUCCESS) {
    FixMemMap (*MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);
#if APTIOFIX_VER == 2
    ShrinkMemMap (MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);
#endif
    //PrintMemMap (*MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);

    // remember last/final memmap
    gLastMemoryMapSize = *MemoryMapSize;
    gLastMemoryMap = MemoryMap;
    gLastDescriptorSize = *DescriptorSize;
    gLastDescriptorVersion = *DescriptorVersion;
  }

  return Status;
}

/** gBS->ExitBootServices override:
 * Patches kernel entry point with jump to our KernelEntryPatchJumpBack ().
 */
EFI_STATUS
EFIAPI
MOExitBootServices (
  IN EFI_HANDLE   ImageHandle,
  IN UINTN        MapKey
) {
  EFI_STATUS              Status;
  UINTN                   NewMapKey;
#if APTIOFIX_VER == 1
  UINTN                   SlideAddr = 0;
  VOID                    *MachOImage = NULL;
#else
  IOHibernateImageHeader  *ImageHeader = NULL;

  // we need hibernate image address for wake
  if (gHibernateWake && (gHibernateImageAddress == 0)) {
    Print (L"OsxAptioFix error: Doing hibernate wake, but did not find hibernate image address.");
    Print (L"... waiting 5 secs ...\n");
    gBS->Stall (5 * 1000000);

    return EFI_INVALID_PARAMETER;
  }
#endif

  // for  tests: we can just return EFI_SUCCESS and continue using Print for debug.
  //  Status = EFI_SUCCESS;
  //Print (L"ExitBootServices ()\n");
  Status = gStoredExitBootServices (ImageHandle, MapKey);

  if (EFI_ERROR (Status)) {
    Status = GetMemoryMapKey (&NewMapKey);
    if (Status == EFI_SUCCESS) {
      // we have latest mem map and NewMapKey
      // we'll try again ExitBootServices with NewMapKey
      Status = gStoredExitBootServices (ImageHandle, NewMapKey);
      if (EFI_ERROR (Status)) {
        // Error!
        Print (L"OsxAptioFixDrv: Error ExitBootServices () 2nd try = Status: %r\n", Status);
      }
    } else {
      Print (L"OsxAptioFixDrv: Error ExitBootServices (), GetMemoryMapKey () = Status: %r\n", Status);
      Status = EFI_INVALID_PARAMETER;
    }
  }

  if (EFI_ERROR (Status)) {
    Print (L"... waiting 10 secs ...\n");
    gBS->Stall (10 * 1000000);
    return Status;
  }

#if APTIOFIX_VER == 1
  //DBG ("ExitBootServices: gMinAllocatedAddr: %lx, gMaxAllocatedAddr: %lx\n", gMinAllocatedAddr, gMaxAllocatedAddr);
  MachOImage = (VOID *)(UINTN)(gRelocBase + 0x200000);
  KernelEntryFromMachOPatchJump (MachOImage, SlideAddr);
#else
  if (!gHibernateWake) {
    // normal boot
    DBG ("ExitBootServices: gMinAllocatedAddr: %lx, gMaxAllocatedAddr: %lx\n", gMinAllocatedAddr, gMaxAllocatedAddr);
    //SlideAddr = gMinAllocatedAddr - 0x100000;
    //MachOImage = (VOID *)(UINTN)(SlideAddr + 0x200000);
    //KernelEntryFromMachOPatchJump (MachOImage, SlideAddr);
  } else {
    // hibernate wake
    // at this stage HIB section is not yet copied from sleep image to it's
    // proper memory destination. so we'll patch entry point in sleep image.
    ImageHeader = (IOHibernateImageHeader *)(UINTN)gHibernateImageAddress;
    KernelEntryPatchJump (((UINT32)(UINTN)&(ImageHeader->fileExtentMap[0])) + ImageHeader->fileExtentMapSize + ImageHeader->restore1CodeOffset);
  }
#endif

  return Status;
}

#if APTIOFIX_VER == 2
/** gRT->SetVirtualAddressMap override:
 * Fixes virtualizing of RT services.
 */
EFI_STATUS
EFIAPI
OvrSetVirtualAddressMap (
  IN UINTN                  MemoryMapSize,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion,
  IN EFI_MEMORY_DESCRIPTOR  *VirtualMap
) {
  EFI_STATUS    Status;
  UINT32        EfiSystemTable;

  DBG ("->SetVirtualAddressMap (%d, %d, 0x%x, %p) START ...\n", MemoryMapSize, DescriptorSize, DescriptorVersion, VirtualMap);

  // restore origs
  gRT->Hdr.CRC32 = OrgRTCRC32;
  gRT->SetVirtualAddressMap = gStoredSetVirtualAddressMap;

  // Protect RT data areas from relocation by marking then MemMapIO
  ProtectRtDataFromRelocation (MemoryMapSize, DescriptorSize, DescriptorVersion, VirtualMap);

  // Remember physical sys table addr
  EfiSystemTable = (UINT32)(UINTN)gST;

  // virtualize RT services with all needed fixes
  Status = ExecSetVirtualAddressesToMemMap (MemoryMapSize, DescriptorSize, DescriptorVersion, VirtualMap);

  CopyEfiSysTableToSeparateRtDataArea (&EfiSystemTable);

  // we will defragment RT data and code that is left unprotected.
  // this will also mark those as AcpiNVS and by this protect it
  // from boot.efi relocation and zeroing
  DefragmentRuntimeServices (
    MemoryMapSize,
    DescriptorSize,
    DescriptorVersion,
    VirtualMap,
    NULL,
    !gHibernateWake
  );

  return Status;
}
#endif

/** Callback called when boot.efi jumps to kernel. */
UINTN
EFIAPI
KernelEntryPatchJumpBack (
  UINTN     bootArgs,
  BOOLEAN   ModeX64
) {
#if APTIOFIX_VER == 1
  bootArgs = FixBootingWithRelocBlock (bootArgs, ModeX64);

  // debug for jumping back to kernel
  // put HLT to kernel entry point to stop there
  //SetMem ((VOID *)(UINTN)(AsmKernelEntry + gRelocBase), 1, 0xF4);
  // put 0 to kernel entry point to restart
  //SetMem64 ((VOID *)(UINTN)(AsmKernelEntry + gRelocBase), 1, 0);
#else
  if (gHibernateWake) {
    bootArgs = FixHibernateWakeWithoutRelocBlock (bootArgs, ModeX64);
  } else {
    bootArgs = FixBootingWithoutRelocBlock (bootArgs, ModeX64);
  }
#endif

  return bootArgs;
}

/** SWITCH_STACK_ENTRY_POINT implementation:
 * Allocates kernel image reloc block, installs UEFI overrides and starts given image.
 * If image returns, then deinstalls overrides and releases kernel image reloc block.
 *
 * If started with ImgContext->JumpBuffer, then it will return with LongJump ().
 */
EFI_STATUS
RunImageWithOverrides (
  IN  EFI_HANDLE                  ImageHandle,
  IN  EFI_LOADED_IMAGE_PROTOCOL   *Image,
  OUT UINTN                       *ExitDataSize,
  OUT CHAR16                      **ExitData  OPTIONAL
) {
  EFI_STATUS    Status;

  // save current 64bit state - will be restored later in callback from kernel jump
  // and relocate MyAsmCopyAndJumpToKernel32 code to higher mem (for copying kernel back to
  // proper place and jumping back to it)
  Status = PrepareJumpFromKernel ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // init VMem memory pool - will be used after ExitBootServices
  Status = VmAllocateMemoryPool ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

#if APTIOFIX_VER == 1
  // allocate block for kernel image relocation
  Status = AllocateRelocBlock ();
  if (EFI_ERROR (Status)) {
    return Status;
  }
#endif

  // clear monitoring vars
  gMinAllocatedAddr = 0;
  gMaxAllocatedAddr = 0;

  // save original BS functions
  gStoredAllocatePages = gBS->AllocatePages;
  gStoredGetMemoryMap = gBS->GetMemoryMap;
  gStoredExitBootServices = gBS->ExitBootServices;
  gHandleProtocol = gBS->HandleProtocol;

  // install our overrides
  gBS->AllocatePages = MOAllocatePages;
  gBS->GetMemoryMap = MOGetMemoryMap;
  gBS->ExitBootServices = MOExitBootServices;
  gBS->HandleProtocol = MOHandleProtocol;

  gBS->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);

#if APTIOFIX_VER == 2
  OrgRTCRC32 = gRT->Hdr.CRC32;
  gStoredSetVirtualAddressMap = gRT->SetVirtualAddressMap;
  gRT->SetVirtualAddressMap = OvrSetVirtualAddressMap;
  gRT->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gRT, gRT->Hdr.HeaderSize, &gRT->Hdr.CRC32);

  // force boot.efi to use our copy od system table
  DBG ("StartImage: orig sys table: %p\n", Image->SystemTable);
  Image->SystemTable = (EFI_SYSTEM_TABLE *)(UINTN)gSysTableRtArea;
  DBG ("StartImage: new sys table: %p\n", Image->SystemTable);
#endif

  // run image
  Status = gStartImage (ImageHandle, ExitDataSize, ExitData);

  // if we get here then boot.efi did not start kernel
  // and we'll try to do some cleanup ...

  // return back originals
  gBS->AllocatePages = gStoredAllocatePages;
  gBS->GetMemoryMap = gStoredGetMemoryMap;
  gBS->ExitBootServices = gStoredExitBootServices;
  gBS->HandleProtocol = gHandleProtocol;

  gBS->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);

#if APTIOFIX_VER == 2
  gRT->SetVirtualAddressMap = gStoredSetVirtualAddressMap;
  gBS->CalculateCrc32 (gRT, gRT->Hdr.HeaderSize, &gRT->Hdr.CRC32);
#else
  // release reloc block
  FreeRelocBlock ();
#endif

  return Status;
}

/** gBS->StartImage override:
 * Called to start an efi image.
 *
 * If this is boot.efi, then run it with our overrides.
 */
EFI_STATUS
EFIAPI
MOStartImage (
  IN  EFI_HANDLE  ImageHandle,
  OUT UINTN       *ExitDataSize,
  OUT CHAR16      **ExitData  OPTIONAL
) {
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *Image;
  CHAR16                      *FilePathText = NULL;
  UINTN                       Size = 0;
  BOOLEAN                     StartFlag;
#if APTIOFIX_VER == 2
  VOID                        *Value = NULL;
#endif

  DBG ("StartImage (%lx)\n", ImageHandle);

  // find out image name from EfiLoadedImageProtocol
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&Image,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (Status != EFI_SUCCESS) {
    DBG ("ERROR: MOStartImage: OpenProtocol (gEfiLoadedImageProtocolGuid) = %r\n", Status);
    return EFI_INVALID_PARAMETER;
  }

  FilePathText = FileDevicePathToText (Image->FilePath);
  if (FilePathText != NULL) {
    DBG ("FilePath: %s\n", FilePathText);
  }

  DBG ("ImageBase: %p - %lx (%lx)\n", Image->ImageBase, (UINT64)Image->ImageBase + Image->ImageSize, Image->ImageSize);

  Status = gBS->CloseProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, gImageHandle, NULL);
  if (EFI_ERROR (Status)) {
    DBG ("CloseProtocol error: %r\n", Status);
  }

  StartFlag = (StriStr (FilePathText, L"boot.efi") != NULL);

  if (StartFlag) {
#if APTIOFIX_VER == 2
    //Check recovery-boot-mode present for nested boot.efi
    Status = GetVariable2 (L"recovery-boot-mode", &gEfiAppleBootGuid, &Value, &Size);
    if (!EFI_ERROR (Status)) {
      //If it presents, then wait for \com.apple.recovery.boot\boot.efi boot
      DBG (" recovery-boot-mode present\n");
      StartFlag = (StriStr (FilePathText, L"\\com.apple.recovery.boot\\boot.efi") != NULL);
    }

    FreePool (Value);
#endif
  }

  // check if this is boot.efi
  if (StartFlag) {
    //the presence of the variable means HibernateWake
    //if the wake is canceled then the variable must be deleted
    Status = gRT->GetVariable (L"boot-switch-vars", &gEfiAppleBootGuid, NULL, &Size, NULL);
    gHibernateWake = (Status == EFI_BUFFER_TOO_SMALL);

    Print (
      L"OsxAptioFixDrv (ver %d):\n - Starting overrides for: %s\n - Using reloc block: %s, hibernate wake: %s\n",
      APTIOFIX_VER,
      FilePathText,
      (APTIOFIX_VER == 1) ? L"Yes" : L"No",
      gHibernateWake ? L"Yes" : L"No"
    );
    //gBS->Stall (2 * 1000000);

    // run with our overrides
    Status = RunImageWithOverrides (ImageHandle, Image, ExitDataSize, ExitData);

  } else {
    // call original function to do the job
    Status = gStartImage (ImageHandle, ExitDataSize, ExitData);
  }

  if (FilePathText != NULL) {
    gBS->FreePool (FilePathText);
  }

  return Status;
}

/** Entry point. Installs our StartImage override.
 * All other stuff will be installed from there when boot.efi is started.
 */
EFI_STATUS
EFIAPI
OsxAptioFixDrvEntrypoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
) {
  EFI_STATUS              Status;
  APTIOFIX_PROTOCOL       *AptioFix;
  EFI_HANDLE              AptioFixIHandle;

#ifndef EMBED_APTIOFIX
  DBG ("Starting module: AptioFix (ver %d)\n", APTIOFIX_VER);
#endif

  // install StartImage override
  // all other overrides will be started when boot.efi is started
  gStartImage = gBS->StartImage;
  gBS->StartImage = MOStartImage;
  gBS->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);

  // install APTIOFIX_PROTOCOL to new handle
  AptioFix = AllocateZeroPool (sizeof (APTIOFIX_PROTOCOL));

  if (AptioFix == NULL)   {
    DBG ("%a: Can not allocate memory for APTIOFIX_PROTOCOL\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  AptioFix->Signature = APTIOFIX_SIGNATURE;
  AptioFixIHandle = NULL; // install to new handle
  Status = gBS->InstallMultipleProtocolInterfaces (&AptioFixIHandle, &gAptioFixProtocolGuid, AptioFix, NULL);

  if (EFI_ERROR (Status)) {
    DBG ("%a: error installing APTIOFIX_PROTOCOL, Status = %r\n", __FUNCTION__, Status);
    return Status;
  }

  return EFI_SUCCESS;
}
