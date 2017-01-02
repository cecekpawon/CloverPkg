/*++

Copyright (c) 2005, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  IO.h

Abstract:

  Information about the IO library function

Revision History

--*/

#ifndef _SHELL_I_O_H
#define _SHELL_I_O_H

//#include <Library/GenericBdsLib.h>

#define EFI_TPL_APPLICATION   4
#define EFI_TPL_CALLBACK      8
#define EFI_TPL_NOTIFY        16
#define EFI_TPL_HIGH_LEVEL    31

UINTN
PrintAt (
  IN UINTN      Column,
  IN UINTN      Row,
  IN CHAR16     *fmt,
  ...
);

CHAR16
*PoolPrint (
  IN CHAR16     *fmt,
  ...
);

VOID
IInput (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    * ConOut,
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL     * ConIn,
  IN CHAR16                             *Prompt OPTIONAL,
  OUT CHAR16                            *InStr,
  IN UINTN                              StrLength
);

EFI_STATUS
WaitForSingleEvent (
  IN EFI_EVENT    Event,
  IN UINT64       Timeout OPTIONAL
);

#endif
