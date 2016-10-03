/*
 * libeg/screen.c
 * Screen handling functions
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

#include "libegint.h"
//#include "lodepng.h"

//#include <efiUgaDraw.h>
#include <Protocol/GraphicsOutput.h>
//#include <Protocol/efiConsoleControl.h>

// Console defines and variables

static EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;

static EFI_GUID UgaDrawProtocolGuid = EFI_UGA_DRAW_PROTOCOL_GUID;
static EFI_UGA_DRAW_PROTOCOL *UgaDraw = NULL;

static EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput = NULL;

static BOOLEAN egHasGraphics = FALSE;
static UINTN egScreenWidth  = 0; //1024;
static UINTN egScreenHeight = 0; //768;

static EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE ConsoleControlGetMode = NULL;

//static EFI_STATUS GopSetModeAndReconnectTextOut();

//
// Null ConsoleControl GetMode() implementation - for blocking resolution switch when using text mode
//
EFI_STATUS
EFIAPI
NullConsoleControlGetModeText (
  IN EFI_CONSOLE_CONTROL_PROTOCOL       *This,
  OUT EFI_CONSOLE_CONTROL_SCREEN_MODE   *Mode,
  OUT BOOLEAN                           *GopUgaExists,
  OPTIONAL OUT BOOLEAN                  *StdInLocked OPTIONAL
) {
  *Mode = EfiConsoleControlScreenText;

  if (GopUgaExists) {
    *GopUgaExists = TRUE;
  }

  if (StdInLocked) {
    *StdInLocked = FALSE;
  }

  return EFI_SUCCESS;
}

//
// Sets mode via GOP protocol, and reconnects simple text out drivers
//

STATIC
EFI_STATUS
GopSetModeAndReconnectTextOut (
  IN UINT32     ModeNumber
) {
  EFI_STATUS  Status;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = GraphicsOutput->SetMode(GraphicsOutput, ModeNumber);
  MsgLog("Video mode change to mode #%d: %r\n", ModeNumber, Status);

  return Status;
}

EFI_STATUS
egSetMaxResolution() {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  EFI_STATUS    Status = EFI_UNSUPPORTED;
  UINT32        Width = 0, Height = 0, BestMode = 0,
                MaxMode, Mode;
  UINTN         SizeOfInfo;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  MsgLog("SetMaxResolution: ");
  MaxMode = GraphicsOutput->Mode->MaxMode;
  for (Mode = 0; Mode < MaxMode; Mode++) {
    Status = GraphicsOutput->QueryMode(GraphicsOutput, Mode, &SizeOfInfo, &Info);
    if (Status == EFI_SUCCESS) {
      if ((Width > Info->HorizontalResolution) || (Height > Info->VerticalResolution)) {
        continue;
      }
      Width = Info->HorizontalResolution;
      Height = Info->VerticalResolution;
      BestMode = Mode;
    }
  }

  MsgLog("Found best mode %d: %dx%d\n", BestMode, Width, Height);

  // check if requested mode is equal to current mode
  if (BestMode == GraphicsOutput->Mode->Mode) {
    //MsgLog(" - already set\n");
    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    Status = EFI_SUCCESS;
  } else {
    //Status = GraphicsOutput->SetMode(GraphicsOutput, BestMode);
    Status = GopSetModeAndReconnectTextOut(BestMode);
    if (Status == EFI_SUCCESS) {
      egScreenWidth = Width;
      egScreenHeight = Height;
      //MsgLog(" - set\n", Status);
    } else {
      // we can not set BestMode - search for first one that we can
      //MsgLog(" - %r\n", Status);
      Status = egSetMode(1);
    }
  }

  return Status;
}

EFI_STATUS
egSetMode(
  INT32     Next
) {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  EFI_STATUS    Status = EFI_UNSUPPORTED;
  UINT32        MaxMode, Index = 0;
  UINTN         SizeOfInfo;
  INT32         Mode;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  MaxMode = GraphicsOutput->Mode->MaxMode;
  Mode = GraphicsOutput->Mode->Mode;
  while (EFI_ERROR(Status) && Index <= MaxMode) {
    Mode = Mode + Next;
    Mode = (Mode >= (INT32)MaxMode) ? 0 : Mode;
    Mode = (Mode < 0) ? ((INT32)MaxMode - 1) : Mode;
    Status = GraphicsOutput->QueryMode(GraphicsOutput, (UINT32)Mode, &SizeOfInfo, &Info);

    MsgLog("QueryMode %d Status=%r\n", Mode, Status);

    if (Status == EFI_SUCCESS) {
      //Status = GraphicsOutput->SetMode(GraphicsOutput, (UINT32)Mode);
      Status = GopSetModeAndReconnectTextOut((UINT32)Mode);
      //MsgLog("SetMode %d Status=%r\n", Mode, Status);
      egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
      egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    }
    Index++;
  }

  return Status;
}

EFI_STATUS
egSetScreenResolution (
  IN CHAR16     *WidthHeight
) {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  EFI_STATUS      Status = EFI_UNSUPPORTED;
  UINT32          Width, Height, MaxMode, Mode;
  CHAR16          *HeightP;
  UINTN           SizeOfInfo;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (WidthHeight == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  MsgLog("SetScreenResolution: %s", WidthHeight);

  // we are expecting WidthHeight=L"1024x768"
  // parse Width and Height
  HeightP = WidthHeight;

  while (*HeightP != L'\0' && *HeightP != L'x' && *HeightP != L'X') {
    HeightP++;
  }

  if (*HeightP == L'\0') {
    return EFI_INVALID_PARAMETER;
  }

  HeightP++;
  Width = (UINT32)StrDecimalToUintn(WidthHeight);
  Height = (UINT32)StrDecimalToUintn(HeightP);

  // check if requested mode is equal to current mode
  if ((GraphicsOutput->Mode->Info->HorizontalResolution == Width) && (GraphicsOutput->Mode->Info->VerticalResolution == Height)) {
    MsgLog(" - already set\n");
    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;

    return EFI_SUCCESS;
  }

  // iterate through modes and set it if found
  MaxMode = GraphicsOutput->Mode->MaxMode;
  for (Mode = 0; Mode < MaxMode; Mode++) {
    Status = GraphicsOutput->QueryMode(GraphicsOutput, Mode, &SizeOfInfo, &Info);
    if (Status == EFI_SUCCESS) {
      if ((Width == Info->HorizontalResolution) && (Height == Info->VerticalResolution)) {
        MsgLog(" - setting Mode %d\n", Mode);

        //Status = GraphicsOutput->SetMode(GraphicsOutput, Mode);
        Status = GopSetModeAndReconnectTextOut(Mode);
        if (Status == EFI_SUCCESS) {
          egScreenWidth = Width;
          egScreenHeight = Height;

          return EFI_SUCCESS;
        }
      }
    }
  }

  MsgLog(" - not found!\n");

  return EFI_UNSUPPORTED;
}

VOID
egInitScreen (
  IN BOOLEAN    SetMaxResolution
) {
  EFI_STATUS    Status;
  UINT32        Width, Height, Depth, RefreshRate;
  CHAR16        *Resolution;

  // get protocols
  Status = EfiLibLocateProtocol(&ConsoleControlProtocolGuid, (VOID **) &ConsoleControl);

  if (EFI_ERROR(Status)) {
    ConsoleControl = NULL;
  }

  Status = EfiLibLocateProtocol(&UgaDrawProtocolGuid, (VOID **) &UgaDraw);

  if (EFI_ERROR(Status)) {
    UgaDraw = NULL;
  }

  Status = EfiLibLocateProtocol(&GraphicsOutputProtocolGuid, (VOID **) &GraphicsOutput);

  if (EFI_ERROR(Status)) {
    GraphicsOutput = NULL;
  }

  // store original GetMode
  if ((ConsoleControl != NULL) && (ConsoleControlGetMode == NULL)) {
    ConsoleControlGetMode = ConsoleControl->GetMode;
  }

  // if it not the first run, just restore resolution
  if ((egScreenWidth != 0) && (egScreenHeight != 0)) {
    Resolution = PoolPrint(L"%dx%d",egScreenWidth,egScreenHeight);
    if (Resolution) {
      Status = egSetScreenResolution(Resolution);
      FreePool(Resolution);

      if (!EFI_ERROR(Status)) {
        return;
      }
    }
  }

  // get screen size
  egHasGraphics = FALSE;

  if (GraphicsOutput != NULL) {
    if (GlobalConfig.ScreenResolution != NULL) {
      if (EFI_ERROR(egSetScreenResolution(GlobalConfig.ScreenResolution))) {
        if (SetMaxResolution) {
          egSetMaxResolution();
        }
      }
    } else {
      if (SetMaxResolution) {
        egSetMaxResolution();
      }
    }

    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    egHasGraphics = TRUE;
  } else if (UgaDraw != NULL) {
    //is there anybody ever see UGA protocol???
    //MsgLog("you are lucky guy having UGA, inform please projectosx!\n");
    Status = UgaDraw->GetMode(UgaDraw, &Width, &Height, &Depth, &RefreshRate);
    if (EFI_ERROR(Status)) {
      UgaDraw = NULL;   // graphics not available
    } else {
      egScreenWidth  = Width;
      egScreenHeight = Height;
      egHasGraphics = TRUE;
    }
  }

  //egDumpSetConsoleVideoModes();
}

VOID
egGetScreenSize (
  OUT INTN    *ScreenWidth,
  OUT INTN    *ScreenHeight
) {
  if (ScreenWidth != NULL) {
    *ScreenWidth = egScreenWidth;
  }

  if (ScreenHeight != NULL) {
    *ScreenHeight = egScreenHeight;
  }
}

CHAR16
*egScreenDescription () {
  if (egHasGraphics) {
    if (GraphicsOutput != NULL) {
      return PoolPrint(L"Graphics Output (UEFI), %dx%d", egScreenWidth, egScreenHeight);
    } else if (UgaDraw != NULL) {
      return PoolPrint(L"UGA Draw (EFI 1.10), %dx%d", egScreenWidth, egScreenHeight);
    } else {
      return L"Internal Error";
    }
  } else {
    return L"Text Console";
  }
}

BOOLEAN
egHasGraphicsMode () {
  return egHasGraphics;
}

BOOLEAN
egIsGraphicsModeEnabled () {
  EFI_CONSOLE_CONTROL_SCREEN_MODE     CurrentMode;

  if (ConsoleControl != NULL) {
    ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);
    return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
  }

  return FALSE;
}

VOID
egSetGraphicsModeEnabled (
  IN BOOLEAN    Enable
) {
  EFI_CONSOLE_CONTROL_SCREEN_MODE     CurrentMode;
  EFI_CONSOLE_CONTROL_SCREEN_MODE     NewMode;

  if (ConsoleControl != NULL) {
    // Some UEFI bioses may cause resolution switch when switching to Text Mode via the ConsoleControl->SetMode command
    // EFI applications wishing to use text, call the ConsoleControl->GetMode() command, and depending on its result may call ConsoleControl->SetMode().
    // To avoid the resolution switch, when we set text mode, we can make ConsoleControl->GetMode report that text mode is enabled.

    // ConsoleControl->SetMode should not be needed on UEFI 2.x to switch to text, but some firmwares seem to block text out if it is not given.
    // We know it blocks text out on HPQ UEFI (HP ProBook for example - reported by dmazar), Apple firmwares with UGA, and some VMs.
    // So, it may be better considering to do this only with firmware vendors where the bug was observed (currently it is known to exist on some AMI firmwares).
    //if (GraphicsOutput != NULL && StrCmp(gST->FirmwareVendor, L"American Megatrends") == 0) {
    if ((GraphicsOutput != NULL) && (StrCmp(gST->FirmwareVendor, L"HPQ") != 0) && (StrCmp(gST->FirmwareVendor, L"VMware, Inc.") != 0)) {
      if (!Enable) {
        // Don't allow switching to text mode, but report that we are in text mode when queried
        ConsoleControl->GetMode = NullConsoleControlGetModeText;
        return;
      } else if (ConsoleControl->GetMode != ConsoleControlGetMode) {
        // Allow switching to graphics mode, and use original GetMode function
        ConsoleControl->GetMode = ConsoleControlGetMode;
      }
    }

    ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);

    NewMode = Enable ? EfiConsoleControlScreenGraphics : EfiConsoleControlScreenText;

    if (CurrentMode != NewMode) {
      ConsoleControl->SetMode(ConsoleControl, NewMode);
    }
  }
}

//
// Drawing to the screen
//

VOID
egClearScreen (
  IN EG_PIXEL     *Color
) {
  EFI_UGA_PIXEL FillColor;

  if (!egHasGraphics) {
    return;
  }

  FillColor.Red       = Color->r;
  FillColor.Green     = Color->g;
  FillColor.Blue      = Color->b;
  FillColor.Reserved  = 0;

  if (GraphicsOutput != NULL) {
    // EFI_GRAPHICS_OUTPUT_BLT_PIXEL and EFI_UGA_PIXEL have the same
    // layout, and the header from TianoCore actually defines them
    // to be the same type.
    GraphicsOutput->Blt(
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)&FillColor, EfiBltVideoFill,
      0, 0, 0, 0, egScreenWidth, egScreenHeight, 0
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt(
      UgaDraw, &FillColor, EfiUgaVideoFill,
      0, 0, 0, 0, egScreenWidth, egScreenHeight, 0
    );
  }
}

VOID
egDrawImageArea (
  IN EG_IMAGE   *Image,
  IN INTN       AreaPosX,
  IN INTN       AreaPosY,
  IN INTN       AreaWidth,
  IN INTN       AreaHeight,
  IN INTN       ScreenPosX,
  IN INTN       ScreenPosY
) {
  if (!egHasGraphics || !Image) {
    return;
  }

  if (ScreenPosX < 0 || ScreenPosX >= UGAWidth || ScreenPosY < 0 || ScreenPosY >= UGAHeight) {
    // This is outside of screen area
    return;
  }

  if (AreaWidth == 0) {
    AreaWidth = Image->Width;
  }

  if (AreaHeight == 0) {
    AreaHeight = Image->Height;
  }

  if ((AreaPosX != 0) || (AreaPosY != 0)) {
    egRestrictImageArea(Image, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);

    if (AreaWidth == 0) {
      return;
    }
  }

  //if (Image->HasAlpha) { // It shouldn't harm Blt
  //  //Image->HasAlpha = FALSE;
  //  egSetPlane(PLPTR(Image, a), 255, Image->Width * Image->Height);
  //}

  if (ScreenPosX + AreaWidth > UGAWidth) {
    AreaWidth = UGAWidth - ScreenPosX;
  }

  if (ScreenPosY + AreaHeight > UGAHeight) {
    AreaHeight = UGAHeight - ScreenPosY;
  }

  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt(
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltBufferToVideo,
      (UINTN)AreaPosX, (UINTN)AreaPosY, (UINTN)ScreenPosX, (UINTN)ScreenPosY,
      (UINTN)AreaWidth, (UINTN)AreaHeight, (UINTN)Image->Width * 4
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt(
      UgaDraw, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaBltBufferToVideo,
      (UINTN)AreaPosX, (UINTN)AreaPosY, (UINTN)ScreenPosX, (UINTN)ScreenPosY,
      (UINTN)AreaWidth, (UINTN)AreaHeight, (UINTN)Image->Width * 4
    );
  }
}

// Blt(this, Buffer, mode, srcX, srcY, destX, destY, w, h, deltaSrc);
VOID
egTakeImage (
  IN EG_IMAGE     *Image,
  INTN            ScreenPosX,
  INTN            ScreenPosY,
  IN INTN         AreaWidth,
  IN INTN         AreaHeight
) {
  if (ScreenPosX + AreaWidth > UGAWidth) {
    AreaWidth = UGAWidth - ScreenPosX;
  }

  if (ScreenPosY + AreaHeight > UGAHeight) {
    AreaHeight = UGAHeight - ScreenPosY;
  }

  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt(
      GraphicsOutput,
      (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltVideoToBltBuffer,
      ScreenPosX,
      ScreenPosY,
      0, 0, AreaWidth, AreaHeight, (UINTN)Image->Width * 4
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt(
      UgaDraw,
      (EFI_UGA_PIXEL *)Image->PixelData,
      EfiUgaVideoToBltBuffer,
      ScreenPosX,
      ScreenPosY,
      0, 0, AreaWidth, AreaHeight, (UINTN)Image->Width * 4
    );
  }
}

//
// Make a screenshot
//

EFI_STATUS egScreenShot() {
  EFI_STATUS      Status = EFI_NOT_READY;
  EG_IMAGE        *Image;
  UINT8           *FileData;
  UINTN           FileDataLength, Index;
  CHAR16          ScreenshotName[128];
  EFI_UGA_PIXEL   *ImagePNG;
  UINTN           ImageSize, i;

  if (!egHasGraphics) {
    return EFI_NOT_READY;
  }

  // allocate a buffer for the whole screen
  Image = egCreateImage(egScreenWidth, egScreenHeight, FALSE);
  if (Image == NULL) {
    Print(L"Error egCreateImage returned NULL\n");
    return EFI_NO_MEDIA;
  }

  // get full screen image
  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt(
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltVideoToBltBuffer,
      0, 0, 0, 0, (UINTN)Image->Width, (UINTN)Image->Height, 0
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt(
      UgaDraw, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaVideoToBltBuffer,
      0, 0, 0, 0, (UINTN)Image->Width, (UINTN)Image->Height, 0
    );
  }

  ImagePNG = (EFI_UGA_PIXEL *)Image->PixelData;
  ImageSize = Image->Width * Image->Height;

  // Convert BGR to RGBA with Alpha set to 0xFF
  for (i = 0; i < ImageSize; i++) {
    UINT8 Temp = ImagePNG[i].Blue;
    ImagePNG[i].Blue = ImagePNG[i].Red;
    ImagePNG[i].Red = Temp;
    ImagePNG[i].Reserved = 0xFF;
  }

  // Encode raw RGB image to PNG format
  lodepng_encode32(
    &FileData,
    &FileDataLength,
    (CONST UINT8*)ImagePNG,
    (UINT32)Image->Width,
    (UINT32)Image->Height
  );

  egFreeImage(Image);

  if (FileData == NULL) {
    Print(L"Error egScreenShot: FileData returned NULL\n");
    return EFI_NO_MEDIA;
  }

  for (Index=0; Index < 60; Index++) {
    UnicodeSPrint(ScreenshotName, 256, L"%s\\screenshot%d.png", DIR_MISC, Index);

    if(!FileExists(SelfRootDir, ScreenshotName)){
      Status = egSaveFile(SelfRootDir, ScreenshotName, FileData, FileDataLength);
      if (!EFI_ERROR(Status)) {
        break;
      }
    }
  }

  // else save to file on the ESP
  if (EFI_ERROR(Status)) {
    for (Index=0; Index < 60; Index++) {
      UnicodeSPrint(ScreenshotName, 256, L"%s\\screenshot%d.png", DIR_MISC, Index);

      //if(!FileExists(NULL, ScreenshotName)){
          Status = egSaveFile(NULL, ScreenshotName, FileData, FileDataLength);
          if (!EFI_ERROR(Status)) {
            break;
          }
      //}
    }

    CheckError(Status, L"Error egSaveFile\n");
  }

  FreePool(FileData);

  return Status;
}

/* EOF */
