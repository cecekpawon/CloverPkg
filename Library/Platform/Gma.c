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

#define DBG(...) DebugLog(DEBUG_GMA, __VA_ARGS__)

#define S_INTELMODEL "INTEL Graphics"

//Slice - corrected all values, still not sure
UINT8 INTELDEF_vals[28][4] = {
  { 0x01, 0x00, 0x00, 0x00 }, //0 "AAPL,HasPanel"
  { 0x01, 0x00, 0x00, 0x00 }, //1 "AAPL,SelfRefreshSupported"
  { 0x01, 0x00, 0x00, 0x00 }, //2 "AAPL,aux-power-connected"
  { 0x01, 0x00, 0x00, 0x08 }, //3 "AAPL,backlight-control"
  { 0x00, 0x00, 0x00, 0x00 }, //4 "AAPL00,blackscreen-preferences"
  { 0x56, 0x00, 0x00, 0x08 }, //5 "AAPL01,BacklightIntensity"
  { 0x00, 0x00, 0x00, 0x00 }, //6 "AAPL01,blackscreen-preferences"
  { 0x01, 0x00, 0x00, 0x00 }, //7 "AAPL01,DataJustify"
  { 0x20, 0x00, 0x00, 0x00 }, //8 "AAPL01,Depth"
  { 0x01, 0x00, 0x00, 0x00 }, //9 "AAPL01,Dither"
  { 0x20, 0x03, 0x00, 0x00 }, //10 "AAPL01,Height"
  { 0x00, 0x00, 0x00, 0x00 }, //11 "AAPL01,Interlace"
  { 0x00, 0x00, 0x00, 0x00 }, //12 "AAPL01,Inverter"
  { 0x08, 0x52, 0x00, 0x00 }, //13 "AAPL01,InverterCurrent"
  { 0x00, 0x00, 0x00, 0x00 }, //14 "AAPL01,LinkFormat"
  { 0x00, 0x00, 0x00, 0x00 }, //15 "AAPL01,LinkType"
  { 0x01, 0x00, 0x00, 0x00 }, //16 "AAPL01,Pipe"
  { 0x01, 0x00, 0x00, 0x00 }, //17 "AAPL01,PixelFormat"
  { 0x01, 0x00, 0x00, 0x00 }, //18 "AAPL01,Refresh"
  { 0x3B, 0x00, 0x00, 0x00 }, //19 "AAPL01,Stretch"
  { 0xc8, 0x95, 0x00, 0x00 }, //20 "AAPL01,InverterFrequency"
  { 0x6B, 0x10, 0x00, 0x00 }, //21 "subsystem-vendor-id"
  { 0xA2, 0x00, 0x00, 0x00 }, //22 "subsystem-id"
  { 0x05, 0x00, 0x62, 0x01 }, //23 "AAPL,ig-platform-id" HD4000 //STLVNUB
  { 0x06, 0x00, 0x62, 0x01 }, //24 "AAPL,ig-platform-id" HD4000 iMac
  { 0x09, 0x00, 0x66, 0x01 }, //25 "AAPL,ig-platform-id" HD4000
  { 0x03, 0x00, 0x22, 0x0d }, //26 "AAPL,ig-platform-id" HD4600 //bcc9 http://www.insanelymac.com/forum/topic/290783-intel-hd-graphics-4600-haswell-working-displayport/
  { 0x00, 0x00, 0x62, 0x01 }  //27 - automatic solution
};
// 5 - 32Mb 6 - 48Mb 9 - 64Mb, 0 - 96Mb

extern CHAR8 ClassFix[];

UINT8 reg_TRUE[]  = { 0x01, 0x00, 0x00, 0x00 };
UINT8 reg_FALSE[] = { 0x00, 0x00, 0x00, 0x00 };
UINT8 OsInfo[] = {
  0x30, 0x49, 0x01, 0x11, 0x01, 0x10, 0x08, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
  0xFF, 0xFF
};

BOOLEAN
setup_gma_devprop (
  pci_dt_t    *gma_dev
) {
  CHAR8           *devicepath, *model;
  DevPropDevice   *device;
  UINT32          DualLink = 0; //local variable must be inited
  UINT8           BuiltIn =   0x00;
  INTN            i;
  BOOLEAN         Injected = FALSE, SetIgPlatform = FALSE;

  devicepath = get_pci_dev_path(gma_dev);

  model = (CHAR8*)AllocateCopyPool(AsciiStrSize(S_INTELMODEL), S_INTELMODEL);

  MsgLog(" - %a [%04x:%04x] | %a\n",
    model, gma_dev->vendor_id, gma_dev->device_id, devicepath);

  if (!string) {
    string = devprop_create_string();
  }

  device = devprop_add_device_pci(string, gma_dev);

  if (!device) {
    DBG(" - Failed initializing dev-prop string dev-entry.\n");
    return FALSE;
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_INTEL) {
        continue;
      }

      Injected = TRUE;

      devprop_add_value (
        device,
        gSettings.AddProperties[i].Key,
        (UINT8*)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }
  }

  if (Injected) {
    DBG(" - custom IntelGFX properties injected\n");
  }

  if (gSettings.FakeIntel) {
    UINT32    FakeID = gSettings.FakeIntel >> 16;

    devprop_add_value(device, "device-id", (UINT8*)&FakeID, 4);
    FakeID = gSettings.FakeIntel & 0xFFFF;
    devprop_add_value(device, "vendor-id", (UINT8*)&FakeID, 4);
  }

  if (gSettings.UseIntelHDMI) {
    devprop_add_value(device, "hda-gfx", (UINT8*)"onboard-1", 10);
  }

  if (gSettings.InjectEDID && gSettings.CustomEDID) {
    devprop_add_value(device, "AAPL00,override-no-connect", gSettings.CustomEDID, 128);
  }

  if (gSettings.IgPlatform != 0) {
    devprop_add_value (
      device,
      (gma_dev->device_id < 0x130) ? "AAPL,snb-platform-id" : "AAPL,ig-platform-id",
      (UINT8*)&gSettings.IgPlatform,
      4
    );

    MsgLog(" - %a-platform-id: 0x%08lx\n", (gma_dev->device_id < 0x130) ? "snb" : "ig", gSettings.IgPlatform);

    SetIgPlatform = TRUE;
  }

  if (gSettings.DualLink != 0) {
    devprop_add_value(device, "AAPL00,DualLink", (UINT8*)&gSettings.DualLink, 1);
  }

  if (gSettings.NoDefaultProperties) {
    //MsgLog(" - no default properties\n");
    return TRUE;
  }

  devprop_add_value(device, "model", (UINT8*)model, (UINT32)AsciiStrLen(model));
  devprop_add_value(device, "device_type", (UINT8*)"display", 7);
  devprop_add_value(device, "subsystem-vendor-id", INTELDEF_vals[21], 4);
  devprop_add_value(device, "class-code", (UINT8*)ClassFix, 4);

  //DualLink = gSettings.DualLink;

  //MacModel = GetModelFromString(gSettings.ProductName);
  switch (gma_dev->device_id) {
    case 0x0102:
      devprop_add_value(device, "class-code", (UINT8*)ClassFix, 4);
    case 0x0106:
    case 0x0112:
    case 0x0116:
    case 0x0122:
    case 0x0126:
      /*
      switch (MacModel) {
        case MacBookPro81:
          SnbId = 0x00000100;
          break;
        case MacBookPro83:
          SnbId = 0x00000200;
          break;
        case MacMini51:
          SnbId = 0x10000300;
          break;
      case Macmini52:
          SnbId = 0x20000300;
          break;
        case MacBookAir41:
          SnbId = 0x00000400;
          break;

        default:
          break;
      }
      */
    case 0x0152:
    case 0x0156:
    case 0x0162:
    case 0x0166:
    case 0x016A:
    case 0x0412:
    case 0x0416:
    case 0x041E:
    case 0x0A0E:
    case 0x0A16:
    case 0x0A1E:
    case 0x0A26:
    case 0x0A2E:
    case 0x0D22:
    case 0x0D26:
    case 0x1602:
    case 0x1606:
    case 0x160A:
    case 0x1612:
    case 0x1616:
    case 0x161A:
    case 0x161E:
    case 0x1622:
    case 0x1626:
    case 0x162A:
    case 0x1632:
    case 0x1636:
    case 0x163A:
    case 0x163E:
    case 0x1902:
    case 0x1906:
    case 0x190A://#
    case 0x190E://#
    case 0x1912:
    case 0x1913://#
    case 0x1915://#
    case 0x1916:
    case 0x1917://#
    case 0x191A:
    case 0x191B://#
    case 0x191D://#
    case 0x191E:
    case 0x1921://#
    //0x1922://#
    case 0x1923://#
    case 0x1926:
    case 0x1927://#
    case 0x192A:
    case 0x192B://#
    case 0x1932:
    case 0x193A:
    case 0x193B://#
    case 0x193D://#
      if (!SetIgPlatform) {
        i = 0;

        switch (gma_dev->device_id) {
          case 0x162:
          case 0x16A:
            i = 23;
            break;
          case 0x152:
            i = 24;
            break;
          case 0x166:
          case 0x156:
            i = 25;
            break;
          case 0x0102:
          case 0x0106:
          case 0x0112:
          case 0x0116:
          case 0x0122:
          case 0x0126:
            break;
          default:
            i = 26;
            break;
        }

        if (i) {
          devprop_add_value(device, "AAPL,ig-platform-id", INTELDEF_vals[i], 4);
          DBG(" - ig-platform-id: 0x%02x%02x%02x%02x\n", INTELDEF_vals[i][3], INTELDEF_vals[i][2], INTELDEF_vals[i][1], INTELDEF_vals[i][0]);
        }
      }
    case 0xA011:
    case 0xA012:
      if (DualLink != 0) {
        devprop_add_value(device, "AAPL00,DualLink", (UINT8*)&DualLink, 1);
      }
    case 0x2582:
    case 0x2592:
    case 0x27A2:
    case 0x27AE:
      devprop_add_value(device, "AAPL,HasPanel", reg_TRUE, 4);
      devprop_add_value(device, "built-in", &BuiltIn, 1);
      break;
    case 0x2772:
    case 0x29C2:
    case 0x0042:
    case 0x0046:
    case 0xA002:
      devprop_add_value(device, "built-in", &BuiltIn, 1);
      devprop_add_value(device, "AAPL00,DualLink", (UINT8*)&DualLink, 1);
      devprop_add_value(device, "AAPL,os-info", (UINT8*)&OsInfo, sizeof(OsInfo));
      break;
    case 0x2A02:
    case 0x2A12:
    case 0x2A42:
      devprop_add_value(device, "AAPL,HasPanel", INTELDEF_vals[0], 4);
      devprop_add_value(device, "AAPL,SelfRefreshSupported", INTELDEF_vals[1], 4);
      devprop_add_value(device, "AAPL,aux-power-connected", INTELDEF_vals[2], 4);
      devprop_add_value(device, "AAPL,backlight-control", INTELDEF_vals[3], 4);
      devprop_add_value(device, "AAPL00,blackscreen-preferences", INTELDEF_vals[4], 4);
      devprop_add_value(device, "AAPL01,BacklightIntensity", INTELDEF_vals[5], 4);
      devprop_add_value(device, "AAPL01,blackscreen-preferences", INTELDEF_vals[6], 4);
      devprop_add_value(device, "AAPL01,DataJustify", INTELDEF_vals[7], 4);
      //devprop_add_value(device, "AAPL01,Depth", INTELDEF_vals[8], 4);
      devprop_add_value(device, "AAPL01,Dither", INTELDEF_vals[9], 4);
      devprop_add_value(device, "AAPL01,DualLink", (UINT8 *)&DualLink, 1);
      //devprop_add_value(device, "AAPL01,Height", INTELDEF_vals[10], 4);
      devprop_add_value(device, "AAPL01,Interlace", INTELDEF_vals[11], 4);
      devprop_add_value(device, "AAPL01,Inverter", INTELDEF_vals[12], 4);
      devprop_add_value(device, "AAPL01,InverterCurrent", INTELDEF_vals[13], 4);
      //devprop_add_value(device, "AAPL01,InverterCurrency", INTELDEF_vals[15], 4);
      devprop_add_value(device, "AAPL01,LinkFormat", INTELDEF_vals[14], 4);
      devprop_add_value(device, "AAPL01,LinkType", INTELDEF_vals[15], 4);
      devprop_add_value(device, "AAPL01,Pipe", INTELDEF_vals[16], 4);
      //devprop_add_value(device, "AAPL01,PixelFormat", INTELDEF_vals[17], 4);
      devprop_add_value(device, "AAPL01,Refresh", INTELDEF_vals[18], 4);
      devprop_add_value(device, "AAPL01,Stretch", INTELDEF_vals[19], 4);
      devprop_add_value(device, "AAPL01,InverterFrequency", INTELDEF_vals[20], 4);
      //devprop_add_value(device, "class-code",   (UINT8*)ClassFix, 4);
      //devprop_add_value(device, "subsystem-vendor-id", INTELDEF_vals[21], 4);
      devprop_add_value(device, "subsystem-id", INTELDEF_vals[22], 4);
      devprop_add_value(device, "built-in", &BuiltIn, 1);
      break;
    default:
      DBG(" - card id=%x unsupported\n", gma_dev->device_id);
      return FALSE;
  }

//#if DEBUG_GMA == 2
//  gBS->Stall(5000000);
//#endif

  return TRUE;
}
