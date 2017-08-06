/**

  Methods for setting callback jump from kernel entry point, callback, fixes to kernel boot image.

  by dmazar

**/

extern EFI_PHYSICAL_ADDRESS   gRelocBase;
extern EFI_PHYSICAL_ADDRESS   gSysTableRtArea;
extern UINTN                  gLastMemoryMapSize;
extern EFI_MEMORY_DESCRIPTOR  *gLastMemoryMap;
extern UINTN                  gLastDescriptorSize;
extern UINT32                 gLastDescriptorVersion;

EFI_STATUS
PrepareJumpFromKernel ();

EFI_STATUS
KernelEntryPatchJump (
  UINT32    KernelEntry
);

EFI_STATUS
ExecSetVirtualAddressesToMemMap (
  IN UINTN                  MemoryMapSize,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap
);

VOID
CopyEfiSysTableToSeparateRtDataArea (
  IN OUT UINT32   *EfiSystemTable
);

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

EFI_STATUS
KernelEntryFromMachOPatchJump (
  VOID    *MachOImage,
  UINTN   SlideAddr
);

/** Fixes stuff for booting with relocation block. Called when boot.efi jumps to kernel. */
UINTN
FixBootingWithRelocBlock (
  UINTN     bootArgs,
  BOOLEAN   ModeX64
);

#else

/** Fixes stuff for booting without relocation block. Called when boot.efi jumps to kernel. */
UINTN
FixBootingWithoutRelocBlock (
  UINTN     bootArgs,
  BOOLEAN   ModeX64
);

#endif
