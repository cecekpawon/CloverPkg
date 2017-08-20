/*
 * libeg/text.c
 * Text drawing functions
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
//Slice 2011 - 2015 numerous improvements

#include <Library/Platform/Platform.h>
#include <Library/UI/EmbeddedFont.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_TEXT
#define DEBUG_TEXT -1
#endif
#else
#ifdef DEBUG_TEXT
#undef DEBUG_TEXT
#endif
#define DEBUG_TEXT DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_TEXT, __VA_ARGS__)

EG_IMAGE    *gFontImage = NULL, *gFontImageHover = NULL;
INTN        gFontWidth = 9, gFontHeight = 18, gTextHeight = 19;

//
// Text rendering
//

VOID
MeasureText (
  IN  CHAR16    *Text,
  OUT INTN      *Width,
  OUT INTN      *Height
) {
  if (Width != NULL) {
    *Width = StrLen (Text) * ((gFontWidth > GlobalConfig.CharWidth) ? gFontWidth : GlobalConfig.CharWidth);
  }

  if (Height != NULL) {
    *Height = gFontHeight;
  }
}

EG_IMAGE *
LoadFontImage (
  IN INTN     Rows,
  IN INTN     Cols
) {
  EG_IMAGE    *NewImage = NULL, *NewFontImage;
  INTN        ImageWidth, ImageHeight, x, y, Ypos, j;
  EG_PIXEL    *PixelPtr, FirstPixel;
  CHAR16      *FontPath;

  if (IsEmbeddedTheme ()) {
    DBG ("Using embedded font\n");
    goto F_EMBEDDED;
  } else {
    NewImage = LoadImage (gThemeDir, GlobalConfig.FontFileName);
    DBG ("Loading font from ThemeDir: %r\n", NewImage ? EFI_SUCCESS : EFI_NOT_FOUND);
  }

  if (NewImage) {
    goto F_THEME;
  } else {
    FontPath = PoolPrint (L"%s\\%s", DIR_FONTS, GlobalConfig.FontFileName);
    NewImage = LoadImage (gSelfRootDir, FontPath);

    if (NewImage) {
      DBG ("font %s loaded from common font dir %s\n", GlobalConfig.FontFileName, DIR_FONTS);
      FreePool (FontPath);
      goto F_THEME;
    }

    DBG ("Font %s is not loaded, using embedded\n", FontPath);
    FreePool (FontPath);
    goto F_EMBEDDED;
  }

  F_EMBEDDED:

  NewImage = DEC_PNG_BUILTIN (emb_font_data);
  GlobalConfig.FontEmbedded = TRUE;

  //if (GlobalConfig.Font != FONT_RAW) {
  //  GlobalConfig.Font = DefaultConfig.Font;
  //}

  F_THEME:

  if (GlobalConfig.FontEmbedded) {
    Rows = DefaultConfig.CharRows;
  } else {
    switch (Rows) {
      case 6:   // (32 - 127)
      case 8:   // (0 - 127)
	    //
        break;

      //case 0:   // Invalid parse theme.plist
      default:
        Rows = 16;
        break;
    }
  }

  GlobalConfig.CharRows = Rows;

  ImageWidth = NewImage->Width;
  //DBG ("ImageWidth=%d\n", ImageWidth);
  ImageHeight = NewImage->Height;
  //DBG ("ImageHeight=%d\n", ImageHeight);
  PixelPtr = NewImage->PixelData;
  DBG ("Font loaded: ImageWidth=%d ImageHeight=%d\n", ImageWidth, ImageHeight);
  NewFontImage = CreateImage (ImageWidth * Rows, ImageHeight / Rows, TRUE);

  if (NewFontImage == NULL) {
    DBG ("Can't create new font image!\n");
    return NULL;
  }

  gFontWidth = ImageWidth / Cols;
  gFontHeight = ImageHeight / Rows;
  FirstPixel = *PixelPtr;

  for (y = 0; y < Rows; y++) {
    for (j = 0; j < gFontHeight; j++) {
      Ypos = ((j * Rows) + y) * ImageWidth;
      for (x = 0; x < ImageWidth; x++) {
        if (
          //WantAlpha &&
          (PixelPtr->b == FirstPixel.b) &&
          (PixelPtr->g == FirstPixel.g) &&
          (PixelPtr->r == FirstPixel.r)
        ) {
          PixelPtr->a = 0;
        }

        NewFontImage->PixelData[Ypos + x] = *PixelPtr++;
      }
    }
  }

  FreeImage (NewImage);

  return NewFontImage;
}

VOID
PaintFont (
  EG_IMAGE    *FImage,
  EG_PIXEL    TextPixel
) {
  EG_PIXEL    *Pixel = FImage->PixelData;
  INTN        i, ImageSize = (FImage->Height * FImage->Width);

  for (i = 0; i < ImageSize; i++) {
    Pixel->r = TextPixel.r;
    Pixel->g = TextPixel.g;
    Pixel->b = TextPixel.b;
    Pixel++;
  }
}

VOID
PrepareFont () {
  EG_PIXEL    TextPixel = ToPixel (GlobalConfig.TextColor),
              TextPixelHover = ToPixel (GlobalConfig.SelectionTextColor);

  // load the font
  DBG ("Load font image type %d\n", GlobalConfig.Font);

  gFontImage = LoadFontImage (GlobalConfig.CharRows, GlobalConfig.CharCols);

  if (gFontImage) {
    gFontImageHover = CopyImage (gFontImage);

    switch (GlobalConfig.Font) {
      case FONT_ALFA:
      case FONT_LOAD:
        if (GlobalConfig.Font == FONT_LOAD) {
          PaintFont (gFontImage, TextPixel);
        }

        PaintFont (gFontImageHover, TextPixelHover);
        break;

      case FONT_GRAY:
        PaintFont (gFontImage, TextPixelHover);
        PaintFont (gFontImageHover, TextPixel);
        break;

      //case FONT_RAW:
      default:
        //
        break;
    }

    gTextHeight = gFontHeight + TEXT_YMARGIN * 2;
    DBG (" - Font %d prepared WxH=%dx%d CharWidth=%d\n", GlobalConfig.Font, gFontWidth, gFontHeight, GlobalConfig.CharWidth);
  } else {
    DBG (" - Failed to load font\n");
  }
}

INTN
RenderText (
  IN      CHAR16     *Text,
  IN OUT  EG_IMAGE   *CompImage,
  IN      INTN       PosX,
  IN      INTN       PosY,
  IN      INTN       Cursor,
  IN      BOOLEAN    Selected
) {
  EG_PIXEL    *BufferPtr, *FontPixelData, *FirstPixelBuf;
  INTN        BufferLineWidth, BufferLineOffset, FontLineOffset, i,
              TextLength, RealWidth = 0/*, NewTextLength = 0 */;
  UINT16      c, c1;
  UINTN       Shift = 0, LeftSpace, RightSpace;

  // clip the text
  TextLength = StrLen (Text);

  if (!gFontImage) {
    PrepareFont ();
  }

  //DBG ("TextLength =%d PosX=%d PosY=%d\n", TextLength, PosX, PosY);
  // render it
  BufferPtr = CompImage->PixelData;
  BufferLineOffset = CompImage->Width;
  BufferLineWidth = BufferLineOffset - PosX; // remove indent from drawing width
  BufferPtr += PosX + PosY * BufferLineOffset;
  FirstPixelBuf = BufferPtr;
  FontPixelData = (Selected && gFontImageHover) ? gFontImageHover->PixelData : gFontImage->PixelData;
  FontLineOffset = (Selected && gFontImageHover) ? gFontImageHover->Width : gFontImage->Width;

  if (GlobalConfig.CharWidth < gFontWidth) {
    Shift = (gFontWidth - GlobalConfig.CharWidth) >> 1;
  }

  RealWidth = GlobalConfig.CharWidth;

  //DBG ("gFontWidth=%d, CharWidth=%d\n", gFontWidth, RealWidth);

  for (i = 0; i < TextLength; i++) {
    c = Text[i];
    c1 = (((c >=0x410) ? (c -= 0x350) : c) & 0xff); // Cyrillic letters
    c = c1;

    if (GlobalConfig.CharRows == 6) {
      c -= 0x20; // Skip 0 - 31
    }

    LeftSpace = 2;
    RightSpace = Shift;

    if (((UINTN)BufferPtr + RealWidth * 4) > ((UINTN)FirstPixelBuf + BufferLineWidth * 4)) {
      break;
    }

    RawCompose (
      BufferPtr - LeftSpace + 2, FontPixelData + c * gFontWidth + RightSpace,
      RealWidth, gFontHeight,
      BufferLineOffset, FontLineOffset
    );

    if (i == Cursor) {
      c = 0x5F;

      if (GlobalConfig.CharRows == 6) {
        c -= 0x20; // Skip 0 - 31
      }

      RawCompose (
        BufferPtr - LeftSpace + 2, FontPixelData + c * gFontWidth + RightSpace,
        RealWidth, gFontHeight,
        BufferLineOffset, FontLineOffset
      );
    }

    BufferPtr += RealWidth - LeftSpace + 2;
  }

  return ((INTN)BufferPtr - (INTN)FirstPixelBuf) / sizeof (EG_PIXEL);
}
