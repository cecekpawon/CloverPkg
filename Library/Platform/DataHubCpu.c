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

//
// CHANGELOG:
//
// 2014/11/20
// Download-Fritz
// Removed commented out code in rev 2963 and 2965
//

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

#include <Library/Platform/Platform.h>

#define EFI_CPU_DATA_MAXIMUM_LENGTH   0x100

// gDataHub
// A pointer to the DataHubProtocol
EFI_DATA_HUB_PROTOCOL     *gDataHub;

EFI_SUBCLASS_TYPE1_HEADER mCpuDataRecordHeader = {
  EFI_PROCESSOR_SUBCLASS_VERSION,         // Version
  sizeof (EFI_SUBCLASS_TYPE1_HEADER),     // Header Size
  0,                                      // Instance (initialize later)
  EFI_SUBCLASS_INSTANCE_NON_APPLICABLE,   // SubInstance
  0                                       // RecordType (initialize later)
};

typedef union {
  EFI_CPU_DATA_RECORD   *DataRecord;
  UINT8                 *Raw;
} EFI_CPU_DATA_RECORD_BUFFER;

// PLATFORM_DATA
// The struct passed to "LogDataHub" holing key and value to be added
#pragma pack (1)
typedef struct {
  EFI_SUBCLASS_TYPE1_HEADER   Hdr;      // 0x48
  UINT32                      NameLen;  // 0x58 (in bytes)
  UINT32                      ValLen;   // 0x5c
  UINT8                       Data[1];  // 0x60 Name Value
} PLATFORM_DATA;
#pragma pack ()

// CopyRecord
// Copy the data provided in arguments into a PLATFORM_DATA buffer
//
// @param Rec    The buffer the data should be copied into
// @param Name   The value for the member "name"
// @param Val    The data the object should have
// @param ValLen The length of the parameter "Val"
//
// @return The size of the new PLATFORM_DATA object is returned
UINT32 EFIAPI
CopyRecord (
  IN        PLATFORM_DATA   *Rec,
  IN  CONST CHAR16          *Name,
  IN        VOID            *Val,
  IN        UINT32          ValLen
) {
  CopyMem (&Rec->Hdr, &mCpuDataRecordHeader, sizeof (EFI_SUBCLASS_TYPE1_HEADER));
  Rec->NameLen = (UINT32)StrLen (Name) * sizeof (CHAR16);
  Rec->ValLen  = ValLen;
  CopyMem (Rec->Data,                Name, Rec->NameLen);
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
  UINT32            RecordSize;
  EFI_STATUS        Status;
  PLATFORM_DATA     *PlatformData;

  PlatformData = (PLATFORM_DATA *)AllocatePool (sizeof (PLATFORM_DATA) + DataSize + EFI_CPU_DATA_MAXIMUM_LENGTH);
  if (PlatformData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RecordSize = CopyRecord (PlatformData, Name, Data, DataSize);
  Status     = gDataHub->LogData (
                          gDataHub,
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
  // The variable names used should be made global constants to prevent them being allocated multiple times

  //
  // firmware Variables
  //

  // As found on a real Mac, the system-id variable solely has the BS flag
  SetNvramVariable (
    NvramData[kSystemID].VariableName,
    NvramData[kSystemID].Guid,
    NvramData[kSystemID].Attribute,
    sizeof (gUuid),
    &gUuid
  );

  SetOrDeleteNvramVariable (
    NvramData[kMLB].VariableName,
    NvramData[kMLB].Guid,
    NvramData[kMLB].Attribute,
    AsciiStrLen (gSettings.RtMLB),
    gSettings.RtMLB,
    (gSettings.RtMLB && (AsciiStrLen (gSettings.RtMLB) == 17))
  );

  SetOrDeleteNvramVariable (
    NvramData[kHWMLB].VariableName,
    NvramData[kHWMLB].Guid,
    NvramData[kHWMLB].Attribute,
    AsciiStrLen (gSettings.RtMLB),
    gSettings.RtMLB,
    (gSettings.RtMLB && (AsciiStrLen (gSettings.RtMLB) == 17))
  );

  SetOrDeleteNvramVariable (
    NvramData[kROM].VariableName,
    NvramData[kROM].Guid,
    NvramData[kROM].Attribute,
    gSettings.RtROMLen,
    gSettings.RtROM,
    (gSettings.RtROM != NULL)
  );

  SetOrDeleteNvramVariable (
    NvramData[kHWROM].VariableName,
    NvramData[kHWROM].Guid,
    NvramData[kHWROM].Attribute,
    gSettings.RtROMLen,
    gSettings.RtROM,
    (gSettings.RtROM != NULL)
  );

  // Force store variables with default / fallback value

  SetNvramVariable (
    NvramData[kFirmwareFeatures].VariableName,
    NvramData[kFirmwareFeatures].Guid,
    NvramData[kFirmwareFeatures].Attribute,
    sizeof (gFwFeatures),
    &gFwFeatures
  );

  SetNvramVariable (
    NvramData[kFirmwareFeaturesMask].VariableName,
    NvramData[kFirmwareFeaturesMask].Guid,
    NvramData[kFirmwareFeaturesMask].Attribute,
    sizeof (gFwFeaturesMask),
    &gFwFeaturesMask
  );

  SetNvramVariable (
    NvramData[kSBoardID].VariableName,
    NvramData[kSBoardID].Guid,
    NvramData[kSBoardID].Attribute,
    AsciiStrLen(gSettings.BoardNumber),
    gSettings.BoardNumber
  );

  SetNvramVariable (
    NvramData[kSystemSerialNumber].VariableName,
    NvramData[kSystemSerialNumber].Guid,
    NvramData[kSystemSerialNumber].Attribute,
    AsciiStrLen(gSettings.SerialNr),
    gSettings.SerialNr
  );

  //
  // OS X non-volatile Variables
  //

  // we should have two UUID: platform and system
  // NO! Only Platform is the best solution
  SetOrDeleteNvramVariable (
    NvramData[kPlatformUUID].VariableName,
    NvramData[kPlatformUUID].Guid,
    NvramData[kPlatformUUID].Attribute,
    16,
    &gUuid,
    (!gSettings.InjectSystemID && gSettings.SmUUIDConfig)
  );

  SetOrDeleteNvramVariable (
    NvramData[kBacklightLevel].VariableName,
    NvramData[kBacklightLevel].Guid,
    NvramData[kBacklightLevel].Attribute,
    sizeof (gSettings.BacklightLevel),
    &gSettings.BacklightLevel,
    (gMobile && gSettings.BacklightLevelConfig)
  );

#ifndef NO_NVRAM_SIP
  //Hack for recovery by Asgorath
  SetOrDeleteNvramVariable (
    NvramData[kCsrActiveConfig].VariableName,
    NvramData[kCsrActiveConfig].Guid,
    NvramData[kCsrActiveConfig].Attribute,
    sizeof (gSettings.CsrActiveConfig),
    &gSettings.CsrActiveConfig,
    (!gSettings.CsrActiveConfig || (gSettings.CsrActiveConfig != 0xFFFF))
  );

  SetOrDeleteNvramVariable (
    NvramData[kBootercfg].VariableName,
    NvramData[kBootercfg].Guid,
    NvramData[kBootercfg].Attribute,
    sizeof (gSettings.BooterConfig),
    &gSettings.BooterConfig,
    (!gSettings.BooterConfig || (gSettings.BooterConfig != 0xFFFF))
  );
#endif

  return EFI_SUCCESS;
}

// SetupDataForOSX
// Sets the DataHub data used by OS X
VOID
EFIAPI
SetupDataForOSX () {
  EFI_STATUS  Status;
  UINT32      DevPathSupportedVal;
  UINT64      FrontSideBus, CpuSpeed, TscFrequency, ARTFrequency;
  CHAR16      *ProductName, *SerialNumber;

  // fool proof
  FrontSideBus = gCPUStructure.FSBFrequency;
  if ((FrontSideBus < (50 * Mega)) || (FrontSideBus > (1000 * Mega))) {
    DBG ("Wrong FrontSideBus=%d, set to 100MHz\n", FrontSideBus);
    FrontSideBus = 100 * Mega;
  }

  if (gSettings.QEMU) {/*
    FrontSideBus = gCPUStructure.TSCFrequency;
    switch (gCPUStructure.Model) {
      case CPU_MODEL_DOTHAN:
      case CPU_MODEL_YONAH:
      case CPU_MODEL_MEROM:
        FrontSideBus = DivU64x32 (FrontSideBus, 4);
        break;
      default:
        break;
    }*/
    DBG ("Using QEMU FrontSideBus=%d\n", FrontSideBus);
  }

  // Save values into gSettings for the genconfig aim
  gSettings.BusSpeed = (UINT32)DivU64x32 (FrontSideBus, kilo);

  CpuSpeed = gCPUStructure.CPUFrequency;
  gSettings.CpuFreqMHz = (UINT32)DivU64x32 (CpuSpeed, Mega);

  // Locate DataHub Protocol
  Status = gBS->LocateProtocol (&gEfiDataHubProtocolGuid, NULL, (VOID **)&gDataHub);
  if (!EFI_ERROR (Status)) {
    UINTN   Len;

    Len = ARRAY_SIZE (gSettings.ProductName);
    ProductName = AllocateZeroPool (Len);
    AsciiStrToUnicodeStrS (gSettings.ProductName, ProductName, Len);

    Len = ARRAY_SIZE (gSettings.SerialNr);
    SerialNumber = AllocateZeroPool (Len);
    AsciiStrToUnicodeStrS (gSettings.SerialNr, SerialNumber, Len);

    LogDataHub (&gEfiProcessorSubClassGuid,    L"FSBFrequency",          &FrontSideBus,          sizeof (UINT64));

    if (gCPUStructure.ARTFrequency && gSettings.UseARTFreq) {
      ARTFrequency = gCPUStructure.ARTFrequency;
      LogDataHub (&gEfiProcessorSubClassGuid,  L"ARTFrequency",          &ARTFrequency,          sizeof (UINT64));
    }

    TscFrequency = gCPUStructure.TSCFrequency;
    LogDataHub (&gEfiProcessorSubClassGuid,    L"InitialTSC",            &TscFrequency,          sizeof (UINT64));
    LogDataHub (&gEfiProcessorSubClassGuid,    L"CPUFrequency",          &CpuSpeed,              sizeof (UINT64));

    DevPathSupportedVal = 1;
    LogDataHub (&gEfiMiscSubClassGuid,         L"DevicePathsSupported",  &DevPathSupportedVal,   sizeof (UINT32));
    LogDataHub (&gEfiMiscSubClassGuid,         L"Model",                 ProductName,            (UINT32)StrSize (ProductName));
    LogDataHub (&gEfiMiscSubClassGuid,         L"SystemSerialNumber",    SerialNumber,           (UINT32)StrSize (SerialNumber));

    if (gSettings.InjectSystemID) {
      LogDataHub (&gEfiMiscSubClassGuid,       L"system-id",             &gUuid,                 sizeof (EFI_GUID));
    }

    //LogDataHub (&gEfiProcessorSubClassGuid,    L"clovergui-revision",    &Revision,              sizeof (UINT32));

    // collect info about real hardware
    LogDataHub (&gEfiMiscSubClassGuid,         L"OEMVendor",             &gSettings.OEMVendor,  (UINT32)AsciiTrimStrLen (gSettings.OEMVendor,  64) + 1);
    LogDataHub (&gEfiMiscSubClassGuid,         L"OEMProduct",            &gSettings.OEMProduct, (UINT32)AsciiTrimStrLen (gSettings.OEMProduct, 64) + 1);
    LogDataHub (&gEfiMiscSubClassGuid,         L"OEMBoard",              &gSettings.OEMBoard,   (UINT32)AsciiTrimStrLen (gSettings.OEMBoard,   64) + 1);

    // SMC helper
    LogDataHub (&gEfiMiscSubClassGuid,         L"RPlt",                  &gSettings.RPlt,        8);
    LogDataHub (&gEfiMiscSubClassGuid,         L"RBr",                   &gSettings.RBr,         8);
    LogDataHub (&gEfiMiscSubClassGuid,         L"EPCI",                  &gSettings.EPCI,        4);
    LogDataHub (&gEfiMiscSubClassGuid,         L"REV",                   &gSettings.REV,         6);
    LogDataHub (&gEfiMiscSubClassGuid,         L"BEMB",                  &gSettings.Mobile,      1);

    // all current settings
    //LogDataHub (&gEfiMiscSubClassGuid,         L"Settings",              &gSettings,             sizeof (gSettings));
  } else {
    // this is the error message that we want user to see on the screen!
    //Print (L"DataHubProtocol is not found! Load the module DataHubDxe manually!\n");
    DBG ("DataHubProtocol is not found! Load the module DataHubDxe manually!\n");
    //gBS->Stall (5000000);
  }
}
