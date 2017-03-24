/*
 * libeg/image.c
 * Image handling functions
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

#ifndef DEBUG_ALL
#ifndef DEBUG_IMG
#define DEBUG_IMG -1
#endif
#else
#ifdef DEBUG_IMG
#undef DEBUG_IMG
#endif
#define DEBUG_IMG DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_IMG, __VA_ARGS__)

//
// Basic image handling
//

EG_IMAGE *
CreateImage (
  IN INTN       Width,
  IN INTN       Height,
  IN BOOLEAN    HasAlpha
) {
  EG_IMAGE    *NewImage;

  NewImage = (EG_IMAGE *)AllocatePool (sizeof (EG_IMAGE));

  if (NewImage == NULL){
    return NULL;
  }

  NewImage->PixelData = (EG_PIXEL *)AllocatePool ((UINTN)(Width * Height * sizeof (EG_PIXEL)));

  if (NewImage->PixelData == NULL) {
    FreePool (NewImage);
    return NULL;
  }

  NewImage->Width = Width;
  NewImage->Height = Height;
  NewImage->HasAlpha = HasAlpha;

  return NewImage;
}

EG_IMAGE *
CreateFilledImage (
  IN INTN       Width,
  IN INTN       Height,
  IN BOOLEAN    HasAlpha,
  IN EG_PIXEL   *Color
) {
  EG_IMAGE    *NewImage;

  NewImage = CreateImage (Width, Height, HasAlpha);

  if (NewImage == NULL) {
    return NULL;
  }

  FillImage (NewImage, Color);

  return NewImage;
}

EG_IMAGE *
CopyImage (
  IN EG_IMAGE   *Image
) {
  EG_IMAGE    *NewImage;

  if (!Image) {
    return NULL;
  }

  NewImage = CreateImage (Image->Width, Image->Height, Image->HasAlpha);

  if (NewImage == NULL) {
    return NULL;
  }

  CopyMem (NewImage->PixelData, Image->PixelData, (UINTN)(Image->Width * Image->Height * sizeof (EG_PIXEL)));

  return NewImage;
}

//Scaling functions
EG_IMAGE *
CopyScaledImage (
  IN EG_IMAGE   *OldImage,
  IN INTN       Ratio
) { //will be N/16
  //(c)Slice 2012
  BOOLEAN     Grey = FALSE;
  EG_IMAGE    *NewImage;
  INTN        x, x0, x1, x2, y, y0, y1, y2,
              NewH, NewW, OldW;
  EG_PIXEL    *Dest, *Src;

  if (Ratio < 0) {
    Ratio = -Ratio;
    Grey = TRUE;
  }

  if (!OldImage) {
    return NULL;
  }

  Src = OldImage->PixelData;
  OldW = OldImage->Width;

  NewW = (OldImage->Width * Ratio) >> 4;
  NewH = (OldImage->Height * Ratio) >> 4;

  if (Ratio == 16) {
    NewImage = CopyImage (OldImage);
  } else {
    NewImage = CreateImage (NewW, NewH, OldImage->HasAlpha);

    if (NewImage == NULL) {
      return NULL;
    }

    Dest = NewImage->PixelData;
    for (y = 0; y < NewH; y++) {
      y1 = (y << 4) / Ratio;
      y0 = ((y1 > 0) ? (y1 - 1) : y1) * OldW;
      y2 = ((y1 < (OldImage->Height - 1)) ? (y1 + 1) : y1) * OldW;
      y1 *= OldW;

      for (x = 0; x < NewW; x++) {
        x1 = (x << 4) / Ratio;
        x0 = (x1 > 0) ? (x1 - 1) : x1;
        x2 = (x1 < (OldW - 1)) ? (x1 + 1) : x1;
        Dest->b = (UINT8)(((INTN)Src[x1 + y1].b * 2 + Src[x0 + y1].b +
                           Src[x2 + y1].b + Src[x1 + y0].b + Src[x1 + y2].b) / 6);
        Dest->g = (UINT8)(((INTN)Src[x1 + y1].g * 2 + Src[x0 + y1].g +
                           Src[x2 + y1].g + Src[x1 + y0].g + Src[x1 + y2].g) / 6);
        Dest->r = (UINT8)(((INTN)Src[x1 + y1].r * 2 + Src[x0 + y1].r +
                           Src[x2 + y1].r + Src[x1 + y0].r + Src[x1 + y2].r) / 6);
        Dest->a = Src[x1 + y1].a;
        Dest++;
      }
    }
  }

  if (Grey) {
    Dest = NewImage->PixelData;
    for (y = 0; y < NewH; y++) {
      for (x = 0; x < NewW; x++) {
        Dest->b = (UINT8)((INTN)((UINTN)Dest->b + (UINTN)Dest->g + (UINTN)Dest->r) / 3);
        Dest->g = Dest->r = Dest->b;
        Dest++;
      }
    }
  }

  return NewImage;
}

BOOLEAN
BigDiff (
  UINT8   a,
  UINT8   b
) {
  if (a > b) {
    if (!GlobalConfig.BackgroundDark) {
      return ((a - b) > (UINT8)(0xFF - GlobalConfig.BackgroundSharp));
    }
  } else if (GlobalConfig.BackgroundDark) {
    return ((b - a) > (UINT8)(0xFF - GlobalConfig.BackgroundSharp));
  }

  return 0;
}

//(c)Slice 2013
#define EDGE(P) \
do { \
  if (BigDiff (a11.P, a10.P)) { \
    if (!BigDiff (a11.P, a01.P) && !BigDiff (a11.P, a21.P)) { \
      a10.P = a11.P; \
    } else if (BigDiff (a11.P, a01.P)) { \
      if ((dx + dy) < cell) { \
        a11.P = a21.P = a12.P = (UINT8)((a10.P * (cell - dy + dx) + a01.P * (cell - dx + dy)) / (cell * 2)); \
      } else { \
        a10.P = a01.P = a11.P; \
      } \
    } else if (BigDiff (a11.P, a21.P)) { \
      if (dx > dy) { \
        a11.P = a01.P = a12.P = (UINT8)((a10.P * (cell * 2 - dy - dx) + a21.P * (dx + dy)) / (cell * 2)); \
      }else { \
        a10.P = a21.P = a11.P; \
      } \
    } \
  } else if (BigDiff (a11.P, a21.P)) { \
    if (!BigDiff (a11.P, a12.P)){ \
      a21.P = a11.P; \
    } else { \
      if ((dx + dy) > cell) { \
        a11.P = a01.P = a10.P = (UINT8)((a21.P * (cell + dx - dy) + a12.P * (cell - dx + dy)) / (cell * 2)); \
      } else { \
        a21.P = a12.P = a11.P; \
      } \
    } \
  } else if (BigDiff (a11.P, a01.P)) { \
    if (!BigDiff (a11.P, a12.P)){ \
      a01.P = a11.P; \
    } else { \
      if (dx < dy) { \
        a11.P = a21.P = a10.P = (UINT8)((a01.P * (cell * 2 - dx - dy) + a12.P * (dy + dx )) / (cell * 2)); \
      } else { \
        a01.P = a12.P = a11.P; \
      } \
    } \
  } else if (BigDiff (a11.P, a12.P)) { \
    a12.P = a11.P; \
  } \
} while (0)

#define SMOOTH(P) \
do { \
  norm = (INTN)a01.P + a10.P + 4 * a11.P + a12.P + a21.P; \
  if (norm == 0) { \
    Dest->P = 0; \
  } else { \
    Dest->P = (UINT8)(a11.P * 2 * (a01.P * (cell - dx) + a10.P * (cell - dy) + \
                      a21.P * dx + a12.P * dy + a11.P * 2 * cell) / (cell * norm)); \
  } \
} while (0)

#define SMOOTH2(P) \
do { \
     Dest->P = (UINT8)((a01.P * (cell - dx) * 3 + a10.P * (cell - dy) * 3 + \
                        a21.P * dx * 3 + a12.P * dy * 3 + a11.P * 2 * cell) / (cell * 8)); \
} while (0)

VOID
ScaleImage (
  OUT EG_IMAGE    *NewImage,
  IN EG_IMAGE     *OldImage
) {
  INTN        W1, W2, H1, H2, i, j, f, cell,
              x, dx, y, y1, dy; //, norm;
  EG_PIXEL    a10, a11, a12, a01, a21,
              *Src = OldImage->PixelData,
              *Dest = NewImage->PixelData;

  W1 = OldImage->Width;
  H1 = OldImage->Height;
  W2 = NewImage->Width;
  H2 = NewImage->Height;

  if (H1 * W2 < H2 * W1) {
    f = (H2 << 12) / H1;
  } else {
    f = (W2 << 12) / W1;
  }

  if (f == 0) {
   return;
  }

  cell = ((f - 1) >> 12) + 1;

  for (j = 0; j < H2; j++) {
    y = (j << 12) / f;
    y1 = y * W1;
    dy = j - ((y * f) >> 12);

    for (i = 0; i < W2; i++) {
      x = (i << 12) / f;
      dx = i - ((x * f) >> 12);
      a11 = Src[x + y1];
      a10 = (y == 0) ? a11 : Src[x + y1 - W1];
      a01 = (x == 0) ? a11 : Src[x + y1 - 1];
      a21 = (x >= W1) ? a11 : Src[x + y1 + 1];
      a12 = (y >= H1) ? a11 : Src[x + y1 + W1];

      if (a11.a == 0) {
        Dest->r = Dest->g = Dest->b = 0x55;
      } else {

        EDGE (r);
        EDGE (g);
        EDGE (b);

        SMOOTH2 (r);
        SMOOTH2 (g);
        SMOOTH2 (b);
      }

      Dest->a = 0xFF;
      Dest++;
    }
  }
}

VOID
FreeImage (
  IN EG_IMAGE   *Image
) {
  if (Image != NULL) {
    if (Image->PixelData != NULL) {
      FreePool (Image->PixelData);
      Image->PixelData = NULL; //FreePool will not zero pointer
    }
    FreePool (Image);
  }
}

//caller is responsible for free image
EG_IMAGE *
LoadImage (
  IN EFI_FILE_HANDLE    BaseDir,
  IN CHAR16             *FileName
) {
  EFI_STATUS    Status;
  UINT8         *FileData = NULL;
  UINTN         FileDataLength = 0;
  EG_IMAGE      *NewImage;

  if (BaseDir == NULL || FileName == NULL) {
    return NULL;
  }

  // load file
  Status = LoadFile (BaseDir, FileName, &FileData, &FileDataLength);
  //DBG ("File=%s loaded with status=%r length=%d\n", FileName, Status, FileDataLength);

  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //DBG ("   extension = %s\n", FindExtension (FileName));
  // decode it
  NewImage = DecodePNG (FileData, FileDataLength);
  //DBG ("decoded\n");

  if (!NewImage) {
    DBG ("%s not decoded\n", FileName);
  }

  FreePool (FileData);
  //DBG ("FreePool OK\n");

  return NewImage;
}

//
// Compositing
//

VOID
RestrictImageArea (
  IN EG_IMAGE     *Image,
  IN INTN         AreaPosX,
  IN INTN         AreaPosY,
  IN OUT INTN     *AreaWidth,
  IN OUT INTN     *AreaHeight
) {
  if (!Image || !AreaWidth || !AreaHeight) {
    return;
  }

  if ((AreaPosX >= Image->Width) || (AreaPosY >= Image->Height)) {
    // out of bounds, operation has no effect
    *AreaWidth  = 0;
    *AreaHeight = 0;
  } else {
    // calculate affected area
    if (*AreaWidth > Image->Width - AreaPosX) {
      *AreaWidth = Image->Width - AreaPosX;
    }

    if (*AreaHeight > Image->Height - AreaPosY) {
      *AreaHeight = Image->Height - AreaPosY;
    }
  }
}

VOID
FillImage (
  IN OUT EG_IMAGE     *CompImage,
  IN EG_PIXEL         *Color
) {
  INTN        i;
  EG_PIXEL    FillColor, *PixelPtr;

  if (!CompImage || !Color) {
    return;
  }

  FillColor = *Color;

  if (!CompImage->HasAlpha) {
    FillColor.a = 0;
  }

  PixelPtr = CompImage->PixelData;
  for (i = 0; i < CompImage->Width * CompImage->Height; i++) {
    *PixelPtr++ = FillColor;
  }
}

VOID
FillImageArea (
  IN OUT EG_IMAGE   *CompImage,
  IN INTN           AreaPosX,
  IN INTN           AreaPosY,
  IN INTN           AreaWidth,
  IN INTN           AreaHeight,
  IN EG_PIXEL       *Color
) {
  INTN        x, y,
              xAreaWidth = AreaWidth,
              xAreaHeight = AreaHeight;
  EG_PIXEL    FillColor, *PixelBasePtr;

  if (!CompImage || !Color) {
    return;
  }

  RestrictImageArea (CompImage, AreaPosX, AreaPosY, &xAreaWidth, &xAreaHeight);

  if (xAreaWidth > 0) {
    FillColor = *Color;

    if (!CompImage->HasAlpha) {
      FillColor.a = 0;
    }

    PixelBasePtr = CompImage->PixelData + AreaPosY * CompImage->Width + AreaPosX;
    for (y = 0; y < xAreaHeight; y++) {
      EG_PIXEL    *PixelPtr = PixelBasePtr;

      for (x = 0; x < xAreaWidth; x++) {
        *PixelPtr++ = FillColor;
      }

      PixelBasePtr += CompImage->Width;
    }
  }
}

VOID
RawCopy (
  IN OUT EG_PIXEL   *CompBasePtr,
  IN EG_PIXEL       *TopBasePtr,
  IN INTN           Width,
  IN INTN           Height,
  IN INTN           CompLineOffset,
  IN INTN           TopLineOffset
) {
  INTN    x, y;

  if (!CompBasePtr || !TopBasePtr) {
    return;
  }

  for (y = 0; y < Height; y++) {
    EG_PIXEL    *TopPtr = TopBasePtr;
    EG_PIXEL    *CompPtr = CompBasePtr;

    for (x = 0; x < Width; x++) {
      *CompPtr = *TopPtr;
      TopPtr++, CompPtr++;
    }

    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
RawCompose (
  IN OUT EG_PIXEL   *CompBasePtr,
  IN EG_PIXEL       *TopBasePtr,
  IN INTN           Width,
  IN INTN           Height,
  IN INTN           CompLineOffset,
  IN INTN           TopLineOffset
) {
  INT64       x, y;
  EG_PIXEL    *TopPtr, *CompPtr;
  //To make native division we need INTN types
  INTN        TopAlpha, Alpha, CompAlpha, RevAlpha, Temp;
  //EG_PIXEL    *CompUp;

  if (!CompBasePtr || !TopBasePtr) {
    return;
  }

  //CompUp = CompBasePtr + Width * Height;
  //Slice - my opinion
  //if TopAlpha=255 then draw Top - non transparent
  //else if TopAlpha=0 then draw Comp - full transparent
  //else draw mixture |-----comp---|--top--|
  //final alpha =(1 - (1 - x) * (1 - y)) =(255 * 255 - (255 - topA) * (255 - compA))/255

  for (y = 0; y < Height; y++) {
    TopPtr = TopBasePtr;
    CompPtr = CompBasePtr;

    for (x = 0; x < Width; x++) {
      TopAlpha = TopPtr->a & 0xFF; //exclude sign
      CompAlpha = CompPtr->a & 0xFF;
      RevAlpha = 255 - TopAlpha;
      //Alpha = 255 * (UINT8)TopAlpha + (UINT8)CompPtr->a * (UINT8)RevAlpha;
      Alpha = (255 * 255 - (255 - TopAlpha) * (255 - CompAlpha)) / 255;

      if (TopAlpha == 0) {
        TopPtr++, CompPtr++; // no need to bother
        continue;
      }

      Temp = (TopPtr->b * TopAlpha) + (CompPtr->b * RevAlpha);
      CompPtr->b = (UINT8)(Temp / 255);

      Temp = (TopPtr->g * TopAlpha) + (CompPtr->g  * RevAlpha);
      CompPtr->g = (UINT8)(Temp / 255);

      Temp = (TopPtr->r * TopAlpha) + (CompPtr->r * RevAlpha);
      CompPtr->r = (UINT8)(Temp / 255);

      CompPtr->a = (UINT8)Alpha;

      TopPtr++, CompPtr++;
    }

    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

//
// This is simplified image composing on solid background. ComposeImage will decide which method to use
//
VOID
RawComposeOnFlat (
  IN OUT EG_PIXEL   *CompBasePtr,
  IN EG_PIXEL       *TopBasePtr,
  IN INTN           Width,
  IN INTN           Height,
  IN INTN           CompLineOffset,
  IN INTN           TopLineOffset
) {
  INT64       x, y;
  EG_PIXEL    *TopPtr, *CompPtr;
  UINT32      TopAlpha, RevAlpha;
  UINTN       Temp;

  if (!CompBasePtr || !TopBasePtr) {
    return;
  }

  for (y = 0; y < Height; y++) {
    TopPtr = TopBasePtr;
    CompPtr = CompBasePtr;
    for (x = 0; x < Width; x++) {
      TopAlpha = TopPtr->a;
      RevAlpha = 255 - TopAlpha;

      Temp = ((UINT8)CompPtr->b * RevAlpha) + ((UINT8)TopPtr->b * TopAlpha);
      CompPtr->b = (UINT8)(Temp / 255);

      Temp = ((UINT8)CompPtr->g * RevAlpha) + ((UINT8)TopPtr->g * TopAlpha);
      CompPtr->g = (UINT8)(Temp / 255);

      Temp = ((UINT8)CompPtr->r * RevAlpha) + ((UINT8)TopPtr->r * TopAlpha);
      CompPtr->r = (UINT8)(Temp / 255);

      CompPtr->a = (UINT8)(255);

      TopPtr++, CompPtr++;
    }

    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
ComposeImage (
  IN OUT EG_IMAGE   *CompImage,
  IN EG_IMAGE       *TopImage,
  IN INTN           PosX,
  IN INTN           PosY
) {
  INTN    CompWidth, CompHeight;

  if (!TopImage || !CompImage) {
    return;
  }

  CompWidth  = TopImage->Width;
  CompHeight = TopImage->Height;
  RestrictImageArea (CompImage, PosX, PosY, &CompWidth, &CompHeight);

  // compose
  if (CompWidth > 0) {
    //if (CompImage->HasAlpha && !BackgroundImage) {
    //  CompImage->HasAlpha = FALSE;
    //}

    if (TopImage->HasAlpha) {
      if (CompImage->HasAlpha) {
        RawCompose (
          CompImage->PixelData + PosY * CompImage->Width + PosX,
          TopImage->PixelData,
          CompWidth, CompHeight, CompImage->Width, TopImage->Width
        );
      } else {
        RawComposeOnFlat (
          CompImage->PixelData + PosY * CompImage->Width + PosX,
          TopImage->PixelData,
          CompWidth, CompHeight, CompImage->Width, TopImage->Width
        );
      }
    } else {
      RawCopy (
        CompImage->PixelData + PosY * CompImage->Width + PosX,
        TopImage->PixelData,
        CompWidth, CompHeight, CompImage->Width, TopImage->Width
      );
    }
  }
}

EG_IMAGE *
EnsureImageSize (
  IN EG_IMAGE   *Image,
  IN INTN       Width,
  IN INTN       Height,
  IN EG_PIXEL   *Color
) {
  EG_IMAGE    *NewImage;

  if (Image == NULL) {
    return NULL;
  }

  if ((Image->Width == Width) && (Image->Height == Height)) {
    return Image;
  }

  NewImage = CreateFilledImage (Width, Height, Image->HasAlpha, Color);

  if (NewImage == NULL) {
    FreeImage (Image);
    return NULL;
  }

  ComposeImage (NewImage, Image, 0, 0);
  FreeImage (Image);

  return NewImage;
}

//
// misc internal functions
//

VOID
InsertPlane (
  IN UINT8    *SrcDataPtr,
  IN UINT8    *DestPlanePtr,
  IN UINTN    PixelCount
) {
  UINTN   i;

  if (!SrcDataPtr || !DestPlanePtr) {
    return;
  }

  for (i = 0; i < PixelCount; i++) {
    *DestPlanePtr = *SrcDataPtr++;
    DestPlanePtr += 4;
  }
}

VOID
SetPlane (
  IN UINT8    *DestPlanePtr,
  IN UINT8    Value,
  IN UINT64   PixelCount
) {
  UINT64  i;

  if (!DestPlanePtr) {
    return;
  }

  for (i = 0; i < PixelCount; i++) {
    *DestPlanePtr = Value;
    DestPlanePtr += 4;
  }
}

VOID
CopyPlane (
  IN UINT8    *SrcPlanePtr,
  IN UINT8    *DestPlanePtr,
  IN UINTN    PixelCount
) {
  UINTN i;

  if (!SrcPlanePtr || !DestPlanePtr) {
    return;
  }

  for (i = 0; i < PixelCount; i++) {
    *DestPlanePtr = *SrcPlanePtr;
    DestPlanePtr += 4, SrcPlanePtr += 4;
  }
}

EG_IMAGE *
DecodePNG (
  IN UINT8      *FileData,
  IN UINTN      FileDataLength
) {
  EG_IMAGE    *NewImage = NULL;
  EG_PIXEL    *PixelData, *Pixel, *PixelD;
  UINT32      PNG_error, Width, Height;
  INTN        i, ImageSize;

  PNG_error = lodepng_decode32 ((UINT8 **)&PixelData, &Width, &Height, (CONST UINT8 *)FileData, FileDataLength);

  if (PNG_error) {
    return NULL;
  }

  // allocate image structure and buffer
  NewImage = CreateImage ((INTN)Width, (INTN)Height, TRUE);
  if ((NewImage == NULL) || (NewImage->Width != (INTN)Width) || (NewImage->Height != (INTN)Height)) {
    return NULL;
  }

  ImageSize = (NewImage->Height * NewImage->Width);

  Pixel = (EG_PIXEL *)NewImage->PixelData;
  PixelD = PixelData;
  for (i = 0; i < ImageSize; i++, Pixel++, PixelD++) {
    Pixel->b = PixelD->r; //change r <-> b
    Pixel->r = PixelD->b;
    Pixel->g = PixelD->g;
    Pixel->a = PixelD->a; // 255 is opaque, 0 - transparent
  }

  lodepng_free (PixelData);

  return NewImage;
}



STATIC
INTN
ConvertEdgeAndPercentageToPixelPosition (
  INTN    Edge,
  INTN    DesiredPercentageFromEdge,
  INTN    ImageDimension,
  INTN    ScreenDimension
) {
  if ((Edge == SCREEN_EDGE_LEFT) || (Edge == SCREEN_EDGE_TOP)) {
    return ((ScreenDimension * DesiredPercentageFromEdge) / 100);
  } else if ((Edge == SCREEN_EDGE_RIGHT) || (Edge == SCREEN_EDGE_BOTTOM)) {
    return (ScreenDimension - ((ScreenDimension * DesiredPercentageFromEdge) / 100) - ImageDimension);
  }

  return 0xFFFF; // to indicate that wrong edge was specified.
}

STATIC
INTN
CalculateNudgePosition (
  INTN    Position,
  INTN    NudgeValue,
  INTN    ImageDimension,
  INTN    ScreenDimension
) {
  INTN    value = Position;

  if ((NudgeValue != INITVALUE) && (NudgeValue != 0) && (NudgeValue >= -32) && (NudgeValue <= 32)) {
    if ((value + NudgeValue >=0) && ((value + NudgeValue) <= (ScreenDimension - ImageDimension))) {
     value += NudgeValue;
    }
  }

  return value;
}

STATIC
BOOLEAN
IsImageWithinScreenLimits (
  INTN    Value,
  INTN    ImageDimension,
  INTN    ScreenDimension
) {
  return ((Value >= 0) && ((Value + ImageDimension) <= ScreenDimension));
}

STATIC
INTN
RepositionFixedByCenter (
  INTN    Value,
  INTN    ScreenDimension,
  INTN    DesignScreenDimension
) {
  return (Value + ((ScreenDimension - DesignScreenDimension) / 2));
}

STATIC
INTN
RepositionRelativeByGapsOnEdges (
  INTN    Value,
  INTN    ImageDimension,
  INTN    ScreenDimension,
  INTN    DesignScreenDimension
) {
  return (Value * (ScreenDimension - ImageDimension) / (DesignScreenDimension - ImageDimension));
}

STATIC
INTN
HybridRepositioning (
  INTN    Edge,
  INTN    Value,
  INTN    ImageDimension,
  INTN    ScreenDimension,
  INTN    DesignScreenDimension
) {
  INTN    pos, posThemeDesign;

  if ((DesignScreenDimension == 0xFFFF) || (ScreenDimension == DesignScreenDimension)) {
    // Calculate the horizontal pixel to place the top left corner of the animation - by screen resolution
    pos = ConvertEdgeAndPercentageToPixelPosition (Edge, Value, ImageDimension, ScreenDimension);
  } else {
    // Calculate the horizontal pixel to place the top left corner of the animation - by theme design resolution
    posThemeDesign = ConvertEdgeAndPercentageToPixelPosition (Edge, Value, ImageDimension, DesignScreenDimension);
    // Try repositioning by center first
    pos = RepositionFixedByCenter (posThemeDesign, ScreenDimension, DesignScreenDimension);

    // If out of edges, try repositioning by gaps on edges
    if (!IsImageWithinScreenLimits (pos, ImageDimension, ScreenDimension)) {
      pos = RepositionRelativeByGapsOnEdges (posThemeDesign, ImageDimension, ScreenDimension, DesignScreenDimension);
    }
  }

  return pos;
}

VOID
BltClearScreen (
  IN BOOLEAN    ShowBanner
) { //ShowBanner always TRUE
  EG_PIXEL  *p1;
  INTN      i, j, x, x1, x2, y, y1, y2,
            BanHeight = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + LAYOUT_BANNER_HEIGHT;

  if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)) {
    // Banner is used in this theme
    if (!Banner) {
      // Banner is not loaded yet
      if (IsEmbeddedTheme ()) {
        Banner = BuiltinIcon (BUILTIN_ICON_BANNER);
        CopyMem (&TmpBackgroundPixel, &GrayBackgroundPixel, sizeof (EG_PIXEL));
      } else  {
        Banner = LoadImage (ThemeDir, GlobalConfig.BannerFileName);
        if (Banner) {
          // Banner was changed, so copy into BlueBackgroundBixel first pixel of banner
          CopyMem (&TmpBackgroundPixel, &Banner->PixelData[0], sizeof (EG_PIXEL));
        } else {
          DBG ("banner file not read\n");
        }
      }
    }

    if (Banner) {
      // Banner was loaded, so calculate its size and position
      BannerPlace.Width = Banner->Width;
      BannerPlace.Height = (BanHeight >= Banner->Height) ? (INTN)Banner->Height : BanHeight;
      // Check if new style placement value was used for banner in theme.plist
      if (
        (((INT32)GlobalConfig.BannerPosX >= 0) && ((INT32)GlobalConfig.BannerPosX <= 100)) &&
        (((INT32)GlobalConfig.BannerPosY >= 0) && ((INT32)GlobalConfig.BannerPosY <= 100))
      ) {
        // Check if screen size being used is different from theme origination size.
        // If yes, then recalculate the placement % value.
        // This is necessary because screen can be a different size, but banner is not scaled.
        BannerPlace.XPos = HybridRepositioning (
                              GlobalConfig.BannerEdgeHorizontal,
                              GlobalConfig.BannerPosX, BannerPlace.Width,  UGAWidth,  GlobalConfig.ThemeDesignWidth
                            );

        BannerPlace.YPos = HybridRepositioning (
                              GlobalConfig.BannerEdgeVertical,
                              GlobalConfig.BannerPosY, BannerPlace.Height, UGAHeight, GlobalConfig.ThemeDesignHeight
                            );

        // Check if banner is required to be nudged.
        BannerPlace.XPos = CalculateNudgePosition (BannerPlace.XPos, GlobalConfig.BannerNudgeX, Banner->Width,  UGAWidth);
        BannerPlace.YPos = CalculateNudgePosition (BannerPlace.YPos, GlobalConfig.BannerNudgeY, Banner->Height, UGAHeight);
      } else {
        // Use rEFIt default (no placement values speicifed)
        BannerPlace.XPos = (UGAWidth - Banner->Width) >> 1;
        BannerPlace.YPos = (BanHeight >= Banner->Height) ? (BanHeight - Banner->Height) : 0;
      }
    }
  }

  if (
    !Banner || (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER) ||
    !IsImageWithinScreenLimits (BannerPlace.XPos, BannerPlace.Width, UGAWidth) ||
    !IsImageWithinScreenLimits (BannerPlace.YPos, BannerPlace.Height, UGAHeight)
  ) {
    // Banner is disabled or it cannot be used, apply defaults for placement
    if (Banner) {
      FreePool (Banner);
      Banner = NULL;
    }

    BannerPlace.XPos = 0;
    BannerPlace.YPos = 0;
    BannerPlace.Width = UGAWidth;
    BannerPlace.Height = BanHeight;
  }

  // Load Background and scale
  if (!BigBack && (GlobalConfig.BackgroundName != NULL)) {
    BigBack = LoadImage (ThemeDir, GlobalConfig.BackgroundName);
  }

  if (
    (BackgroundImage != NULL) &&
    ((BackgroundImage->Width != UGAWidth) || (BackgroundImage->Height != UGAHeight))
  ) {
    // Resolution changed
    FreeImage (BackgroundImage);
    BackgroundImage = NULL;
  }

  if (BackgroundImage == NULL) {
    BackgroundImage = CreateFilledImage (UGAWidth, UGAHeight, FALSE, &TmpBackgroundPixel);
  }

  if (BigBack != NULL) {
    switch (GlobalConfig.BackgroundScale) {
      case Scale:
        ScaleImage (BackgroundImage, BigBack);
        break;

      case Crop:
        x = UGAWidth - BigBack->Width;
        if (x >= 0) {
          x1 = x >> 1;
          x2 = 0;
          x = BigBack->Width;
        } else {
          x1 = 0;
          x2 = (-x) >> 1;
          x = UGAWidth;
        }

        y = UGAHeight - BigBack->Height;

        if (y >= 0) {
          y1 = y >> 1;
          y2 = 0;
          y = BigBack->Height;
        } else {
          y1 = 0;
          y2 = (-y) >> 1;
          y = UGAHeight;
        }

        RawCopy (
          BackgroundImage->PixelData + y1 * UGAWidth + x1,
          BigBack->PixelData + y2 * BigBack->Width + x2,
          x, y, UGAWidth, BigBack->Width
        );
        break;

      case Tile:
        x = (BigBack->Width * ((UGAWidth - 1) / BigBack->Width + 1) - UGAWidth) >> 1;
        y = (BigBack->Height * ((UGAHeight - 1) / BigBack->Height + 1) - UGAHeight) >> 1;
        p1 = BackgroundImage->PixelData;

        for (j = 0; j < UGAHeight; j++) {
          y2 = ((j + y) % BigBack->Height) * BigBack->Width;

          for (i = 0; i < UGAWidth; i++) {
            *p1++ = BigBack->PixelData[y2 + ((i + x) % BigBack->Width)];
          }
        }
        break;

      case None:
      default:
        // already scaled
        break;
    }
  }

  // Draw background
  if (BackgroundImage) {
    BltImage (BackgroundImage, 0, 0); //if NULL then do nothing
  } else {
    ClearScreen (IsEmbeddedTheme () ? &GrayBackgroundPixel : &BlackBackgroundPixel);
  }

  // Draw banner
  if (Banner && ShowBanner) {
    BltImageAlpha (Banner, BannerPlace.XPos, BannerPlace.YPos, &TransparentBackgroundPixel, 16);
  }

  GraphicsScreenDirty = FALSE;
}

VOID
BltImage (
  IN EG_IMAGE   *Image,
  IN INTN       XPos,
  IN INTN       YPos
) {
  if (!Image) {
    return;
  }

  DrawImageArea (Image, 0, 0, 0, 0, XPos, YPos);

  GraphicsScreenDirty = TRUE;
}

VOID
BltImageAlpha (
  IN EG_IMAGE   *Image,
  IN INTN       XPos,
  IN INTN       YPos,
  IN EG_PIXEL   *BackgroundPixel,
  IN INTN       Scale
) {
  EG_IMAGE    *CompImage, *NewImage = NULL;
  INTN        Width = Scale << 3, Height = Width;

  GraphicsScreenDirty = TRUE;

  if (Image) {
    NewImage = CopyScaledImage (Image, Scale); //will be Scale/16
    Width = NewImage->Width;
    Height = NewImage->Height;
  }

  // compose on background
  CompImage = CreateFilledImage (Width, Height, (BackgroundImage != NULL), BackgroundPixel);
  ComposeImage (CompImage, NewImage, 0, 0);

  if (NewImage) {
    FreeImage (NewImage);
  }

  if (!BackgroundImage) {
    DrawImageArea (CompImage, 0, 0, 0, 0, XPos, YPos);
    FreeImage (CompImage);
    return;
  }

  NewImage = CreateImage (Width, Height, FALSE);

  if (!NewImage) {
    return;
  }

  RawCopy (
    NewImage->PixelData,
    BackgroundImage->PixelData + YPos * BackgroundImage->Width + XPos,
    Width, Height,
    Width,
    BackgroundImage->Width
  );

  ComposeImage (NewImage, CompImage, 0, 0);
  FreeImage (CompImage);

  // blit to screen and clean up
  DrawImageArea (NewImage, 0, 0, 0, 0, XPos, YPos);
  FreeImage (NewImage);
}

VOID
BltImageComposite (
  IN EG_IMAGE   *BaseImage,
  IN EG_IMAGE   *TopImage,
  IN INTN       XPos,
  IN INTN       YPos
) {
  INTN        TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
  EG_IMAGE    *CompImage;

  if (!BaseImage || !TopImage) {
    return;
  }

  // initialize buffer with base image
  CompImage = CopyImage (BaseImage);
  TotalWidth  = BaseImage->Width;
  TotalHeight = BaseImage->Height;

  // place the top image
  CompWidth = TopImage->Width;

  if (CompWidth > TotalWidth) {
    CompWidth = TotalWidth;
  }

  OffsetX = (TotalWidth - CompWidth) >> 1;
  CompHeight = TopImage->Height;

  if (CompHeight > TotalHeight) {
    CompHeight = TotalHeight;
  }

  OffsetY = (TotalHeight - CompHeight) >> 1;
  ComposeImage (CompImage, TopImage, OffsetX, OffsetY);

  // blit to screen and clean up
  //DrawImageArea (CompImage, 0, 0, TotalWidth, TotalHeight, XPos, YPos);
  BltImageAlpha (CompImage, XPos, YPos, &TransparentBackgroundPixel, 16);
  FreeImage (CompImage);

  GraphicsScreenDirty = TRUE;
}

/*
  --------------------------------------------------------------------
  Pos                           : Bottom    -> Mid        -> Top
  --------------------------------------------------------------------
   GlobalConfig.SelectionOnTop  : MainImage -> Badge      -> Selection
  !GlobalConfig.SelectionOnTop  : Selection -> MainImage  -> Badge
*/

VOID
BltImageCompositeBadge (
  IN EG_IMAGE   *BaseImage,
  IN EG_IMAGE   *TopImage,
  IN EG_IMAGE   *BadgeImage,
  IN INTN       XPos,
  IN INTN       YPos,
  IN INTN       Scale
) {

  INTN          TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY, OffsetXTmp, OffsetYTmp;
  EG_IMAGE      *CompImage = NULL, *NewBaseImage, *NewTopImage;
  BOOLEAN       Selected = TRUE;

  if (!BaseImage || !TopImage) {
    return;
  }

  if (Scale < 0) {
    Scale = -Scale;
    Selected = FALSE;
  }

  NewBaseImage = CopyScaledImage (BaseImage, Scale); //will be Scale/16
  TotalWidth = NewBaseImage->Width;
  TotalHeight = NewBaseImage->Height;
  //DBG ("BaseImage: Width=%d Height=%d Alfa=%d\n", TotalWidth, TotalHeight, NewBaseImage->HasAlpha);

  NewTopImage = CopyScaledImage (TopImage, Scale); //will be Scale/16
  CompWidth = NewTopImage->Width;
  CompHeight = NewTopImage->Height;
  //DBG ("TopImage: Width=%d Height=%d Alfa=%d\n", CompWidth, CompHeight, NewTopImage->HasAlpha);

  CompImage = CreateFilledImage (
                (CompWidth > TotalWidth) ? CompWidth : TotalWidth,
                (CompHeight > TotalHeight) ? CompHeight : TotalHeight,
                TRUE,
                &TransparentBackgroundPixel
              );

  if (!CompImage) {
    DBG ("Can't create CompImage\n");
    return;
  }

  //to simplify suppose square images
  if (CompWidth < TotalWidth) {
    OffsetX = (TotalWidth - CompWidth) >> 1;
    OffsetY = (TotalHeight - CompHeight) >> 1;

    ComposeImage (CompImage, NewBaseImage, 0, 0);

    if (!GlobalConfig.SelectionOnTop) {
      ComposeImage (CompImage, NewTopImage, OffsetX, OffsetY);
    }

    CompWidth = TotalWidth;
    CompHeight = TotalHeight;
  } else {
    OffsetX = (CompWidth - TotalWidth) >> 1;
    OffsetY = (CompHeight - TotalHeight) >> 1;

    ComposeImage (CompImage, NewBaseImage, OffsetX, OffsetY);

    if (!GlobalConfig.SelectionOnTop) {
      ComposeImage (CompImage, NewTopImage, 0, 0);
    }
  }

  OffsetXTmp = OffsetX;
  OffsetYTmp = OffsetY;

  // place the badge image
  if (
    (BadgeImage != NULL) &&
    ((BadgeImage->Width + 8) < CompWidth) &&
    ((BadgeImage->Height + 8) < CompHeight)
  ) {
    //blackosx
    // Check for user badge x offset from theme.plist
    if (GlobalConfig.BadgeOffsetX != 0xFFFF) {
      // Check if value is between 0 and ( width of the main icon - width of badge )
      if ((GlobalConfig.BadgeOffsetX < 0) || (GlobalConfig.BadgeOffsetX > (CompWidth - BadgeImage->Width))) {
        DBG ("User offset X %d is out of range\n", GlobalConfig.BadgeOffsetX);
        GlobalConfig.BadgeOffsetX = CompWidth  - 8 - BadgeImage->Width;
        DBG ("   corrected to default %d\n", GlobalConfig.BadgeOffsetX);
      }
      OffsetX += GlobalConfig.BadgeOffsetX;
    } else {
      // Set default position
      OffsetX += CompWidth - 8 - BadgeImage->Width;
    }

    // Check for user badge y offset from theme.plist
    if (GlobalConfig.BadgeOffsetY != 0xFFFF) {
      // Check if value is between 0 and ( height of the main icon - height of badge )
      if ((GlobalConfig.BadgeOffsetY < 0) || (GlobalConfig.BadgeOffsetY > (CompHeight - BadgeImage->Height))) {
        DBG ("User offset Y %d is out of range\n",GlobalConfig.BadgeOffsetY);
        GlobalConfig.BadgeOffsetY = CompHeight - 8 - BadgeImage->Height;
        DBG ("   corrected to default %d\n", GlobalConfig.BadgeOffsetY);
      }
      OffsetY += GlobalConfig.BadgeOffsetY;
    } else {
      // Set default position
      OffsetY += CompHeight - 8 - BadgeImage->Height;
    }

    ComposeImage (CompImage, BadgeImage, OffsetX, OffsetY);
  }

  if (GlobalConfig.SelectionOnTop) {
    if (CompWidth < TotalWidth) {
      ComposeImage (CompImage, NewTopImage, OffsetXTmp, OffsetYTmp);
    } else {
      ComposeImage (CompImage, NewTopImage, 0, 0);
    }
  }

  BltImageAlpha (CompImage, XPos, YPos, &TransparentBackgroundPixel, (GlobalConfig.NonSelectedGrey && !Selected) ? -16 : 16);

  FreeImage (CompImage);
  FreeImage (NewBaseImage);
  FreeImage (NewTopImage);

  GraphicsScreenDirty = TRUE;
}

//
//  ANIME
//

VOID
FreeAnime (
  GUI_ANIME   *Anime
) {
  if (Anime) {
    if (Anime->Path) {
      FreePool (Anime->Path);
      Anime->Path = NULL;
    }

    FreePool (Anime);
    //Anime = NULL;
  }
}

STATIC EG_IMAGE *AnimeImage = NULL;

VOID
UpdateAnime (
  REFIT_MENU_SCREEN   *Screen,
  EG_RECT             *Place
) {
  UINT64      Now;
  INTN        x, y, MenuWidth = 50;

  if (!Screen || !Screen->AnimeRun || !Screen->Film || GlobalConfig.TextOnly) {
   return;
  }

  if (
    !AnimeImage ||
    (AnimeImage->Width != Screen->Film[0]->Width) ||
    (AnimeImage->Height != Screen->Film[0]->Height)
  ){
    if (AnimeImage) {
      FreeImage (AnimeImage);
    }

    AnimeImage = CreateImage (Screen->Film[0]->Width, Screen->Film[0]->Height, TRUE);
  }

  // Retained for legacy themes without new anim placement options.
  x = Place->XPos + (Place->Width - AnimeImage->Width) / 2;
  y = Place->YPos + (Place->Height - AnimeImage->Height) / 2;

  if (
    !IsImageWithinScreenLimits (x, Screen->Film[0]->Width, UGAWidth) ||
    !IsImageWithinScreenLimits (y, Screen->Film[0]->Height, UGAHeight)
  ) {
    // This anime can't be displayed
    return;
  }

  // Check if the theme.plist setting for allowing an anim to be moved horizontally in the quest
  // to avoid overlapping the menu text on menu pages at lower resolutions is set.
  if ((Screen->ID > 1) && (GlobalConfig.LayoutAnimMoveForMenuX != 0)) { // these screens have text menus which the anim may interfere with.
    MenuWidth = TEXT_XMARGIN * 2 + (50 * GlobalConfig.CharWidth); // taken from menu.c
    if ((x + Screen->Film[0]->Width) > (UGAWidth - MenuWidth) >> 1) {
      if (
        (x + GlobalConfig.LayoutAnimMoveForMenuX >= 0) ||
        ((UGAWidth - (x + GlobalConfig.LayoutAnimMoveForMenuX + Screen->Film[0]->Width)) <= 100)
      ) {
        x += GlobalConfig.LayoutAnimMoveForMenuX;
      }
    }
  }

  Now = AsmReadTsc ();

  if (Screen->LastDraw == 0) {
    //first start, we should save background into last frame
    FillImageArea (AnimeImage, 0, 0, AnimeImage->Width, AnimeImage->Height, &TransparentBackgroundPixel);
    TakeImage (
      Screen->Film[Screen->Frames],
      x, y,
      Screen->Film[Screen->Frames]->Width,
      Screen->Film[Screen->Frames]->Height
    );
  }

  if (TimeDiff (Screen->LastDraw, Now) < Screen->FrameTime) {
    return;
  }

  if (Screen->Film[Screen->CurrentFrame]) {
    RawCopy (
      AnimeImage->PixelData, Screen->Film[Screen->Frames]->PixelData,
      Screen->Film[Screen->Frames]->Width,
      Screen->Film[Screen->Frames]->Height,
      AnimeImage->Width,
      Screen->Film[Screen->Frames]->Width
    );
    ComposeImage (AnimeImage, Screen->Film[Screen->CurrentFrame], 0, 0);
    BltImage (AnimeImage, x, y);
  }

  Screen->CurrentFrame++;

  if (Screen->CurrentFrame >= Screen->Frames) {
    Screen->AnimeRun = !Screen->Once;
    Screen->CurrentFrame = 0;
  }
  Screen->LastDraw = Now;
}

VOID
InitAnime (
  REFIT_MENU_SCREEN   *Screen
) {
  CHAR16      FileName[AVALUE_MAX_SIZE], *Path;
  EG_IMAGE    *p = NULL, *Last = NULL;
  GUI_ANIME   *Anime;

  if (!Screen || GlobalConfig.TextOnly){
    return;
  }

  for (Anime = GuiAnime; Anime != NULL && Anime->ID != Screen->ID; Anime = Anime->Next);

  // Check if we should clear old film vars (no anime or anime path changed)
  //
  if (
    gThemeOptionsChanged ||
    !Anime ||
    !Screen->Film ||
    IsEmbeddedTheme () ||
    !Screen->Theme ||
    (/*gThemeChanged && */StriCmp (GlobalConfig.Theme, Screen->Theme) != 0)
  ) {
    //DBG (" free screen\n");
    if (Screen->Film) {
      //free images in the film
      INTN  i;

      for (i = 0; i <= Screen->Frames; i++) { //really there are N + 1 frames
        // free only last occurrence of repeated frames
        if ((Screen->Film[i] != NULL) && ((i == Screen->Frames) || (Screen->Film[i] != Screen->Film[i + 1]))) {
          FreePool (Screen->Film[i]);
        }
      }

      FreePool (Screen->Film);
      Screen->Film = NULL;
    }

    if (Screen->Theme) {
      FreePool (Screen->Theme);
      Screen->Theme = NULL;
    }
  }

  // Check if we should load anime files (first run or after theme change)
  if (Anime && (Screen->Film == NULL)) {
    Path = Anime->Path;
    Screen->Film = (EG_IMAGE **)AllocateZeroPool ((Anime->Frames + 1) * sizeof (VOID *));

    if (Path && Screen->Film) {
      // Look through contents of the directory
      UINTN   i;

      for (i = 0; i < Anime->Frames; i++) {
        UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\%s_%03d.png", Path, Path, i);
        //DBG ("Try to load file %s\n", FileName);

        p = LoadImage (ThemeDir, FileName);
        if (!p) {
          p = Last;
          if (!p) break;
        } else {
          Last = p;
        }

        Screen->Film[i] = p;
      }

      if (Screen->Film[0] != NULL) {
        Screen->Frames = i;
        //DBG (" found %d frames of the anime\n", i);
        // Create background frame
        Screen->Film[i] = CreateImage (Screen->Film[0]->Width, Screen->Film[0]->Height, FALSE);
        // Copy some settings from Anime into Screen
        Screen->FrameTime = Anime->FrameTime;
        Screen->Once = Anime->Once;
        Screen->Theme = AllocateCopyPool (StrSize (GlobalConfig.Theme), GlobalConfig.Theme);
      }
    }
  }
  // Check if a new style placement value has been specified
  if (
    Anime && (Anime->FilmX >=0) && (Anime->FilmX <=100) &&
    (Anime->FilmY >=0) && (Anime->FilmY <=100) &&
    (Screen->Film != NULL) && (Screen->Film[0] != NULL)
  ) {
    // Check if screen size being used is different from theme origination size.
    // If yes, then recalculate the animation placement % value.
    // This is necessary because screen can be a different size, but anim is not scaled.
    Screen->FilmPlace.XPos = HybridRepositioning (
                                Anime->ScreenEdgeHorizontal,
                                Anime->FilmX,
                                Screen->Film[0]->Width,
                                UGAWidth,
                                GlobalConfig.ThemeDesignWidth
                              );

    Screen->FilmPlace.YPos = HybridRepositioning (
                                Anime->ScreenEdgeVertical,
                                Anime->FilmY,
                                Screen->Film[0]->Height,
                                UGAHeight,
                                GlobalConfig.ThemeDesignHeight
                              );

    // Does the user want to fine tune the placement?
    Screen->FilmPlace.XPos = CalculateNudgePosition (Screen->FilmPlace.XPos, Anime->NudgeX, Screen->Film[0]->Width, UGAWidth);
    Screen->FilmPlace.YPos = CalculateNudgePosition (Screen->FilmPlace.YPos, Anime->NudgeY, Screen->Film[0]->Height, UGAHeight);

    Screen->FilmPlace.Width = Screen->Film[0]->Width;
    Screen->FilmPlace.Height = Screen->Film[0]->Height;
    //DBG ("recalculated Screen->Film position\n");
  } else {
    // We are here if there is no anime, or if we use oldstyle placement values
    // For both these cases, FilmPlace will be set after banner/menutitle positions are known
    Screen->FilmPlace.XPos = 0;
    Screen->FilmPlace.YPos = 0;
    Screen->FilmPlace.Width = 0;
    Screen->FilmPlace.Height = 0;
  }

  if ((Screen->Film != NULL) && (Screen->Film[0] != NULL)) {
    // Anime seems OK, init it
    Screen->AnimeRun = TRUE;
    Screen->CurrentFrame = 0;
    Screen->LastDraw = 0;
  } else {
    Screen->AnimeRun = FALSE;
  }
  //DBG ("anime inited\n");
}

BOOLEAN
GetAnime (
  REFIT_MENU_SCREEN   *Screen
) {
  GUI_ANIME   *Anime;

  if (!Screen || !GuiAnime) {
    return FALSE;
  }

  for (Anime = GuiAnime; Anime != NULL && Anime->ID != Screen->ID; Anime = Anime->Next);

  if (Anime == NULL) {
    return FALSE;
  }

  //DBG ("Use anime=%s frames=%d\n", Anime->Path, Anime->Frames);

  return TRUE;
}

EG_PIXEL
ToPixel (
  UINTN   rgba
) {
  EG_PIXEL color;

  color.r = (rgba >> 24) & 0XFF;
  color.g = (rgba >> 16) & 0XFF;
  color.b = (rgba >> 8) & 0XFF;
  color.a = rgba & 0XFF;

  return color;
}

/* EOF */
