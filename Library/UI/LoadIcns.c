/*
 * libeg/load_icns.c
 * Loading function for .icns Apple icon images
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

#include <Library/Platform/Platform.h>
#include <Library/UI/EmbeddedIcons.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_ICNS
#define DEBUG_ICNS -1
#endif
#else
#ifdef DEBUG_ICNS
#undef DEBUG_ICNS
#endif
#define DEBUG_ICNS DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_ICNS, __VA_ARGS__)

//
// well-known icons
//

BUILTIN_ICON BuiltinIconTable[BUILTIN_ICON_COUNT] = {
  { NULL, L"icons\\func_about",         L"png",  32 },
  { NULL, L"icons\\func_options",       L"png",  32 },
  //{ NULL, L"icons\\func_clover",        L"png",  32 },
  { NULL, L"icons\\func_reset",         L"png",  32 },
  //{ NULL, L"icons\\func_shutdown",      L"png",  32 },
  { NULL, L"icons\\func_exit",          L"png",  32 },
  { NULL, L"icons\\func_help",          L"png",  32 },
  { NULL, L"icons\\tool_shell",         L"png",  32 },
  //{ NULL, L"icons\\pointer",            L"png",  32 },

  { NULL, L"icons\\vol_internal",       L"icns", 128 },
  { NULL, L"icons\\vol_external",       L"icns", 128 },
  { NULL, L"icons\\vol_optical",        L"icns", 128 },
  { NULL, L"icons\\vol_firewire",       L"icns", 128 },
  { NULL, L"icons\\vol_clover",         L"icns", 128 },
  { NULL, L"icons\\vol_internal_hfs",   L"icns", 128 },
  { NULL, L"icons\\vol_internal_ntfs",  L"icns", 128 },
  { NULL, L"icons\\vol_internal_ext3",  L"icns", 128 },
  { NULL, L"icons\\vol_recovery",       L"icns", 128 },
  { NULL, L"logo",                      L"png",  128 },

  { NULL, L"selection_small",           L"png",  64  },
  { NULL, L"selection_big",             L"png",  144 }
};

UI_IMG ScrollbarImg[] = {
  { NULL, L"scrollbar\\bar_fill",       L"png" },
  { NULL, L"scrollbar\\bar_start",      L"png" },
  { NULL, L"scrollbar\\bar_end",        L"png" },
  { NULL, L"scrollbar\\scroll_fill",    L"png" },
  { NULL, L"scrollbar\\scroll_start",   L"png" },
  { NULL, L"scrollbar\\scroll_end",     L"png" },
  { NULL, L"scrollbar\\up_button",      L"png" },
  { NULL, L"scrollbar\\down_button",    L"png" },
};

CONST INTN    ScrollbarImgCount = ARRAY_SIZE (ScrollbarImg);

UI_IMG ButtonsImg[] = {
  { NULL, L"radio_button",              L"png" },
  { NULL, L"radio_button_selected",     L"png" },
  { NULL, L"checkbox",                  L"png" },
  { NULL, L"checkbox_checked",          L"png" },
};

CONST INTN    ButtonsImgCount = ARRAY_SIZE (ButtonsImg);

UI_IMG SelectionImg[] = {
  { NULL, NULL, NULL },
  { NULL, NULL, NULL },
  { NULL, NULL, NULL },
  { NULL, NULL, NULL },
  { NULL, NULL, NULL },
  { NULL, NULL, NULL },
};

CONST INTN      SelectionImgCount = ARRAY_SIZE (SelectionImg);
      INTN      ScrollbarYMovement;

      BOOLEAN   ScrollEnabled = FALSE, IsDragging = FALSE;

      EG_RECT   BarStart, BarEnd, ScrollStart, ScrollEnd, ScrollTotal,
                UpButton, DownButton, ScrollbarBackground, Scrollbar,
                ScrollbarOldPointerPlace, ScrollbarNewPointerPlace;

#define DEC_BUILTIN_ICON(id, ico) BuiltinIconTable[id].Image = DecodePNG (&ico[0], ARRAY_SIZE (ico), 0, TRUE)

STATIC CHAR16 *BuiltinIconNames[] = {
  L"Internal",
  L"External",
  L"Optical",
  L"FireWire",
  L"Boot",
  L"HFS",
  L"NTFS",
  L"EXT",
  L"Recovery",
};

STATIC CONST UINTN BuiltinIconNamesCount = ARRAY_SIZE (BuiltinIconNames);

//
// Load an image from a .icns file
//

EG_IMAGE *
LoadIcns (
  IN EFI_FILE_HANDLE    BaseDir,
  IN CHAR16             *FileName,
  IN UINTN              PixelSize
) {
  if (GlobalConfig.TextOnly) {
    return NULL;
  }

  if (BaseDir) {
    return LoadIcon (BaseDir, FileName, PixelSize);
  }

  return DummyImage (PixelSize);
}

STATIC EG_PIXEL BlackPixel  = { 0x00, 0x00, 0x00, 0 };

EG_IMAGE *
DummyImage (
  IN UINTN    PixelSize
) {
  EG_IMAGE    *Image;
  UINTN       x, y, LineOffset;
  CHAR8       *Ptr, *YPtr;

  Image = CreateFilledImage (PixelSize, PixelSize, TRUE, &BlackPixel);

  LineOffset = PixelSize * 4;

  YPtr = (CHAR8 *)Image->PixelData + ((PixelSize - 32) >> 1) * (LineOffset + 4);
  for (y = 0; y < 32; y++) {
    Ptr = YPtr;
    for (x = 0; x < 32; x++) {
        if (((x + y) % 12) < 6) {
          *Ptr++ = 0;
          *Ptr++ = 0;
          *Ptr++ = 0;
        } else {
          *Ptr++ = 0;
          *Ptr++ = 255; //yellow
          *Ptr++ = 255;
        }
        *Ptr++ = 144;
    }

    YPtr += LineOffset;
  }

  return Image;
}

EG_IMAGE *
LoadIcnsFallback (
  IN EFI_FILE_HANDLE  BaseDir,
  IN CHAR16           *FileName,
  IN UINTN            PixelSize
) {
  if (GlobalConfig.TextOnly) {
    return NULL;
  }

  return LoadIcon (BaseDir, FileName, PixelSize);
}

CHAR16 *
GetIconsExt (
  IN CHAR16   *Icon,
  IN CHAR16   *Def
) {
  return PoolPrint (L"%s.%s",
            Icon,
            (GlobalConfig.IconFormat != ICON_FORMAT_DEF)
              ? ((GlobalConfig.IconFormat == ICON_FORMAT_ICNS) ? L"icns" : L"png")
              : Def
          );
}

//
// Graphics helper functions
//

VOID
InitUISelection () {
  INTN  i, iSize = 0;

  if (SelectionImg[0].Image != NULL) {
    return;
  }

  for (i = 0; i < SelectionImgCount; ++i) {
    if (SelectionImg[i].Path != NULL) {
      FreePool (SelectionImg[i].Path);
      //SelectionImg[i].Path = NULL;
    }

    switch (i) {
      case kSmallImage:
      case kBigImage:
      case kIndicatorImage:
        if (i == kBigImage) {
          iSize = GlobalConfig.row0TileSize;
          SelectionImg[i].Path = AllocateCopyPool (StrSize (GlobalConfig.SelectionBigFileName), GlobalConfig.SelectionBigFileName);
        } else if (i == kSmallImage) {
          iSize = GlobalConfig.row1TileSize;
          SelectionImg[i].Path = AllocateCopyPool (StrSize (GlobalConfig.SelectionSmallFileName), GlobalConfig.SelectionSmallFileName);
        } else if (i == kIndicatorImage) {
          if (!GlobalConfig.BootCampStyle) {
            break;
          }

          SelectionImg[i].Path = AllocateCopyPool (StrSize (GlobalConfig.SelectionIndicatorName), GlobalConfig.SelectionIndicatorName);
          iSize = INDICATOR_SIZE;
        }

        if (ThemeDir && (SelectionImg[i].Path != NULL)) {
          SelectionImg[i].Image = LoadImage (ThemeDir, SelectionImg[i].Path, TRUE); //FALSE
        }

        if (SelectionImg[i].Image == NULL) {
          if (i == kBigImage) {
            SelectionImg[i].Image = DEC_PNG_BUILTIN (emb_selection_big);
          } else if (i == kSmallImage) {
            SelectionImg[i].Image = DEC_PNG_BUILTIN (emb_selection_small);
          } else if (i == kIndicatorImage) {
            SelectionImg[i].Image = DEC_PNG_BUILTIN (emb_selection_indicator);
          }

          CopyMem (&TmpBackgroundPixel, IsEmbeddedTheme () ? &GrayBackgroundPixel : &BlackBackgroundPixel, sizeof (EG_PIXEL));
        }

        SelectionImg[i].Image = EnsureImageSize (
                                  SelectionImg[i].Image,
                                  iSize, iSize, &TransparentBackgroundPixel
                                );

        if (SelectionImg[i].Image == NULL) {
          return;
        }

        SelectionImg[i + 1].Image = CreateFilledImage (
                                      iSize, iSize,
                                      TRUE, &TransparentBackgroundPixel
                                    );
        break;

      default:
        break;
    }
  }

  for (i = 0; i < ButtonsImgCount; ++i) {
    if (ThemeDir && !IsEmbeddedTheme ()) {
      ButtonsImg[i].Image = LoadImage (ThemeDir, GetIconsExt (ButtonsImg[i].Path, ButtonsImg[i].Format), TRUE);
    }

    if (!ButtonsImg[i].Image) {
      switch (i) {
        case kRadioImage:
          ButtonsImg[i].Image =  DEC_PNG_BUILTIN (emb_radio_button);
          break;

        case kRadioSelectedImage:
          ButtonsImg[i].Image =  DEC_PNG_BUILTIN (emb_radio_button_selected);
          break;

        case kCheckboxImage:
          ButtonsImg[i].Image =  DEC_PNG_BUILTIN (emb_checkbox);
          break;

        case kCheckboxCheckedImage:
          ButtonsImg[i].Image =  DEC_PNG_BUILTIN (emb_checkbox_checked);
          break;
      }
    }
  }

  //  DBG ("selections inited\n");
}

VOID
FreeButtons () {
  INTN    i;

  for (i = 0; i < ButtonsImgCount; ++i) {
    if (ButtonsImg[i].Image) {
      FreeImage (ButtonsImg[i].Image);
      ButtonsImg[i].Image = NULL;
    }
  }
}

VOID
FreeSelections () {
  INTN    i;

  for (i = 0; i < SelectionImgCount; ++i) {
    if (SelectionImg[i].Image) {
      FreeImage (SelectionImg[i].Image);
      SelectionImg[i].Image = NULL;
    }
  }
}

VOID
FreeBuiltinIcons () {
  INTN    i;

  for (i = 0; i < BUILTIN_ICON_COUNT; ++i) {
    if (BuiltinIconTable[i].Image) {
      FreeImage (BuiltinIconTable[i].Image);
      BuiltinIconTable[i].Image = NULL;
    }
  }
}

VOID
FreeBanner () {
  if (Banner != NULL) {
    if (Banner != BuiltinIconTable[BUILTIN_ICON_BANNER].Image) {
      FreeImage (Banner);
      //Banner  = NULL;
    }

    Banner  = NULL;
  }
}

VOID
FreeAnims () {
  while (GuiAnime != NULL) {
    GUI_ANIME *NextAnime = GuiAnime->Next;
    FreeAnime (GuiAnime);
    GuiAnime             = NextAnime;
  }

  GuiAnime = NULL;
}

VOID
InitUIBar () {
  INTN    i;

  for (i = 0; i < ScrollbarImgCount; ++i) {
    if (ThemeDir && !ScrollbarImg[i].Image) {
      ScrollbarImg[i].Image = LoadImage (ThemeDir, GetIconsExt (ScrollbarImg[i].Path, ScrollbarImg[i].Format), TRUE);
    }

    if (!ScrollbarImg[i].Image) {
      switch (i) {
        case kScrollbarBackgroundImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_bar_fill);
          break;

        case kBarStartImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_bar_start);
          break;

        case kBarEndImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_bar_end);
          break;

        case kScrollbarImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_scroll_fill);
          break;

        case kScrollStartImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_scroll_start);
          break;

        case kScrollEndImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_scroll_end);
          break;

        case kUpButtonImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_up_button);
          break;

        case kDownButtonImage:
          ScrollbarImg[i].Image = DEC_PNG_BUILTIN (emb_scroll_down_button);
          break;
      }
    }
  }
}

VOID
InitBar () {
  InitUIBar ();

  UpButton.Width      = GlobalConfig.ScrollWidth;
  UpButton.Height     = GlobalConfig.ScrollButtonsHeight;
  DownButton.Width    = UpButton.Width;
  DownButton.Height   = GlobalConfig.ScrollButtonsHeight;
  BarStart.Height     = GlobalConfig.ScrollBarDecorationsHeight;
  BarEnd.Height       = GlobalConfig.ScrollBarDecorationsHeight;
  ScrollStart.Height  = GlobalConfig.ScrollScrollDecorationsHeight;
  ScrollEnd.Height    = GlobalConfig.ScrollScrollDecorationsHeight;
}

VOID
SetBar (
  IN  INTN          PosX,
  IN  INTN          UpPosY,
  IN  INTN          DownPosY,
  IN  SCROLL_STATE  *State
) {
  //DBG ("SetBar <= %d %d %d %d %d\n", UpPosY, DownPosY, State->MaxVisible, State->MaxIndex, State->FirstVisible);
  UpButton.XPos = PosX;
  UpButton.YPos = UpPosY;

  DownButton.XPos = UpButton.XPos;
  DownButton.YPos = DownPosY;

  ScrollbarBackground.XPos = UpButton.XPos;
  ScrollbarBackground.YPos = UpButton.YPos + UpButton.Height;
  ScrollbarBackground.Width = UpButton.Width;
  ScrollbarBackground.Height = DownButton.YPos - (UpButton.YPos + UpButton.Height);

  BarStart.XPos = ScrollbarBackground.XPos;
  BarStart.YPos = ScrollbarBackground.YPos;
  BarStart.Width = ScrollbarBackground.Width;

  BarEnd.Width = ScrollbarBackground.Width;
  BarEnd.XPos = ScrollbarBackground.XPos;
  BarEnd.YPos = DownButton.YPos - BarEnd.Height;

  ScrollStart.XPos = ScrollbarBackground.XPos;
  ScrollStart.YPos = ScrollbarBackground.YPos + ScrollbarBackground.Height * State->FirstVisible / (State->MaxIndex + 1);
  ScrollStart.Width = ScrollbarBackground.Width;

  Scrollbar.XPos = ScrollbarBackground.XPos;
  Scrollbar.YPos = ScrollStart.YPos + ScrollStart.Height;
  Scrollbar.Width = ScrollbarBackground.Width;
  Scrollbar.Height = ScrollbarBackground.Height * (State->MaxVisible + 1) / (State->MaxIndex + 1) - ScrollStart.Height;

  ScrollEnd.Width = ScrollbarBackground.Width;
  ScrollEnd.XPos = ScrollbarBackground.XPos;
  ScrollEnd.YPos = Scrollbar.YPos + Scrollbar.Height - ScrollEnd.Height;

  Scrollbar.Height -= ScrollEnd.Height;

  ScrollTotal.XPos = UpButton.XPos;
  ScrollTotal.YPos = UpButton.YPos;
  ScrollTotal.Width = UpButton.Width;
  ScrollTotal.Height = DownButton.YPos + DownButton.Height - UpButton.YPos;
  //DBG ("ScrollTotal.Height = %d\n", ScrollTotal.Height);
}

VOID
ScrollingBar (
  IN SCROLL_STATE   *State
) {
  EG_IMAGE    *Total;
  INTN        i;

  ScrollEnabled = (State->MaxFirstVisible != 0);
  if (ScrollEnabled) {
    Total = CreateFilledImage (ScrollTotal.Width, ScrollTotal.Height, TRUE, &TransparentBackgroundPixel);
    for (i = 0; i < ScrollbarBackground.Height; i++) {
      ComposeImage (
        Total,
        ScrollbarImg[kScrollbarBackgroundImage].Image,
        ScrollbarBackground.XPos - ScrollTotal.XPos,
        ScrollbarBackground.YPos + i - ScrollTotal.YPos
      );
    }

    ComposeImage (
      Total,
      ScrollbarImg[kBarStartImage].Image,
      BarStart.XPos - ScrollTotal.XPos,
      BarStart.YPos - ScrollTotal.YPos
    );

    ComposeImage (
      Total,
      ScrollbarImg[kBarEndImage].Image,
      BarEnd.XPos - ScrollTotal.XPos,
      BarEnd.YPos - ScrollTotal.YPos
    );

    for (i = 0; i < Scrollbar.Height; i++) {
      ComposeImage (
        Total,
        ScrollbarImg[kScrollbarImage].Image,
        Scrollbar.XPos - ScrollTotal.XPos,
        Scrollbar.YPos + i - ScrollTotal.YPos
      );
    }

    ComposeImage (
      Total,
      ScrollbarImg[kUpButtonImage].Image,
      UpButton.XPos - ScrollTotal.XPos,
      UpButton.YPos - ScrollTotal.YPos
    );

    ComposeImage (
      Total,
      ScrollbarImg[kDownButtonImage].Image,
      DownButton.XPos - ScrollTotal.XPos,
      DownButton.YPos - ScrollTotal.YPos
    );

    ComposeImage (
      Total,
      ScrollbarImg[kScrollStartImage].Image,
      ScrollStart.XPos - ScrollTotal.XPos,
      ScrollStart.YPos - ScrollTotal.YPos
    );

    ComposeImage (
      Total,
      ScrollbarImg[kScrollEndImage].Image,
      ScrollEnd.XPos - ScrollTotal.XPos,
      ScrollEnd.YPos - ScrollTotal.YPos
    );

    BltImageAlpha (Total, ScrollTotal.XPos, ScrollTotal.YPos, &TransparentBackgroundPixel, GlobalConfig.ScrollWidth);
    FreeImage (Total);
  }
}

VOID
FreeScrollBar () {
  INTN    i;

  for (i = 0; i < ScrollbarImgCount; ++i) {
    if (ScrollbarImg[i].Image) {
      FreeImage (ScrollbarImg[i].Image);
      ScrollbarImg[i].Image = NULL;
    }
  }
}

EG_IMAGE *
GetSmallHover (
  IN UINTN    Id
) {
  if (IsEmbeddedTheme ()) {
    return NULL;
  } else {
    CHAR16    *Path = AllocateZeroPool (AVALUE_MAX_SIZE);

    Path = GetIconsExt (PoolPrint (L"%s_hover", BuiltinIconTable[Id].Path), BuiltinIconTable[Id].Format);
    return LoadIcon (ThemeDir, Path, BuiltinIconTable[Id].PixelSize);
  }
}

EG_IMAGE *
BuiltinIcon (
  IN UINTN    Id
) {
  INTN      Size = 0;
  EG_IMAGE  *TextBuffer = NULL;
  CHAR16    *p, *Text;

  if (Id >= BUILTIN_ICON_COUNT) {
    return NULL;
  }

  if (BuiltinIconTable[Id].Image != NULL) {
    return BuiltinIconTable[Id].Image;
  }

  if (IsEmbeddedTheme ()) {
    goto GET_EMBEDDED;
  }

  Size = BuiltinIconTable[Id].PixelSize;

  if (ThemeDir) {
    CHAR16    *Path = GetIconsExt (BuiltinIconTable[Id].Path, BuiltinIconTable[Id].Format);

    BuiltinIconTable[Id].Image = LoadIcnsFallback (ThemeDir, Path, Size);

    if (!BuiltinIconTable[Id].Image) {
      DBG ("        [!] Icon %d (%s) not found (path: %s)\n", Id, Path, ThemePath);
      if (Id >= BUILTIN_ICON_VOL_INTERNAL) {
        FreePool (Path);
        Path = GetIconsExt (BuiltinIconTable[BUILTIN_ICON_VOL_INTERNAL].Path, BuiltinIconTable[BUILTIN_ICON_VOL_INTERNAL].Format);
        BuiltinIconTable[Id].Image = LoadIcnsFallback (ThemeDir, Path, Size);
      }
    }

    FreePool (Path);
  }

  if (BuiltinIconTable[Id].Image != NULL) {
    return BuiltinIconTable[Id].Image;
  }

GET_EMBEDDED:

  switch (Id) {
    case BUILTIN_ICON_FUNC_ABOUT:
      DEC_BUILTIN_ICON (Id, emb_func_about); break;
    case BUILTIN_ICON_FUNC_OPTIONS:
      DEC_BUILTIN_ICON (Id, emb_func_options); break;
    //case BUILTIN_ICON_FUNC_CLOVER:
    //  DEC_BUILTIN_ICON (Id, emb_func_clover); break;
    case BUILTIN_ICON_FUNC_RESET:
      DEC_BUILTIN_ICON (Id, emb_func_reset); break;
    //case BUILTIN_ICON_FUNC_SHUTDOWN:
    //  DEC_BUILTIN_ICON (Id, emb_func_shutdown); break;
    case BUILTIN_ICON_FUNC_EXIT:
      DEC_BUILTIN_ICON (Id, emb_func_exit); break;
    case BUILTIN_ICON_FUNC_HELP:
      DEC_BUILTIN_ICON (Id, emb_func_help); break;
    case BUILTIN_ICON_TOOL_SHELL:
      DEC_BUILTIN_ICON (Id, emb_func_shell); break;
    //case BUILTIN_ICON_POINTER:
    //  DEC_BUILTIN_ICON (Id, emb_pointer); break;
    case BUILTIN_ICON_VOL_INTERNAL:
    case BUILTIN_ICON_VOL_EXTERNAL:
    case BUILTIN_ICON_VOL_OPTICAL:
    case BUILTIN_ICON_VOL_FIREWIRE:
      DEC_BUILTIN_ICON (Id, emb_vol_internal); break;
    //case BUILTIN_ICON_VOL_BOOTER:
    //  DEC_BUILTIN_ICON (Id, emb_vol_internal_booter); break;
    case BUILTIN_ICON_VOL_INTERNAL_HFS:
      DEC_BUILTIN_ICON (Id, emb_vol_internal_hfs); break;
    case BUILTIN_ICON_VOL_INTERNAL_NTFS:
      DEC_BUILTIN_ICON (Id, emb_vol_internal_ntfs); break;
    case BUILTIN_ICON_VOL_INTERNAL_EXT3:
      DEC_BUILTIN_ICON (Id, emb_vol_internal_ext); break;
    case BUILTIN_ICON_VOL_INTERNAL_REC:
      DEC_BUILTIN_ICON (Id, emb_vol_internal_recovery); break;
    case BUILTIN_ICON_BANNER:
      DEC_BUILTIN_ICON (Id, emb_logo); break;
    case BUILTIN_SELECTION_SMALL:
      DEC_BUILTIN_ICON (Id, emb_selection_small); break;
    case BUILTIN_SELECTION_BIG:
      DEC_BUILTIN_ICON (Id, emb_selection_big); break;
  }

  if (!BuiltinIconTable[Id].Image) {
    TextBuffer = CreateImage (Size, Size, TRUE);
    FillImage (TextBuffer, &TransparentBackgroundPixel);
    p = StrStr (BuiltinIconTable[Id].Path, L"_"); p++;
    Text = (CHAR16 *)AllocateCopyPool (30, (VOID *)p);
    p = StrStr (Text, L".");
    *p = L'\0';

    if (StrCmp (Text, L"shutdown") == 0) {
      // icon name is shutdown from historic reasons, but function is now exit
      UnicodeSPrint (Text, 30, L"exit");
    }

    RenderText (Text, TextBuffer, 0, 0, 0xFFFF, FALSE);
    BuiltinIconTable[Id].Image = TextBuffer;
    DBG ("        [!] Icon %d: Text <%s> rendered\n", Id, Text);
    FreePool (Text);
    //BuiltinIconTable[Id].Image = DummyImage (Size);
  }

  return BuiltinIconTable[Id].Image;
}

EG_IMAGE *
LoadBuiltinIcon (
  IN CHAR16   *IconName
) {
  UINTN   Index = 0;

  if (IconName == NULL) {
    return NULL;
  }

  while (Index < BuiltinIconNamesCount) {
    if (StriCmp (IconName, BuiltinIconNames[Index]) == 0) {
      return BuiltinIcon (BUILTIN_ICON_VOL_INTERNAL + Index);
    }
    ++Index;
  }

  return NULL;
}

EG_IMAGE *
ScanVolumeDefaultIcon (
  IN UINTN    DiskKind,
  IN UINT8    OSType
) {
  UINTN   IconNum;

  // default volume icon based on disk kind
  switch (DiskKind) {
    case DISK_KIND_INTERNAL:
      switch (OSType) {
        case OSTYPE_OSX:
        case OSTYPE_OSX_INSTALLER:
          IconNum = BUILTIN_ICON_VOL_INTERNAL_HFS;
          break;

        case OSTYPE_RECOVERY:
          IconNum = BUILTIN_ICON_VOL_INTERNAL_REC;
          break;

        case OSTYPE_LIN:
        case OSTYPE_LINEFI:
          IconNum = BUILTIN_ICON_VOL_INTERNAL_EXT3;
          break;

        case OSTYPE_WIN:
        case OSTYPE_WINEFI:
          IconNum = BUILTIN_ICON_VOL_INTERNAL_NTFS;
          break;

        default:
          IconNum = BUILTIN_ICON_VOL_INTERNAL;
          break;
      }
      return BuiltinIcon (IconNum);

    case DISK_KIND_EXTERNAL:
      return BuiltinIcon (BUILTIN_ICON_VOL_EXTERNAL);

    case DISK_KIND_OPTICAL:
      return BuiltinIcon (BUILTIN_ICON_VOL_OPTICAL);

    case DISK_KIND_FIREWIRE:
      return BuiltinIcon (BUILTIN_ICON_VOL_FIREWIRE);

    //case DISK_KIND_BOOTER:
    //  return BuiltinIcon (BUILTIN_ICON_VOL_BOOTER);

    default:
      break;
  }

  return NULL;
}

//
// Load an icon for an operating system
//

EG_IMAGE *
LoadOSIcon (
  IN  CHAR16    *OSIconName OPTIONAL,
  OUT CHAR16    **OSIconNameHover,
  IN  CHAR16    *FallbackIconName,
  IN  UINTN     PixelSize,
  IN  BOOLEAN   BootLogo,
  IN  BOOLEAN   WantDummy
) {
  EG_IMAGE    *Image;
  CHAR16      CutoutName[16], TmpName[64], FileName[AVALUE_MAX_SIZE];
  UINTN       StartIndex, Index, NextIndex;

  *OSIconNameHover = NULL;

  if (GlobalConfig.TextOnly || IsEmbeddedTheme ()) {
    return NULL;
  }

  Image = NULL;

  // try the names from OSIconName
  for (StartIndex = 0; OSIconName != NULL && OSIconName[StartIndex]; StartIndex = NextIndex) {
    // find the next name in the list
    NextIndex = 0;

    for (Index = StartIndex; OSIconName[Index]; Index++) {
      if (OSIconName[Index] == ',') {
        NextIndex = Index + 1;
        break;
      }
    }

    if (OSIconName[Index] == 0)
      NextIndex = Index;

    // construct full path
    if (Index > StartIndex + 15) {  // prevent buffer overflow
      continue;
    }

    CopyMem (CutoutName, OSIconName + StartIndex, (Index - StartIndex) * sizeof (CHAR16));
    CutoutName[Index - StartIndex] = 0;
    UnicodeSPrint (TmpName, 64, L"%s_%s", BootLogo ? L"boot" : L"os", CutoutName);
    UnicodeSPrint (FileName, ARRAY_SIZE (FileName), GetIconsExt (PoolPrint (L"icons\\%s", TmpName), L"icns"));

    // try to load it
    Image = LoadIcon (ThemeDir, FileName, PixelSize);
    if (Image != NULL) {
      *OSIconNameHover = AllocateZeroPool (64);
      UnicodeSPrint (*OSIconNameHover, 64, L"%s_hover", TmpName);
      return Image;
    }
  }

  // try the fallback name
  UnicodeSPrint (TmpName, ARRAY_SIZE (TmpName), L"%s_%s", BootLogo ? L"boot" : L"os", FallbackIconName);
  UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s", GetIconsExt (PoolPrint (L"icons\\%s", TmpName), L"icns"));

  Image = LoadIcon (ThemeDir, FileName, PixelSize);

  if (Image != NULL) {
    *OSIconNameHover = AllocateZeroPool (64);
    UnicodeSPrint (*OSIconNameHover, 64, L"%s_hover", TmpName);
    return Image;
  }

  // try the fallback name with os_ instead of boot_
  if (BootLogo) {
    Image = LoadOSIcon (NULL, NULL, FallbackIconName, PixelSize, FALSE, WantDummy);

    if (Image != NULL) {
      return Image;
    }
  }

  return WantDummy ? DummyImage (PixelSize) : NULL;
}

EG_IMAGE *
LoadHoverIcon (
  IN CHAR16   *OSIconName,
  IN UINTN    PixelSize
) {
  if (GlobalConfig.TextOnly || IsEmbeddedTheme ()) {
    return NULL;
  }

  return LoadIcon (ThemeDir, OSIconName, PixelSize);
}

//////////////////////////////////////////////////

//
// Decompress .icns RLE data
//

VOID
DecompressIcnsRLE (
  IN OUT  UINT8   **CompData,
  IN OUT  UINTN   *CompLen,
  IN      UINT8   *PixelData,
  IN      UINTN   PixelCount
) {
  UINT8   *cp, *cp_end, *pp, value;
  UINTN   pp_left, len, i;

  // setup variables
  cp = *CompData;
  cp_end = cp + *CompLen;
  pp = PixelData;
  pp_left = PixelCount;

  // decode
  while (cp + 1 < cp_end && pp_left > 0) {
    len = *cp++;

    if (len & 0x80) { // compressed data: repeat next byte
      len -= 125;

      if (len > pp_left) {
        break;
      }

      value = *cp++;

      for (i = 0; i < len; i++) {
        *pp = value;
        pp += 4;
      }
    } else {  // uncompressed data: copy bytes
      len++;
      if ((len > pp_left )|| ((cp + len) > cp_end)) {
        break;
      }

      for (i = 0; i < len; i++) {
        *pp = *cp++;
        pp += 4;
      }
    }

    pp_left -= len;
  }

  if (pp_left > 0) {
    DBG (" DecompressIcnsRLE: still need %d bytes of pixel data\n", pp_left);
  }

  // record what's left of the compressed data stream
  *CompData = cp;
  *CompLen = (UINTN)(cp_end - cp);
}

//
// Load Apple .icns icons
//

EG_IMAGE *
DecodeICNS (
  IN UINT8      *FileData,
  IN UINTN      FileDataLength,
  IN UINTN      IconSize,
  IN BOOLEAN    WantAlpha
) {
  EG_IMAGE    *NewImage;
  EG_PIXEL    *DestPtr;
  UINT8       *Ptr, *BufferEnd, *DataPtr, *MaskPtr, *CompData, *SrcPtr;
  UINT32      BlockLen, DataLen, MaskLen;
  UINTN       FetchPixelSize, PixelCount, i, CompLen;

  if (
    (FileDataLength < 8) || (FileData == NULL) ||
    (FileData[0] != 'i') || (FileData[1] != 'c') || (FileData[2] != 'n') || (FileData[3] != 's')
  ) {
    // not an icns file...
    DBG ("not icns\n");
    return NULL;
  }

  FetchPixelSize = IconSize;

  for (;;) {
    DataPtr = NULL;
    DataLen = 0;
    MaskPtr = NULL;
    MaskLen = 0;

    Ptr = FileData + 8;
    BufferEnd = FileData + FileDataLength;

    // iterate over tagged blocks in the file
    while (Ptr + 8 <= BufferEnd) {
      BlockLen = ((UINT32)Ptr[4] << 24) + ((UINT32)Ptr[5] << 16) + ((UINT32)Ptr[6] << 8) + (UINT32)Ptr[7];

      if (Ptr + BlockLen > BufferEnd) {   // block continues beyond end of file
        break;
      }

      // extract the appropriate blocks for each pixel size
      if (FetchPixelSize == 128) {
        if (((Ptr[0] == 'i') && (Ptr[1] == 't')) && (Ptr[2] == '3') && (Ptr[3] == '2')) {
          if ((Ptr[8] == 0) && (Ptr[9] == 0) && (Ptr[10] == 0) && (Ptr[11] == 0)) {
            DataPtr = Ptr + 12;
            DataLen = BlockLen - 12;
          }
        } else if ((Ptr[0] == 't') && (Ptr[1] == '8') && (Ptr[2] == 'm') && (Ptr[3] == 'k')) {
          MaskPtr = Ptr + 8;
          MaskLen = BlockLen - 8;
        }
      } else if (FetchPixelSize == 48) {
        if ((Ptr[0] == 'i') && (Ptr[1] == 'h') && (Ptr[2] == '3') && (Ptr[3] == '2')) {
          DataPtr = Ptr + 8;
          DataLen = BlockLen - 8;
        } else if ((Ptr[0] == 'h') && (Ptr[1] == '8') && (Ptr[2] == 'm') && (Ptr[3] == 'k')) {
          MaskPtr = Ptr + 8;
          MaskLen = BlockLen - 8;
        }
      } else if (FetchPixelSize == 32) {
          if ((Ptr[0] == 'i') && (Ptr[1] == 'l') && (Ptr[2] == '3') && (Ptr[3] == '2')) {
            DataPtr = Ptr + 8;
            DataLen = BlockLen - 8;
          } else if ((Ptr[0] == 'l') && (Ptr[1] == '8') && (Ptr[2] == 'm') && (Ptr[3] == 'k')) {
            MaskPtr = Ptr + 8;
            MaskLen = BlockLen - 8;
          }
      } else if (FetchPixelSize == 16) {
        if ((Ptr[0] == 'i') && (Ptr[1] == 's') && (Ptr[2] == '3') && (Ptr[3] == '2')) {
          DataPtr = Ptr + 8;
          DataLen = BlockLen - 8;
        } else if ((Ptr[0] == 's') && (Ptr[1] == '8') && (Ptr[2] == 'm') && (Ptr[3] == 'k')) {
          MaskPtr = Ptr + 8;
          MaskLen = BlockLen - 8;
        }
      }

      Ptr += BlockLen;
    }

    //TODO - take different larger size if not found
    // FUTURE: try to load a different size and scale it later
    if ((DataPtr == NULL) && (FetchPixelSize == 128)) {
      FetchPixelSize = 32;
      continue;
    }

    break;
  }

  if (DataPtr == NULL) {
    DBG ("not found such IconSize\n");
    return NULL;   // no image found
  }

  // allocate image structure and buffer
  NewImage = CreateImage (FetchPixelSize, FetchPixelSize, WantAlpha);

  if (NewImage == NULL) {
    return NULL;
  }

  PixelCount = FetchPixelSize * FetchPixelSize;

  if (DataLen < PixelCount * 3) {
    // pixel data is compressed, RGB planar
    CompData = DataPtr;
    CompLen  = DataLen;
    DecompressIcnsRLE (&CompData, &CompLen, PLPTR (NewImage, r), PixelCount);
    DecompressIcnsRLE (&CompData, &CompLen, PLPTR (NewImage, g), PixelCount);
    DecompressIcnsRLE (&CompData, &CompLen, PLPTR (NewImage, b), PixelCount);
    // possible assertion: CompLen == 0
    if (CompLen > 0) {
      DBG (" LoadICNSIcon: %d bytes of compressed data left\n", CompLen);
    }
  } else {
    // pixel data is uncompressed, RGB interleaved
    SrcPtr  = DataPtr;
    DestPtr = NewImage->PixelData;
    for (i = 0; i < PixelCount; i++, DestPtr++) {
      DestPtr->r = *SrcPtr++;
      DestPtr->g = *SrcPtr++;
      DestPtr->b = *SrcPtr++;
    }
  }

  // add/set alpha plane
  if (MaskPtr != NULL && MaskLen >= PixelCount && WantAlpha) {
    InsertPlane (MaskPtr, PLPTR (NewImage, a), PixelCount);
  } else {
    SetPlane (PLPTR (NewImage, a), WantAlpha ? 255 : 0, PixelCount);
  }

  // FUTURE: scale to originally requested size if we had to load another size

  return NewImage;
}
