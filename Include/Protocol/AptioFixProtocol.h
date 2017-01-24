/** @file

Module Name:

  AptioFixProtocol.h

  AptioFix driver - Replaces EFI_SIMPLE_FILE_SYSTEM_PROTOCOL on target volume
  and injects content of specified source folder on source (injection) volume
  into target folder in target volume.

  initial version - dmazar

**/

#ifndef __AptioFixProtocol_H__
#define __AptioFixProtocol_H__

//
// Struct for holding APTIOFIX_PROTOCOL.
//
typedef struct {
  UINT64   Signature;
} APTIOFIX_PROTOCOL;

#define APTIOFIX_SIGNATURE SIGNATURE_64('A','P','T','I','O','F','I','X')

#endif