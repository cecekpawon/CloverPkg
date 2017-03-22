/*
  Original patch by Nawcom
  http://forum.voodooprojects.org/index.php/topic,1029.0.html
*/

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_GMA
#define DEBUG_GMA -1
#endif
#else
#ifdef DEBUG_GMA
#undef DEBUG_GMA
#endif
#define DEBUG_GMA DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_GMA, __VA_ARGS__)

#define S_INTELMODEL "Intel Graphics"

extern CHAR8 ClassFix[];

UINT8 RegTRUE[]                          = { 0x01, 0x00, 0x00, 0x00 };
UINT8 RegFALSE[]                         = { 0x00, 0x00, 0x00, 0x00 };
UINT8 OsInfo[]                            = {
                                              0x30, 0x49, 0x01, 0x11, 0x01, 0x10, 0x08, 0x00, 0x00,
                                              0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
                                              0xFF, 0xFF
                                            };

#if 0
UINT8 RegBacklightControl[]              = { 0x00, 0x00, 0x00, 0x00 };
//UINT8 RegHeight[]                      = { 0x20, 0x03, 0x00, 0x00 };
UINT8 RegStretch[]                       = { 0x3B, 0x00, 0x00, 0x00 };
UINT8 RegInverterFrequency[]             = { 0xc8, 0x95, 0x00, 0x00 };
UINT8 RegSubsystemId[]                   = { 0xA2, 0x00, 0x00, 0x00 };
#endif

UINT8 RegSubsystemVendorId[]             = { 0x6B, 0x10, 0x00, 0x00 };

UINT8 RegIgPlatformIdSKLMobile[]         = { 0x02, 0x00, 0x26, 0x19 };
UINT8 RegIgPlatformIdSKLDesktop[]        = { 0x02, 0x00, 0x26, 0x19 };

UINT8 RegIgPlatformIdBDWMobile[]         = { 0x06, 0x00, 0x26, 0x16 };
UINT8 RegIgPlatformIdBDWDesktop[]        = { 0x06, 0x00, 0x26, 0x16 };

UINT8 RegIgPlatformIdAzulMobile[]        = { 0x06, 0x00, 0x26, 0x0A };
UINT8 RegIgPlatformIdAzulDesktop[]       = { 0x03, 0x00, 0x22, 0x0D };

UINT8 RegIgPlatformIdCapriMobile[]       = { 0x03, 0x00, 0x66, 0x01 };
UINT8 RegIgPlatformIdCapriDesktop[]      = { 0x05, 0x00, 0x62, 0x01 };

UINT8 RegIgPlatformIdSNBMobile[]         = { 0x00, 0x00, 0x01, 0x00 };
UINT8 RegIgPlatformIdSNBDesktop[]        = { 0x10, 0x00, 0x03, 0x00 };

//UINT8 SKLVals[][4]                        = {
//                                            { 0x01, 0x00, 0x00, 0x00 }, //0 "AAPL,Gfx324"
//                                            { 0x01, 0x00, 0x00, 0x00 }, //1 "AAPL,GfxYTile"
//                                            { 0xfa, 0x00, 0x00, 0x00 }, //2 "AAPL00,PanelCycleDelay"
//                                            { 0x3c, 0x00, 0x00, 0x08 }, //3 "AAPL00,PanelPowerDown"
//                                            { 0x11, 0x00, 0x00, 0x00 }, //4 "AAPL00,PanelPowerOff"
//                                            { 0x19, 0x01, 0x00, 0x08 }, //5 "AAPL00,PanelPowerOn"
//                                            { 0x30, 0x00, 0x00, 0x00 }, //6 "AAPL00,PanelPowerUp"
//                                            { 0x0c, 0x00, 0x00, 0x00 }, //7 "graphic-options"
//                                          };

UINT8 RegGfxYTile[]                      = { 0x01, 0x00, 0x00, 0x00 };

STATIC
VOID
SetIgPlatform (
  DevPropDevice   *device,
  BOOLEAN         NeedIgPlatform,
  CHAR8           *Name,
  UINT8           *Value
) {
  if (NeedIgPlatform) {
    DevpropAddValue (device, Name, Value, 4);
    MsgLog (" - %a: 0x%02x%02x%02x%02x\n", Name, Value[3], Value[2], Value[1], Value[0]);
  }
}

BOOLEAN
SetupGmaDevprop (
  pci_dt_t    *GMADev
) {
#if 0
  UINT32          DualLink = 0;
  UINT8           BuiltIn = 0x00;
#endif

  CHAR8           *Model;
  DevPropDevice   *Device;
  INTN            i;
  UINT16          DevId = GMADev->device_id;
  BOOLEAN         Injected = FALSE, NeedIgPlatform = TRUE, NeedDualLink = TRUE;

  Model = (CHAR8 *)AllocateCopyPool (AsciiStrSize (S_INTELMODEL), S_INTELMODEL);

  MsgLog (" - %a [%04x:%04x] | %a\n",
    Model, GMADev->vendor_id, DevId, GetPciDevPath (GMADev));

  if (!gDevPropString) {
    gDevPropString = DevpropCreateString ();
  }

  Device = DevpropAddDevicePci (gDevPropString, GMADev);

  if (!Device) {
    DBG (" - Failed initializing dev-prop string dev-entry.\n");
    return FALSE;
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_INTEL) {
        continue;
      }

      Injected = TRUE;

      DevpropAddValue (
        Device,
        gSettings.AddProperties[i].Key,
        (UINT8 *)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }
  }

  if (Injected) {
    DBG (" - custom IntelGFX properties injected\n");
  }

  if (gSettings.FakeIntel) {
    UINT32    FakeID = gSettings.FakeIntel >> 16;

    DevpropAddValue (Device, "device-id", (UINT8 *)&FakeID, 4);
    FakeID = gSettings.FakeIntel & 0xFFFF;
    DevpropAddValue (Device, "vendor-id", (UINT8 *)&FakeID, 4);
  }

  if (gSettings.UseIntelHDMI) {
    DevpropAddValue (Device, "hda-gfx", (UINT8 *)"onboard-1", 10);
  }

  if (gSettings.InjectEDID && gSettings.CustomEDID) {
    DevpropAddValue (Device, "AAPL00,override-no-connect", gSettings.CustomEDID, 128);
  }

  if (gSettings.IgPlatform != 0) {
    SetIgPlatform (
      Device,
      NeedIgPlatform,
      (DevId < 0x0150)
        ? "AAPL,snb-platform-id"
        : "AAPL,ig-platform-id",
      (UINT8 *)&gSettings.IgPlatform
    );

    NeedIgPlatform = FALSE;
  }

  if (gSettings.DualLink != 0) {
    DevpropAddValue (Device, "AAPL00,DualLink", (UINT8 *)&gSettings.DualLink, 1);
    NeedDualLink = FALSE;
  }

  //MacModel = GetModelFromString (gSettings.ProductName);

  if (DevId >= 0x0102) {
    // Skylake
    if (DevId >= 0x1900) {
      SetIgPlatform (
        Device,
        NeedIgPlatform,
        "AAPL,ig-platform-id",
        gMobile
          ? RegIgPlatformIdSKLMobile
          : RegIgPlatformIdSKLDesktop
      );

      DevpropAddValue (Device, "AAPL,GfxYTile", RegGfxYTile, 4);
    }

    // Broadwell
    else if (DevId >= 0x1600) {
      SetIgPlatform (
        Device,
        NeedIgPlatform,
        "AAPL,ig-platform-id",
        gMobile
          ? RegIgPlatformIdBDWMobile
          : RegIgPlatformIdBDWDesktop
      );
    }

    // Haswell
    else if (DevId >= 0x0400) {
      SetIgPlatform (
        Device,
        NeedIgPlatform,
        "AAPL,ig-platform-id",
        gMobile
          ? RegIgPlatformIdAzulMobile
          : RegIgPlatformIdAzulDesktop
      );
    }

    // Ivy Bridge
    else if (DevId >= 0x0150) {
      SetIgPlatform (
        Device,
        NeedIgPlatform,
        "AAPL,ig-platform-id",
        gMobile
          ? RegIgPlatformIdCapriMobile
          : RegIgPlatformIdCapriDesktop
      );
    }

    // Sandy Bridge
    else {
      SetIgPlatform (
        Device,
        NeedIgPlatform,
        "AAPL,snb-platform-id",
        gMobile
          ? RegIgPlatformIdSNBMobile
          : RegIgPlatformIdSNBDesktop
      );
    }
  } else {
    DBG (" - card id=%x unsupported\n", DevId);
    return FALSE;
  }

  if (gSettings.NoDefaultProperties) {
    return TRUE;
  }

  DevpropAddValue (Device, "model", (UINT8 *)Model, (UINT32)AsciiStrLen (Model));
  DevpropAddValue (Device, "device_type", (UINT8 *)"display", 7);
  DevpropAddValue (Device, "subsystem-vendor-id", RegSubsystemVendorId, 4);
  DevpropAddValue (Device, "class-code", (UINT8 *)ClassFix, 4);

#if 0
  //DevpropAddValue (Device, "AAPL,os-info", (UINT8 *)&OsInfo, sizeof (OsInfo));
  DevpropAddValue (Device, "AAPL,HasPanel", RegTRUE, 4);
  DevpropAddValue (Device, "AAPL,SelfRefreshSupported", RegTRUE, 4);
  DevpropAddValue (Device, "AAPL,aux-power-connected", RegTRUE, 4);
  DevpropAddValue (Device, "AAPL,backlight-control", RegBacklightControl, 4);
  DevpropAddValue (Device, "AAPL00,blackscreen-preferences", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,BacklightIntensity", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,blackscreen-preferences", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,DataJustify", RegTRUE, 4);
  //DevpropAddValue (Device, "AAPL01,Depth", RegTRUE, 4);
  DevpropAddValue (Device, "AAPL01,Dither", RegTRUE, 4);
  if (NeedDualLink) {
    DevpropAddValue (Device, "AAPL01,DualLink", (UINT8 *)&DualLink, 1);
  }
  //DevpropAddValue (Device, "AAPL01,Height", RegHeight, 4);
  DevpropAddValue (Device, "AAPL01,Interlace", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,Inverter", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,InverterCurrent", RegFALSE, 4);
  //DevpropAddValue (Device, "AAPL01,InverterCurrency", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,LinkFormat", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,LinkType", RegFALSE, 4);
  DevpropAddValue (Device, "AAPL01,Pipe", RegTRUE, 4);
  //DevpropAddValue (Device, "AAPL01,PixelFormat", RegTRUE, 4);
  DevpropAddValue (Device, "AAPL01,Refresh", RegTRUE, 4);
  DevpropAddValue (Device, "AAPL01,Stretch", RegStretch, 4);
  DevpropAddValue (Device, "AAPL01,InverterFrequency", RegInverterFrequency, 4);
  DevpropAddValue (Device, "subsystem-id", RegSubsystemId, 4);
  DevpropAddValue (Device, "built-in", &BuiltIn, 1);
#endif

  return TRUE;
}
