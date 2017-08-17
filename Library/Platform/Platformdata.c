/**
 platformdata.c
 **/

#include <Library/Platform/Platform.h>

/* Machine Default Data */

typedef struct {
  CHAR8   *AppleFirmwareVersion, *AppleBoardID, *AppleProductName, *AppleSerialNumber, *SmcPlatform;
  UINT8   SmcRevision[6];
  UINT32  SmcConfig;
} S_MAC_MODELS;

S_MAC_MODELS   MAC_MODELS[] = {
  // Desktop

  { "MM51.88Z.0078.B01.1706261437"   , "Mac-7BA5B2794B2CDB12" , "Macmini5,3"     , "C07GA041DJD0", "k92"      , { 0x01, 0x77, 0x0F, 0, 0, 0x00 }, 0x7d005  },// sandy
  { "MM61.88Z.0107.B01.1706261217"   , "Mac-F65AE981FFA204ED" , "Macmini6,2"     , "C07JD041DWYN", "j50s"     , { 0x02, 0x13, 0x0F, 0, 0, 0x15 }, 0x7d006  },// ivy
  { "MM71.88Z.0221.B02.1706261516"   , "Mac-35C5E08120C7EEAF" , "Macmini7,1"     , "C02NN7NHG1J0", "j64"      , { 0x02, 0x24, 0x0F, 0, 0, 0x32 }, 0xf04008 },// haswell
  { "IM162.88Z.0209.B02.1706261516"  , "Mac-FFE5EF870D7BA81A" , "iMac16,2"       , "C02PNHACGG7G", "j94"      , { 0x02, 0x32, 0x0F, 0, 0, 0x20 }, 0xf00008 },// broadwell
  { "IM171.88Z.0106.B01.1706260138"  , "Mac-DB15BD556843C820" , "iMac17,1"       , "C02QFHACGG7L", "j95"      , { 0x02, 0x33, 0x0F, 0, 0, 0x10 }, 0xf0c008 },// skylake
  { "IM183.88Z.0147.B01.1706251744"  , "Mac-BE088AF8C5EB4FA2" , "iMac18,3"       , "C02V7P7AJ1GG", "j133_4_5" , { 0x02, 0x33, 0x0F, 0, 0, 0x10 }, 0xf07009 },// kabylake

  { "MP61.88Z.0117.B01.1706261354"   , "Mac-F60DEB81FF30ACF6" , "MacPro6,1"      , "F5KLA770F9VM", "j90"      , { 0x02, 0x20, 0x0F, 0, 0, 0x18 }, 0xf0f006 },

  // Mobile

  { "MBP81.88Z.0049.B01.1706261922"  , "Mac-942459F5819B171B" , "MacBookPro8,3"  , "W88F9CDEDF93", "k92"      , { 0x01, 0x70, 0x0F, 0, 0, 0x06 }, 0x7c005  },// sandy
  { "MBP102.88Z.0107.B01.1706261827" , "Mac-AFD8A9D944EA4843" , "MacBookPro10,2" , "C02K2HACG4N7", "d1"       , { 0x02, 0x06, 0x0F, 0, 0, 0x59 }, 0x73007  },// ivy
  { "MBP114.88Z.0173.B01.1706261516" , "Mac-06F11F11946D27C5" , "MacBookPro11,5" , "C02LSHACG85Y", "j64"      , { 0x02, 0x30, 0x0F, 0, 0, 0x02 }, 0xf0b007 },// haswell
  { "MBP121.88Z.0168.B01.1706261516" , "Mac-E43C1C25D4880AD6" , "MacBookPro12,1" , "C02Q51OSH1DP", "j52"      , { 0x02, 0x28, 0x0F, 0, 0, 0x07 }, 0xf01008 },// broadwell
  { "MBP133.88Z.0227.B01.1706260154" , "Mac-A5C67F76ED83108C" , "MacBookPro13,3" , "C02SLHACGTFN", "2016mb"   , { 0x02, 0x38, 0x0F, 0, 0, 0x07 }, 0xf02009 },// skylake
  { "MBP143.88Z.0161.B01.1706252330" , "Mac-551B86E5744E2388" , "MacBookPro14,3" , "C02V7PCYHTD5", "2017mbp"  , { 0x02, 0x38, 0x0F, 0, 0, 0x07 }, 0xf0a009 } // kabylake
};

STATIC
CHAR8 *
GetAppleFamilies (
  IN CHAR8  *Name
) {
  CHAR8   *Res = AllocateCopyPool (AsciiStrSize (Name), Name);
  UINTN   i, Len = AsciiStrLen (Res);

  for (i = 0; i < Len; i++) {
    if (IS_DIGIT (Res[i])) {
      Res[i] = 0;
      break;
    }
  }

  return Res;
}

CHAR8 *
GetAppleReleaseDate (
  IN CHAR8  *Version
) {
  CHAR8   *Res = AllocateZeroPool (11);

  Version += AsciiStrLen (Version);

  while (*Version != '.') {
    Version--;
  }

  AsciiSPrint (Res, 11, "%c%c/%c%c/20%c%c\n", Version[3], Version[4], Version[5], Version[6], Version[1], Version[2]);

  return Res;
}

STATIC
CHAR8 *
GetAppleChassisAsset (
  IN CHAR8  *Families
) {
  CHAR8   *Aluminum = "Aluminum",
          *Pro = "Pro-Enclosure",
          *Res = AllocateZeroPool (ARRAY_SIZE (gSettings.ChassisAssetTag));
  UINTN   Len = AsciiStrLen (Families),
          LenAluminum = AsciiStrLen (Aluminum) + 2,
          LenPro = AsciiStrLen (Pro) + 1;

  if (AsciiStriStr (Families, "Pro")) {
    AsciiSPrint (Res, LenPro, Pro);
  } else {
    LenAluminum += Len;
    AsciiSPrint (Res, LenAluminum, "%a-%a", Families, Aluminum);
  }

  return Res;
}

VOID
SetDMISettingsForModel (
  MACHINE_TYPES   Model,
  BOOLEAN         Redefine
) {
  AsciiStrCpyS (gSettings.VendorName,           ARRAY_SIZE (gSettings.VendorName), DARWIN_SYSTEM_VENDOR);
  AsciiStrCpyS (gSettings.RomVersion,           ARRAY_SIZE (gSettings.RomVersion), MAC_MODELS[Model].AppleFirmwareVersion);
  AsciiStrCpyS (gSettings.ReleaseDate,          ARRAY_SIZE (gSettings.ReleaseDate), GetAppleReleaseDate (MAC_MODELS[Model].AppleFirmwareVersion));
  AsciiStrCpyS (gSettings.ManufactureName,      ARRAY_SIZE (gSettings.ManufactureName), DARWIN_SYSTEM_VENDOR);

  if (Redefine) {
    AsciiStrCpyS (gSettings.ProductName,        ARRAY_SIZE (gSettings.ProductName), MAC_MODELS[Model].AppleProductName);
  }

  AsciiStrCpyS (gSettings.VersionNr,            ARRAY_SIZE (gSettings.VersionNr), DARWIN_SYSTEM_VERSION);
  AsciiStrCpyS (gSettings.SerialNr,             ARRAY_SIZE (gSettings.SerialNr), MAC_MODELS[Model].AppleSerialNumber);
  AsciiStrCpyS (gSettings.FamilyName,           ARRAY_SIZE (gSettings.FamilyName), GetAppleFamilies (MAC_MODELS[Model].AppleProductName));
  AsciiStrCpyS (gSettings.BoardManufactureName, ARRAY_SIZE (gSettings.BoardManufactureName), DARWIN_SYSTEM_VENDOR);
  AsciiStrCpyS (gSettings.BoardSerialNumber,    ARRAY_SIZE (gSettings.BoardSerialNumber), MAC_MODELS[Model].AppleSerialNumber);
  AsciiStrCpyS (gSettings.BoardNumber,          ARRAY_SIZE (gSettings.BoardNumber), MAC_MODELS[Model].AppleBoardID);
  AsciiStrCpyS (gSettings.BoardVersion,         ARRAY_SIZE (gSettings.BoardVersion), MAC_MODELS[Model].AppleProductName);
  AsciiStrCpyS (gSettings.LocationInChassis,    ARRAY_SIZE (gSettings.LocationInChassis), DARWIN_BOARD_LOCATION);
  AsciiStrCpyS (gSettings.ChassisManufacturer,  ARRAY_SIZE (gSettings.ChassisManufacturer), DARWIN_SYSTEM_VENDOR);
  AsciiStrCpyS (gSettings.ChassisAssetTag,      ARRAY_SIZE (gSettings.ChassisAssetTag), GetAppleChassisAsset (gSettings.ProductName));

  gSettings.BoardType = BaseBoardTypeProcessorMemoryModule;

  gSettings.Mobile = FALSE;

  if (AsciiStriStr (gSettings.FamilyName, "MacBook")) {
    gSettings.ChassisType = MiscChassisTypeLapTop;
    gSettings.Mobile = TRUE;

  } else if (AsciiStriStr (gSettings.FamilyName, "iMac")) {
    gSettings.ChassisType = MiscChassisTypeAllInOne;

  } else if (AsciiStriStr (gSettings.FamilyName, "Macmini")) {
    gSettings.ChassisType = MiscChassisTypeLunchBox;

  } else if (AsciiStriStr (gSettings.FamilyName, "MacPro")) {
    gSettings.ChassisType = MiscChassisTypeUnknown; // MiscChassisTypeMiniTower

  } else {
    gSettings.ChassisType = 0;
    gSettings.CustomMobile = gSettings.Mobile;
  }

  // smc helper
  if (gSettings.FakeSMCOverrides) {
    UINTN   Len = ARRAY_SIZE (gSettings.RPlt);

    if (MAC_MODELS[Model].SmcPlatform[0] != 'N') {
      AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[Model].SmcPlatform);
    } else {
      switch (gSettings.CPUStructure.Model) {
        case CPUID_MODEL_KABYLAKE:
        case CPUID_MODEL_KABYLAKE_DT:
          AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[gSettings.Mobile ? MacBookPro143 : iMac183].SmcPlatform);
          break;

        case CPUID_MODEL_SKYLAKE:
        case CPUID_MODEL_SKYLAKE_DT:
          AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[gSettings.Mobile ? MacBookPro133 : iMac171].SmcPlatform);
          break;

        case CPUID_MODEL_BROADWELL:
        case CPUID_MODEL_BRYSTALWELL:
          AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[gSettings.Mobile ? MacBookPro121 : iMac162].SmcPlatform);
          break;

        case CPUID_MODEL_CRYSTALWELL:
        case CPUID_MODEL_HASWELL:
        case CPUID_MODEL_HASWELL_EP:
        case CPUID_MODEL_HASWELL_ULT:
          AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[gSettings.Mobile ? MacBookPro115 : MacMini71].SmcPlatform);
          break;

        case CPUID_MODEL_IVYBRIDGE_EP:
        case CPUID_MODEL_IVYBRIDGE:
          AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[gSettings.Mobile ? MacBookPro121 : MacMini62].SmcPlatform);
          break;

        case CPUID_MODEL_SANDYBRIDGE:
        //case CPUID_MODEL_JAKETOWN:
        default:
          AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[gSettings.Mobile ? MacBookPro83 : MacMini53].SmcPlatform);
          break;
      }
    }

    CopyMem (gSettings.REV, MAC_MODELS[Model].SmcRevision, 6);
    AsciiStrCpyS (gSettings.RBr, ARRAY_SIZE (gSettings.RBr), gSettings.RPlt);
    CopyMem (gSettings.EPCI, &MAC_MODELS[Model].SmcConfig, 4);
  }
}

MACHINE_TYPES
GetModelFromString (
  CHAR8   *ProductName
) {
  UINTN   i;

  for (i = MinMachineType; i < MaxMachineType; ++i) {
    if (AsciiStrCmp (MAC_MODELS[i].AppleProductName, ProductName) == 0) {
      return i;
    }
  }

  // return ending enum as "not found"
  return MaxMachineType;
}
