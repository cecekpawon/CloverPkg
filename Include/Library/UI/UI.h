/*
 * libeg/libeg.h
 * EFI graphics library header for users
 *
 * Copyright (c) 2006 Christoph Pfisterer
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

#ifndef __LIBEG_UI_H__
#define __LIBEG_UI_H__

#define TEXT_YMARGIN                            (2)
#define TEXT_XMARGIN                            (8)

#define EG_EIPIXELMODE_GRAY                     (0)
#define EG_EIPIXELMODE_GRAY_ALPHA               (1)
#define EG_EIPIXELMODE_COLOR                    (2)
#define EG_EIPIXELMODE_COLOR_ALPHA              (3)
#define EG_EIPIXELMODE_ALPHA                    (4)
#define EG_MAX_EIPIXELMODE                      EG_EIPIXELMODE_ALPHA

#define EG_EICOMPMODE_NONE                      (0)
#define EG_EICOMPMODE_RLE                       (1)
#define EG_EICOMPMODE_EFICOMPRESS               (2)

//some unreal values
#define FILM_CENTRE                             40000
#define INITVALUE                               40000

#define HIDEUI_FLAG_SHELL                       (0x0010)
#define HIDEUI_FLAG_TOOLS                       (0x0020)
#define HIDEUI_FLAG_HELP                        (0x0030)
#define HIDEUI_FLAG_RESET                       (0x0040)
#define HIDEUI_FLAG_SHUTDOWN                    (0x0050)
#define HIDEUI_FLAG_EXIT                        (0x0060)

#define HIDEUI_FLAG_BANNER                      (0x0100)
#define HIDEUI_FLAG_FUNCS                       (0x0200)

#define HIDEUI_FLAG_LABEL                       (0x1000)
#define HIDEUI_FLAG_MENU_TITLE                  (0x2000)
#define HIDEUI_FLAG_MENU_TITLE_IMAGE            (0x3000)
#define HIDEUI_FLAG_REVISION_TEXT               (0x4000)
#define HIDEUI_FLAG_HELP_TEXT                   (0x5000)

#define HIDEUI_ALL                              (0XFFFF & (~VOLTYPE_INTERNAL))

#define HDBADGES_SWAP                           (1 << 0)
#define HDBADGES_SHOW                           (1 << 1)
#define HDBADGES_INLINE                         (1 << 2)

#define SCREEN_UNKNOWN                          (0)
#define SCREEN_MAIN                             (1)
#define SCREEN_ABOUT                            (2)
#define SCREEN_HELP                             (3)
#define SCREEN_OPTIONS                          (4)
#define SCREEN_CONFIGS                          (5)
#define SCREEN_DEVICES                          (6)
#define SCREEN_PATCHES                          (7)
#define SCREEN_DEBUG                            (8)
#define SCREEN_TABLES                           (9)
#define SCREEN_DSDT                             (10)
#define SCREEN_THEMES                           (11)

//some unreal values
#define SCREEN_EDGE_LEFT                        (50000)
#define SCREEN_EDGE_TOP                         (60000)
#define SCREEN_EDGE_RIGHT                       (70000)
#define SCREEN_EDGE_BOTTOM                      (80000)

#define MAX_ANIME                               (41)
#define ANIME_INFINITE                          ((UINTN)-1)

#define BUILTIN_ICON_FUNC_ABOUT                 (0)
#define BUILTIN_ICON_FUNC_OPTIONS               (1)
//#define BUILTIN_ICON_FUNC_CLOVER              (2)
#define BUILTIN_ICON_FUNC_RESET                 (2)
//#define BUILTIN_ICON_FUNC_SHUTDOWN            (3)
#define BUILTIN_ICON_FUNC_EXIT                  (3)
#define BUILTIN_ICON_FUNC_HELP                  (4)
#define BUILTIN_ICON_TOOL_SHELL                 (5)
//#define BUILTIN_ICON_POINTER                  (8)
#define BUILTIN_ICON_VOL_INTERNAL               (6)
#define BUILTIN_ICON_VOL_EXTERNAL               (7)
#define BUILTIN_ICON_VOL_OPTICAL                (8)
#define BUILTIN_ICON_VOL_FIREWIRE               (9)
#define BUILTIN_ICON_VOL_BOOTER                 (10)
#define BUILTIN_ICON_VOL_INTERNAL_HFS           (11)
#define BUILTIN_ICON_VOL_INTERNAL_NTFS          (12)
#define BUILTIN_ICON_VOL_INTERNAL_EXT3          (13)
#define BUILTIN_ICON_VOL_INTERNAL_REC           (14)
#define BUILTIN_ICON_BANNER                     (15)
#define BUILTIN_SELECTION_SMALL                 (16)
#define BUILTIN_SELECTION_BIG                   (17)
#define BUILTIN_ICON_COUNT                      (18)

#define TAG_ABOUT                               (1)
#define TAG_RESET                               (2)
#define TAG_SHUTDOWN                            (3)
#define TAG_TOOL                                (4)
#define TAG_LOADER                              (5)
#define TAG_LABEL                               (6)
#define TAG_INFO                                (7)
#define TAG_OPTIONS                             (8)
#define TAG_INPUT                               (9)
#define TAG_HELP                                (10)
#define TAG_SWITCH                              (11)
#define TAG_CHECKBIT                            (12)
#define TAG_EXIT                                (101)
#define TAG_RETURN                              ((UINTN)(-1))

//#define TAG_LEGACY                            (6)
//#define TAG_SECURE_BOOT                       (13)
//#define TAG_SECURE_BOOT_CONFIG                (14)
//#define TAG_CLOVER                            (100)

//
// menu module
//

#define MENU_EXIT_ENTER                   (1)
#define MENU_EXIT_ESCAPE                  (2)
#define MENU_EXIT_DETAILS                 (3)
#define MENU_EXIT_TIMEOUT                 (4)
#define MENU_EXIT_OPTIONS                 (5)
#define MENU_EXIT_HELP                    (6)
#define MENU_EXIT_HIDE_TOGGLE             (7)
#define MENU_EXIT_OTHER                   (8) // Just Exit

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

#define LAYOUT_TEXT_WIDTH                 (500)
#define LAYOUT_TOTAL_HEIGHT               (376)
#define LAYOUT_BANNER_HEIGHT              (32)

#define INDICATOR_SIZE                    (52)

typedef enum {
  //FONT_NONE,
  FONT_ALFA,
  FONT_GRAY,
  FONT_LOAD,
  FONT_RAW
} FONT_TYPE;

/* This should be compatible with EFI_UGA_PIXEL */
typedef struct {
  UINT8 b, g, r, a;
} EG_PIXEL;

typedef struct {
  INTN     XPos, YPos, Width, Height;
} EG_RECT;

typedef struct {
  INTN        Width;
  INTN        Height;
  EG_PIXEL    *PixelData;
  BOOLEAN     HasAlpha;
} EG_IMAGE;

typedef struct REFIT_MENU_SCREEN REFIT_MENU_SCREEN;

typedef struct {
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

struct REFIT_MENU_SCREEN {
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

typedef enum {
  None,
  Scale,
  Crop,
  Tile
} SCALING;

typedef struct {
        INTN    Width;
        INTN    Height;
        UINTN   PixelMode;
        UINTN   CompressMode;
  CONST UINT8   *Data;
        UINTN   DataLength;
} EG_EMBEDDED_IMAGE;

typedef struct {
  EG_IMAGE    *Image;
  CHAR16      *Path;
  CHAR16      *Format;
  UINTN       PixelSize; // for .icns
} BUILTIN_ICON;

typedef enum {
  kScrollbarBackgroundImage,
  kBarStartImage,
  kBarEndImage,
  kScrollbarImage,
  kScrollStartImage,
  kScrollEndImage,
  kUpButtonImage,
  kDownButtonImage,
} SCROLLBAR_IMG_K;

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

typedef struct {
  INTN      CurrentSelection, LastSelection,
            MaxScroll, MaxIndex,
            FirstVisible, LastVisible, MaxVisible, MaxFirstVisible,
            ScrollMode, FinalRow0, InitialRow1;

  BOOLEAN   IsScrolling, PaintAll, PaintSelection;
} SCROLL_STATE;

typedef struct UI_IMG {
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

typedef enum {
  kBigImage,
  kBig2Image,
  kSmallImage,
  kSmall2Image,
  kIndicatorImage,
  kIndicator2Image,
} SELECTION_IMG_K;

typedef struct GUI_ANIME {
          UINTN       ID;
          CHAR16      *Path;
          UINTN       Frames;
          UINTN       FrameTime;
          INTN        FilmX, FilmY;  //relative
          INTN        ScreenEdgeHorizontal;
          INTN        ScreenEdgeVertical;
          INTN        NudgeX, NudgeY;
          BOOLEAN     Once;
  struct  GUI_ANIME   *Next;
} GUI_ANIME;

// Menu

typedef VOID (*ADD_MENU_INFO)(IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *InfoLine);

VOID
AddMenuInfoLine (
  IN REFIT_MENU_SCREEN  *Screen,
  IN CHAR16             *InfoLine
);

//VOID
//AddMenuInfo (
//  REFIT_MENU_SCREEN   *SubScreen,
//  CHAR16              *Label
//);

VOID
AddMenuEntry (
  IN REFIT_MENU_SCREEN  *Screen,
  IN REFIT_MENU_ENTRY   *Entry
);

//REFIT_MENU_ENTRY *
//AddMenuCheck (
//  REFIT_MENU_SCREEN   *Screen,
//  CHAR16              *Title,
//  UINTN               Bit,
//  INTN                ItemNum
//);

VOID
FreeMenu (
  IN REFIT_MENU_SCREEN    *Screen
);

UINTN
RunMenu (
  IN  REFIT_MENU_SCREEN   *Screen,
  OUT REFIT_MENU_ENTRY    **ChosenEntry
);

UINTN
RunMainMenu (
  IN  REFIT_MENU_SCREEN   *Screen,
  IN  INTN                DefaultSelection,
  OUT REFIT_MENU_ENTRY    **ChosenEntry
);

//VOID
//AddOptionEntries (
//  REFIT_MENU_SCREEN   *SubScreen,
//  LOADER_ENTRY        *SubEntry
//);

VOID
OptionsMenu (
  OUT REFIT_MENU_ENTRY    **ChosenEntry
);

//UINT32
//EncodeOptions (
//  CHAR16  *Options
//);

//VOID
//DecodeOptions (
//  LOADER_ENTRY    *Entry
//);

VOID
ScrollingBar (
  IN SCROLL_STATE   *State
);

VOID
FreeScrollBar ();

VOID
FreeButtons ();

VOID
FreeSelections ();

VOID
FreeBuiltinIcons ();

VOID
FreeBanner ();

VOID
FreeAnims ();

VOID
DrawFuncIcons ();

INTN
DrawTextXY (
  IN CHAR16   *Text,
  IN INTN     XPos,
  IN INTN     YPos,
  IN UINT8    XAlign
);

VOID
DrawBCSText (
  IN CHAR16     *Text,
  IN INTN       XPos,
  IN INTN       YPos,
  IN UINT8      XAlign
);

//VOID
//DrawMenuText (
//  IN CHAR16     *Text,
//  IN INTN       SelectedWidth,
//  IN INTN       XPos,
//  IN INTN       YPos,
//  IN INTN       Cursor,
//  IN EG_IMAGE   *Button
//);

INTN
GetSubMenuCount ();

VOID
SplitMenuInfo (
  IN REFIT_MENU_SCREEN  *SubScreen,
  IN CHAR16             *Str,
  IN ADD_MENU_INFO      MenuInfo
);

VOID
HelpRefit ();

VOID
AboutRefit ();

//
// icns loader module
//

EG_IMAGE *
LoadOSIcon (
  IN  CHAR16    *OSIconName OPTIONAL,
  OUT CHAR16    **OSIconNameHover,
  IN  CHAR16    *FallbackIconName,
  IN  UINTN     PixelSize,
  IN  BOOLEAN   BootLogo,
  IN  BOOLEAN   WantDummy
);

EG_IMAGE *
LoadHoverIcon (
  IN CHAR16   *OSIconName,
  IN UINTN    PixelSize
);

//EG_IMAGE *
//LoadIcns (
//  IN EFI_FILE_HANDLE    BaseDir,
//  IN CHAR16             *FileName,
//  IN UINTN              PixelSize
//);

//EG_IMAGE *
//LoadIcnsFallback (
//  IN EFI_FILE_HANDLE  BaseDir,
//  IN CHAR16           *FileName,
//  IN UINTN            PixelSize
//);

EG_IMAGE *
DummyImage (
  IN UINTN    PixelSize
);

EG_IMAGE *
BuiltinIcon (
  IN UINTN    Id
);

EG_IMAGE *
GetSmallHover (
  IN UINTN    Id
);

CHAR16 *
GetIconsExt (
  IN CHAR16   *Icon,
  IN CHAR16   *Def
);

EG_IMAGE *
LoadBuiltinIcon (
  IN CHAR16   *IconName
);

EG_IMAGE *
ScanVolumeDefaultIcon (
  IN UINTN    DiskKind,
  IN UINT8    OSType
);

VOID
InitUISelection ();

VOID
InitBar ();

VOID
SetBar (
  IN  INTN          PosX,
  IN  INTN          UpPosY,
  IN  INTN          DownPosY,
  IN  SCROLL_STATE  *State
);

// Image

VOID
BltClearScreen (
  IN BOOLEAN  ShowBanner
);

VOID
BltImage (
  IN EG_IMAGE   *Image,
  IN INTN       XPos,
  IN INTN       YPos
);

VOID
BltImageAlpha (
  IN EG_IMAGE   *Image,
  IN INTN       XPos,
  IN INTN       YPos,
  IN EG_PIXEL   *BackgroundPixel,
  IN INTN       Scale
);

VOID
BltImageComposite (
  IN EG_IMAGE   *BaseImage,
  IN EG_IMAGE   *TopImage,
  IN INTN       XPos,
  IN INTN       YPos
);

VOID
BltImageCompositeBadge (
  IN EG_IMAGE   *BaseImage,
  IN EG_IMAGE   *TopImage,
  IN EG_IMAGE   *BadgeImage,
  IN INTN       XPos,
  IN INTN       YPos,
  IN INTN       Scale
);

BOOLEAN
GetAnime (
  REFIT_MENU_SCREEN   *Screen
);

VOID
InitAnime (
  REFIT_MENU_SCREEN   *Screen
);

VOID
UpdateAnime (
  REFIT_MENU_SCREEN   *Screen,
  EG_RECT             *Place
);

VOID
FreeAnime (
  GUI_ANIME   *Anime
);

EG_PIXEL
ToPixel (
  UINTN   rgba
);

EG_IMAGE *
EnsureImageSize (
  IN EG_IMAGE   *Image,
  IN INTN       Width,
  IN INTN       Height,
  IN EG_PIXEL   *Color
);

EG_IMAGE *
CreateImage (
  IN INTN       Width,
  IN INTN       Height,
  IN BOOLEAN    HasAlpha
);

EG_IMAGE *
CreateFilledImage (
  IN INTN       Width,
  IN INTN       Height,
  IN BOOLEAN    HasAlpha,
  IN EG_PIXEL   *Color
);

EG_IMAGE *
CopyImage (
  IN EG_IMAGE   *Image
);

EG_IMAGE *
CopyScaledImage (
  IN EG_IMAGE   *OldImage,
  IN INTN       Ratio
);

VOID
FreeImage (
  IN EG_IMAGE   *Image
);

VOID
ScaleImage (
  OUT EG_IMAGE    *NewImage,
  IN EG_IMAGE     *OldImage
);

EG_IMAGE *
LoadImage (
  IN EFI_FILE_HANDLE    BaseDir,
  IN CHAR16             *FileName,
  IN BOOLEAN            WantAlpha
);

EG_IMAGE *
LoadIcon (
  IN EFI_FILE_HANDLE  BaseDir,
  IN CHAR16           *FileName,
  IN UINTN            IconSize
);

EG_IMAGE *
PrepareEmbeddedImage (
  IN EG_EMBEDDED_IMAGE    *EmbeddedImage,
  IN BOOLEAN              WantAlpha
);

VOID
FillImage (
  IN OUT EG_IMAGE     *CompImage,
  IN EG_PIXEL         *Color
);

VOID
FillImageArea (
  IN OUT EG_IMAGE   *CompImage,
  IN INTN           AreaPosX,
  IN INTN           AreaPosY,
  IN INTN           AreaWidth,
  IN INTN           AreaHeight,
  IN EG_PIXEL       *Color
);

VOID
ComposeImage (
  IN OUT EG_IMAGE   *CompImage,
  IN EG_IMAGE       *TopImage,
  IN INTN           PosX,
  IN INTN           PosY
);

VOID
RestrictImageArea (
  IN EG_IMAGE   *Image,
  IN INTN       AreaPosX,
  IN INTN       AreaPosY,
  IN OUT INTN   *AreaWidth,
  IN OUT INTN   *AreaHeight
);

VOID
RawCopy (
  IN OUT EG_PIXEL   *CompBasePtr,
  IN EG_PIXEL       *TopBasePtr,
  IN INTN           Width,
  IN INTN           Height,
  IN INTN           CompLineOffset,
  IN INTN           TopLineOffset
);

VOID
RawCompose (
  IN OUT EG_PIXEL   *CompBasePtr,
  IN EG_PIXEL       *TopBasePtr,
  IN INTN           Width,
  IN INTN           Height,
  IN INTN           CompLineOffset,
  IN INTN           TopLineOffset
);

VOID
RawComposeOnFlat (
  IN OUT EG_PIXEL   *CompBasePtr,
  IN EG_PIXEL       *TopBasePtr,
  IN INTN           Width,
  IN INTN           Height,
  IN INTN           CompLineOffset,
  IN INTN           TopLineOffset
);

VOID
DecompressIcnsRLE (
  IN OUT  UINT8   **CompData,
  IN OUT  UINTN   *CompLen,
  IN      UINT8   *PixelData,
  IN      UINTN   PixelCount
);

VOID
InsertPlane (
  IN UINT8    *SrcDataPtr,
  IN UINT8    *DestPlanePtr,
  IN UINTN    PixelCount
);

VOID
SetPlane (
  IN UINT8    *DestPlanePtr,
  IN UINT8    Value,
  IN UINT64   PixelCount
);

VOID
CopyPlane (
  IN UINT8    *SrcPlanePtr,
  IN UINT8    *DestPlanePtr,
  IN UINTN    PixelCount
);

// Text

VOID
PrepareFont ();

VOID
MeasureText (
  IN  CHAR16    *Text,
  OUT INTN      *Width,
  OUT INTN      *Height
);

INTN
RenderText (
  IN CHAR16         *Text,
  IN OUT EG_IMAGE   *CompImage,
  IN INTN           PosX,
  IN INTN           PosY,
  IN INTN           Cursor,
     BOOLEAN        Selected
);

// Image Format

EG_IMAGE *
DecodeBMP (
  IN UINT8    *FileData,
  IN UINTN    FileDataLength,
  IN UINTN    IconSize,
  IN BOOLEAN  WantAlpha
);

EG_IMAGE *
DecodeICNS (
  IN UINT8      *FileData,
  IN UINTN      FileDataLength,
  IN UINTN      IconSize,
  IN BOOLEAN    WantAlpha
);

EG_IMAGE *
DecodePNG (
  IN UINT8      *FileData,
  IN UINTN      FileDataLength,
  IN UINTN      IconSize,
  IN BOOLEAN    WantAlpha
);

VOID
EncodeBMP (
  IN  EG_IMAGE  *Image,
  OUT UINT8     **FileDataReturn,
  OUT UINTN     *FileDataLengthReturn
);

#define PLPTR(imagevar, colorname) ((UINT8 *) &((imagevar)->PixelData->colorname))
#define DEC_PNG_BUILTIN(ico) DecodePNG (ico, ARRAY_SIZE (ico), 0, TRUE)

typedef EG_IMAGE * (*EG_DECODE_FUNC)(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize, IN BOOLEAN WantAlpha);


extern UI_IMG               ButtonsImg[];
extern UI_IMG               SelectionImg[];
//extern INTN               ButtonsImgCount;

extern INTN                 FontWidth;
extern INTN                 FontHeight;
extern INTN                 TextHeight;

extern UINTN                ConWidth;
extern UINTN                ConHeight;
extern CHAR16               *BlankLine;

extern INTN                 UGAWidth;
extern INTN                 UGAHeight;
extern BOOLEAN              AllowGraphicsMode;

extern EG_PIXEL             BlackBackgroundPixel;
extern EG_PIXEL             BlueBackgroundPixel;
extern EG_PIXEL             GrayBackgroundPixel;
extern EG_PIXEL             GreenBackgroundPixel;
extern EG_PIXEL             RedBackgroundPixel;
extern EG_PIXEL             SelectionBackgroundPixel;
extern EG_PIXEL             TmpBackgroundPixel;
extern EG_PIXEL             TransparentBackgroundPixel;

extern EG_RECT              BannerPlace;
extern EG_IMAGE             *BackgroundImage;

extern EG_IMAGE             *Banner;
extern EG_IMAGE             *BigBack;
extern EG_IMAGE             *FontImage;
extern EG_IMAGE             *FontImageHover;

extern BOOLEAN              MainAnime;
extern GUI_ANIME            *GuiAnime;

extern BOOLEAN              gThemeChanged;
extern BOOLEAN              gBootChanged;
extern BOOLEAN              gThemeOptionsChanged;
extern BOOLEAN              gTextOnly;

extern REFIT_MENU_SCREEN    OptionMenu;

extern BOOLEAN              ScrollEnabled;
extern EG_RECT              ScrollStart;
extern EG_RECT              ScrollEnd;

extern EG_RECT              DownButton;
extern EG_RECT              ScrollbarBackground;
extern EG_RECT              Scrollbar;


#endif /* __LIBEG_LIBEG_H__ */
