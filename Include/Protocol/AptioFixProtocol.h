/** @file

Module Name:

  AptioFixProtocol.h

  initial version - dmazar

**/

#ifndef __AptioFixProtocol_H__
#define __AptioFixProtocol_H__

//
// Struct for holding APTIOFIX_PROTOCOL.
//
typedef struct {
  UINT64   Signature;
  UINT8    Version;
} APTIOFIX_PROTOCOL;

#define APTIOFIX_SIGNATURE                      SIGNATURE_64('A','P','T','I','O','F','I','X')
#define APTIOFIX_RELOCBASE_VARIABLE_NAME        L"OsxAptioFixDrv-RelocBase"
#define APTIOFIX_RELOCBASE_VARIABLE_GUID        gEfiGlobalVariableGuid
#define APTIOFIX_RELOCBASE_VARIABLE_ATTRIBUTE   (/* EFI_VARIABLE_NON_VOLATILE |*/ EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)

#endif
