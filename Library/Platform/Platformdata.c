/**
 platformdata.c
 **/

#include <Library/Platform/Platform.h>

/* Machine Default Data */

CHAR8   *DefaultMemEntry        = "N/A";

CHAR8   *DefaultSerial          = "CT288GT9VT6";

CHAR8   *BiosVendor             = "Apple Inc.";

UINT32  gFwFeatures             = 0XE807E136;
UINT32  gFwFeaturesMask         = 0XFF1FFF3F;

#define APPLE_SYSTEM_VERSION    "1.0"

typedef struct {
  CHAR8   *AppleFirmwareVersion, *AppleBoardID, *AppleProductName, *AppleSerialNumber, *SmcPlatform;
  UINT8   SmcRevision[6];
  UINT32  SmcConfig;
} S_MAC_MODELS;

S_MAC_MODELS   MAC_MODELS[] = {
  // Desktop

  { "MM51.88Z.0077.B31.1705011214"   , "Mac-7BA5B2794B2CDB12" , "Macmini5,3"     , "C07GA041DJD0", "NA"   , { 0x01, 0x77, 0x0F, 0, 0, 0x00 }, 0x7d005  },// sandy
  { "MM61.88Z.0106.B1D.1705011514"   , "Mac-F65AE981FFA204ED" , "Macmini6,2"     , "C07JD041DWYN", "j50s" , { 0x02, 0x13, 0x0F, 0, 0, 0x15 }, 0x7d006  },// ivy
  { "MM71.88Z.0220.B28.1705011413"   , "Mac-35C5E08120C7EEAF" , "Macmini7,1"     , "C02NN7NHG1J0", "j64"  , { 0x02, 0x24, 0x0F, 0, 0, 0x32 }, 0xf04008 },// haswell
  { "IM162.88Z.0206.B19.1705011413"  , "Mac-FFE5EF870D7BA81A" , "iMac16,2"       , "C02PNHACGG7G", "j94"  , { 0x02, 0x32, 0x0F, 0, 0, 0x20 }, 0xf00008 },// broadwell
  { "IM171.88Z.0105.B25.1704292104"  , "Mac-DB15BD556843C820" , "iMac17,1"       , "C02QFHACGG7L", "j95"  , { 0x02, 0x33, 0x0F, 0, 0, 0x10 }, 0xf0c008 },// skylake
  { "IM183.88Z.0145.B07.1705082121"  , "Mac-BE088AF8C5EB4FA2" , "iMac18,3"       , "C02V7P7AJ1GG", "NA"   , { 0x02, 0x33, 0x0F, 0, 0, 0x10 }, 0xf0c008 },// kabylake

  { "MP61.88Z.0116.B44.1705011717"   , "Mac-F60DEB81FF30ACF6" , "MacPro6,1"      , "F5KLA770F9VM", "j90"  , { 0x02, 0x20, 0x0F, 0, 0, 0x18 }, 0xf0f006 },

  // Mobile

  { "MBP81.88Z.0047.B39.1705101327"  , "Mac-942459F5819B171B" , "MacBookPro8,3"  , "W88F9CDEDF93", "k92"  , { 0x01, 0x70, 0x0F, 0, 0, 0x06 }, 0x7c005  },// sandy
  { "MBP102.88Z.0106.B1C.1705011311" , "Mac-AFD8A9D944EA4843" , "MacBookPro10,2" , "C02K2HACG4N7", "d1"   , { 0x02, 0x06, 0x0F, 0, 0, 0x59 }, 0x73007  },// ivy
  { "MBP114.88Z.0172.B23.1705011413" , "Mac-06F11F11946D27C5" , "MacBookPro11,5" , "C02LSHACG85Y", "NA"   , { 0x02, 0x30, 0x0F, 0, 0, 0x02 }, 0xf0b007 },// haswell
  { "MBP121.88Z.0167.B31.1705011413" , "Mac-E43C1C25D4880AD6" , "MacBookPro12,1" , "C02Q51OSH1DP", "j52"  , { 0x02, 0x28, 0x0F, 0, 0, 0x07 }, 0xf01008 },// broadwell
  { "MBP133.88Z.0226.B11.1702161827" , "Mac-A5C67F76ED83108C" , "MacBookPro13,3" , "C02SLHACGTFN", "NA"   , { 0x02, 0x38, 0x0F, 0, 0, 0x07 }, 0xf02009 },// skylake
  { "MBP143.88Z.0160.B00.1705090111" , "Mac-551B86E5744E2388" , "MacBookPro14,3" , "C02V7PCYHTD5", "NA"   , { 0x02, 0x38, 0x0F, 0, 0, 0x07 }, 0xf02009 } // kabylake
};

CHAR8   *AppleBoardSN       = "C02140302D5DMT31M";
CHAR8   *AppleBoardLocation = "MLB";//"Part Component";

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

STATIC
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
  AsciiStrCpyS (gSettings.VendorName,           ARRAY_SIZE (gSettings.VendorName), BiosVendor);
  AsciiStrCpyS (gSettings.RomVersion,           ARRAY_SIZE (gSettings.RomVersion), MAC_MODELS[Model].AppleFirmwareVersion);
  AsciiStrCpyS (gSettings.ReleaseDate,          ARRAY_SIZE (gSettings.ReleaseDate), GetAppleReleaseDate (MAC_MODELS[Model].AppleFirmwareVersion));
  AsciiStrCpyS (gSettings.ManufactureName,      ARRAY_SIZE (gSettings.ManufactureName), BiosVendor);

  if (Redefine) {
    AsciiStrCpyS (gSettings.ProductName,        ARRAY_SIZE (gSettings.ProductName), MAC_MODELS[Model].AppleProductName);
  }

  AsciiStrCpyS (gSettings.VersionNr,            ARRAY_SIZE (gSettings.VersionNr), APPLE_SYSTEM_VERSION);
  AsciiStrCpyS (gSettings.SerialNr,             ARRAY_SIZE (gSettings.SerialNr), MAC_MODELS[Model].AppleSerialNumber);
  AsciiStrCpyS (gSettings.FamilyName,           ARRAY_SIZE (gSettings.FamilyName), GetAppleFamilies (MAC_MODELS[Model].AppleProductName));
  AsciiStrCpyS (gSettings.BoardManufactureName, ARRAY_SIZE (gSettings.BoardManufactureName), BiosVendor);
  AsciiStrCpyS (gSettings.BoardSerialNumber,    ARRAY_SIZE (gSettings.BoardSerialNumber), AppleBoardSN);
  AsciiStrCpyS (gSettings.BoardNumber,          ARRAY_SIZE (gSettings.BoardNumber), MAC_MODELS[Model].AppleBoardID);
  AsciiStrCpyS (gSettings.BoardVersion,         ARRAY_SIZE (gSettings.BoardVersion), MAC_MODELS[Model].AppleProductName);
  AsciiStrCpyS (gSettings.LocationInChassis,    ARRAY_SIZE (gSettings.LocationInChassis), AppleBoardLocation);
  AsciiStrCpyS (gSettings.ChassisManufacturer,  ARRAY_SIZE (gSettings.ChassisManufacturer), BiosVendor);
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
    gSettings.Mobile = gMobile;
  }

  // smc helper
  if (gSettings.FakeSMCOverrides) {
    UINTN   Len = ARRAY_SIZE (gSettings.RPlt);

    if (MAC_MODELS[Model].SmcPlatform[0] != 'N') {
      AsciiStrCpyS (gSettings.RPlt, Len, MAC_MODELS[Model].SmcPlatform);
    } else {
      switch (gCPUStructure.Model) {
        case CPUID_MODEL_KABYLAKE:
        case CPUID_MODEL_KABYLAKE_DT:
        case CPUID_MODEL_SKYLAKE:
        case CPUID_MODEL_SKYLAKE_DT:
        case CPUID_MODEL_BROADWELL:
        case CPUID_MODEL_BRYSTALWELL:
          AsciiStrCpyS (gSettings.RPlt, Len, "j95");
          break;

        case CPUID_MODEL_CRYSTALWELL:
        case CPUID_MODEL_HASWELL:
        case CPUID_MODEL_HASWELL_EP:
        case CPUID_MODEL_HASWELL_ULT:
          AsciiStrCpyS (gSettings.RPlt, Len, "j44");
          break;

        case CPUID_MODEL_IVYBRIDGE_EP:
          AsciiStrCpyS (gSettings.RPlt, Len, "j90");
          break;

        case CPUID_MODEL_IVYBRIDGE:
          AsciiStrCpyS (gSettings.RPlt, Len, "j30");
          break;

        case CPUID_MODEL_SANDYBRIDGE:
        //case CPUID_MODEL_JAKETOWN:
          AsciiStrCpyS (gSettings.RPlt, Len, gSettings.Mobile ? "k90i" : "k60");
          break;

        default:
          AsciiStrCpyS (gSettings.RPlt, Len, "t9");
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
