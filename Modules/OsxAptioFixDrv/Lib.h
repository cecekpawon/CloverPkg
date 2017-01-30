/**

  Various helper functions.

  by dmazar

**/

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

// MemMap reversed scan
#define PREV_MEMORY_DESCRIPTOR(MemoryDescriptor, Size) \
  ((EFI_MEMORY_DESCRIPTOR *)((UINT8 *)(MemoryDescriptor) - (Size)))

/** Our internal structure to hold boot args params to make the code independent of the boot args version. */
typedef struct InternalBootArgs InternalBootArgs;
struct InternalBootArgs
{
  UINT32    *MemoryMap;               /* We will change this value so we need pointer to original field. */
  UINT32    *MemoryMapSize;
  UINT32    *MemoryMapDescriptorSize;
  UINT32    *MemoryMapDescriptorVersion;

  UINT32    *deviceTreeP;
  UINT32    *deviceTreeLength;

  UINT32    *kaddr;
  UINT32    *ksize;

  UINT32    *efiRuntimeServicesPageStart;
  UINT32    *efiRuntimeServicesPageCount;
  UINT64    *efiRuntimeServicesVirtualPageStart;
  UINT32    *efiSystemTable;
};

/** Applies some fixes to mem map. */
VOID EFIAPI
FixMemMap (
  IN UINTN                  MemoryMapSize,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion
);

/** Shrinks mem map by joining EfiBootServicesCode and EfiBootServicesData records. */
VOID
EFIAPI
ShrinkMemMap (
  IN UINTN                  *MemoryMapSize,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion
);

/** Prints mem map. */
VOID
EFIAPI
PrintMemMap (
  IN UINTN                  MemoryMapSize,
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  DescriptorSize,
  IN UINT32                 DescriptorVersion
);

/** Prints some values from Sys table and Runt. services. */
//VOID EFIAPI
//PrintSystemTable (IN EFI_SYSTEM_TABLE  *ST);

/** Prints Message and waits for a key press. */
//VOID
//WaitForKeyPress (CHAR16 *Message);

/** Returns file path from FilePath device path in allocated memory. Mem should be released by caller.*/
CHAR16 *
EFIAPI
FileDevicePathToText (
  EFI_DEVICE_PATH_PROTOCOL    *FilePathProto
);

/** Helper function that calls GetMemoryMap (), allocates space for mem map and returns it. */
EFI_STATUS
EFIAPI
GetMemoryMapAlloc (
  IN  EFI_GET_MEMORY_MAP      GetMemoryMapFunction,
  OUT UINTN                   *MemoryMapSize,
  OUT EFI_MEMORY_DESCRIPTOR   **MemoryMap,
  OUT UINTN                   *MapKey,
  OUT UINTN                   *DescriptorSize,
  OUT UINT32                  *DescriptorVersion
);

/** Alloctes Pages from the top of mem, up to address specified in Memory. Returns allocated address in Memory. */
EFI_STATUS
EFIAPI
AllocatePagesFromTop (
  IN      EFI_MEMORY_TYPE       MemoryType,
  IN      UINTN                 Pages,
  IN OUT  EFI_PHYSICAL_ADDRESS  *Memory
);

VOID                EFIAPI BootArgsPrint  (VOID *bootArgs);
InternalBootArgs *  EFIAPI GetBootArgs    (VOID *bootArgs);
VOID                EFIAPI BootArgsFix    (InternalBootArgs *BA, EFI_PHYSICAL_ADDRESS gRellocBase);
//VOID *            EFIAPI BootArgsFind   (IN EFI_PHYSICAL_ADDRESS Start);
