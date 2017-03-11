/**

  Methods for setting callback jump from kernel entry point, callback, fixes to kernel boot image.

  by dmazar

**/

/* DevTree may contain /chosen/memory-map
 * with properties with values = UINT32 address, UINT32 length:
 * "BootCLUT" = 8bit boot time colour lookup table
 * "Pict-FailedBoot" = picture shown if booting fails
 * "RAMDisk" = ramdisk
 * "Driver-<hex addr of DriverInfo>" = Kext, UINT32 address points to BooterKextFileInfo
 * "DriversPackage-..." = MKext, UINT32 address points to mkext_header (libkern/libkern/mkext.h), UINT32 length
 *
*/
#define BOOTER_KEXT_PREFIX      "Driver-"
#define BOOTER_MKEXT_PREFIX     "DriversPackage-"
#define BOOTER_RAMDISK_PREFIX   "RAMDisk"

extern EFI_PHYSICAL_ADDRESS   gRelocBase;
extern EFI_PHYSICAL_ADDRESS   gSysTableRtArea;
extern BOOLEAN                gHibernateWake;
extern UINTN                  gLastMemoryMapSize;
extern EFI_MEMORY_DESCRIPTOR  *gLastMemoryMap;
extern UINTN                  gLastDescriptorSize;
extern UINT32                 gLastDescriptorVersion;

EFI_STATUS PrepareJumpFromKernel ();
EFI_STATUS KernelEntryPatchJump (UINT32 KernelEntry);
#if APTIOFIX_VER == 1
EFI_STATUS KernelEntryFromMachOPatchJump (VOID *MachOImage, UINTN SlideAddr);
#endif
//EFI_STATUS KernelEntryPatchJumpFill ();
//EFI_STATUS KernelEntryPatchHalt (UINT32 KernelEntry);
//EFI_STATUS KernelEntryPatchZero (UINT32 KernelEntry);
EFI_STATUS
ExecSetVirtualAddressesToMemMap (
  IN UINTN                  MemoryMapSize,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap
);
VOID CopyEfiSysTableToSeparateRtDataArea (IN OUT UINT32 *EfiSystemTable);
VOID
ProtectRtDataFromRelocation (
  IN UINTN                  MemoryMapSize,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap
);
VOID
DefragmentRuntimeServices (
  IN UINTN                  MemoryMapSize,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN OUT UINT32             *EfiSystemTable,
  IN BOOLEAN                SkipOurSysTableRtArea
);

#if APTIOFIX_VER == 1

/** Fixes stuff for booting with relocation block. Called when boot.efi jumps to kernel. */
UINTN FixBootingWithRelocBlock (UINTN bootArgs, BOOLEAN ModeX64);

#else

/** Fixes stuff for booting without relocation block. Called when boot.efi jumps to kernel. */
UINTN FixBootingWithoutRelocBlock (UINTN bootArgs, BOOLEAN ModeX64);

/** Fixes stuff for hibernate wake booting without relocation block. Called when boot.efi jumps to kernel. */
UINTN FixHibernateWakeWithoutRelocBlock (UINTN imageHeaderPage, BOOLEAN ModeX64);

#endif
