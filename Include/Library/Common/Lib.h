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

//#define UGASUPPORT 1

#include <Library/UI/Libeg.h>

#define REFIT_DEBUG (2)
#define Print if ((!GlobalConfig.Quiet) || (GlobalConfig.TextOnly)) Print
//#include "GenericBdsLib.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof (array) / sizeof (array[0]))
#endif

#define CONSTRAIN_MIN(Variable, MinValue) if (Variable < MinValue) Variable = MinValue
#define CONSTRAIN_MAX(Variable, MaxValue) if (Variable > MaxValue) Variable = MaxValue

#define CONFIG_FILENAME           L"config"

#define CONFIG_THEME_FILENAME     L"theme"
#define CONFIG_THEME_RANDOM       L"random"
#define CONFIG_THEME_EMBEDDED     L"embedded"
#define CONFIG_THEME_CHRISTMAS    L"christmas"
#define CONFIG_THEME_NEWYEAR      L"newyear"

extern EFI_HANDLE                 gImageHandle;
extern EFI_SYSTEM_TABLE           *gST;
extern EFI_BOOT_SERVICES          *gBS;
extern EFI_RUNTIME_SERVICES       *gRS;

#define TAG_ABOUT                 (1)
#define TAG_RESET                 (2)
#define TAG_SHUTDOWN              (3)
#define TAG_TOOL                  (4)
#define TAG_LOADER                (5)
#define TAG_LABEL                 (6)
#define TAG_INFO                  (7)
#define TAG_OPTIONS               (8)
#define TAG_INPUT                 (9)
#define TAG_HELP                  (10)
#define TAG_SWITCH                (11)
#define TAG_CHECKBIT              (12)
#define TAG_EXIT                  (101)
#define TAG_RETURN                ((UINTN)(-1))

//#define TAG_LEGACY              (6)
//#define TAG_SECURE_BOOT         (13)
//#define TAG_SECURE_BOOT_CONFIG  (14)
//#define TAG_CLOVER              (100)

//
// lib module
//

typedef struct {
  EFI_STATUS          LastStatus;
  EFI_FILE            *DirHandle;
  BOOLEAN             CloseDirHandle;
  EFI_FILE_INFO       *LastFileInfo;
} REFIT_DIR_ITER;

typedef struct {
  UINT8 Flags;
  UINT8 StartCHS[3];
  UINT8 Type;
  UINT8 EndCHS[3];
  UINT32 StartLBA;
  UINT32 Size;
} MBR_PARTITION_INFO;

#define DISK_KIND_INTERNAL      (0)
#define DISK_KIND_EXTERNAL      (1)
#define DISK_KIND_OPTICAL       (2)
#define DISK_KIND_FIREWIRE      (3)
#define DISK_KIND_NODISK        (4)
//#define DISK_KIND_BOOTER        (5)

#define BOOTING_BY_BOOTLOADER   (1)
#define BOOTING_BY_EFI          (2)
#define BOOTING_BY_BOOTEFI      (3)
#define BOOTING_BY_MBR          (4)
#define BOOTING_BY_PBR          (5)
#define BOOTING_BY_CD           (6)

#define OSTYPE_OSX              (1)
#define OSTYPE_WIN              (2)
#define OSTYPE_VAR              (3)
#define OSTYPE_LIN              (4)
#define OSTYPE_LINEFI           (5)
#define OSTYPE_EFI              (6)
#define OSTYPE_WINEFI           (7)
#define OSTYPE_RECOVERY         (8)
#define OSTYPE_OSX_INSTALLER    (9)
//#define OSTYPE_BOOT_OSX       (9)
#define OSTYPE_OTHER            (99)
//#define OSTYPE_HIDE           (100)

#define OSTYPE_IS_OSX(type) ((type == OSTYPE_OSX) || (type == OSTYPE_VAR))
#define OSTYPE_IS_OSX_RECOVERY(type) ((type == OSTYPE_RECOVERY) /*|| ((type >= OSTYPE_TIGER) && (type <= OSTYPE_MAV))*/ || (type == OSTYPE_VAR))
#define OSTYPE_IS_OSX_INSTALLER(type) ((type == OSTYPE_OSX_INSTALLER) /*|| ((type >= OSTYPE_TIGER) && (type <= OSTYPE_MAV))*/ || (type == OSTYPE_VAR))
#define OSTYPE_IS_OSX_GLOB(type) (OSTYPE_IS_OSX(type) || OSTYPE_IS_OSX_RECOVERY(type) || OSTYPE_IS_OSX_INSTALLER(type))
#define OSTYPE_IS_WINDOWS(type) ((type == OSTYPE_WIN) || (type == OSTYPE_WINEFI) || (type == OSTYPE_EFI) || (type == OSTYPE_VAR))
#define OSTYPE_IS_WINDOWS_GLOB(type) ((type == OSTYPE_WIN) || (type == OSTYPE_WINEFI))
#define OSTYPE_IS_LINUX(type) ((type == OSTYPE_LIN) || (type == OSTYPE_LINEFI) || (type == OSTYPE_EFI) || (type == OSTYPE_VAR))
#define OSTYPE_IS_LINUX_GLOB(type) ((type == OSTYPE_LIN) || (type == OSTYPE_LINEFI))
#define OSTYPE_IS_OTHER(type) ((type == OSTYPE_OTHER) || (type == OSTYPE_EFI) || (type == OSTYPE_VAR))
//#define OSTYPE_IS_NOT(type1, type2) (type1 != type2)
#define OSTYPE_COMPARE_IMP(comparator, type1, type2) (comparator(type1) && comparator(type2))
#define OSTYPE_COMPARE(type1, type2) \
          (OSTYPE_COMPARE_IMP(OSTYPE_IS_OSX, type1, type2) || OSTYPE_COMPARE_IMP(OSTYPE_IS_OSX_RECOVERY, type1, type2) || \
          OSTYPE_COMPARE_IMP(OSTYPE_IS_OSX_INSTALLER, type1, type2) || OSTYPE_COMPARE_IMP(OSTYPE_IS_WINDOWS, type1, type2) || \
          OSTYPE_COMPARE_IMP(OSTYPE_IS_LINUX, type1, type2) || OSTYPE_COMPARE_IMP(OSTYPE_IS_OTHER, type1, type2))

#define OSFLAG_ISSET(flags, flag) ((flags & flag) == flag)
#define OSFLAG_ISUNSET(flags, flag) ((flags & flag) != flag)
#define OSFLAG_SET(flags, flag) (flags | flag)
#define OSFLAG_UNSET(flags, flag) (flags & (~flag))
#define OSFLAG_TOGGLE(flags, flag) (flags ^ flag)

#define OSFLAG_USEGRAPHICS      (1 << 0)
#define OSFLAG_WITHKEXTS        (1 << 1)
//#define OSFLAG_CHECKFAKESMC   (1 << 2)
#define OSFLAG_NOCACHES         (1 << 2)
#define OSFLAG_NODEFAULTARGS    (1 << 3)
#define OSFLAG_NODEFAULTMENU    (1 << 4)
#define OSFLAG_HIDDEN           (1 << 5)
#define OSFLAG_DISABLED         (1 << 6)
#define OSFLAG_HIBERNATED       (1 << 7)
#define OSFLAG_NOSIP            (1 << 8)
#define OSFLAG_DBGPATCHES       (1 << 9)

#define OPT_VERBOSE             (1 << 0)
#define OPT_SINGLE_USER         (1 << 1)
#define OPT_SAFE                (1 << 2)

#define OPT_QUIET               (1 << 3)
#define OPT_SPLASH              (1 << 4)
#define OPT_NOMODESET           (1 << 5)
#define OPT_HDD                 (1 << 6)
#define OPT_CDROM               (1 << 7)

#define IS_EXTENDED_PART_TYPE(type) ((type) == 0x05 || (type) == 0x0f || (type) == 0x85)

typedef struct {
  UINT8                 Type;
  CHAR16                *IconName;
  CHAR16                *Name;
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
  BOOLEAN               IsAppleLegacy;
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
  UINT64                SleepImageOffset;
} REFIT_VOLUME;

//GUI types
typedef enum {
  BoolValue,
  Decimal,
  Hex,
  ASString,
  UNIString,
  RadioSwitch,
  CheckBit,
} ITEM_TYPE;

typedef struct {
  ITEM_TYPE   ItemType; //string, value, boolean
  BOOLEAN     Valid;
  BOOLEAN     BValue;
  UINT32      IValue;
  //UINT64    UValue;
  //CHAR8     *AValue;
  CHAR16      *SValue; // Max Size (see below) so the field can be edit by the GUI
  UINTN       LineShift;
  UINTN       ID;
} INPUT_ITEM;

// Allow for 255 unicode characters + 2 byte unicode null terminator.
#define SVALUE_MAX_SIZE 512
#define AVALUE_MAX_SIZE 256

typedef struct {
  INTN      CurrentSelection, LastSelection;
  INTN      MaxScroll, MaxIndex;
  INTN      FirstVisible, LastVisible, MaxVisible, MaxFirstVisible;
  INTN      ScrollMode;
  BOOLEAN   IsScrolling, PaintAll, PaintSelection;
} SCROLL_STATE;

extern BOOLEAN    ScrollEnabled;
extern EG_RECT    ScrollStart;
extern EG_RECT    ScrollEnd;

extern EG_RECT    DownButton;
extern EG_RECT    ScrollbarBackground;
extern EG_RECT    Scrollbar;

typedef struct UI_IMG
{
  EG_IMAGE    *Image;
  CHAR16      *Path;
  CHAR16      *Format;
} UI_IMG;

typedef enum {
  kRadioImage,
  kRadioSelectedImage,
  kCheckboxImage,
  kCheckboxCheckedImage,
} BUTTON_IMG_K;

extern UI_IMG   ButtonsImg[];

typedef enum {
  kBigImage,
  kBig2Image,
  kSmallImage,
  kSmall2Image,
  kIndicatorImage,
  kIndicator2Image,
} SELECTION_IMG_K;

extern UI_IMG   SelectionImg[];
//extern INTN    ButtonsImgCount;

#define SCREEN_UNKNOWN      (0)
#define SCREEN_MAIN         (1)
#define SCREEN_ABOUT        (2)
#define SCREEN_HELP         (3)
#define SCREEN_OPTIONS      (4)
#define SCREEN_CONFIGS      (5)
#define SCREEN_DEVICES      (6)
#define SCREEN_PATCHES      (7)
#define SCREEN_DEBUG        (8)
#define SCREEN_TABLES       (9)
#define SCREEN_DSDT         (10)
#define SCREEN_THEMES       (11)

//some unreal values
#define SCREEN_EDGE_LEFT    (50000)
#define SCREEN_EDGE_TOP     (60000)
#define SCREEN_EDGE_RIGHT   (70000)
#define SCREEN_EDGE_BOTTOM  (80000)

#define MAX_ANIME           (41)

typedef struct _refit_menu_screen REFIT_MENU_SCREEN;

typedef struct _refit_menu_entry {
  UINTN               ID;
  CHAR16              *Title;
  UINTN               Tag;
  UINTN               Row;
  CHAR16              ShortcutDigit;
  CHAR16              ShortcutLetter;
  EG_IMAGE            *Image;
  EG_IMAGE            *ImageHover;
  EG_IMAGE            *DriveImage;
  EG_IMAGE            *BadgeImage;
  EG_RECT             Place;
  REFIT_MENU_SCREEN   *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_input_dialog {
  REFIT_MENU_ENTRY  Entry;
  INPUT_ITEM        *Item;
} REFIT_INPUT_DIALOG;

//some unreal values
#define FILM_CENTRE     40000
#define INITVALUE       40000

struct _refit_menu_screen {
  UINTN             ID;
  CHAR16            *Title;
  EG_IMAGE          *TitleImage;
  INTN              InfoLineCount;
  CHAR16            **InfoLines;
  INTN              EntryCount;
  REFIT_MENU_ENTRY  **Entries;
  INTN              TimeoutSeconds;
  CHAR16            *TimeoutText;
  CHAR16            *Theme;
  BOOLEAN           AnimeRun;
  BOOLEAN           Once;
  UINT64            LastDraw;
  INTN              CurrentFrame;
  INTN              Frames;
  UINTN             FrameTime; //ms
  EG_RECT           FilmPlace;
  EG_IMAGE          **Film;
};

#define VOLTYPE_OPTICAL               (0x0001)
#define VOLTYPE_EXTERNAL              (0x0002)
#define VOLTYPE_INTERNAL              (0x0003)
#define VOLTYPE_FIREWIRE              (0x0004)

#define HIDEUI_FLAG_SHELL             (0x0010)
#define HIDEUI_FLAG_TOOLS             (0x0020)
#define HIDEUI_FLAG_HELP              (0x0030)
#define HIDEUI_FLAG_RESET             (0x0040)
#define HIDEUI_FLAG_SHUTDOWN          (0x0050)
#define HIDEUI_FLAG_EXIT              (0x0060)

#define HIDEUI_FLAG_BANNER            (0x0100)
#define HIDEUI_FLAG_FUNCS             (0x0200)

#define HIDEUI_FLAG_LABEL             (0x1000)
#define HIDEUI_FLAG_MENU_TITLE        (0x2000)
#define HIDEUI_FLAG_MENU_TITLE_IMAGE  (0x3000)
#define HIDEUI_FLAG_REVISION_TEXT     (0x4000)
#define HIDEUI_FLAG_HELP_TEXT         (0x5000)

#define HIDEUI_ALL                    (0XFFFF & (~VOLTYPE_INTERNAL))

#define HDBADGES_SWAP                 (1<<0)
#define HDBADGES_SHOW                 (1<<1)
#define HDBADGES_INLINE               (1<<2)

typedef enum {
  None,
  Scale,
  Crop,
  Tile
} SCALING;

typedef struct {
  BOOLEAN     TextOnly;
  INTN        Timeout;
  UINTN       DisableFlags;
  UINTN       HideBadges;
  UINTN       HideUIFlags;
  BOOLEAN     Quiet;
  BOOLEAN     DebugLog;
  BOOLEAN     FastBoot;
  BOOLEAN     NeverHibernate;
  FONT_TYPE   Font;
  INTN        CharWidth;
  //INTN        CharHeight;
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
  //INTN        ConsoleMode;
  CHAR16      *BackgroundName;
  SCALING     BackgroundScale;
  INT32       BackgroundSharp;
  BOOLEAN     BackgroundDark;
  //BOOLEAN     CustomIcons;
  BOOLEAN     SelectionOnTop;
  BOOLEAN     BootCampStyle;
  INTN        BadgeOffsetX;
  INTN        BadgeOffsetY;
  INTN        BadgeScale;
  INTN        ThemeDesignWidth;
  INTN        ThemeDesignHeight;
  INTN        BannerPosX;
  INTN        BannerPosY;
  INTN        BannerEdgeHorizontal;
  INTN        BannerEdgeVertical;
  INTN        BannerNudgeX;
  INTN        BannerNudgeY;
  //BOOLEAN     VerticalLayout;
  BOOLEAN     NonSelectedGrey;
  INTN        MainEntriesSize;
  INTN        TileXSpace;
  INTN        TileYSpace;
  //BOOLEAN     Proportional;
  BOOLEAN     NoEarlyProgress;
  INTN        PruneScrollRows;
  INTN        IconFormat;

  INTN        row0TileSize;
  INTN        row1TileSize;
  INTN        LayoutBannerOffset;
  INTN        LayoutButtonOffset;
  INTN        LayoutTextOffset;
  INTN        LayoutAnimMoveForMenuX;

  INTN        ScrollWidth;
  INTN        ScrollButtonsHeight;
  INTN        ScrollBarDecorationsHeight;
  INTN        ScrollScrollDecorationsHeight;

} REFIT_CONFIG;

// types

typedef struct {
  CHAR8     *Name;
  CHAR8     *Label;
  CHAR8     *Filename; // Private
  BOOLEAN   IsPlistPatch;
  INTN      DataLen;

  UINT8     *Data;
  UINT8     *Patch;
  CHAR8     *MatchOS;
  CHAR8     *MatchBuild;
  BOOLEAN   Disabled;
  BOOLEAN   Patched;
} KEXT_PATCH;

typedef struct {
  CHAR8     *Label;
  INTN      DataLen;
  UINT8     *Data;
  UINT8     *Patch;
  INTN      Count;
  CHAR8     *MatchOS;
  CHAR8     *MatchBuild;
  BOOLEAN   Disabled;
} KERNEL_PATCH;

typedef struct KERNEL_AND_KEXT_PATCHES
{
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
  INT32           NrKexts;
  KEXT_PATCH      *KextPatches;


  INT32           NrForceKexts;
  CHAR16          **ForceKexts;

  INT32           NrKernels;
  KERNEL_PATCH    *KernelPatches;
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
  CHAR8                     *BuildVersion;
  KERNEL_AND_KEXT_PATCHES   *KernelAndKextPatches;
  CHAR16                    *Settings;
} LOADER_ENTRY;

#define ANIME_INFINITE      ((UINTN)-1)

typedef struct GUI_ANIME GUI_ANIME;
struct GUI_ANIME {
  UINTN       ID;
  CHAR16      *Path;
  UINTN       Frames;
  UINTN       FrameTime;
  INTN        FilmX, FilmY;  //relative
  INTN        ScreenEdgeHorizontal;
  INTN        ScreenEdgeVertical;
  INTN        NudgeX, NudgeY;
  BOOLEAN     Once;
  GUI_ANIME   *Next;
};

extern EFI_HANDLE         SelfImageHandle;
extern EFI_LOADED_IMAGE   *SelfLoadedImage;
extern EFI_FILE           *SelfRootDir;
extern EFI_FILE           *SelfDir;
extern EFI_DEVICE_PATH    *SelfFullDevicePath;
extern EFI_FILE           *ThemeDir;
extern CHAR16             *ThemePath;
extern EFI_FILE           *OEMDir;
extern CHAR16             *OEMPath;

extern BOOLEAN            MainAnime;
extern GUI_ANIME          *GuiAnime;

extern REFIT_VOLUME       *SelfVolume;
extern REFIT_VOLUME       **Volumes;
extern UINTN              VolumesCount;

extern EG_IMAGE           *Banner;
extern EG_IMAGE           *BigBack;
extern EG_IMAGE           *FontImage;
extern EG_IMAGE           *FontImageHover;

extern BOOLEAN            gThemeChanged;
extern BOOLEAN            gBootChanged;
extern BOOLEAN            gThemeOptionsChanged;
extern BOOLEAN            gTextOnly;

extern REFIT_MENU_SCREEN  OptionMenu;

EFI_STATUS    InitRefitLib (IN EFI_HANDLE ImageHandle);
//EFI_STATUS    GetRootFromPath (IN EFI_DEVICE_PATH_PROTOCOL* DevicePath, OUT EFI_FILE **Root);
VOID          UninitRefitLib ();
//EFI_STATUS    ReinitRefitLib ();
EFI_STATUS    ReinitSelfLib ();
//VOID          PauseForKey (IN CHAR16 *Msg);
BOOLEAN       IsEmbeddedTheme ();

VOID          CreateList (OUT VOID ***ListPtr, OUT UINTN *ElementCount, IN UINTN InitialElementCount);
VOID          AddListElement (IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement);
//VOID        FreeList (IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount /*, IN Callback*/);

VOID          GetListOfThemes ();
VOID          GetListOfACPI ();
VOID          GetListOfConfigs ();

VOID          ScanVolumes ();
REFIT_VOLUME  *FindVolumeByName (IN CHAR16 *VolName);

BOOLEAN       FileExists (IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath);
BOOLEAN       DeleteFile (IN EFI_FILE *Root, IN CHAR16 *RelativePath);

EFI_STATUS    DirNextEntry (IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode);
VOID          DirIterOpen (IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath OPTIONAL, OUT REFIT_DIR_ITER *DirIter);
BOOLEAN       DirIterNext (IN OUT REFIT_DIR_ITER *DirIter, IN UINTN FilterMode, IN CHAR16 *FilePattern OPTIONAL, OUT EFI_FILE_INFO **DirEntry);
EFI_STATUS    DirIterClose (IN OUT REFIT_DIR_ITER *DirIter);

CHAR16        *Basename (IN CHAR16 *Path);
CHAR16        *Dirname (IN CHAR16 *Path);
VOID          ReplaceExtension (IN OUT CHAR16 *Path, IN CHAR16 *Extension);
CHAR16        *egFindExtension (IN CHAR16 *FileName);

INTN          FindMem (IN VOID *Buffer, IN UINTN BufferLength, IN VOID *SearchString, IN UINTN SearchStringLength);

CHAR8
*SearchString (
  IN  CHAR8       *Source,
  IN  UINT64      SourceSize,
  IN  CHAR8       *Search,
  IN  UINTN       SearchSize
);

CHAR16        *FileDevicePathToStr (IN EFI_DEVICE_PATH_PROTOCOL *DevPath);
CHAR16        *FileDevicePathFileToStr (IN EFI_DEVICE_PATH_PROTOCOL *DevPath);

EFI_STATUS    InitializeUnicodeCollationProtocol ();

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

#define LAYOUT_TEXT_WIDTH         (500)
#define LAYOUT_TOTAL_HEIGHT       (376)
#define LAYOUT_BANNER_HEIGHT      (32)

#define INDICATOR_SIZE            (52)

extern INTN       FontWidth;
extern INTN       FontHeight;
extern INTN       TextHeight;

extern UINTN      ConWidth;
extern UINTN      ConHeight;
extern CHAR16     *BlankLine;

extern INTN       UGAWidth;
extern INTN       UGAHeight;
extern BOOLEAN    AllowGraphicsMode;

//extern EG_PIXEL InputBackgroundPixel;
extern EG_PIXEL   StdBackgroundPixel;
extern EG_PIXEL   MenuBackgroundPixel;
extern EG_PIXEL   BlueBackgroundPixel;
extern EG_PIXEL   DarkBackgroundPixel;
extern EG_PIXEL   SelectionBackgroundPixel;

extern EG_RECT    BannerPlace;
extern EG_IMAGE   *BackgroundImage;

VOID  InitScreen (IN BOOLEAN SetMaxResolution);
VOID  SetupScreen ();
VOID  BeginTextScreen (IN CHAR16 *Title);
VOID  FinishTextScreen (IN BOOLEAN WaitAlways);
VOID  BeginExternalScreen (IN BOOLEAN UseGraphicsMode, IN CHAR16 *Title);
VOID  FinishExternalScreen ();
VOID  TerminateScreen ();
VOID  SetNextScreenMode (INT32 Next);

CHAR16
*GetRevisionString (
  BOOLEAN   CloverRev
);

EG_PIXEL ToPixel (UINTN rgba);

#if REFIT_DEBUG > 0
VOID DebugPause();
#else
#define DebugPause()
#endif

//VOID EndlessIdleLoop();

BOOLEAN CheckFatalError (IN EFI_STATUS Status, IN CHAR16 *where);
BOOLEAN CheckError (IN EFI_STATUS Status, IN CHAR16 *where);

VOID  SwitchToGraphicsAndClear ();
VOID  BltClearScreen (IN BOOLEAN ShowBanner);
VOID  BltImage (IN EG_IMAGE *Image, IN INTN XPos, IN INTN YPos);
VOID  BltImageAlpha (IN EG_IMAGE *Image, IN INTN XPos, IN INTN YPos, IN EG_PIXEL *BackgroundPixel, INTN Scale);
VOID  BltImageComposite (IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN INTN XPos, IN INTN YPos);

VOID
BltImageCompositeBadge (
  IN EG_IMAGE   *BaseImage,
  IN EG_IMAGE   *TopImage,
  IN EG_IMAGE   *BadgeImage,
  IN INTN       XPos,
  IN INTN       YPos,
  INTN          Scale
);

BOOLEAN   GetAnime (REFIT_MENU_SCREEN *Screen);
VOID      InitAnime (REFIT_MENU_SCREEN *Screen);
VOID      UpdateAnime (REFIT_MENU_SCREEN *Screen, EG_RECT *Place);
VOID      FreeAnime (GUI_ANIME *Anime);

//
// icns loader module
//
EG_IMAGE
*LoadOSIcon (
  IN  CHAR16    *OSIconName OPTIONAL,
  OUT CHAR16    **OSIconNameHover,
  IN  CHAR16    *FallbackIconName,
  IN  UINTN     PixelSize,
  IN  BOOLEAN   BootLogo,
  IN  BOOLEAN   WantDummy
);

EG_IMAGE    *LoadHoverIcon (IN CHAR16 *OSIconName, IN UINTN PixelSize);
//EG_IMAGE  *LoadIcns (IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize);
//EG_IMAGE  *LoadIcnsFallback (IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize);
EG_IMAGE    *DummyImage (IN UINTN PixelSize);
EG_IMAGE    *BuiltinIcon (IN UINTN Id);
EG_IMAGE    *GetSmallHover (IN UINTN Id);
CHAR16      *GetIconsExt (IN CHAR16 *Icon, IN CHAR16 *Def);
EG_IMAGE    *LoadBuiltinIcon (IN CHAR16 *IconName);
EG_IMAGE    *ScanVolumeDefaultIcon (REFIT_VOLUME *Volume, IN UINT8 OSType);

VOID      InitUISelection ();
VOID      InitBar ();
VOID      SetBar (INTN PosX, INTN UpPosY, INTN DownPosY, IN SCROLL_STATE *State);

#define BUILTIN_ICON_FUNC_ABOUT           (0)
#define BUILTIN_ICON_FUNC_OPTIONS         (1)
//#define BUILTIN_ICON_FUNC_CLOVER        (2)
#define BUILTIN_ICON_FUNC_RESET           (2)
//#define BUILTIN_ICON_FUNC_SHUTDOWN      (3)
#define BUILTIN_ICON_FUNC_EXIT            (3)
#define BUILTIN_ICON_FUNC_HELP            (4)
#define BUILTIN_ICON_TOOL_SHELL           (5)
//#define BUILTIN_ICON_POINTER            (8)
#define BUILTIN_ICON_VOL_INTERNAL         (6)
#define BUILTIN_ICON_VOL_EXTERNAL         (7)
#define BUILTIN_ICON_VOL_OPTICAL          (8)
#define BUILTIN_ICON_VOL_FIREWIRE         (9)
#define BUILTIN_ICON_VOL_BOOTER           (10)
#define BUILTIN_ICON_VOL_INTERNAL_HFS     (11)
#define BUILTIN_ICON_VOL_INTERNAL_NTFS    (12)
#define BUILTIN_ICON_VOL_INTERNAL_EXT3    (13)
#define BUILTIN_ICON_VOL_INTERNAL_REC     (14)
#define BUILTIN_ICON_BANNER               (15)
#define BUILTIN_SELECTION_SMALL           (16)
#define BUILTIN_SELECTION_BIG             (17)
#define BUILTIN_ICON_COUNT                (18)

//
// menu module
//

#define MENU_EXIT_ENTER                   (1)
#define MENU_EXIT_ESCAPE                  (2)
#define MENU_EXIT_DETAILS                 (3)
#define MENU_EXIT_TIMEOUT                 (4)
#define MENU_EXIT_OPTIONS                 (5)
#define MENU_EXIT_EJECT                   (6)
#define MENU_EXIT_HELP                    (7)
#define MENU_EXIT_HIDE_TOGGLE             (8)

#define MENU_ENTRY_ID_GEN                 (0)
#define MENU_ENTRY_ID_BOOT                (1)

#define X_IS_LEFT                         (64)
#define X_IS_RIGHT                        (0)
#define X_IS_CENTER                       (1)
#define BADGE_DIMENSION                   (64)

// IconFormat
#define ICON_FORMAT_DEF                   (0)
#define ICON_FORMAT_ICNS                  (1)
#define ICON_FORMAT_PNG                   (2)
//#define ICON_FORMAT_BMP                 (3)

VOID      AddMenuInfoLine (IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *InfoLine);
//VOID    AddMenuInfo (IN REFIT_MENU_SCREEN  *SubScreen, IN CHAR16 *Line);
VOID      AddMenuEntry (IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry);
//VOID    AddMenuCheck (REFIT_MENU_SCREEN *SubScreen, CONST CHAR8 *Text, UINTN Bit, INTN ItemNum);
VOID      FreeMenu (IN REFIT_MENU_SCREEN *Screen);
UINTN     RunMenu (IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry);
UINTN     RunMainMenu (IN REFIT_MENU_SCREEN *Screen, IN INTN DefaultSelection, OUT REFIT_MENU_ENTRY **ChosenEntry);
VOID      AddOptionEntries (REFIT_MENU_SCREEN *SubScreen, LOADER_ENTRY *SubEntry);
VOID      ReinitVolumes ();
BOOLEAN   ReadAllKeyStrokes ();
VOID      OptionsMenu (OUT REFIT_MENU_ENTRY **ChosenEntry);
VOID      ScrollingBar (IN SCROLL_STATE *State);
VOID      FreeScrollBar ();
VOID      FreeButtons ();
VOID      FreeSelections ();
VOID      FreeBuiltinIcons ();
VOID      FreeBanner ();
VOID      FreeAnims ();
VOID      DrawFuncIcons ();
INTN      DrawTextXY (IN CHAR16 *Text, IN INTN XPos, IN INTN YPos, IN UINT8 XAlign);
VOID      DrawBCSText (IN CHAR16 *Text, IN INTN XPos, IN INTN YPos, IN UINT8 XAlign);
//VOID    DrawMenuText (IN CHAR16 *Text, IN INTN SelectedWidth, IN INTN XPos, IN INTN YPos, IN INTN Cursor);
INTN      GetSubMenuCount ();
VOID      SplitInfoLine (IN REFIT_MENU_SCREEN *SubScreen, IN CHAR16 *Str);
BOOLEAN   BootArgsExists (IN CHAR16 *LoadOptions, IN CHAR16 *LoadOption);

UINT32    EncodeOptions (CHAR16 *Options);
//VOID    DecodeOptions (LOADER_ENTRY *Entry);
VOID      HelpRefit ();
VOID      AboutRefit ();

LOADER_ENTRY *DuplicateLoaderEntry (IN LOADER_ENTRY *Entry);

//
// config module
//

//extern BUILTIN_ICON BuiltinIconTable[];
extern REFIT_CONFIG   GlobalConfig;
extern REFIT_CONFIG   DefaultConfig;

VOID ReadConfig (INTN What);

EG_IMAGE
*egEnsureImageSize (
  IN EG_IMAGE   *Image,
  IN INTN       Width,
  IN INTN       Height,
  IN EG_PIXEL   *Color
);

EFI_STATUS egLoadFile (
  IN EFI_FILE_HANDLE  BaseDir,
  IN CHAR16           *FileName,
  OUT UINT8           **FileData,
  OUT UINTN           *FileDataLength
);

EFI_STATUS egSaveFile (
  IN EFI_FILE_HANDLE  BaseDir OPTIONAL,
  IN CHAR16           *FileName,
  IN UINT8            *FileData,
  IN UINTN            FileDataLength
);

EFI_STATUS
egMkDir (
  IN EFI_FILE_HANDLE    BaseDir OPTIONAL,
  IN CHAR16             *DirName
);

EFI_STATUS
egFindESP (
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

EFI_FILE_SYSTEM_VOLUME_LABEL
*EfiLibFileSystemVolumeLabelInfo (
  IN EFI_FILE_HANDLE    FHand
);

CHAR16
*EfiStrDuplicate (
  IN CHAR16   *Src
);

INTN StriCmp (
  IN CONST CHAR16   *FirstString,
  IN CONST CHAR16   *SecondString
);

//INTN EFIAPI AsciiStriCmp (
//  IN CONST CHAR8    *FirstString,
//  IN CONST CHAR8    *SecondString
//);

BOOLEAN AsciiStriNCmp (
  IN CONST CHAR8    *FirstString,
  IN CONST CHAR8    *SecondString,
  IN CONST UINTN     sSize
);

BOOLEAN AsciiStrStriN (
  IN CONST CHAR8    *WhatString,
  IN CONST UINTN     sWhatSize,
  IN CONST CHAR8    *WhereString,
  IN CONST UINTN     sWhereSize
);

EFI_FILE_INFO           *EfiLibFileInfo (IN EFI_FILE_HANDLE FHand);
EFI_FILE_SYSTEM_INFO    *EfiLibFileSystemInfo (IN EFI_FILE_HANDLE Root);

//UINTN
//EfiDevicePathInstanceCount (
//  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
//);

VOID *
EfiReallocatePool (
  IN VOID     *OldPool,
  IN UINTN    OldSize,
  IN UINTN    NewSize
);

BOOLEAN DumpVariable (CHAR16* Name, EFI_GUID* Guid, INTN DevicePathAt);

/*
BOOLEAN
TimeCompare (
  IN EFI_TIME    *FirstTime,
  IN EFI_TIME    *SecondTime
);
*/

INTN
TimeCompare (
  IN EFI_TIME    *Time1,
  IN EFI_TIME    *Time2
);

#define KERNEL_MAX_SIZE 40000000
//#define FSearchReplace (Source, Search, Replace) SearchAndReplace(Source, KERNEL_MAX_SIZE, Search, sizeof(Search), Replace, 1)
#define FSearchReplace (Source, Size, Search, Replace) SearchAndReplace((UINT8*)(UINTN)Source, Size, Search, sizeof(Search), Replace, 1)
BOOLEAN IsKernelIs64BitOnly (IN LOADER_ENTRY *Entry);
VOID    DbgHeader (CHAR8 *str);

#endif

/*
 EOF
*/
