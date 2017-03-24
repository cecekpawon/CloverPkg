/**

  Methods for finding, checking and fixing boot args

  by dmazar

**/

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#include "Lib.h"

VOID
EFIAPI
BootArgsPrint (
  VOID    *bootArgs
) {
#if (DBG_TO > 0)
  BootArgs2   *BA2 = bootArgs;

  // Lion and up
  DBG ("BootArgs at 0x%p\n", BA2);

  DBG (" Revision: 0x%x, Version: 0x%x, efiMode: 0x%x (%d), debugMode: 0x%x, flags: 0x%x\n",
    BA2->Revision, BA2->Version, BA2->efiMode, BA2->efiMode, BA2->debugMode, BA2->flags
  );

  DBG (" CommandLine: %a\n", BA2->CommandLine);

  DBG (" MemoryMap: 0x%x, MMSize: 0x%x, MMDescSize: 0x%x, MMDescVersion: 0x%x\n",
    BA2->MemoryMap, BA2->MemoryMapSize, BA2->MemoryMapDescriptorSize, BA2->MemoryMapDescriptorVersion
  );

  DBG (" Boot_Video v_baseAddr: 0x%x, v_display: 0x%x, v_rowBytes: 0x%x, v_width: %d, v_height: %d, v_depth: %d\n",
    BA2->Video.v_baseAddr, BA2->Video.v_display, BA2->Video.v_rowBytes,
    BA2->Video.v_width, BA2->Video.v_height, BA2->Video.v_depth
  );

  DBG (" deviceTreeP: 0x%x, deviceTreeLength: 0x%x, kaddr: 0x%x, ksize: 0x%x\n",
    BA2->deviceTreeP, BA2->deviceTreeLength, BA2->kaddr, BA2->ksize
  );

  DBG (" efiRTServPgStart: 0x%x, efiRTServPgCount: 0x%x, efiRTServVPgStart: 0x%lx\n",
    BA2->efiRuntimeServicesPageStart, BA2->efiRuntimeServicesPageCount, BA2->efiRuntimeServicesVirtualPageStart
  );

  DBG (" efiSystemTable: 0x%x, kslide: 0x%x, perfDataStart: 0x%x, perfDataSize: 0x%x\n",
    BA2->efiSystemTable, BA2->kslide, BA2->performanceDataStart, BA2->performanceDataSize
  );

  DBG (" keyStDtStart: 0x%x, keyStDtSize: 0x%x, bootMemStart: 0x%lx, bootMemSize: 0x%lx\n",
    BA2->keyStoreDataStart, BA2->keyStoreDataSize, BA2->bootMemStart, BA2->bootMemSize
  );

  DBG (" PhysicalMemorySize: 0x%lx (%d GB), FSBFrequency: 0x%lx (%d MHz)\n",
    BA2->PhysicalMemorySize, BA2->PhysicalMemorySize / (1024 * 1024 * 1024),
    BA2->FSBFrequency, BA2->FSBFrequency / 1000000
  );

  DBG (" pciConfigSpaceBaseAddress: 0x%lx, pciConfigSpaceStartBusNumber: 0x%x, pciConfigSpaceEndBusNumber: 0x%x\n",
    BA2->pciConfigSpaceBaseAddress, BA2->pciConfigSpaceStartBusNumber, BA2->pciConfigSpaceEndBusNumber
  );
#endif
}

InternalBootArgs *
EFIAPI
GetBootArgs (
  VOID    *bootArgs
) {
  BootArgs2   *BA2 = bootArgs;

  ZeroMem (&gAptioBootArgs, sizeof (gAptioBootArgs));

  // Lion and up
  gAptioBootArgs.MemoryMap = &BA2->MemoryMap;
  gAptioBootArgs.MemoryMapSize = &BA2->MemoryMapSize;
  gAptioBootArgs.MemoryMapDescriptorSize = &BA2->MemoryMapDescriptorSize;
  gAptioBootArgs.MemoryMapDescriptorVersion = &BA2->MemoryMapDescriptorVersion;

  gAptioBootArgs.deviceTreeP = &BA2->deviceTreeP;
  gAptioBootArgs.deviceTreeLength = &BA2->deviceTreeLength;

  gAptioBootArgs.kaddr = &BA2->kaddr;
  gAptioBootArgs.ksize = &BA2->ksize;

  gAptioBootArgs.efiRuntimeServicesPageStart = &BA2->efiRuntimeServicesPageStart;
  gAptioBootArgs.efiRuntimeServicesPageCount = &BA2->efiRuntimeServicesPageCount;
  gAptioBootArgs.efiRuntimeServicesVirtualPageStart = &BA2->efiRuntimeServicesVirtualPageStart;
  gAptioBootArgs.efiSystemTable = &BA2->efiSystemTable;

  return &gAptioBootArgs;
}

VOID
EFIAPI
BootArgsFix (
  InternalBootArgs        *BA,
  EFI_PHYSICAL_ADDRESS    gRellocBase
) {
  *BA->MemoryMap -= (UINT32)gRellocBase;
  *BA->deviceTreeP -= (UINT32)gRellocBase;
  *BA->kaddr -= (UINT32)gRellocBase;
}

#if 0
/** Searches for bootArgs from Start and returns pointer to bootArgs or ... does not return if can not be found.  **/
VOID *
EFIAPI
BootArgsFind (
  IN EFI_PHYSICAL_ADDRESS   Start
) {
  UINT8       *ptr, archMode = sizeof (UINTN) * 8;
  BootArgs2   *BA2;

  // start searching from 0x200000.
  ptr = (UINT8 *)(UINTN)Start;

  while (TRUE) {
    // check bootargs for 10.7 and up
    BA2 = (BootArgs2 *)ptr;

    if (
      (BA2->Version == 2) &&
      (BA2->Revision == 0) &&
      // plus additional checks - some values are not inited by boot.efi yet
      (BA2->efiMode == archMode) &&
      (BA2->kaddr == 0) &&
      (BA2->ksize == 0) &&
      (BA2->efiSystemTable == 0)
    ) {
      break;
    }

    ptr += 0x1000;
  }

  DBG ("Found bootArgs2 at %p\n", ptr);
  return ptr;
}
#endif
