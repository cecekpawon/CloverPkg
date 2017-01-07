/**

  UEFI driver for enabling loading of OSX by using memory relocation.

  by dmazar

**/

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>

#include <Library/Common/CommonLib.h>

#include <Protocol/LoadedImage.h>

#include <Protocol/AptioFixProtocol.h>

#include "BootFixes.h"
#include "VMem.h"
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

// defines the size of block that will be allocated for kernel image relocation,
//   without RT and MMIO regions
// rehabman - Increase the size for ElCapitan to 128Mb 0x8000
// stinga11 - 0x6000
#define KERNEL_BLOCK_NO_RT_SIZE_PAGES 0x8000

// TRUE if we are doing hibernate wake
BOOLEAN                   gHibernateWake = FALSE;

// placeholders for storing original Boot Services functions
EFI_ALLOCATE_PAGES        gStoredAllocatePages = NULL;
EFI_GET_MEMORY_MAP        gStoredGetMemoryMap = NULL;
EFI_EXIT_BOOT_SERVICES    gStoredExitBootServices = NULL;
EFI_IMAGE_START           gStartImage = NULL;
EFI_HANDLE_PROTOCOL       gHandleProtocol = NULL;

// monitoring AlocatePages
EFI_PHYSICAL_ADDRESS      gMinAllocatedAddr = 0;
EFI_PHYSICAL_ADDRESS      gMaxAllocatedAddr = 0;

// relocation base address
EFI_PHYSICAL_ADDRESS      gRelocBase = 0;
// relocation block size in pages
UINTN                     gRelocSizePages = 0;

// location of memory allocated by boot.efi for hibernate image
EFI_PHYSICAL_ADDRESS      gHibernateImageAddress = 0;

// last memory map obtained by boot.efi
UINTN                     gLastMemoryMapSize = 0;
EFI_MEMORY_DESCRIPTOR     *gLastMemoryMap = NULL;
UINTN                     gLastDescriptorSize = 0;
UINT32                    gLastDescriptorVersion = 0;

EFI_GUID  gAptioFixProtocolGuid = {0xB79DCC2E, 0x61BE, 0x453F, {0xBC, 0xAC, 0xC2, 0x60, 0xFA, 0xAE, 0xCC, 0xDA}};

/** Helper function that calls GetMemoryMap() and returns new MapKey.
 * Uses gStoredGetMemoryMap, so can be called only after gStoredGetMemoryMap is set.
 */
EFI_STATUS
GetMemoryMapKey (
  OUT UINTN   *MapKey
) {
  EFI_STATUS              Status;
  UINTN                   MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap;
  UINTN                   DescriptorSize;
  UINT32                  DescriptorVersion;

  Status = GetMemoryMapAlloc (gStoredGetMemoryMap, &MemoryMapSize, &MemoryMap, MapKey, &DescriptorSize, &DescriptorVersion);
  return Status;
}

/** Helper function that calculates number of RT and MMIO pages from mem map. */
EFI_STATUS
GetNumberOfRTPages (
  OUT UINTN   *NumPages
) {
  EFI_STATUS              Status;
  UINTN                   MemoryMapSize, MapKey, DescriptorSize, NumEntries, Index;
  UINT32                  DescriptorVersion;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap, *Desc;

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
    //DBG ("OsxAptioFixDrv: CalculateRelocBlockSize(): GetNumberOfRTPages: %r\n", Status);
    //Print (L"OsxAptioFixDrv: CalculateRelocBlockSize(): GetNumberOfRTPages: %r\n", Status);
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
  Addr = 0x100000000; // max address
  Status = AllocatePagesFromTop (EfiBootServicesData, gRelocSizePages, &Addr);
  if (Status != EFI_SUCCESS) {
    //DBG ("OsxAptioFixDrv: AllocateRelocBlock(): can not allocate relocation block (0x%x pages below 0x%lx): %r\n",
    //  gRelocSizePages, Addr, Status);
    //Print (L"OsxAptioFixDrv: AllocateRelocBlock(): can not allocate relocation block (0x%x pages below 0x%lx): %r\n",
    //  gRelocSizePages, Addr, Status);
    return Status;
  } else {
    gRelocBase = Addr;
    //DBG ("OsxAptioFixDrv: AllocateRelocBlock(): gRelocBase set to %lx - %lx\n", gRelocBase, gRelocBase + EFI_PAGES_TO_SIZE(gRelocSizePages) - 1);
  }

  // set reloc addr in runtime vars for boot manager
  //Print (L"OsxAptioFixDrv: AllocateRelocBlock(): gRelocBase set to %lx - %lx\n", gRelocBase, gRelocBase + EFI_PAGES_TO_SIZE(gRelocSizePages) - 1);
  /*Status = */gRT->SetVariable (L"OsxAptioFixDrv-RelocBase", &gEfiGlobalVariableGuid,
                /*   EFI_VARIABLE_NON_VOLATILE |*/ EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                sizeof (gRelocBase) ,&gRelocBase);

  return Status;
}

/** Releases relocation block. */
EFI_STATUS
FreeRelocBlock () {
  return gBS->FreePages (gRelocBase, gRelocSizePages);
}

/** gBS->HandleProtocol override:
  * Boot.efi requires EfiGraphicsOutputProtocol on ConOutHandle, but it is not present
  * there on Aptio 2.0. EfiGraphicsOutputProtocol exists on some other handle.
  * If this is the case, we'll intercept that call and return EfiGraphicsOutputProtocol
  * from that other handle.
  */
EFI_STATUS EFIAPI
MOHandleProtocol (
  IN  EFI_HANDLE    Handle,
  IN  EFI_GUID      *Protocol,
  OUT VOID          **Interface
) {
  EFI_STATUS                    res;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;

  // special handling if gEfiGraphicsOutputProtocolGuid is requested by boot.efi
  if (CompareGuid (Protocol, &gEfiGraphicsOutputProtocolGuid)) {
    res = gHandleProtocol (Handle, Protocol, Interface);
    if (res != EFI_SUCCESS) {
      // let's find it on some other handle
      res = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&GraphicsOutput);
      if (res == EFI_SUCCESS) {
        // return it
        *Interface = GraphicsOutput;
        //DBG ("->HandleProtocol(%p, %s, %p) = %r (returning from other handle)\n", Handle, GuidStr(Protocol), *Interface, res);

        return res;
      }
    }
  } else {
    res = gHandleProtocol (Handle, Protocol, Interface);
  }

  //DBG ("->HandleProtocol(%p, %s, %p) = %r\n", Handle, GuidStr(Protocol), *Interface, res);

  return res;
}

/** gBS->AllocatePages override:
  * Returns pages from free memory block to boot.efi for kernel boot image.
  */
EFI_STATUS
EFIAPI
MOAllocatePages (
  IN EFI_ALLOCATE_TYPE          Type,
  IN EFI_MEMORY_TYPE            MemoryType,
  IN UINTN                      NumberOfPages,
  IN OUT EFI_PHYSICAL_ADDRESS   *Memory
) {
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      UpperAddr;
  //EFI_PHYSICAL_ADDRESS    MemoryIn;
  //BOOLEAN                 FromRelocBlock = FALSE;

  //MemoryIn = *Memory;

  if ((Type == AllocateAddress) && (MemoryType == EfiLoaderData)) {
    // called from boot.efi

    UpperAddr = *Memory + EFI_PAGES_TO_SIZE (NumberOfPages);

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
      //    NumberOfPages, EFI_PAGES_TO_SIZE(NumberOfPages)
      //    );
      //Print (L"Reloc block: %lx - %lx, Pages: %x, Size: %lx\n",
      //    gRelocBase, gRelocBase + EFI_PAGES_TO_SIZE(gRelocSizePages) - 1,
      //    gRelocSizePages, EFI_PAGES_TO_SIZE(gRelocSizePages)
      //    );
      //Print (L"Reloc block can handle mem requests: %lx - %lx\n",
      //    0, EFI_PAGES_TO_SIZE (gRelocSizePages) - 1
      //    );
      Print (L"Exiting in 30 secs ...\n");
      gBS->Stall (30 * 1000000);

      return EFI_OUT_OF_RESOURCES;
    }

    // store min and max mem - can be used later to determine start and end of kernel boot image
    if ((gMinAllocatedAddr == 0) || (*Memory < gMinAllocatedAddr)) {
      gMinAllocatedAddr = *Memory;
    }

    if (UpperAddr > gMaxAllocatedAddr) {
      gMaxAllocatedAddr = UpperAddr;
    }

    // give it from our allocated block
    *Memory += gRelocBase;
    //Status = gStoredAllocatePages (Type, MemoryType, NumberOfPages, Memory);
    //FromRelocBlock = TRUE;
    Status = EFI_SUCCESS;

  } else {
    // default page allocation
    Status = gStoredAllocatePages (Type, MemoryType, NumberOfPages, Memory);
  }

  //DBG ("AllocatePages(%s, %s, %x, %lx/%lx) = %r %c\n",
  //  EfiAllocateTypeDesc[Type], EfiMemoryTypeDesc[MemoryType], NumberOfPages, MemoryIn, *Memory, Status, FromRelocBlock ? L'+' : L' ');
  return Status;
}


/** gBS->GetMemoryMap override:
  * Returns shrinked memory map. I think kernel can handle up to 128 entries.
  */
EFI_STATUS
EFIAPI
MOGetMemoryMap (
  IN OUT UINTN                  *MemoryMapSize,
  IN OUT EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  OUT UINTN                     *MapKey,
  OUT UINTN                     *DescriptorSize,
  OUT UINT32                    *DescriptorVersion
) {
  EFI_STATUS    Status;

  Status = gStoredGetMemoryMap (MemoryMapSize, MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
  //PrintMemMap (*MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);
  if (Status == EFI_SUCCESS) {
    FixMemMap (*MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);
    //ShrinkMemMap (MemoryMapSize, MemoryMap, *DescriptorSize, *DescriptorVersion);
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
  * Patches kernel entry point with jump to our KernelEntryPatchJumpBack().
  */
EFI_STATUS
EFIAPI
MOExitBootServices (
  IN EFI_HANDLE   ImageHandle,
  IN UINTN        MapKey
) {
  EFI_STATUS    Status;
  UINTN         NewMapKey, SlideAddr = 0;
  VOID          *MachOImage = NULL;

  // for  tests: we can just return EFI_SUCCESS and continue using Print for debug.
  //Status = EFI_SUCCESS;
  //Print (L"ExitBootServices()\n");
  Status = gStoredExitBootServices (ImageHandle, MapKey);

  if (EFI_ERROR (Status)) {
    // just report error as var in nvram to be visible from OSX with "nvrap -p"
    //gRT->SetVariable (L"OsxAptioFixDrv-ErrorExitingBootServices",
    //         &gEfiAppleBootGuid,
    //         EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
    //         3,
    //         "Yes"
    //         );

    Status = GetMemoryMapKey (&NewMapKey);
    if (Status == EFI_SUCCESS) {
      // we have latest mem map and NewMapKey
      // we'll try again ExitBootServices with NewMapKey
      Status = gStoredExitBootServices (ImageHandle, NewMapKey);
      //if (EFI_ERROR (Status)) {
      //  // Error!
      //  Print (L"OsxAptioFixDrv: Error ExitBootServices() 2nd try = Status: %r\n", Status);
      //}
    } else {
      //Print (L"OsxAptioFixDrv: Error ExitBootServices(), GetMemoryMapKey() = Status: %r\n", Status);
      Status = EFI_INVALID_PARAMETER;
    }
  }

  if (EFI_ERROR (Status)) {
    Print (L"... waiting 10 secs ...\n");
    gBS->Stall (10*1000000);
    return Status;
  }

  //DBG ("ExitBootServices: gMinAllocatedAddr: %lx, gMaxAllocatedAddr: %lx\n", gMinAllocatedAddr, gMaxAllocatedAddr);
  MachOImage = (VOID*)(UINTN)(gRelocBase + 0x200000);
  KernelEntryFromMachOPatchJump (MachOImage, SlideAddr);

  return Status;
}


/** Callback called when boot.efi jumps to kernel. */
UINTN
EFIAPI
KernelEntryPatchJumpBack (
  UINTN     bootArgs,
  BOOLEAN   ModeX64
) {
  bootArgs = FixBootingWithRelocBlock (bootArgs, ModeX64);

  // debug for jumping back to kernel
  // put HLT to kernel entry point to stop there
  //SetMem ((VOID*)(UINTN)(AsmKernelEntry + gRelocBase), 1, 0xF4);
  // put 0 to kernel entry point to restart
  //SetMem64((VOID*)(UINTN)(AsmKernelEntry + gRelocBase), 1, 0);

  return bootArgs;
}



/** SWITCH_STACK_ENTRY_POINT implementation:
  * Allocates kernel image reloc block, installs UEFI overrides and starts given image.
  * If image returns, then deinstalls overrides and releases kernel image reloc block.
  *
  * If started with ImgContext->JumpBuffer, then it will return with LongJump().
  */
EFI_STATUS
RunImageWithOverrides (
  IN  EFI_HANDLE  ImageHandle,
  OUT UINTN       *ExitDataSize,
  OUT CHAR16      **ExitData  OPTIONAL
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

  // allocate block for kernel image relocation
  Status = AllocateRelocBlock ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

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
  gBS->CalculateCrc32(gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);

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
  gBS->CalculateCrc32(gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);

  // release reloc block
  FreeRelocBlock ();

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
  IN  EFI_HANDLE    ImageHandle,
  OUT UINTN         *ExitDataSize,
  OUT CHAR16        **ExitData  OPTIONAL
) {
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *Image;
  CHAR16                      *FilePathText = NULL;
  UINTN                       Size = 0;

  //DBG ("StartImage(%lx)\n", ImageHandle);

  // find out image name from EfiLoadedImageProtocol
  Status = gBS->OpenProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **) &Image, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (Status != EFI_SUCCESS) {
    //DBG("ERROR: MOStartImage: OpenProtocol(gEfiLoadedImageProtocolGuid) = %r\n", Status);
    return EFI_INVALID_PARAMETER;
  }

  FilePathText = FileDevicePathToText (Image->FilePath);
  //if (FilePathText != NULL) {
  //  DBG ("FilePath: %s\n", FilePathText);
  //}
  //DBG ("ImageBase: %p - %lx (%lx)\n", Image->ImageBase, (UINT64)Image->ImageBase + Image->ImageSize, Image->ImageSize);
  Status = gBS->CloseProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, gImageHandle, NULL);
  //if (EFI_ERROR (Status)) {
  //  DBG ("CloseProtocol error: %r\n", Status);
  //}

  //the presence of the variable means HibernateWake
  //if the wake is canceled then the variable must be deleted
  Status = gRT->GetVariable (L"boot-switch-vars", &gEfiAppleBootGuid, NULL, &Size, NULL);
  gHibernateWake = (Status == EFI_BUFFER_TOO_SMALL);

  // check if this is boot.efi
  if (StriStr (FilePathText, L"boot.efi") && !gHibernateWake) {

    Print (L"OsxAptioFixDrv: Starting overrides for %s\nUsing reloc block: yes, hibernate wake: %s \n",
        FilePathText, gHibernateWake ? L"yes" : L"no");
    //gBS->Stall (2000000);

    // run with our overrides
    Status = RunImageWithOverrides (ImageHandle, ExitDataSize, ExitData);

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

  // install StartImage override
  // all other overrides will be started when boot.efi is started
  gStartImage = gBS->StartImage;
  gBS->StartImage = MOStartImage;
  gBS->Hdr.CRC32 = 0;
  gBS->CalculateCrc32(gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);

  // install APTIOFIX_PROTOCOL to new handle
  AptioFix = AllocateZeroPool(sizeof(APTIOFIX_PROTOCOL));

  if (AptioFix == NULL)   {
    DBG("%a: Can not allocate memory for APTIOFIX_PROTOCOL\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  AptioFix->Signature = APTIOFIX_SIGNATURE;
  AptioFixIHandle = NULL; // install to new handle
  Status = gBS->InstallMultipleProtocolInterfaces(&AptioFixIHandle, &gAptioFixProtocolGuid, AptioFix, NULL);

  if (EFI_ERROR(Status)) {
    DBG("%a: error installing APTIOFIX_PROTOCOL, Status = %r\n", __FUNCTION__, Status);
  }

  return EFI_SUCCESS;
}

