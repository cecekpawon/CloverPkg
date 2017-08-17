// Slice 2014

#include <Library/Platform/Platform.h>
#include <Library/NetLib.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_MAC_ADDRESS
#define DEBUG_MAC_ADDRESS -1
#endif
#else
#ifdef DEBUG_MAC_ADDRESS
#undef DEBUG_MAC_ADDRESS
#endif
#define DEBUG_MAC_ADDRESS DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_MAC_ADDRESS, __VA_ARGS__)

//Marvell Yukon
#define B2_MAC_1                        0x0100    /* NA reg MAC Address 1 */
#define B2_MAC_2                        0x0108    /* NA reg MAC Address 2 */

//Atheros
#define L1C_STAD0                       0x1488

//Intel
#define INTEL_MAC_1                     0x5400
#define INTEL_MAC_2                     0x54E0

// Broadcom MAC Address Registers
#define EMAC_MACADDR0_HI                0x00000410
#define EMAC_MACADDR1_HI                0x00000418

VOID
GetMacAddress () {
  EFI_STATUS                  Status;
  //EFI_MAC_ADDRESS             MacAddr;
  UINTN                       HwAddressSize, Index, Index2, Offset, NumberOfHandles;
  EFI_HANDLE                  *HandleBuffer;
  EFI_DEVICE_PATH_PROTOCOL    *Node, *DevicePath;
  MAC_ADDR_DEVICE_PATH        *MacAddressNode;
  BOOLEAN                     Found, Swap;
  UINT16                      PreviousVendor = 0;
  UINT32                      Mac0, Mac4;
  UINT8                       *HwAddress;

  HwAddressSize = 6;

  //
  // Locate Service Binding handles.
  //
  NumberOfHandles = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDevicePathProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &HandleBuffer
                );

  if (EFI_ERROR (Status)) {
    return;
  }

  DbgHeader ("GetMacAddress");

  Found = FALSE;
  for (Index = 0; Index < NumberOfHandles; Index++) {
    Node = NULL;
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiDevicePathProtocolGuid,
                    (VOID **)&Node
                  );

    if (EFI_ERROR (Status)) {
      continue;
    }

    DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)Node;

    //
    while (!IsDevicePathEnd (DevicePath)) {
      if (
        (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MSG_MAC_ADDR_DP)
      ) {
        //
        // Get MAC address.
        //
        MacAddressNode = (MAC_ADDR_DEVICE_PATH *)DevicePath;
        //HwAddressSize = sizeof (EFI_MAC_ADDRESS);
        //if (MacAddressNode->IfType == 0x01 || MacAddressNode->IfType == 0x00) {
        //  HwAddressSize = 6;
        //}
        //CopyMem (&MacAddr, &MacAddressNode->MacAddress.Addr[0], HwAddressSize);
        MsgLog ("MAC address of LAN #%d = ", gSettings.GLAN.Paths);
        HwAddress = &MacAddressNode->MacAddress.Addr[0];

        for (Index2 = 0; Index2 < HwAddressSize; Index2++) {
          MsgLog ("%a%02x", !Index2 ? "" : ":", *HwAddress++);
        }

        MsgLog ("\n");
        Found = TRUE;
        CopyMem (&gSettings.GLAN.Mac[gSettings.GLAN.Paths++], &MacAddressNode->MacAddress.Addr[0], HwAddressSize);
        break;
      }

      DevicePath = NextDevicePathNode (DevicePath);
    }

    if (gSettings.GLAN.Paths > 3) {
      break;
    }
  }

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  if (!Found/* && GetLegacyLanAddress*/) {
    ////
    //
    //  Legacy boot. Get MAC-address from hardwaredirectly
    //
    ////
    DBG ("Legacy LAN MAC, %d card found\n", gSettings.GLAN.Cards);
    for (Index = 0; Index < gSettings.GLAN.Cards; Index++) {
      if (!gSettings.GLAN.Mmio[Index]) {  //security
        continue;
      }

      Offset = 0;
      Swap = FALSE;

      switch (gSettings.GLAN.Vendor[Index]) {
        case 0x11ab:   //Marvell Yukon
          if (PreviousVendor == gSettings.GLAN.Vendor[Index]) {
            Offset = B2_MAC_2;
          } else {
            Offset = B2_MAC_1;
          }
          CopyMem (&gSettings.GLAN.Mac[0][Index], gSettings.GLAN.Mmio[Index] + Offset, 6);
          goto done;

        case 0x10ec:   //Realtek
          Mac0 = IoRead32 ((UINTN)gSettings.GLAN.Mmio[Index]);
          Mac4 = IoRead32 ((UINTN)gSettings.GLAN.Mmio[Index] + 4);
          goto copy;

        case 0x14e4:   //Broadcom
          if (PreviousVendor == gSettings.GLAN.Vendor[Index]) {
            Offset = EMAC_MACADDR1_HI;
          } else {
            Offset = EMAC_MACADDR0_HI;
          }
          break;

        case 0x1969:   //Atheros
          Offset = L1C_STAD0;
          Swap = TRUE;
          break;

        case 0x8086:   //Intel
          if (PreviousVendor == gSettings.GLAN.Vendor[Index]) {
            Offset = INTEL_MAC_2;
          } else {
            Offset = INTEL_MAC_1;
          }
          break;

        default:
          break;
      }

      if (!Offset) {
        continue;
      }

      Mac0 = *(UINT32 *)(gSettings.GLAN.Mmio[Index] + Offset);
      Mac4 = *(UINT32 *)(gSettings.GLAN.Mmio[Index] + Offset + 4);

      if (Swap) {
        gSettings.GLAN.Mac[Index][0] = (UINT8)((Mac4 & 0xFF00) >> 8);
        gSettings.GLAN.Mac[Index][1] = (UINT8)(Mac4 & 0xFF);
        gSettings.GLAN.Mac[Index][2] = (UINT8)((Mac0 & 0xFF000000) >> 24);
        gSettings.GLAN.Mac[Index][3] = (UINT8)((Mac0 & 0x00FF0000) >> 16);
        gSettings.GLAN.Mac[Index][4] = (UINT8)((Mac0 & 0x0000FF00) >> 8);
        gSettings.GLAN.Mac[Index][5] = (UINT8)(Mac0 & 0x000000FF);
        goto done;
      }

    copy:
      CopyMem (&gSettings.GLAN.Mac[Index][0], &Mac0, 4);
      CopyMem (&gSettings.GLAN.Mac[Index][4], &Mac4, 2);

    done:
      PreviousVendor = gSettings.GLAN.Vendor[Index];
      MsgLog ("Legacy MAC address of LAN #%d = ", Index);
      HwAddress = &gSettings.GLAN.Mac[Index][0];
      for (Index2 = 0; Index2 < HwAddressSize; Index2++) {
        MsgLog ("%a%02x", !Index2 ? "" : ":", *HwAddress++);
      }

      MsgLog ("\n");
    }
  }
}
