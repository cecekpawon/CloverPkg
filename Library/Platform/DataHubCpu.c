//
// @file rEFIt_UEFI/Platform/DataHubCpu.c
//
// VirtualBox CPU descriptors
//
// VirtualBox CPU descriptors also used to set OS X-used NVRAM variables and DataHub data
//

// Copyright (C) 2009-2010 Oracle Corporation
//
// This file is part of VirtualBox Open Source Edition (OSE), as
// available from http://www.virtualbox.org. This file is free software;
// you can redistribute it and/or modify it under the terms of the GNU
// General Public License (GPL) as published by the Free Software
// Foundation, in version 2 as it comes in the "COPYING" file of the
// VirtualBox OSE distribution. VirtualBox OSE is distributed in the
// hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_DATAHUB
#define DEBUG_DATAHUB -1
#endif
#else
#ifdef DEBUG_DATAHUB
#undef DEBUG_DATAHUB
#endif
#define DEBUG_DATAHUB DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_DATAHUB, __VA_ARGS__)

#define EFI_CPU_DATA_MAXIMUM_LENGTH   0x100

EFI_SUBCLASS_TYPE1_HEADER   mCpuDataRecordHeader = {
  EFI_PROCESSOR_SUBCLASS_VERSION,         // Version
  sizeof (EFI_SUBCLASS_TYPE1_HEADER),     // Header Size
  0,                                      // Instance (initialize later)
  EFI_SUBCLASS_INSTANCE_NON_APPLICABLE,   // SubInstance
  0                                       // RecordType (initialize later)
};

// PLATFORM_DATA
// The struct passed to "LogDataHub" holing key and value to be added
typedef struct {
  EFI_SUBCLASS_TYPE1_HEADER   Hdr;      // 0x48
  UINT32                      NameLen;  // 0x58 (in bytes)
  UINT32                      ValLen;   // 0x5c
  UINT8                       Data[1];  // 0x60 Name Value
} PLATFORM_DATA;

// CopyRecord
// Copy the data provided in arguments into a PLATFORM_DATA buffer
//
// @param Rec    The buffer the data should be copied into
// @param Name   The value for the member "name"
// @param Val    The data the object should have
// @param ValLen The length of the parameter "Val"
//
// @return The size of the new PLATFORM_DATA object is returned
UINT32
EFIAPI
CopyRecord (
  IN        PLATFORM_DATA   *Rec,
  IN  CONST CHAR16          *Name,
  IN        VOID            *Val,
  IN        UINT32          ValLen
) {
  CopyMem (&Rec->Hdr, &mCpuDataRecordHeader, sizeof (EFI_SUBCLASS_TYPE1_HEADER));
  Rec->NameLen = (UINT32)StrLen (Name) * sizeof (CHAR16);
  Rec->ValLen  = ValLen;
  CopyMem (Rec->Data, Name, Rec->NameLen);
  CopyMem (Rec->Data + Rec->NameLen, Val,  ValLen);

  return (sizeof (EFI_SUBCLASS_TYPE1_HEADER) + 8 + Rec->NameLen + Rec->ValLen);
}

// LogDataHub
// Adds a key-value-pair to the DataHubProtocol
EFI_STATUS
EFIAPI
LogDataHub (
  IN  EFI_GUID    *TypeGuid,
  IN  CHAR16      *Name,
  IN  VOID        *Data,
  IN  UINT32      DataSize
) {
  EFI_STATUS              Status;
  EFI_DATA_HUB_PROTOCOL   *DataHub;
  PLATFORM_DATA           *PlatformData;
  UINT32                  RecordSize;

  //
  // Locate DataHub protocol.
  //
  Status = EfiLibLocateProtocol (&gEfiDataHubProtocolGuid, (VOID**)&DataHub);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformData = (PLATFORM_DATA *)AllocatePool (sizeof (PLATFORM_DATA) + DataSize + EFI_CPU_DATA_MAXIMUM_LENGTH);
  if (PlatformData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RecordSize = CopyRecord (PlatformData, Name, Data, DataSize);
  Status     = DataHub->LogData (
                          DataHub,
                          TypeGuid,                   // DataRecordGuid
                          &gDataHubPlatformGuid,      // ProducerName (always)
                          EFI_DATA_RECORD_CLASS_DATA,
                          PlatformData,
                          RecordSize
                        );

  FreePool (PlatformData);

  return Status;
}

// SetVariablesForOSX
// Sets the volatile and non-volatile variables used by OS X
EFI_STATUS
EFIAPI
SetVariablesForOSX () {
  //UINT8   BlackMode = 1;

  // The variable names used should be made global constants to prevent them being allocated multiple times

  //
  // firmware Variables
  //

  SetNvramVariable (
    gNvramData[kNvSystemID].VariableName,
    gNvramData[kNvSystemID].Guid,
    gNvramData[kNvSystemID].Attribute,
    sizeof (EFI_GUID),
    SwapGuid (gSettings.SystemID)
  );

  SetOrDeleteNvramVariable (
    gNvramData[kNvMLB].VariableName,
    gNvramData[kNvMLB].Guid,
    gNvramData[kNvMLB].Attribute,
    AsciiStrLen (gSettings.RtMLB),
    gSettings.RtMLB,
    (gSettings.RtMLB && (AsciiStrLen (gSettings.RtMLB) == 17))
  );

  SetOrDeleteNvramVariable (
    gNvramData[kNvHWMLB].VariableName,
    gNvramData[kNvHWMLB].Guid,
    gNvramData[kNvHWMLB].Attribute,
    AsciiStrLen (gSettings.RtMLB),
    gSettings.RtMLB,
    (gSettings.RtMLB && (AsciiStrLen (gSettings.RtMLB) == 17))
  );

  SetOrDeleteNvramVariable (
    gNvramData[kNvROM].VariableName,
    gNvramData[kNvROM].Guid,
    gNvramData[kNvROM].Attribute,
    gSettings.RtROMLen,
    gSettings.RtROM,
    (gSettings.RtROM != NULL)
  );

  SetOrDeleteNvramVariable (
    gNvramData[kNvHWROM].VariableName,
    gNvramData[kNvHWROM].Guid,
    gNvramData[kNvHWROM].Attribute,
    gSettings.RtROMLen,
    gSettings.RtROM,
    (gSettings.RtROM != NULL)
  );

  // Force store variables with default / fallback value

  SetNvramVariable (
    gNvramData[kNvFirmwareFeatures].VariableName,
    gNvramData[kNvFirmwareFeatures].Guid,
    gNvramData[kNvFirmwareFeatures].Attribute,
    sizeof (gSettings.FwFeatures),
    &gSettings.FwFeatures
  );

  SetNvramVariable (
    gNvramData[kNvFirmwareFeaturesMask].VariableName,
    gNvramData[kNvFirmwareFeaturesMask].Guid,
    gNvramData[kNvFirmwareFeaturesMask].Attribute,
    sizeof (gSettings.FwFeaturesMask),
    &gSettings.FwFeaturesMask
  );

  SetNvramVariable (
    gNvramData[kNvSBoardID].VariableName,
    gNvramData[kNvSBoardID].Guid,
    gNvramData[kNvSBoardID].Attribute,
    AsciiStrLen(gSettings.BoardNumber),
    gSettings.BoardNumber
  );

  SetNvramVariable (
    gNvramData[kNvSystemSerialNumber].VariableName,
    gNvramData[kNvSystemSerialNumber].Guid,
    gNvramData[kNvSystemSerialNumber].Attribute,
    AsciiStrLen(gSettings.SerialNr),
    gSettings.SerialNr
  );

  //SetOrDeleteNvramVariable (
  //  gNvramData[kNvBlackMode].VariableName,
  //  gNvramData[kNvBlackMode].Guid,
  //  gNvramData[kNvBlackMode].Attribute,
  //  sizeof (UINT8),
  //  &BlackMode,
  //  TRUE
  //);

  //
  // OS X non-volatile Variables
  //

  // we should have two UUID: platform and system
  // NO! Only Platform is the best solution
  SetOrDeleteNvramVariable (
    gNvramData[kNvPlatformUUID].VariableName,
    gNvramData[kNvPlatformUUID].Guid,
    gNvramData[kNvPlatformUUID].Attribute,
    sizeof (EFI_GUID),
    SwapGuid (gSettings.PlatformUUID),
    !CompareGuid (&gSettings.PlatformUUID, &gEfiPartTypeUnusedGuid)
  );

  SetOrDeleteNvramVariable (
    gNvramData[kNvBacklightLevel].VariableName,
    gNvramData[kNvBacklightLevel].Guid,
    gNvramData[kNvBacklightLevel].Attribute,
    sizeof (gSettings.BacklightLevel),
    &gSettings.BacklightLevel,
    (gSettings.Mobile && (gSettings.BacklightLevel != 0xFFFF))
  );

  // Hack for recovery by Asgorath
  //SetOrDeleteNvramVariable (
  //  gNvramData[kNvCsrActiveConfig].VariableName,
  //  gNvramData[kNvCsrActiveConfig].Guid,
  //  gNvramData[kNvCsrActiveConfig].Attribute,
  //  sizeof (gSettings.CsrActiveConfig),
  //  &gSettings.CsrActiveConfig,
  //  (gSettings.CsrActiveConfig > 0)
  //);

  //SetOrDeleteNvramVariable (
  //  gNvramData[kNvBootercfg].VariableName,
  //  gNvramData[kNvBootercfg].Guid,
  //  gNvramData[kNvBootercfg].Attribute,
  //  sizeof (gSettings.BooterConfig),
  //  &gSettings.BooterConfig,
  //  (gSettings.BooterConfig > 0)
  //);

  DeleteNvramVariable (gNvramData[kNvBootercfg].VariableName, gNvramData[kNvBootercfg].Guid);
  DeleteNvramVariable (gNvramData[kNvBootercfgOnce].VariableName, gNvramData[kNvBootercfgOnce].Guid);
  DeleteNvramVariable (gNvramData[kNvCsrActiveConfig].VariableName, gNvramData[kNvCsrActiveConfig].Guid);

  DeleteNvramVariable (gNvramData[kNvBootSwitchVar].VariableName, gNvramData[kNvBootSwitchVar].Guid);
  DeleteNvramVariable (gNvramData[kNvRecoveryBootMode].VariableName, gNvramData[kNvRecoveryBootMode].Guid); // ICloud Lock?

  return EFI_SUCCESS;
}

// SetupDataForOSX
// Sets the DataHub data used by OS X
VOID
EFIAPI
SetupDataForOSX () {
  //EFI_STATUS  Status;
  UINT32      DevPathSupportedVal = 1;

  // Locate DataHub Protocol
  //Status = gBS->LocateProtocol (&gEfiDataHubProtocolGuid, NULL, (VOID **)&gDataHub);
  //if (!EFI_ERROR (Status)) {
    LogDataHub (&gEfiMiscSubClassGuid,  L"FSBFrequency",  &gSettings.CPUStructure.FSBFrequency,  sizeof (UINT64));

    if (gSettings.CPUStructure.ARTFrequency && gSettings.UseARTFreq) {
      LogDataHub (&gEfiMiscSubClassGuid,  L"ARTFrequency",  &gSettings.CPUStructure.ARTFrequency,  sizeof (UINT64));
    }

    LogDataHub (&gEfiMiscSubClassGuid,  L"InitialTSC",            &gSettings.CPUStructure.TSCFrequency,  sizeof (UINT64));
    LogDataHub (&gEfiMiscSubClassGuid,  L"CPUFrequency",          &gSettings.CPUStructure.CPUFrequency,  sizeof (UINT64));

    LogDataHub (&gEfiMiscSubClassGuid,  L"DevicePathsSupported",  &DevPathSupportedVal,         sizeof (UINT32));
    LogDataHub (&gEfiMiscSubClassGuid,  L"Model",                 gSettings.ProductName,        ARRAY_SIZE (gSettings.ProductName));
    LogDataHub (&gEfiMiscSubClassGuid,  L"SystemSerialNumber",    gSettings.SerialNr,           ARRAY_SIZE (gSettings.SerialNr));

    if (CompareGuid (&gSettings.PlatformUUID, &gEfiPartTypeUnusedGuid)) {
      LogDataHub (
        &gEfiMiscSubClassGuid,
        L"system-id",
        CompareGuid (&gSettings.SystemID, &gEfiPartTypeUnusedGuid) ? &gSettings.OemSystemID : &gSettings.SystemID,
        sizeof (EFI_GUID)
      );
    }

    //LogDataHub (&gEfiMiscSubClassGuid, L"clovergui-revision", &Revision, sizeof (UINT32));

    // collect info about real hardware (read by FakeSMC)
    LogDataHub (&gEfiMiscSubClassGuid,  L"OEMVendor",   &gSettings.OEMVendor,   (UINT32)AsciiTrimStrLen (gSettings.OEMVendor,  ARRAY_SIZE (gSettings.OEMVendor)) + 1);
    LogDataHub (&gEfiMiscSubClassGuid,  L"OEMProduct",  &gSettings.OEMProduct,  (UINT32)AsciiTrimStrLen (gSettings.OEMProduct, ARRAY_SIZE (gSettings.OEMProduct)) + 1);
    LogDataHub (&gEfiMiscSubClassGuid,  L"OEMBoard",    &gSettings.OEMBoard,    (UINT32)AsciiTrimStrLen (gSettings.OEMBoard,   ARRAY_SIZE (gSettings.OEMBoard)) + 1);

    // SMC helper
    if (gSettings.FakeSMCOverrides) {
      LogDataHub (&gEfiMiscSubClassGuid,  L"RPlt",  &gSettings.RPlt,    8);
      LogDataHub (&gEfiMiscSubClassGuid,  L"RBr",   &gSettings.RBr,     8);
      LogDataHub (&gEfiMiscSubClassGuid,  L"EPCI",  &gSettings.EPCI,    4);
      LogDataHub (&gEfiMiscSubClassGuid,  L"REV",   &gSettings.REV,     6);
      LogDataHub (&gEfiMiscSubClassGuid,  L"BEMB",  &gSettings.Mobile,  1);
    }

    // all current settings (we could dump settings in plain text instead below)
    //LogDataHub (&gEfiMiscSubClassGuid,  L"Settings",  &gSettings, sizeof (gSettings));
  //} else {
    // this is the error message that we want user to see on the screen!
    //Print (L"DataHubProtocol is not found! Load the module DataHubDxe manually!\n");
    //DBG ("DataHubProtocol is not found! Load the module DataHubDxe manually!\n");
    //gBS->Stall (5000000);
  //}
}

VOID
EFIAPI
SaveDarwinLog () {
  DTEntry   PlatformEntry;
  VOID      *PropValue;
  UINT32    PropSize;

  if (DTLookupEntry (NULL, "/efi/platform", &PlatformEntry) == kSuccess) {
    if (DTGetProperty (PlatformEntry, DATAHUB_LOG, &PropValue, &PropSize) == kSuccess) {
      CONSTRAIN_MAX (PropSize, (UINT32)GetMemLogLen ());
      CopyMem (PropValue, GetMemLogBuffer (), PropSize);
    }
  }
}
