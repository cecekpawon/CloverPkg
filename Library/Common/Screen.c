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
#ifndef DEBUG_SCREEN
#define DEBUG_SCREEN -1
#endif
#else
#ifdef DEBUG_SCREEN
#undef DEBUG_SCREEN
#endif
#define DEBUG_SCREEN DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_SCREEN, __VA_ARGS__)

// Console defines and variables

UINTN     ConWidth, ConHeight;
CHAR16    *BlankLine = NULL;

// UGA defines and variables

INTN      UGAWidth, UGAHeight;
BOOLEAN   AllowGraphicsMode;
BOOLEAN   GraphicsScreenDirty;

EG_RECT   BannerPlace = {0, 0, 0, 0};

EG_PIXEL  StdBackgroundPixel           = { 0xbf, 0xbf, 0xbf, 0xff },
          MenuBackgroundPixel          = { 0x00, 0x00, 0x00, 0x00 },
          BlueBackgroundPixel          = { 0x7f, 0x0f, 0x0f, 0xff },
          DarkBackgroundPixel          = { 0x00, 0x00, 0x00, 0xFF },
          SelectionBackgroundPixel     = { 0xef, 0xef, 0xef, 0xff }; //non-trasparent

EG_IMAGE  *BackgroundImage = NULL, *Banner = NULL, *BigBack = NULL;

STATIC    BOOLEAN haveError = FALSE;


//
// LibScreen.c
//

#include <Protocol/GraphicsOutput.h>

// Console defines and variables

STATIC EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
STATIC EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;

STATIC EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput = NULL;

STATIC BOOLEAN egHasGraphics = FALSE;
STATIC UINTN egScreenWidth  = 0; //1024;
STATIC UINTN egScreenHeight = 0; //768;

STATIC EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE ConsoleControlGetMode = NULL;

//STATIC EFI_STATUS GopSetModeAndReconnectTextOut ();

//
// Null ConsoleControl GetMode () implementation - for blocking resolution switch when using text mode
//
EFI_STATUS
EFIAPI
NullConsoleControlGetModeText (
  IN  struct _EFI_CONSOLE_CONTROL_PROTOCOL  *This,
  OUT EFI_CONSOLE_CONTROL_SCREEN_MODE       *Mode,
  OUT BOOLEAN                               *GopUgaExists, OPTIONAL
  OUT BOOLEAN                               *StdInLocked OPTIONAL
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

  Status = GraphicsOutput->SetMode (GraphicsOutput, ModeNumber);
  DBG ("Video mode change to mode #%d: %r\n", ModeNumber, Status);

  return Status;
}

EFI_STATUS
SetMaxResolution () {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  EFI_STATUS                              Status = EFI_UNSUPPORTED;
  UINT32                                  Width = 0, Height = 0, BestMode = 0,
                                          MaxMode, Mode;
  UINTN                                   SizeOfInfo;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  MaxMode = GraphicsOutput->Mode->MaxMode;
  for (Mode = 0; Mode < MaxMode; Mode++) {
    Status = GraphicsOutput->QueryMode (GraphicsOutput, Mode, &SizeOfInfo, &Info);
    if (Status == EFI_SUCCESS) {
      if ((Width > Info->HorizontalResolution) || (Height > Info->VerticalResolution)) {
        continue;
      }
      Width = Info->HorizontalResolution;
      Height = Info->VerticalResolution;
      BestMode = Mode;
    }
  }

  MsgLog ("SetMaxResolution: found best mode #%d (%dx%d)\n", BestMode, Width, Height);

  // check if requested mode is equal to current mode
  if (BestMode == GraphicsOutput->Mode->Mode) {
    //MsgLog (" - already set\n");
    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    Status = EFI_SUCCESS;
  } else {
    //Status = GraphicsOutput->SetMode (GraphicsOutput, BestMode);
    Status = GopSetModeAndReconnectTextOut (BestMode);
    if (Status == EFI_SUCCESS) {
      egScreenWidth = Width;
      egScreenHeight = Height;
      //MsgLog (" - set\n", Status);
    } else {
      // we can not set BestMode - search for first one that we can
      //MsgLog (" - %r\n", Status);
      Status = InternalSetMode (1);
    }
  }

  return Status;
}

EFI_STATUS
InternalSetMode (
  INT32     Next
) {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  EFI_STATUS                              Status = EFI_UNSUPPORTED;
  UINT32                                  MaxMode, Index = 0;
  UINTN                                   SizeOfInfo;
  INT32                                   Mode;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  MaxMode = GraphicsOutput->Mode->MaxMode;
  Mode = GraphicsOutput->Mode->Mode;
  while (EFI_ERROR (Status) && (Index <= MaxMode)) {
    Mode = Mode + Next;
    Mode = (Mode >= (INT32)MaxMode) ? 0 : Mode;
    Mode = (Mode < 0) ? ((INT32)MaxMode - 1) : Mode;
    Status = GraphicsOutput->QueryMode (GraphicsOutput, (UINT32)Mode, &SizeOfInfo, &Info);

    DBG ("QueryMode %d Status=%r\n", Mode, Status);

    if (Status == EFI_SUCCESS) {
      //Status = GraphicsOutput->SetMode (GraphicsOutput, (UINT32)Mode);
      Status = GopSetModeAndReconnectTextOut ((UINT32)Mode);
      //DBG ("SetMode %d Status=%r\n", Mode, Status);
      egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
      egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    }

    Index++;
  }

  return Status;
}

EFI_STATUS
SetScreenResolution (
  IN CHAR16   *WidthHeight
) {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  EFI_STATUS                              Status = EFI_UNSUPPORTED;
  UINT32                                  Width, Height, MaxMode, Mode;
  CHAR16                                  *HeightP;
  UINTN                                   SizeOfInfo;

  if (GraphicsOutput == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (WidthHeight == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DBG ("SetScreenResolution: %s", WidthHeight);

  // we are expecting WidthHeight=L"1024x768"
  // parse Width and Height
  HeightP = WidthHeight;

  while ((*HeightP != L'\0') && (*HeightP != L'x') && (*HeightP != L'X')) {
    HeightP++;
  }

  if (*HeightP == L'\0') {
    return EFI_INVALID_PARAMETER;
  }

  HeightP++;
  Width = (UINT32)StrDecimalToUintn (WidthHeight);
  Height = (UINT32)StrDecimalToUintn (HeightP);

  // check if requested mode is equal to current mode
  if ((GraphicsOutput->Mode->Info->HorizontalResolution == Width) && (GraphicsOutput->Mode->Info->VerticalResolution == Height)) {
    DBG (" - already set\n");
    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;

    return EFI_SUCCESS;
  }

  // iterate through modes and set it if found
  MaxMode = GraphicsOutput->Mode->MaxMode;
  for (Mode = 0; Mode < MaxMode; Mode++) {
    Status = GraphicsOutput->QueryMode (GraphicsOutput, Mode, &SizeOfInfo, &Info);
    if (Status == EFI_SUCCESS) {
      if ((Width == Info->HorizontalResolution) && (Height == Info->VerticalResolution)) {
        DBG (" - setting Mode %d\n", Mode);

        //Status = GraphicsOutput->SetMode (GraphicsOutput, Mode);
        Status = GopSetModeAndReconnectTextOut (Mode);
        if (Status == EFI_SUCCESS) {
          egScreenWidth = Width;
          egScreenHeight = Height;

          return EFI_SUCCESS;
        }
      }
    }
  }

  DBG (" - not found!\n");

  return EFI_UNSUPPORTED;
}

VOID
InternalInitScreen (
  IN BOOLEAN    egSetMaxResolution
) {
  EFI_STATUS    Status;
  CHAR16        *Resolution;

  // get protocols
  Status = EfiLibLocateProtocol (&ConsoleControlProtocolGuid, (VOID **)&ConsoleControl);

  if (EFI_ERROR (Status)) {
    ConsoleControl = NULL;
  }

  Status = EfiLibLocateProtocol (&GraphicsOutputProtocolGuid, (VOID **)&GraphicsOutput);

  if (EFI_ERROR (Status)) {
    GraphicsOutput = NULL;
  }

  // store original GetMode
  if ((ConsoleControl != NULL) && (ConsoleControlGetMode == NULL)) {
    ConsoleControlGetMode = ConsoleControl->GetMode;
  }

  // if it not the first run, just restore resolution
  if ((egScreenWidth != 0) && (egScreenHeight != 0)) {
    Resolution = PoolPrint (L"%dx%d",egScreenWidth,egScreenHeight);
    if (Resolution) {
      Status = SetScreenResolution (Resolution);
      FreePool (Resolution);

      if (!EFI_ERROR (Status)) {
        return;
      }
    }
  }

  // get screen size
  egHasGraphics = FALSE;

  if (GraphicsOutput != NULL) {
    if (GlobalConfig.ScreenResolution != NULL) {
      if (EFI_ERROR (SetScreenResolution (GlobalConfig.ScreenResolution))) {
        if (egSetMaxResolution) {
          SetMaxResolution ();
        }
      }
    } else {
      if (egSetMaxResolution) {
        SetMaxResolution ();
      }
    }

    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    egHasGraphics = TRUE;
  }

  //egDumpSetConsoleVideoModes ();
}

VOID
GetScreenSize (
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

CHAR16 *
ScreenDescription () {
  if (egHasGraphics) {
    if (GraphicsOutput != NULL) {
      return PoolPrint (L"Graphics Output (UEFI), %dx%d", egScreenWidth, egScreenHeight);
    } else {
      return L"Internal Error";
    }
  } else {
    return L"Text Console";
  }
}

BOOLEAN
HasGraphicsMode () {
  return egHasGraphics;
}

BOOLEAN
IsGraphicsModeEnabled () {
  EFI_CONSOLE_CONTROL_SCREEN_MODE     CurrentMode;

  if (ConsoleControl != NULL) {
    ConsoleControl->GetMode (ConsoleControl, &CurrentMode, NULL, NULL);
    return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
  }

  return FALSE;
}

VOID
SetGraphicsModeEnabled (
  IN BOOLEAN    Enable
) {
  EFI_CONSOLE_CONTROL_SCREEN_MODE     CurrentMode;
  EFI_CONSOLE_CONTROL_SCREEN_MODE     NewMode;

  if (ConsoleControl != NULL) {
    // Some UEFI bioses may cause resolution switch when switching to Text Mode via the ConsoleControl->SetMode command
    // EFI applications wishing to use text, call the ConsoleControl->GetMode () command, and depending on its result may call ConsoleControl->SetMode ().
    // To avoid the resolution switch, when we set text mode, we can make ConsoleControl->GetMode report that text mode is enabled.

    // ConsoleControl->SetMode should not be needed on UEFI 2.x to switch to text, but some firmwares seem to block text out if it is not given.
    // We know it blocks text out on HPQ UEFI (HP ProBook for example - reported by dmazar), Apple firmwares with UGA, and some VMs.
    // So, it may be better considering to do this only with firmware vendors where the bug was observed (currently it is known to exist on some AMI firmwares).
    //if (GraphicsOutput != NULL && StrCmp (gST->FirmwareVendor, L"American Megatrends") == 0) {
    if (
      (GraphicsOutput != NULL) &&
      (StrCmp (gST->FirmwareVendor, L"HPQ") != 0) &&
      (StrCmp (gST->FirmwareVendor, L"VMware, Inc.") != 0)
    ) {
      if (!Enable) {
        // Don't allow switching to text mode, but report that we are in text mode when queried
        ConsoleControl->GetMode = NullConsoleControlGetModeText;
        return;
      } else if (ConsoleControl->GetMode != ConsoleControlGetMode) {
        // Allow switching to graphics mode, and use original GetMode function
        ConsoleControl->GetMode = ConsoleControlGetMode;
      }
    }

    ConsoleControl->GetMode (ConsoleControl, &CurrentMode, NULL, NULL);

    NewMode = Enable ? EfiConsoleControlScreenGraphics : EfiConsoleControlScreenText;

    if (CurrentMode != NewMode) {
      ConsoleControl->SetMode (ConsoleControl, NewMode);
    }
  }
}

//
// Drawing to the screen
//

VOID
ClearScreen (
  IN EG_PIXEL   *Color
) {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL FillColor;

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
  }
}

VOID
DrawImageArea (
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

  if ((ScreenPosX < 0) || (ScreenPosX >= UGAWidth) || (ScreenPosY < 0) || (ScreenPosY >= UGAHeight)) {
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
    RestrictImageArea (Image, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);

    if (AreaWidth == 0) {
      return;
    }
  }

  //if (Image->HasAlpha) { // It shouldn't harm Blt
  //  //Image->HasAlpha = FALSE;
  //  SetPlane (PLPTR (Image, a), 255, Image->Width * Image->Height);
  //}

  if ((ScreenPosX + AreaWidth) > UGAWidth) {
    AreaWidth = UGAWidth - ScreenPosX;
  }

  if ((ScreenPosY + AreaHeight) > UGAHeight) {
    AreaHeight = UGAHeight - ScreenPosY;
  }

  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt (
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltBufferToVideo,
      (UINTN)AreaPosX, (UINTN)AreaPosY, (UINTN)ScreenPosX, (UINTN)ScreenPosY,
      (UINTN)AreaWidth, (UINTN)AreaHeight, (UINTN)Image->Width * 4
    );
  }
}

// Blt (this, Buffer, mode, srcX, srcY, destX, destY, w, h, deltaSrc);
VOID
TakeImage (
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
  }
}

//
// Make a screenshot
//

EFI_STATUS
ScreenShot () {
  EFI_STATUS                      Status = EFI_NOT_READY;
  EG_IMAGE                        *Image;
  UINT8                           *FileData;
  UINTN                           FileDataLength, Index;
  CHAR16                          ScreenshotName[AVALUE_MAX_SIZE];
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *ImagePNG;
  UINTN                           ImageSize, i;

  if (!egHasGraphics) {
    return EFI_NOT_READY;
  }

  // allocate a buffer for the whole screen
  Image = CreateImage (egScreenWidth, egScreenHeight, FALSE);
  if (Image == NULL) {
    Print (L"Error CreateImage returned NULL\n");
    return EFI_NO_MEDIA;
  }

  // get full screen image
  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt (
      GraphicsOutput, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
      EfiBltVideoToBltBuffer,
      0, 0, 0, 0, (UINTN)Image->Width, (UINTN)Image->Height, 0
    );
  }

  ImagePNG = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData;
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
    (CONST UINT8 *)ImagePNG,
    (UINT32)Image->Width,
    (UINT32)Image->Height
  );

  FreeImage (Image);

  if (FileData == NULL) {
    Print (L"Error ScreenShot: FileData returned NULL\n");
    return EFI_NO_MEDIA;
  }

  for (Index=0; Index < 60; Index++) {
    UnicodeSPrint (ScreenshotName, ARRAY_SIZE (ScreenshotName), L"%s\\screenshot%d.png", DIR_MISC, Index);

    if (!FileExists (SelfRootDir, ScreenshotName)){
      Status = SaveFile (SelfRootDir, ScreenshotName, FileData, FileDataLength);
      if (!EFI_ERROR (Status)) {
        break;
      }
    }
  }

  // else save to file on the ESP
  if (EFI_ERROR (Status)) {
    for (Index=0; Index < 60; Index++) {
      UnicodeSPrint (ScreenshotName, ARRAY_SIZE (ScreenshotName), L"%s\\screenshot%d.png", DIR_MISC, Index);

      //if (!FileExists (NULL, ScreenshotName)){
          Status = SaveFile (NULL, ScreenshotName, FileData, FileDataLength);
          if (!EFI_ERROR (Status)) {
            break;
          }
      //}
    }

    CheckError (Status, L"Error SaveFile\n");
  }

  FreePool (FileData);

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
    Print (L"\n %s", msg);
  }

  Print (L"\n* Hit any key to continue *");

  if (ReadAllKeyStrokes ()) {  // remove buffered key strokes
    gBS->Stall (5 * 1000000);      // 5 seconds delay
    ReadAllKeyStrokes ();      // empty the buffer again
  }

  gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &index);
  ReadAllKeyStrokes ();        // empty the buffer to protect the menu

  Print (L"\n");
#endif
}

#if REFIT_DEBUG > 0
VOID
DebugPause () {
  // show console and wait for key
  SwitchToText (FALSE);
  PauseForKey (L"");

  // reset error flag
  haveError = FALSE;
}
#endif

VOID
EndlessIdleLoop () {
  UINTN index;

  for (;;) {
    ReadAllKeyStrokes ();
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &index);
  }
}

//
// Error handling
//
/*
VOID StatusToString (OUT CHAR16 *Buffer, EFI_STATUS Status) {
  UnicodeSPrint (Buffer, 64, L"EFI Error %r", Status);
}
*/
#endif

BOOLEAN
CheckFatalError (
  IN EFI_STATUS   Status,
  IN CHAR16       *where
) {
  if (!EFI_ERROR (Status)) {
    return FALSE;
  }

  //StatusToString (ErrorName, Status);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
  Print (L"Fatal Error: %r %s\n", Status, where);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  haveError = TRUE;

  //gBS->Exit (ImageHandle, ExitStatus, ExitDataSize, ExitData);

  return TRUE;
}

BOOLEAN
CheckError (
  IN EFI_STATUS   Status,
  IN CHAR16       *where
) {
  //CHAR16 ErrorName[64];

  if (!EFI_ERROR (Status)) {
    return FALSE;
  }

  //StatusToString (ErrorName, Status);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
  Print (L"Error: %r %s\n", Status, where);
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  haveError = TRUE;

  return TRUE;
}

//
// Updates console variables, according to ConOut resolution
// This should be called when initializing screen, or when resolution changes
//

STATIC
VOID
UpdateConsoleVars () {
  UINTN   i;

  // get size of text console
  if  (gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &ConWidth, &ConHeight) != EFI_SUCCESS) {
    // use default values on error
    ConWidth = 80;
    ConHeight = 25;
  }

  // free old BlankLine when it exists
  if (BlankLine != NULL) {
    FreePool (BlankLine);
  }

  // make a buffer for a whole text line
  BlankLine = AllocatePool ((ConWidth + 1) * sizeof (CHAR16));

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
  IN BOOLEAN    egSetMaxResolution
) {
  //DbgHeader ("InitScreen");
  // initialize libeg
  InternalInitScreen (egSetMaxResolution);

  if (HasGraphicsMode ()) {
    GetScreenSize (&UGAWidth, &UGAHeight);
    AllowGraphicsMode = TRUE;
  } else {
    AllowGraphicsMode = FALSE;
    SetGraphicsModeEnabled (FALSE);   // just to be sure we are in text mode
  }

  GraphicsScreenDirty = TRUE;

  // disable cursor
  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  UpdateConsoleVars ();

  // show the banner (even when in graphics mode)
  //DrawScreenHeader (L"Initializing...");
}

STATIC
VOID
SwitchToText (
  IN BOOLEAN    CursorEnabled
) {
  SetGraphicsModeEnabled (FALSE);
  gST->ConOut->EnableCursor (gST->ConOut, CursorEnabled);
}

STATIC
VOID
SwitchToGraphics () {
  if (AllowGraphicsMode && !IsGraphicsModeEnabled ()) {
    InitScreen (FALSE);
    SetGraphicsModeEnabled (TRUE);
    GraphicsScreenDirty = TRUE;
  }
}

VOID
SetupScreen () {
  if (GlobalConfig.TextOnly) {
    // switch to text mode if requested
    AllowGraphicsMode = FALSE;
    SwitchToText (FALSE);
  } else if (AllowGraphicsMode) {
    // clear screen and show banner
    // (now we know we'll stay in graphics mode)
    SwitchToGraphics ();
    //BltClearScreen (TRUE);
  }
}

//
// Screen control for running tools
//

STATIC
VOID
DrawScreenHeader (
  IN CHAR16   *Title
) {
  UINTN     i;

  ClearScreen (&DarkBackgroundPixel);
  // clear to black background
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  //gST->ConOut->ClearScreen (gST->ConOut);
  // paint header background
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BANNER);

  for (i = 0; i < 3; i++) {
    gST->ConOut->SetCursorPosition (gST->ConOut, 0, i);
    Print (BlankLine);
  }

  // print header text
  gST->ConOut->SetCursorPosition (gST->ConOut, 3, 1);
  Print (L"%a - %s", CLOVER_REVISION_STR /*GetRevisionString (TRUE)*/, Title);

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
  DrawScreenHeader (Title);
  SwitchToText (FALSE);

  // reset error flag
  haveError = FALSE;
}

VOID
FinishTextScreen (
  IN BOOLEAN    WaitAlways
) {
  if (haveError || WaitAlways) {
    SwitchToText (FALSE);
    //PauseForKey (L"FinishTextScreen");
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
    SwitchToGraphics ();
    //BltClearScreen (FALSE);
  }

  // show the header
  //DrawScreenHeader (Title);

  if (!UseGraphicsMode) {
    SwitchToText (TRUE);
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
    //PauseForKey (L"was error, press any key\n");
    SwitchToText (FALSE);
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

  Status = InternalSetMode (Next);
  if (!EFI_ERROR (Status)) {
    UpdateConsoleVars ();
  }
}

//
// Graphics functions
//

VOID
SwitchToGraphicsAndClear () {
  SwitchToGraphics ();

  if (GraphicsScreenDirty) {
    BltClearScreen (TRUE);
  }
}
