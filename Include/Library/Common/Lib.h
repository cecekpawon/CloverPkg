/*
 * refit/lib.h
 * General header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __REFITLIB_STANDARD_H__
#define __REFITLIB_STANDARD_H__

#define REFIT_DEBUG (2)

//#define Print if ((!GlobalConfig.Quiet) || (GlobalConfig.TextOnly)) Print

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof (array) / sizeof (array[0]))
#endif

#define CONSTRAIN_MIN(Variable, MinValue) if (Variable < MinValue) Variable = MinValue
#define CONSTRAIN_MAX(Variable, MaxValue) if (Variable > MaxValue) Variable = MaxValue

//
// lib module
//

typedef struct {
  EFI_STATUS      LastStatus;
  EFI_FILE        *DirHandle;
  BOOLEAN         CloseDirHandle;
  EFI_FILE_INFO   *LastFileInfo;
} REFIT_DIR_ITER;

typedef struct {
  UINT8 Flags;
  UINT8 StartCHS[3];
  UINT8 Type;
  UINT8 EndCHS[3];
  UINT32 StartLBA;
  UINT32 Size;
} MBR_PARTITION_INFO;

#define DISK_KIND_INTERNAL        (0)
#define DISK_KIND_EXTERNAL        (1)
#define DISK_KIND_OPTICAL         (2)
#define DISK_KIND_FIREWIRE        (3)
#define DISK_KIND_NODISK          (4)
//#define DISK_KIND_BOOTER        (5)

#define BOOTING_BY_BOOTLOADER     (1)
#define BOOTING_BY_EFI            (2)
#define BOOTING_BY_BOOTEFI        (3)
#define BOOTING_BY_MBR            (4)
#define BOOTING_BY_PBR            (5)
#define BOOTING_BY_CD             (6)

#define OSTYPE_DARWIN             (1)
#define OSTYPE_DARWIN_RECOVERY    (2)
#define OSTYPE_DARWIN_INSTALLER   (3)
#define OSTYPE_WIN                (4)
#define OSTYPE_VAR                (5)
#define OSTYPE_LIN                (6)
#define OSTYPE_LINEFI             (7)
#define OSTYPE_EFI                (8)
#define OSTYPE_WINEFI             (9)
#define OSTYPE_OTHER              (99)
//#define OSTYPE_HIDE             (100)

#define OSTYPE_DARWIN_STR         L"Darwin"
#define OSTYPE_LINUX_STR          L"Linux"
#define OSTYPE_WINDOWS_STR        L"Windows"

#define OSTYPE_IS_DARWIN(type) ((type == OSTYPE_DARWIN) || (type == OSTYPE_VAR))
#define OSTYPE_IS_DARWIN_RECOVERY(type) ((type == OSTYPE_DARWIN_RECOVERY) /*|| ((type >= OSTYPE_TIGER) && (type <= OSTYPE_MAV))*/ || (type == OSTYPE_VAR))
#define OSTYPE_IS_DARWIN_INSTALLER(type) ((type == OSTYPE_DARWIN_INSTALLER) /*|| ((type >= OSTYPE_TIGER) && (type <= OSTYPE_MAV))*/ || (type == OSTYPE_VAR))
#define OSTYPE_IS_DARWIN_GLOB(type) (OSTYPE_IS_DARWIN(type) || OSTYPE_IS_DARWIN_RECOVERY(type) || OSTYPE_IS_DARWIN_INSTALLER(type))
#define OSTYPE_IS_WINDOWS(type) ((type == OSTYPE_WIN) || (type == OSTYPE_WINEFI) || (type == OSTYPE_EFI) || (type == OSTYPE_VAR))
#define OSTYPE_IS_WINDOWS_GLOB(type) ((type == OSTYPE_WIN) || (type == OSTYPE_WINEFI))
#define OSTYPE_IS_LINUX(type) ((type == OSTYPE_LIN) || (type == OSTYPE_LINEFI) || (type == OSTYPE_EFI) || (type == OSTYPE_VAR))
#define OSTYPE_IS_LINUX_GLOB(type) ((type == OSTYPE_LIN) || (type == OSTYPE_LINEFI))
#define OSTYPE_IS_OTHER(type) ((type == OSTYPE_OTHER) || (type == OSTYPE_EFI) || (type == OSTYPE_VAR))
//#define OSTYPE_IS_NOT(type1, type2) (type1 != type2)
#define OSTYPE_COMPARE_IMP(comparator, type1, type2) (comparator(type1) && comparator(type2))
#define OSTYPE_COMPARE(type1, type2) \
          (OSTYPE_COMPARE_IMP(OSTYPE_IS_DARWIN, type1, type2) || OSTYPE_COMPARE_IMP(OSTYPE_IS_DARWIN_RECOVERY, type1, type2) || \
          OSTYPE_COMPARE_IMP(OSTYPE_IS_DARWIN_INSTALLER, type1, type2) || OSTYPE_COMPARE_IMP(OSTYPE_IS_WINDOWS, type1, type2) || \
          OSTYPE_COMPARE_IMP(OSTYPE_IS_LINUX, type1, type2) || OSTYPE_COMPARE_IMP(OSTYPE_IS_OTHER, type1, type2))

#define BIT_ISSET(flags, flag) ((flags & flag) == flag)
#define BIT_ISUNSET(flags, flag) ((flags & flag) != flag)
#define BIT_SET(flags, flag) (flags | flag)
#define BIT_UNSET(flags, flag) (flags & (~flag))
#define BIT_TOGGLE(flags, flag) (flags ^ flag)

#define OSFLAG_USEGRAPHICS              BIT0
#define OSFLAG_WITHKEXTS                BIT1
#define OSFLAG_CHECKFAKESMC             BIT2
#define OSFLAG_NOCACHES                 BIT3
#define OSFLAG_NODEFAULTARGS            BIT4
#define OSFLAG_NODEFAULTMENU            BIT5
#define OSFLAG_HIDDEN                   BIT6
#define OSFLAG_DISABLED                 BIT7
#define OSFLAG_NOSIP                    BIT8
#define OSFLAG_DBGPATCHES               BIT9
#define OSFLAG_ALLOW_KEXT_PATCHES       BIT10
#define OSFLAG_ALLOW_KERNEL_PATCHES     BIT11
#define OSFLAG_ALLOW_BOOTER_PATCHES     BIT12

#define OPT_VERBOSE                     BIT13
#define OPT_SINGLE_USER                 BIT14
#define OPT_SAFE                        BIT15

#define OPT_QUIET                       BIT16
#define OPT_SPLASH                      BIT17
#define OPT_NOMODESET                   BIT18
#define OPT_HDD                         BIT19
#define OPT_CDROM                       BIT20

#define VOLTYPE_OPTICAL                 (0x0001)
#define VOLTYPE_EXTERNAL                (0x0002)
#define VOLTYPE_INTERNAL                (0x0003)
#define VOLTYPE_FIREWIRE                (0x0004)

#define PARTITION_TYPE_UNKNOWN          (0)
#define PARTITION_TYPE_HFS              (1)
#define PARTITION_TYPE_RECOVERY         (2)
#define PARTITION_TYPE_APFS_CONTAINER   (3)
#define PARTITION_TYPE_APFS_VOLUME      (4)
#define PARTITION_TYPE_KERNELCOREDUMP   (5)

#define OSFLAG_DEFAULTS                 (\
                                          OSFLAG_USEGRAPHICS          | \
                                          OSFLAG_WITHKEXTS            | \
                                          OSFLAG_CHECKFAKESMC         | \
                                          OSFLAG_ALLOW_KEXT_PATCHES   | \
                                          OSFLAG_ALLOW_KERNEL_PATCHES | \
                                          OSFLAG_ALLOW_BOOTER_PATCHES \
                                        )

#define IS_EXTENDED_PART_TYPE(type) ((type) == 0x05 || (type) == 0x0f || (type) == 0x85)

typedef struct {
  UINT8     Type;
  CHAR16    *IconName;
  CHAR16    *Name;
} LEGACY_OS;

typedef struct {
  EFI_DEVICE_PATH       *DevicePath;
  EFI_HANDLE            DeviceHandle;
  EFI_FILE              *RootDir;
  CHAR16                *DevicePathString;
  CHAR16                *VolName;
  UINT8                 DiskKind;
  LEGACY_OS             *LegacyOS;
  BOOLEAN               Hidden;
  UINT8                 BootType;
  //BOOLEAN               IsAppleLegacy;
  BOOLEAN               HasBootCode;
  BOOLEAN               IsMbrPartition;
  UINTN                 MbrPartitionIndex;
  EFI_BLOCK_IO          *BlockIO;
  UINT64                BlockIOOffset;
  EFI_BLOCK_IO          *WholeDiskBlockIO;
  EFI_DEVICE_PATH       *WholeDiskDevicePath;
  EFI_HANDLE            WholeDiskDeviceHandle;
  MBR_PARTITION_INFO    *MbrPartitionTable;
  UINT32                DriveCRC32;
  EFI_GUID              RootUUID;
  UINT8                 PartitionType;
  EFI_GUID              PartitionTypeGUID;
  EFI_GUID              VenMediaGUID;
  CHAR16                *BlessedPath;
  CHAR16                *BooterDir;
} REFIT_VOLUME;

typedef struct {
  EFI_GUID              PartitionTypeGUID;
  EFI_GUID              UniquePartitionGUID;
} REFIT_VOLUME_GUID;

typedef struct {
  REFIT_MENU_ENTRY  Entry;
  INPUT_ITEM        *Item;
} REFIT_INPUT_DIALOG;

typedef struct {
  UINTN       DisableFlags;
  UINTN       HideBadges;
  UINTN       HideUIFlags;
  FONT_TYPE   Font;
  INTN        CharWidth;
  INTN        CharRows;
  INTN        CharCols;
  UINTN       SelectionColor;
  UINTN       SelectionTextColor;
  UINTN       TextColor;
  BOOLEAN     FontEmbedded;
  CHAR16      *FontFileName;
  CHAR16      *Theme;
  CHAR16      *BannerFileName;
  CHAR16      *SelectionSmallFileName;
  CHAR16      *SelectionBigFileName;
  CHAR16      *SelectionIndicatorName;
  CHAR16      *DefaultSelection;
  CHAR16      *ScreenResolution;
  CHAR16      *BackgroundName;
  SCALING     BackgroundScale;
  INT32       BackgroundSharp;
  BOOLEAN     BackgroundDark;
  BOOLEAN     SelectionOnTop;
  BOOLEAN     BootCampStyle;
  INTN        BadgeOffsetX;
  INTN        BadgeOffsetY;
  INTN        BadgeScale;
  INTN        IconScale;
  INTN        ThemeDesignWidth;
  INTN        ThemeDesignHeight;
  INTN        BannerPosX;
  INTN        BannerPosY;
  INTN        BannerEdgeHorizontal;
  INTN        BannerEdgeVertical;
  INTN        BannerNudgeX;
  INTN        BannerNudgeY;
  BOOLEAN     NonSelectedGrey;
  INTN        MainEntriesSize;
  INTN        TileXSpace;
  INTN        TileYSpace;
  INTN        PruneScrollRows;

  INTN        row0TileSize;
  INTN        row1TileSize;
  INTN        LayoutBannerOffset;
  INTN        LayoutButtonOffset;
  INTN        LayoutTextOffset;
  INTN        LayoutAnimMoveForMenuX;

  INTN        ScrollButtonWidth;
  INTN        ScrollButtonHeight;
  INTN        ScrollBarDecorationsHeight;
  INTN        ScrollBackDecorationsHeight;
} REFIT_CONFIG;

extern REFIT_CONFIG   GlobalConfig;
extern REFIT_CONFIG   DefaultConfig;

// types

typedef struct {
  CHAR8     *Name;
  CHAR8     *Label;
  CHAR8     *Filename; // Private
  BOOLEAN   IsPlistPatch;
  INTN      DataLen;
  UINT8     *Data;
  UINT8     *Patch;
  UINT8     Wildcard;
  CHAR8     *MatchOS;
  CHAR8     *MatchBuild;
  BOOLEAN   Disabled;
  BOOLEAN   Patched;
  INTN      Count;
} KEXT_PATCH;

typedef struct {
  CHAR8     *Label;
  INTN      DataLen;
  UINT8     *Data;
  UINT8     *Patch;
  INTN      Count;
  UINT8     Wildcard;
  CHAR8     *MatchOS;
  CHAR8     *MatchBuild;
  BOOLEAN   Disabled;
} KERNEL_PATCH;

typedef struct {
  CHAR8     *Label;
  INTN      DataLen;
  UINT8     *Data;
  UINT8     *Patch;
  INTN      Count;
  UINT8     Wildcard;
  CHAR8     *MatchOS;
  BOOLEAN   Disabled;
} BOOTER_PATCH;

typedef struct KERNEL_AND_KEXT_PATCHES {
  BOOLEAN         KPDebug;
  //BOOLEAN       KPKernelCpu;
  //BOOLEAN       KPLapicPanic;
  //BOOLEAN       KPHaswellE;
  BOOLEAN         KPAsusAICPUPM;
  //BOOLEAN       KPAppleRTC;
  BOOLEAN         KPKernelPm;
  UINT32          FakeCPUID;

  CHAR16          *KPATIConnectorsController;
  UINT8           *KPATIConnectorsData;
  UINTN           KPATIConnectorsDataLen;
  UINT8           *KPATIConnectorsPatch;
  UINT8           KPATIConnectorsWildcard;

  UINTN           NrKexts;
  KEXT_PATCH      *KextPatches;

  UINTN           NrForceKexts;
  CHAR16          **ForceKexts;

  UINTN           NrKernels;
  KERNEL_PATCH    *KernelPatches;

  UINTN           NrBooters;
  BOOTER_PATCH    *BooterPatches;
} KERNEL_AND_KEXT_PATCHES;

typedef struct {
  REFIT_MENU_ENTRY          me;
  REFIT_VOLUME              *Volume;
  CHAR16                    *DevicePathString;
  CHAR16                    *LoadOptions; // moved here for compatibility with legacy
  CHAR16                    *ToolOptions; // independent
  UINTN                     BootNum;
  CHAR16                    *LoaderPath;
  CHAR16                    *VolName;
  EFI_DEVICE_PATH           *DevicePath;
  UINT16                    Flags;
  UINT8                     LoaderType;
  CHAR8                     *OSVersion;
  CHAR8                     *OSBuildVersion;
  UINT8                     OSVersionMajor;
  UINT8                     OSVersionMinor;
  UINT8                     OSRevision;
  //UINT8                     KernelVersionMajor;
  //UINT8                     KernelVersionMinor;
  //UINT8                     KernelRevision;
  KERNEL_AND_KEXT_PATCHES   *KernelAndKextPatches;
  CHAR16                    *Settings;
} LOADER_ENTRY;

typedef struct CUSTOM_LOADER_ENTRY {
  struct  CUSTOM_LOADER_ENTRY       *Next;
  struct  CUSTOM_LOADER_ENTRY       *SubEntries;
          EG_IMAGE                  *Image;
          EG_IMAGE                  *DriveImage;
          CHAR16                    *ImagePath;
          CHAR16                    *DriveImagePath;
          CHAR16                    *Volume;
          CHAR16                    *Path;
          CHAR16                    *Options;
          CHAR16                    *FullTitle;
          CHAR16                    *Title;
          CHAR16                    *Settings;
          CHAR16                    Hotkey;
          BOOLEAN                   CommonSettings;
          UINT8                     Flags;
          UINT8                     Type;
          UINT8                     VolumeType;
          UINT8                     KernelScan;
          KERNEL_AND_KEXT_PATCHES   KernelAndKextPatches;
} CUSTOM_LOADER_ENTRY;

typedef struct CUSTOM_TOOL_ENTRY {
  struct  CUSTOM_TOOL_ENTRY   *Next;
          EG_IMAGE            *Image;
          CHAR16              *ImagePath;
          CHAR16              *Volume;
          CHAR16              *Path;
          CHAR16              *Options;
          CHAR16              *FullTitle;
          CHAR16              *Title;
          CHAR16              Hotkey;
          UINT8               Flags;
          UINT8               VolumeType;
} CUSTOM_TOOL_ENTRY;

extern EFI_HANDLE         SelfImageHandle;
extern EFI_LOADED_IMAGE   *SelfLoadedImage;
extern EFI_FILE           *SelfRootDir;
extern EFI_FILE           *SelfDir;
extern EFI_DEVICE_PATH    *SelfFullDevicePath;
extern EFI_FILE           *ThemeDir;
extern CHAR16             *ThemePath;

extern REFIT_VOLUME       *SelfVolume;
extern REFIT_VOLUME       **Volumes;
extern UINTN              VolumesCount;

extern EFI_GUID           **VenMediaGUID;
extern UINTN              VenMediaGUIDCount;

EFI_STATUS
InitRefitLib (
  IN EFI_HANDLE     ImageHandle
);

VOID
UninitRefitLib ();

EFI_STATUS
ReinitRefitLib ();

//VOID
//PauseForKey (IN CHAR16 *Msg);

BOOLEAN
IsEmbeddedTheme ();

//VOID
//CreateList (
//  OUT VOID    ***ListPtr,
//  OUT UINTN   *ElementCount,
//  IN  UINTN   InitialElementCount
//);

VOID
AddListElement (
  IN OUT  VOID    ***ListPtr,
  IN OUT  UINTN   *ElementCount,
  IN      VOID    *NewElement
);

VOID
FreeList (
  IN OUT VOID   ***ListPtr,
  IN OUT UINTN  *ElementCount
);

VOID
ScanVolumes ();

REFIT_VOLUME *
FindVolumeByName (
  IN CHAR16   *VolName
);

BOOLEAN
FileExists (
  IN EFI_FILE   *Root,
  IN CHAR16     *RelativePath
);

BOOLEAN
DeleteFile (
  IN EFI_FILE   *Root,
  IN CHAR16     *RelativePath
);

EFI_STATUS
MkDir (
  IN EFI_FILE_HANDLE    BaseDir OPTIONAL,
  IN CHAR16             *DirName
);

EFI_STATUS
DirNextEntry (
  IN      EFI_FILE        *Directory,
  IN OUT  EFI_FILE_INFO   **DirEntry,
  IN      UINTN           FilterMode
);

VOID
DirIterOpen (
  IN  EFI_FILE        *BaseDir,
  IN  CHAR16          *RelativePath OPTIONAL,
  OUT REFIT_DIR_ITER  *DirIter
);

BOOLEAN
DirIterNext (
  IN OUT  REFIT_DIR_ITER  *DirIter,
  IN      UINTN           FilterMode,
  IN      CHAR16          *FilePattern OPTIONAL,
  OUT     EFI_FILE_INFO   **DirEntry
);

EFI_STATUS
DirIterClose (
  IN OUT REFIT_DIR_ITER   *DirIter
);

CHAR16 *
Basename (
  IN CHAR16   *Path
);

CHAR16 *
Dirname (
  IN CHAR16   *Path
);

CHAR16 *
ReplaceExtension (
  IN CHAR16   *Path,
  IN CHAR16   *Extension
);

CHAR16 *
FindExtension (
  IN CHAR16   *FileName
);

CHAR16 *
RemoveExtension (
  IN CHAR16    *Path
);

INTN
FindMem (
  IN VOID   *Buffer,
  IN UINTN  BufferLength,
  IN VOID   *SearchString,
  IN UINTN  SearchStringLength
);

CHAR8 *
SearchString (
  IN  CHAR8       *Source,
  IN  UINT64      SourceSize,
  IN  CHAR8       *Search,
  IN  UINTN       SearchSize
);

CHAR16 *
FileDevicePathToStr (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevPath
);

CHAR16 *
FileDevicePathFileToStr (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevPath
);

EFI_STATUS
InitializeUnicodeCollationProtocol ();

#if REFIT_DEBUG > 0
VOID DebugPause ();
#else
#define DebugPause()
#endif

//VOID EndlessIdleLoop();

BOOLEAN
CheckFatalError (
  IN EFI_STATUS   Status,
  IN CHAR16       *Where
);

BOOLEAN
CheckError (
  IN EFI_STATUS   Status,
  IN CHAR16       *Where
);

VOID
ReinitVolumes ();

VOID
DbgHeader (
  CHAR8   *str
);

BOOLEAN
BootArgsExists (
  IN CHAR16   *LoadOptions,
  IN CHAR16   *LoadOption
);

LOADER_ENTRY *
DuplicateLoaderEntry (
  IN LOADER_ENTRY   *Entry
);

INTN
FindDefaultEntry ();

VOID
AddOptionEntries (
  REFIT_MENU_SCREEN   *SubScreen,
  LOADER_ENTRY        *SubEntry
);

EFI_STATUS
LoadEFIImageList (
  IN  EFI_DEVICE_PATH   **DevicePaths,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
);

EFI_STATUS
StartEFILoadedImage (
  IN  EFI_HANDLE    ChildImageHandle,
  IN  CHAR16        *LoadOptions,
  IN  CHAR16        *LoadOptionsPrefix,
  IN  CHAR16        *ImageTitle,
  OUT UINTN         *ErrorInStep
);

EFI_STATUS
LoadEFIImage (
  IN  EFI_DEVICE_PATH   *DevicePath,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
);

EFI_STATUS
StartEFIImage (
  IN  EFI_DEVICE_PATH   *DevicePath,
  IN  CHAR16            *LoadOptions,
  IN  CHAR16            *LoadOptionsPrefix,
  IN  CHAR16            *ImageTitle,
  OUT UINTN             *ErrorInStep,
  OUT EFI_HANDLE        *NewImageHandle
);

VOID
StartLoader (
  IN LOADER_ENTRY   *Entry
);

UINT32
EncodeOptions (
  CHAR16  *Options
);

VOID
DecodeOptions (
  LOADER_ENTRY  *Entry
);

EFI_STATUS
LoadFile (
  IN  EFI_FILE_HANDLE   BaseDir,
  IN  CHAR16            *FileName,
  OUT UINT8             **FileData,
  OUT UINTN             *FileDataLength
);

EFI_STATUS
SaveFile (
  IN EFI_FILE_HANDLE  BaseDir OPTIONAL,
  IN CHAR16           *FileName,
  IN UINT8            *FileData,
  IN UINTN            FileDataLength
);

EFI_STATUS
FindESP (
  OUT EFI_FILE_HANDLE   *RootDir
);

//
// BmLib
//
EFI_STATUS
EfiLibLocateProtocol (
  IN  EFI_GUID  *ProtocolGuid,
  OUT VOID      **Interface
);

EFI_FILE_HANDLE
EfiLibOpenRoot (
  IN EFI_HANDLE   DeviceHandle
);

EFI_FILE_SYSTEM_VOLUME_LABEL *
EfiLibFileSystemVolumeLabelInfo (
  IN EFI_FILE_HANDLE    FHand
);

EFI_FILE_INFO *
EfiLibFileInfo (
  IN EFI_FILE_HANDLE  FHand
);

EFI_FILE_SYSTEM_INFO *
EfiLibFileSystemInfo (
  IN EFI_FILE_HANDLE  Root
);

EFI_DEVICE_PATH_PROTOCOL *
EfiLibPathInfo (
  IN EFI_FILE_HANDLE    FHand,
  IN EFI_GUID           *InformationType
);

BOOLEAN
DumpVariable (
  CHAR16    *Name,
  EFI_GUID  *Guid,
  INTN      DevicePathAt
);

INTN
TimeCompare (
  IN EFI_TIME    *Time1,
  IN EFI_TIME    *Time2
);

//
// screen module
//

#define ATTR_BASIC                (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_ERROR                (EFI_RED | EFI_BACKGROUND_BLACK)
//#define ATTR_BANNER             (EFI_WHITE | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER               (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_CHOICE_BASIC         ATTR_BASIC
#define ATTR_CHOICE_CURRENT       (EFI_WHITE | EFI_BACKGROUND_LIGHTGRAY)
#define ATTR_SCROLLARROW          (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

VOID
DumpGOPVideoModes ();

EFI_STATUS
SetMaxResolution ();

EFI_STATUS
SetScreenResolution (
  IN CHAR16   *WidthHeight
);

EFI_STATUS
InternalSetMode (
  INT32   Next
);

VOID
GetScreenSize (
  OUT UINT32    *ScreenWidth,
  OUT UINT32    *ScreenHeight
);

CHAR16 *
ScreenDescription ();

BOOLEAN
HasGraphicsMode ();

BOOLEAN
IsGraphicsModeEnabled ();

VOID
SetGraphicsModeEnabled (
  IN BOOLEAN  Enable
);

VOID
InitScreen (
  IN BOOLEAN  egSetMaxResolution
);

VOID
SetupScreen ();

VOID
BeginTextScreen (
  IN CHAR16   *Title
);

VOID
FinishTextScreen (
  IN BOOLEAN  WaitAlways
);

VOID
BeginExternalScreen (
  IN BOOLEAN  UseGraphicsMode,
  IN CHAR16   *Title
);

VOID
FinishExternalScreen ();

VOID
TerminateScreen ();

VOID
SetNextScreenMode (
  INT32   Next
);

VOID
SwitchToGraphicsAndClear ();

BOOLEAN
ReadAllKeyStrokes ();

VOID
ClearScreen (
  IN EG_PIXEL   *Color
);

VOID
DrawImageArea (
  IN EG_IMAGE   *Image,
  IN INTN       AreaPosX,
  IN INTN       AreaPosY,
  IN INTN       AreaWidth,
  IN INTN       AreaHeight,
  IN INTN       ScreenPosX,
  IN INTN       ScreenPosY
);

VOID
TakeImage (
  IN EG_IMAGE   *Image,
  IN INTN       ScreenPosX,
  IN INTN       ScreenPosY,
  IN INTN       AreaWidth,
  IN INTN       AreaHeight
);

EFI_STATUS
ScreenShot ();

//
// IO
//

#define PoolPrint(...) CatSPrint(NULL, __VA_ARGS__)

//EFI_STATUS
//WaitForSingleEvent (
//  IN EFI_EVENT    Event,
//  IN UINT64       Timeout OPTIONAL
//);

#endif
