/*
 * Copyright (c) 2009-2010 Frank peng. All rights reserved.
 *
 */
#include "Platform.h"
#include "LoaderUefi.h"
#include "device_tree.h"

//##################
#define MACH_PATCH
//##################

#ifndef __LIBSAIO_KERNEL_PATCHER_H
#define __LIBSAIO_KERNEL_PATCHER_H

#define CPUIDFAMILY_DEFAULT                 6

#define CPUID_MODEL_6_13                    13
#define CPUID_MODEL_YONAH                   14
#define CPUID_MODEL_MEROM                   15
#define CPUID_MODEL_PENRYN                  35

#define MACH_GET_MAGIC(hdr)                 (((struct mach_header_64*)(hdr))->magic)
#define MACH_GET_NCMDS(hdr)                 (((struct mach_header_64*)(hdr))->ncmds)
#define MACH_GET_CPU(hdr)                   (((struct mach_header_64*)(hdr))->cputype)
#define MACH_GET_FLAGS(hdr)                 (((struct mach_header_64*)(hdr))->flags)
#define SC_GET_CMD(hdr)                     (((struct segment_command_64*)(hdr))->cmd)

#define PTR_OFFSET(SourcePtr, Offset, ReturnType) ((ReturnType)(((UINT8*)SourcePtr) + Offset))


#define kPrelinkTextSegment                 "__PRELINK_TEXT"
#define kPrelinkTextSection                 "__text"

#define kPrelinkLinkStateSegment            "__PRELINK_STATE"
#define kPrelinkKernelLinkStateSection      "__kernel"
#define kPrelinkKextsLinkStateSection       "__kexts"

#define kPrelinkInfoSegment                 "__PRELINK_INFO"
#define kPrelinkInfoSection                 "__info"

#define kPrelinkBundlePathKey               "_PrelinkBundlePath"
#define kPrelinkExecutableRelativePathKey   "_PrelinkExecutableRelativePath"
#define kPrelinkExecutableLoadKey           "_PrelinkExecutableLoadAddr"
#define kPrelinkExecutableSourceKey         "_PrelinkExecutableSourceAddr"
#define kPrelinkExecutableSizeKey           "_PrelinkExecutableSize"
#define kPrelinkInfoDictionaryKey           "_PrelinkInfoDictionary"
#define kPrelinkInterfaceUUIDKey            "_PrelinkInterfaceUUID"
#define kPrelinkKmodInfoKey                 "_PrelinkKmodInfo"
#define kPrelinkLinkStateKey                "_PrelinkLinkState"
#define kPrelinkLinkStateSizeKey            "_PrelinkLinkStateSize"

#define kPropCFBundleIdentifier             ("CFBundleIdentifier")
#define kPropCFBundleExecutable             ("CFBundleExecutable")
#define kPropOSBundleRequired               ("OSBundleRequired")
#define kPropOSBundleLibraries              ("OSBundleLibraries")
#define kPropIOKitPersonalities             ("IOKitPersonalities")
#define kPropIONameMatch                    ("IONameMatch")

typedef struct _BooterKextFileInfo {
  UINT32  infoDictPhysAddr;
  UINT32  infoDictLength;
  UINT32  executablePhysAddr;
  UINT32  executableLength;
  UINT32  bundlePathPhysAddr;
  UINT32  bundlePathLength;
} _BooterKextFileInfo;

typedef struct _DeviceTreeBuffer {
  uint32_t paddr;
  uint32_t length;
} _DeviceTreeBuffer;

typedef struct KERNEL_INFO {
  UINT32                Slide;
  UINT32                KldAddr;
  UINT32                KldSize;
  UINT32                KldOff;
  UINT32                TextAddr;
  UINT32                TextSize;
  UINT32                TextOff;
  //UINT32                ConstAddr;
  //UINT32                ConstSize;
  //UINT32                ConstOff;
  //UINT32                CStringAddr;
  //UINT32                CStringSize;
  //UINT32                CStringOff;
  UINT32                DataAddr;
  UINT32                DataSize;
  UINT32                DataOff;
                        // notes:
                        // - 64bit segCmd64->vmaddr is 0xffffff80xxxxxxxx and we are taking
                        //   only lower 32bit part into PrelinkTextAddr
                        // - PrelinkTextAddr is segCmd64->vmaddr + KernelRelocBase
  UINT32                PrelinkTextAddr;
  UINT32                PrelinkTextSize;
                        // notes:
                        // - 64bit sect->addr is 0xffffff80xxxxxxxx and we are taking
                        //   only lower 32bit part into PrelinkInfoAddr
                        // - PrelinkInfoAddr is sect->addr + KernelRelocBase
  UINT32                PrelinkInfoAddr;
  UINT32                PrelinkInfoSize;
  UINT32                XCPMStart;
  UINT32                XCPMEnd;
  UINT32                CPUInfoStart;
  UINT32                CPUInfoEnd;
  UINT32                VersionMajor;
  UINT32                VersionMinor;
  UINT32                Revision;
  CHAR8                 *Version;
  BOOLEAN               isCache;
  BOOLEAN               is64Bit;
  //BOOLEAN               SSSE3,
  BOOLEAN               PatcherInited;
  EFI_PHYSICAL_ADDRESS  RelocBase;
  VOID                  *Bin;
} KERNEL_INFO;

//extern BootArgs2    *bootArgs2;
extern CHAR8        *dtRoot;
extern KERNEL_INFO  *KernelInfo;

VOID KernelAndKextsPatcherStart(IN LOADER_ENTRY *Entry);


/////////////////////
//
// kext_patcher.c
//

//
// Called from SetFSInjection(), before boot.efi is started,
// to allow patchers to prepare FSInject to force load needed kexts.
//
VOID KextPatcherRegisterKexts(FSINJECTION_PROTOCOL *FSInject, FSI_STRING_LIST *ForceLoadKexts, LOADER_ENTRY *Entry);

//
// Entry for all kext patches.
// Will iterate through kext in prelinked kernel (kernelcache)
// or DevTree (drivers boot) and do patches.
//
VOID KextPatcherStart(LOADER_ENTRY *Entry);

//
// Searches Source for Search pattern of size SearchSize
// and returns the number of occurences.
//
UINTN SearchAndCount(UINT8 *Source, UINT32 SourceSize, UINT8 *Search, UINTN SearchSize);

//
// Searches Source for Search pattern of size SearchSize
// and replaces it with Replace up to MaxReplaces times.
// If MaxReplaces <= 0, then there is no restriction on number of replaces.
// Replace should have the same size as Search.
// Returns number of replaces done.
//
UINTN SearchAndReplace(UINT8 *Source, UINT32 SourceSize, UINT8 *Search, UINTN SearchSize, UINT8 *Replace, INTN MaxReplaces);

CHAR8
*ExtractKextBundleIdentifier (
  CHAR8     *Plist
);

BOOLEAN
isPatchNameMatch (
  CHAR8   *BundleIdentifier,
  CHAR8   *Name
);

#endif /* !__LIBSAIO_KERNEL_PATCHER_H */
