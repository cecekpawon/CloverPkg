/*
 * refit/screen.c
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

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#define DEBUG_SCR 1
#else
#define DEBUG_SCR DEBUG_ALL
#endif

#if DEBUG_SCR == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SCR, __VA_ARGS__)
#endif

// Console defines and variables

UINTN     ConWidth, ConHeight;
CHAR16    *BlankLine = NULL;

// UGA defines and variables

INTN      UGAWidth, UGAHeight;
BOOLEAN   AllowGraphicsMode;

EG_RECT   BannerPlace = {0, 0, 0, 0};

EG_PIXEL  StdBackgroundPixel           = { 0xbf, 0xbf, 0xbf, 0xff },
          MenuBackgroundPixel          = { 0x00, 0x00, 0x00, 0x00 },
          BlueBackgroundPixel          = { 0x7f, 0x0f, 0x0f, 0xff },
          DarkBackgroundPixel          = { 0x00, 0x00, 0x00, 0xFF },
          SelectionBackgroundPixel     = { 0xef, 0xef, 0xef, 0xff }; //non-trasparent

EG_IMAGE  *BackgroundImage = NULL, *Banner = NULL, *BigBack = NULL;

static    BOOLEAN GraphicsScreenDirty, haveError = FALSE;



//
// LibScreen.c

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
egSetMode (
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
    GraphicsOutput->Blt (
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)&FillColor, EfiBltVideoFill,
      0, 0, 0, 0, egScreenWidth, egScreenHeight, 0
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt (
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
    GraphicsOutput->Blt (
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltBufferToVideo,
      (UINTN)AreaPosX, (UINTN)AreaPosY, (UINTN)ScreenPosX, (UINTN)ScreenPosY,
      (UINTN)AreaWidth, (UINTN)AreaHeight, (UINTN)Image->Width * 4
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt (
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
    GraphicsOutput->Blt (
      GraphicsOutput,
      (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltVideoToBltBuffer,
      ScreenPosX,
      ScreenPosY,
      0, 0, AreaWidth, AreaHeight, (UINTN)Image->Width * 4
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt (
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
    GraphicsOutput->Blt (
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltVideoToBltBuffer,
      0, 0, 0, 0, (UINTN)Image->Width, (UINTN)Image->Height, 0
    );
  } else if (UgaDraw != NULL) {
    UgaDraw->Blt (
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
  lodepng_encode32 (
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

/* End LibScreen.c */

//
// Keyboard input
//

BOOLEAN
ReadAllKeyStrokes () {
  BOOLEAN         GotKeyStrokes = FALSE;
  EFI_STATUS      Status;
  EFI_INPUT_KEY   key;

  for (;;) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);
    if (Status == EFI_SUCCESS) {
      GotKeyStrokes = TRUE;
      continue;
    }
    break;
  }

  return GotKeyStrokes;
}

#if 0
VOID
PauseForKey (
  CHAR16    *msg
) {
#if REFIT_DEBUG > 0
  UINTN index;
  if (msg) {
    Print(L"\n %s", msg);
  }

  Print(L"\n* Hit any key to continue *");

  if (ReadAllKeyStrokes()) {  // remove buffered key strokes
    gBS->Stall(5000000);      // 5 seconds delay
    ReadAllKeyStrokes();      // empty the buffer again
  }

  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);
  ReadAllKeyStrokes();        // empty the buffer to protect the menu

  Print(L"\n");
#endif
}

#if REFIT_DEBUG > 0
VOID
DebugPause() {
  // show console and wait for key
  SwitchToText(FALSE);
  PauseForKey(L"");

  // reset error flag
  haveError = FALSE;
}
#endif

VOID
EndlessIdleLoop () {
  UINTN index;

  for (;;) {
    ReadAllKeyStrokes();
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);
  }
}

//
// Error handling
//
/*
VOID StatusToString (OUT CHAR16 *Buffer, EFI_STATUS Status) {
  UnicodeSPrint(Buffer, 64, L"EFI Error %r", Status);
}
*/
#endif

BOOLEAN
CheckFatalError (
  IN EFI_STATUS   Status,
  IN CHAR16       *where
) {
  if (!EFI_ERROR(Status)) {
    return FALSE;
  }

  //StatusToString(ErrorName, Status);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
  Print(L"Fatal Error: %r %s\n", Status, where);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  haveError = TRUE;

  //gBS->Exit(ImageHandle, ExitStatus, ExitDataSize, ExitData);

  return TRUE;
}

BOOLEAN
CheckError (
  IN EFI_STATUS   Status,
  IN CHAR16       *where
) {
  //CHAR16 ErrorName[64];

  if (!EFI_ERROR(Status)) {
    return FALSE;
  }

  //StatusToString(ErrorName, Status);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
  Print(L"Error: %r %s\n", Status, where);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  haveError = TRUE;

  return TRUE;
}

//
// Updates console variables, according to ConOut resolution
// This should be called when initializing screen, or when resolution changes
//

static
VOID
UpdateConsoleVars() {
  UINTN   i;

  // get size of text console
  if  (gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &ConWidth, &ConHeight) != EFI_SUCCESS) {
    // use default values on error
    ConWidth = 80;
    ConHeight = 25;
  }

  // free old BlankLine when it exists
  if (BlankLine != NULL) {
    FreePool(BlankLine);
  }

  // make a buffer for a whole text line
  BlankLine = AllocatePool((ConWidth + 1) * sizeof(CHAR16));

  for (i = 0; i < ConWidth; i++){
    BlankLine[i] = ' ';
  }

  BlankLine[i] = 0;
}

//
// Screen initialization and switching
//

VOID
InitScreen (
  IN BOOLEAN    SetMaxResolution
) {
  //DbgHeader("InitScreen");
  // initialize libeg
  egInitScreen(SetMaxResolution);

  if (egHasGraphicsMode()) {
    egGetScreenSize(&UGAWidth, &UGAHeight);
    AllowGraphicsMode = TRUE;
  } else {
    AllowGraphicsMode = FALSE;
    egSetGraphicsModeEnabled(FALSE);   // just to be sure we are in text mode
  }

  GraphicsScreenDirty = TRUE;

  // disable cursor
  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  UpdateConsoleVars();

  // show the banner (even when in graphics mode)
  //DrawScreenHeader(L"Initializing...");
}

static
VOID
SwitchToText (
  IN BOOLEAN    CursorEnabled
) {
  egSetGraphicsModeEnabled(FALSE);
  gST->ConOut->EnableCursor (gST->ConOut, CursorEnabled);
}

static
VOID
SwitchToGraphics () {
  if (AllowGraphicsMode && !egIsGraphicsModeEnabled()) {
    InitScreen(FALSE);
    egSetGraphicsModeEnabled(TRUE);
    GraphicsScreenDirty = TRUE;
  }
}

VOID
SetupScreen () {
  if (GlobalConfig.TextOnly) {
    // switch to text mode if requested
    AllowGraphicsMode = FALSE;
    SwitchToText(FALSE);
  } else if (AllowGraphicsMode) {
    // clear screen and show banner
    // (now we know we'll stay in graphics mode)
    SwitchToGraphics();
    //BltClearScreen(TRUE);
  }
}

//
// Screen control for running tools
//

static
VOID
DrawScreenHeader (
  IN CHAR16   *Title
) {
  UINTN     i;

  egClearScreen(&DarkBackgroundPixel);
  // clear to black background
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  //gST->ConOut->ClearScreen (gST->ConOut);
  // paint header background
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BANNER);

  for (i = 0; i < 3; i++) {
    gST->ConOut->SetCursorPosition (gST->ConOut, 0, i);
    Print(BlankLine);
  }

  // print header text
  gST->ConOut->SetCursorPosition (gST->ConOut, 3, 1);
  Print(L"Clover (rev %s) - %s", GetRevisionString(TRUE), Title);

  // reposition cursor
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 4);
}

VOID
TerminateScreen () {
  // clear text screen
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  gST->ConOut->ClearScreen (gST->ConOut);

  // enable cursor
  gST->ConOut->EnableCursor (gST->ConOut, TRUE);
}

VOID
BeginTextScreen  (
  IN CHAR16   *Title
) {
  DrawScreenHeader(Title);
  SwitchToText(FALSE);

  // reset error flag
  haveError = FALSE;
}

VOID
FinishTextScreen (
  IN BOOLEAN    WaitAlways
) {
  if (haveError || WaitAlways) {
    SwitchToText(FALSE);
    //PauseForKey(L"FinishTextScreen");
  }

  // reset error flag
  haveError = FALSE;
}

VOID
BeginExternalScreen (
  IN BOOLEAN    UseGraphicsMode,
  IN CHAR16     *Title
) {
  if (!AllowGraphicsMode) {
    UseGraphicsMode = FALSE;
  }

  if (UseGraphicsMode) {
    SwitchToGraphics();
    //BltClearScreen(FALSE);
  }

  // show the header
  //DrawScreenHeader(Title);

  if (!UseGraphicsMode) {
    SwitchToText(TRUE);
  }

  // reset error flag
  haveError = FALSE;
}

VOID
FinishExternalScreen () {
  // make sure we clean up later
  GraphicsScreenDirty = TRUE;

  if (haveError) {
    // leave error messages on screen in case of error,
    // wait for a key press, and then switch
    //PauseForKey(L"was error, press any key\n");
    SwitchToText(FALSE);
  }

  // reset error flag
  haveError = FALSE;
}

//
// Sets next/previous available screen resolution, according to specified offset
//

VOID
SetNextScreenMode (
  INT32   Next
) {
  EFI_STATUS Status;

  Status = egSetMode(Next);
  if (!EFI_ERROR(Status)) {
    UpdateConsoleVars();
  }
}

//
// Graphics functions
//

VOID
SwitchToGraphicsAndClear () {
  SwitchToGraphics();

  if (GraphicsScreenDirty) {
    BltClearScreen(TRUE);
  }
}

static
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

static
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

static
BOOLEAN
IsImageWithinScreenLimits (
  INTN    Value,
  INTN    ImageDimension,
  INTN    ScreenDimension
) {
  return ((Value >= 0) && ((Value + ImageDimension) <= ScreenDimension));
}

static
INTN
RepositionFixedByCenter (
  INTN    Value,
  INTN    ScreenDimension,
  INTN    DesignScreenDimension
) {
  return (Value + ((ScreenDimension - DesignScreenDimension) / 2));
}

static
INTN
RepositionRelativeByGapsOnEdges (
  INTN    Value,
  INTN    ImageDimension,
  INTN    ScreenDimension,
  INTN    DesignScreenDimension
) {
  return (Value * (ScreenDimension - ImageDimension) / (DesignScreenDimension - ImageDimension));
}

static
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
    pos = ConvertEdgeAndPercentageToPixelPosition(Edge, Value, ImageDimension, ScreenDimension);
  } else {
    // Calculate the horizontal pixel to place the top left corner of the animation - by theme design resolution
    posThemeDesign = ConvertEdgeAndPercentageToPixelPosition(Edge, Value, ImageDimension, DesignScreenDimension);
    // Try repositioning by center first
    pos = RepositionFixedByCenter(posThemeDesign, ScreenDimension, DesignScreenDimension);

    // If out of edges, try repositioning by gaps on edges
    if (!IsImageWithinScreenLimits(pos, ImageDimension, ScreenDimension)) {
      pos = RepositionRelativeByGapsOnEdges(posThemeDesign, ImageDimension, ScreenDimension, DesignScreenDimension);
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
      if (IsEmbeddedTheme()) {
        Banner = BuiltinIcon(BUILTIN_ICON_BANNER);
        CopyMem(&BlueBackgroundPixel, &StdBackgroundPixel, sizeof(EG_PIXEL));
      } else  {
        Banner = egLoadImage(ThemeDir, GlobalConfig.BannerFileName, FALSE);
        if (Banner) {
          // Banner was changed, so copy into BlueBackgroundBixel first pixel of banner
          CopyMem(&BlueBackgroundPixel, &Banner->PixelData[0], sizeof(EG_PIXEL));
        } else {
          DBG("banner file not read\n");
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
        BannerPlace.XPos = CalculateNudgePosition(BannerPlace.XPos, GlobalConfig.BannerNudgeX, Banner->Width,  UGAWidth);
        BannerPlace.YPos = CalculateNudgePosition(BannerPlace.YPos, GlobalConfig.BannerNudgeY, Banner->Height, UGAHeight);
      } else {
        // Use rEFIt default (no placement values speicifed)
        BannerPlace.XPos = (UGAWidth - Banner->Width) >> 1;
        BannerPlace.YPos = (BanHeight >= Banner->Height) ? (BanHeight - Banner->Height) : 0;
      }
    }
  }

  if (
    !Banner || (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER) ||
    !IsImageWithinScreenLimits(BannerPlace.XPos, BannerPlace.Width, UGAWidth) ||
    !IsImageWithinScreenLimits(BannerPlace.YPos, BannerPlace.Height, UGAHeight)
  ) {
    // Banner is disabled or it cannot be used, apply defaults for placement
    if (Banner) {
      FreePool(Banner);
      Banner = NULL;
    }

    BannerPlace.XPos = 0;
    BannerPlace.YPos = 0;
    BannerPlace.Width = UGAWidth;
    BannerPlace.Height = BanHeight;
  }

  // Load Background and scale
  if (!BigBack && (GlobalConfig.BackgroundName != NULL)) {
    BigBack = egLoadImage(ThemeDir, GlobalConfig.BackgroundName, FALSE);
  }

  if (
    (BackgroundImage != NULL) &&
    ((BackgroundImage->Width != UGAWidth) || (BackgroundImage->Height != UGAHeight))
  ) {
    // Resolution changed
    egFreeImage(BackgroundImage);
    BackgroundImage = NULL;
  }

  if (BackgroundImage == NULL) {
    BackgroundImage = egCreateFilledImage(UGAWidth, UGAHeight, FALSE, &BlueBackgroundPixel);
  }

  if (BigBack != NULL) {
    switch (GlobalConfig.BackgroundScale) {
      case Scale:
        ScaleImage(BackgroundImage, BigBack);
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

        egRawCopy (
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
    BltImage(BackgroundImage, 0, 0); //if NULL then do nothing
  } else {
    egClearScreen(&StdBackgroundPixel);
  }

  // Draw banner
  if (Banner && ShowBanner) {
    BltImageAlpha(Banner, BannerPlace.XPos, BannerPlace.YPos, &MenuBackgroundPixel, 16);
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

  egDrawImageArea(Image, 0, 0, 0, 0, XPos, YPos);

  GraphicsScreenDirty = TRUE;
}

VOID
BltImageAlpha (
  IN EG_IMAGE   *Image,
  IN INTN       XPos,
  IN INTN       YPos,
  IN EG_PIXEL   *BackgroundPixel,
  INTN          Scale
) {
  EG_IMAGE    *CompImage, *NewImage = NULL;
  INTN        Width = Scale << 3, Height = Width;

  GraphicsScreenDirty = TRUE;

  if (Image) {
    NewImage = egCopyScaledImage(Image, Scale); //will be Scale/16
    Width = NewImage->Width;
    Height = NewImage->Height;
  }

  // compose on background
  CompImage = egCreateFilledImage(Width, Height, (BackgroundImage != NULL), BackgroundPixel);
  egComposeImage(CompImage, NewImage, 0, 0);

  if (NewImage) {
    egFreeImage(NewImage);
  }

  if (!BackgroundImage) {
    egDrawImageArea(CompImage, 0, 0, 0, 0, XPos, YPos);
    egFreeImage(CompImage);
    return;
  }

  NewImage = egCreateImage(Width, Height, FALSE);

  if (!NewImage) {
    return;
  }

  egRawCopy (
    NewImage->PixelData,
    BackgroundImage->PixelData + YPos * BackgroundImage->Width + XPos,
    Width, Height,
    Width,
    BackgroundImage->Width
  );

  egComposeImage(NewImage, CompImage, 0, 0);
  egFreeImage(CompImage);

  // blit to screen and clean up
  egDrawImageArea(NewImage, 0, 0, 0, 0, XPos, YPos);
  egFreeImage(NewImage);
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
  CompImage = egCopyImage(BaseImage);
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
  egComposeImage(CompImage, TopImage, OffsetX, OffsetY);

  // blit to screen and clean up
  //egDrawImageArea(CompImage, 0, 0, TotalWidth, TotalHeight, XPos, YPos);
  BltImageAlpha(CompImage, XPos, YPos, &MenuBackgroundPixel, 16);
  egFreeImage(CompImage);

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
  INTN          Scale
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

  NewBaseImage = egCopyScaledImage (BaseImage, Scale); //will be Scale/16
  TotalWidth = NewBaseImage->Width;
  TotalHeight = NewBaseImage->Height;
  //DBG("BaseImage: Width=%d Height=%d Alfa=%d\n", TotalWidth, TotalHeight, NewBaseImage->HasAlpha);

  NewTopImage = egCopyScaledImage (TopImage, Scale); //will be Scale/16
  CompWidth = NewTopImage->Width;
  CompHeight = NewTopImage->Height;
  //DBG("TopImage: Width=%d Height=%d Alfa=%d\n", CompWidth, CompHeight, NewTopImage->HasAlpha);

  CompImage = egCreateFilledImage (
                (CompWidth > TotalWidth) ? CompWidth : TotalWidth,
                (CompHeight > TotalHeight) ? CompHeight : TotalHeight,
                TRUE,
                &MenuBackgroundPixel
              );

  if (!CompImage) {
    DBG("Can't create CompImage\n");
    return;
  }

  //to simplify suppose square images
  if (CompWidth < TotalWidth) {
    OffsetX = (TotalWidth - CompWidth) >> 1;
    OffsetY = (TotalHeight - CompHeight) >> 1;

    egComposeImage (CompImage, NewBaseImage, 0, 0);

    if (!GlobalConfig.SelectionOnTop) {
      egComposeImage (CompImage, NewTopImage, OffsetX, OffsetY);
    }

    CompWidth = TotalWidth;
    CompHeight = TotalHeight;
  } else {
    OffsetX = (CompWidth - TotalWidth) >> 1;
    OffsetY = (CompHeight - TotalHeight) >> 1;

    egComposeImage (CompImage, NewBaseImage, OffsetX, OffsetY);

    if (!GlobalConfig.SelectionOnTop) {
      egComposeImage (CompImage, NewTopImage, 0, 0);
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

    egComposeImage (CompImage, BadgeImage, OffsetX, OffsetY);
  }

  if (GlobalConfig.SelectionOnTop) {
    if (CompWidth < TotalWidth) {
      egComposeImage (CompImage, NewTopImage, OffsetXTmp, OffsetYTmp);
    } else {
      egComposeImage (CompImage, NewTopImage, 0, 0);
    }
  }

  BltImageAlpha (CompImage, XPos, YPos, &MenuBackgroundPixel, (GlobalConfig.NonSelectedGrey && !Selected) ? -16 : 16);

  egFreeImage (CompImage);
  egFreeImage (NewBaseImage);
  egFreeImage (NewTopImage);

  GraphicsScreenDirty = TRUE;
}

//
//  ANIME
//

//#define MAX_SIZE_ANIME 256

VOID
FreeAnime (
  GUI_ANIME   *Anime
) {
  if (Anime) {
    if (Anime->Path) {
      FreePool(Anime->Path);
      Anime->Path = NULL;
    }

    FreePool(Anime);
    //Anime = NULL;
  }
}

static EG_IMAGE *AnimeImage = NULL;

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
      egFreeImage(AnimeImage);
    }

    AnimeImage = egCreateImage(Screen->Film[0]->Width, Screen->Film[0]->Height, TRUE);
  }

  // Retained for legacy themes without new anim placement options.
  x = Place->XPos + (Place->Width - AnimeImage->Width) / 2;
  y = Place->YPos + (Place->Height - AnimeImage->Height) / 2;

  if (
    !IsImageWithinScreenLimits(x, Screen->Film[0]->Width, UGAWidth) ||
    !IsImageWithinScreenLimits(y, Screen->Film[0]->Height, UGAHeight)
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
        ((UGAWidth-(x + GlobalConfig.LayoutAnimMoveForMenuX + Screen->Film[0]->Width)) <= 100)
      ) {
        x += GlobalConfig.LayoutAnimMoveForMenuX;
      }
    }
  }

  Now = AsmReadTsc();

  if (Screen->LastDraw == 0) {
    //first start, we should save background into last frame
    egFillImageArea(AnimeImage, 0, 0, AnimeImage->Width, AnimeImage->Height, &MenuBackgroundPixel);
    egTakeImage (
      Screen->Film[Screen->Frames],
      x, y,
      Screen->Film[Screen->Frames]->Width,
      Screen->Film[Screen->Frames]->Height
    );
  }

  if (TimeDiff(Screen->LastDraw, Now) < Screen->FrameTime) {
    return;
  }

  if (Screen->Film[Screen->CurrentFrame]) {
    egRawCopy (
      AnimeImage->PixelData, Screen->Film[Screen->Frames]->PixelData,
      Screen->Film[Screen->Frames]->Width,
      Screen->Film[Screen->Frames]->Height,
      AnimeImage->Width,
      Screen->Film[Screen->Frames]->Width
    );
    egComposeImage(AnimeImage, Screen->Film[Screen->CurrentFrame], 0, 0);
    BltImage(AnimeImage, x, y);
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
  CHAR16      FileName[256], *Path;
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
    IsEmbeddedTheme() ||
    !Screen->Theme ||
    (/*gThemeChanged && */StriCmp(GlobalConfig.Theme, Screen->Theme) != 0)
  ) {
    //DBG(" free screen\n");
    if (Screen->Film) {
      //free images in the film
      INTN  i;

      for (i = 0; i <= Screen->Frames; i++) { //really there are N+1 frames
        // free only last occurrence of repeated frames
        if ((Screen->Film[i] != NULL) && ((i == Screen->Frames) || (Screen->Film[i] != Screen->Film[i+1]))) {
          FreePool(Screen->Film[i]);
        }
      }

      FreePool(Screen->Film);
      Screen->Film = NULL;
    }

    if (Screen->Theme) {
      FreePool(Screen->Theme);
      Screen->Theme = NULL;
    }
  }

  // Check if we should load anime files (first run or after theme change)
  if (Anime && (Screen->Film == NULL)) {
    Path = Anime->Path;
    Screen->Film = (EG_IMAGE**)AllocateZeroPool((Anime->Frames + 1) * sizeof(VOID*));

    if (Path && Screen->Film) {
      // Look through contents of the directory
      UINTN   i;

      for (i = 0; i < Anime->Frames; i++) {
        UnicodeSPrint(FileName, 512, L"%s\\%s_%03d.png", Path, Path, i);
        //DBG("Try to load file %s\n", FileName);

        p = egLoadImage(ThemeDir, FileName, TRUE);
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
        //DBG(" found %d frames of the anime\n", i);
        // Create background frame
        Screen->Film[i] = egCreateImage(Screen->Film[0]->Width, Screen->Film[0]->Height, FALSE);
        // Copy some settings from Anime into Screen
        Screen->FrameTime = Anime->FrameTime;
        Screen->Once = Anime->Once;
        Screen->Theme = AllocateCopyPool(StrSize(GlobalConfig.Theme), GlobalConfig.Theme);
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
    Screen->FilmPlace.XPos = CalculateNudgePosition(Screen->FilmPlace.XPos, Anime->NudgeX, Screen->Film[0]->Width, UGAWidth);
    Screen->FilmPlace.YPos = CalculateNudgePosition(Screen->FilmPlace.YPos, Anime->NudgeY, Screen->Film[0]->Height, UGAHeight);

    Screen->FilmPlace.Width = Screen->Film[0]->Width;
    Screen->FilmPlace.Height = Screen->Film[0]->Height;
    //DBG("recalculated Screen->Film position\n");
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
  //DBG("anime inited\n");
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

  //DBG("Use anime=%s frames=%d\n", Anime->Path, Anime->Frames);

  return TRUE;
}


