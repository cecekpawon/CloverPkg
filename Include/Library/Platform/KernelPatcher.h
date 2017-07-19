/*
 * Copyright (c) 2009-2010 Frank peng. All rights reserved.
 *
 */

#ifndef __LIBSAIO_KERNEL_PATCHER_H
#define __LIBSAIO_KERNEL_PATCHER_H

#include "DeviceTree.h"

#include <Library/Common/LoaderUefi.h>
#include <Library/Platform/Platform.h>

#define MACH_GET_MAGIC(hdr)                 (((struct mach_header_64 *)(hdr))->magic)
#define MACH_GET_NCMDS(hdr)                 (((struct mach_header_64 *)(hdr))->ncmds)
#define MACH_GET_CPU(hdr)                   (((struct mach_header_64 *)(hdr))->cputype)
#define MACH_GET_FLAGS(hdr)                 (((struct mach_header_64 *)(hdr))->flags)
#define SC_GET_CMD(hdr)                     (((struct segment_command_64 *)(hdr))->cmd)

#define PTR_OFFSET(SourcePtr, Offset, ReturnType) ((ReturnType)(((UINT8 *)SourcePtr) + Offset))

#define kLinkeditSegment                    "__LINKEDIT"

#define kKldSegment                         "__KLD"
#define kKldTextSection                     "__text"

#define kDataSegment                        "__DATA"
#define kDataDataSection                    "__data"
#define kDataCommonSection                  "__common"

#define kTextSegment                        "__TEXT"
#define kTextTextSection                    "__text"
#define kTextConstSection                   "__const"
#define kTextCstringSection                 "__cstring"

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

#define kPropCFBundleIdentifier             "CFBundleIdentifier"
#define kPropCFBundleVersion                "CFBundleVersion"
#define kPropCFBundleExecutable             "CFBundleExecutable"
#define kPropOSBundleRequired               "OSBundleRequired"
#define kPropOSBundleLibraries              "OSBundleLibraries"
#define kPropIOKitPersonalities             "IOKitPersonalities"
#define kPropIONameMatch                    "IONameMatch"

#if 0
typedef struct compressed_kernel_header {
  UINT32  signature;
  UINT32  compress_type;
  UINT32  adler32;
  UINT32  uncompressed_size;
  UINT32  compressed_size;
  UINT32  reserved[11];
  CHAR8   platform_name[64];
  CHAR8   root_path[256];
  UINT8   data[0];
} compressed_kernel_header;
#endif

typedef struct KERNEL_INFO {
  UINT32                Slide;
  UINT32                KldAddr;
  UINT32                KldSize;
  UINT32                KldOff;
  UINT8                 KldIndex;
  UINT32                TextAddr;
  UINT32                TextSize;
  UINT32                TextOff;
  UINT8                 TextIndex;
  UINT32                ConstAddr;
  UINT32                ConstSize;
  UINT32                ConstOff;
  UINT8                 ConstIndex;
  //UINT32                CStringAddr;
  //UINT32                CStringSize;
  //UINT32                CStringOff;
  UINT32                DataAddr;
  UINT32                DataSize;
  UINT32                DataOff;
  UINT8                 DataIndex;
  //UINT32                DataCommonAddr;
  //UINT32                DataCommonSize;
  //UINT32                DataCommonOff;
  //UINT8                 DataCommonIndex;
                        // notes:
                        // - 64bit segCmd64->vmaddr is 0xffffff80xxxxxxxx and we are taking
                        //   only lower 32bit part into PrelinkTextAddr
                        // - PrelinkTextAddr is segCmd64->vmaddr + KernelRelocBase
  UINT32                PrelinkTextAddr;
  UINT32                PrelinkTextSize;
  UINT32                PrelinkTextOff;
  UINT8                 PrelinkTextIndex;
                        // notes:
                        // - 64bit sect->addr is 0xffffff80xxxxxxxx and we are taking
                        //   only lower 32bit part into PrelinkInfoAddr
                        // - PrelinkInfoAddr is sect->addr + KernelRelocBase
  UINT32                PrelinkInfoAddr;
  UINT32                PrelinkInfoSize;
  UINT32                PrelinkInfoOff;
  UINT8                 PrelinkInfoIndex;
  UINT32                LoadEXEStart;
  UINT32                LoadEXEEnd;
  UINT32                LoadEXESize;
  UINT32                StartupExtStart;
  UINT32                StartupExtEnd;
  UINT32                StartupExtSize;
  //UINT32                PrelinkedStart;
  //UINT32                PrelinkedEnd;
  //UINT32                PrelinkedSize;
  UINT32                XCPMStart;
  UINT32                XCPMEnd;
  UINT32                XCPMSize;
  UINT32                CPUInfoStart;
  UINT32                CPUInfoEnd;
  UINT32                CPUInfoSize;
  UINT32                VersionMajor;
  UINT32                VersionMinor;
  UINT32                Revision;
  CHAR8                 *Version;
  BOOLEAN               Cached;
  BOOLEAN               A64Bit;
  //BOOLEAN               SSSE3,
  BOOLEAN               PatcherInited;
  //EFI_PHYSICAL_ADDRESS  RelocBase;

  UINT32                PrelinkedSize;
  UINT32                KernelSize;

  VOID                  *Bin;
} KERNEL_INFO;

extern CHAR8            *gDtRoot;
extern KERNEL_INFO      *KernelInfo;

typedef enum {
  kLoadEXEStart,
  kLoadEXEEnd,
  kCPUInfoStart,
  kCPUInfoEnd,
  kVersion,
  kVersionMajor,
  kVersionMinor,
  kRevision,
  kXCPMStart,
  kXCPMEnd,
  kStartupExtStart,
  kStartupExtEnd,
  //kPrelinkedStart,
  //kPrelinkedEnd
} KernelPatchSymbolLookupIndex;

typedef struct KERNEL_PATCH_SYMBOL_LOOKUP
{
  KernelPatchSymbolLookupIndex    Index;
  CHAR8                           *Name;
} KERNEL_PATCH_SYMBOL_LOOKUP;

STATIC KERNEL_PATCH_SYMBOL_LOOKUP KernelPatchSymbolLookup[] = {
  { kLoadEXEStart, "__ZN6OSKext14loadExecutableEv" },
  { kLoadEXEEnd, "__ZN6OSKext23jettisonLinkeditSegmentEv" },
  { kCPUInfoStart, "_cpuid_set_info" },
  { kCPUInfoEnd, "_cpuid_info" },
  { kVersion, "_version" },
  { kVersionMajor, "_version_major" },
  { kVersionMinor, "_version_minor" },
  { kRevision, "_version_revision" },
  { kXCPMStart, "_xcpm_core_scope_msrs" },
  { kXCPMEnd, "_xcpm_SMT_scope_msrs" },
  { kStartupExtStart, "__ZN12KLDBootstrap21readStartupExtensionsEv" },
  { kStartupExtEnd, "__ZN12KLDBootstrap23readPrelinkedExtensionsEP10section_64" },
  //{ kPrelinkedStart, "__ZN12KLDBootstrap23readPrelinkedExtensionsEP10section_64" },
  //{ kPrelinkedEnd, "__ZN12KLDBootstrap20readBooterExtensionsEv" }
};

STATIC CONST UINTN KernelPatchSymbolLookupCount = ARRAY_SIZE (KernelPatchSymbolLookup);


/////////////////////
//
// kext_patcher.c
//

#define PropCFBundleIdentifierKey "<key>" kPropCFBundleIdentifier "</key>"
#define PropCFBundleVersionKey "<key>" kPropCFBundleVersion "</key>"

VOID
ExtractKextPropString (
  OUT CHAR8   *Res,
  IN  INTN    Len,
  IN  CHAR8   *Key,
  IN  CHAR8   *Plist
);

BOOLEAN
IsKernelIs64BitOnly (
  IN LOADER_ENTRY   *Entry
);

VOID
KernelAndKextsPatcherStart (
  IN LOADER_ENTRY   *Entry
);

VOID
FilterKernelPatches (
  IN LOADER_ENTRY   *Entry
);

VOID
FilterKextPatches (
  IN LOADER_ENTRY   *Entry
);

//
// Called from SetFSInjection (), before boot.efi is started,
// to allow patchers to prepare FSInject to force load needed kexts.
//
VOID
KextPatcherRegisterKexts (
  FSINJECTION_PROTOCOL    *FSInject,
  FSI_STRING_LIST         *ForceLoadKexts,
  LOADER_ENTRY            *Entry
);

VOID
CheckForFakeSMC (
  CHAR8         *InfoPlist,
  LOADER_ENTRY  *Entry
);

//
// Entry for all kext patches.
// Will iterate through kext in prelinked kernel (kernelcache)
// or DevTree (drivers boot) and do patches.
//
VOID
KextPatcher (
  LOADER_ENTRY    *Entry
);

//
// Searches Source for Search pattern of size SearchSize
// and returns the number of occurences.
//
UINTN
SearchAndCount (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize
);

//
// Searches Source for Search pattern of size SearchSize
// and replaces it with Replace up to MaxReplaces times.
// If MaxReplaces <= 0, then there is no restriction on number of replaces.
// Replace should have the same size as Search.
// Returns number of replaces done.
//
UINTN
SearchAndReplace (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize,
  UINT8     *Replace,
  UINT8     Wildcard,
  INTN      MaxReplaces
);

UINTN
SearchAndReplaceTxt (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize,
  UINT8     *Replace,
  UINT8     Wildcard,
  INTN      MaxReplaces
);

BOOLEAN
KernelAndKextPatcherInit (
  IN LOADER_ENTRY   *Entry
);

VOID
AnyKextPatch (
  CHAR8         *BundleIdentifier,
  UINT8         *Driver,
  UINT32        DriverSize,
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,
  KEXT_PATCH    *KextPatch
);

BOOLEAN
IsPatchNameMatch (
  CHAR8   *BundleIdentifier,
  CHAR8   *Name,
  CHAR8   *InfoPlist,
  INT32   *isBundle
);

BOOT_EFI_HEADER *
ParseBooterHeader (
  IN  VOID  *FileBuffer
);

VOID
PatchBooter (
  IN LOADER_ENTRY       *Entry,
  IN EFI_LOADED_IMAGE   *LoadedImage,
  IN BOOT_EFI_HEADER    *BootEfiHeader,
  IN CHAR8              *OSVer
);

#endif /* !__LIBSAIO_KERNEL_PATCHER_H */
