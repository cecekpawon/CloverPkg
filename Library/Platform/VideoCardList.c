/*
 * Copyright (c) 2012 cparm <armelcadetpetit@gmail.com>. All rights reserved.
 *
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_VCARDLIST
#define DEBUG_VCARDLIST -1
#endif
#else
#ifdef DEBUG_VCARDLIST
#undef DEBUG_VCARDLIST
#endif
#define DEBUG_VCARDLIST DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_VCARDLIST, __VA_ARGS__)

/*
 injection for NVIDIA card usage e.g (to be placed in the config.plist, under graphics tag):
 <key>Graphics</key>
 <dict>
    <key>NVIDIA</key>
    <array>
      <dict>
        <key>Model</key>
        <string>Quadro FX 380</string>
        <key>IOPCIPrimaryMatch</key>
        <string>0x10DE0658</string>
        <key>VRAM</key>
        <integer>256</integer>
        <key>VideoPorts</key>
        <integer>2</integer>
        <key>LoadVBios</key>
        <true/>
      </dict>
    <dict>
      <key>Model</key>
      <string>YOUR_SECOND_CARD_NAME</string>
      <key>IOPCIPrimaryMatch</key>
      <string>YOUR_SECOND_CARD_ID</string>
      <key>IOPCISubDevId</key>
      <string>YOUR_SECOND_CARD_SUB_ID (if necessary)</string>
      <key>VRAM</key>
      <integer>YOUR_SECOND_CARD_VRAM_SIZE</integer>
      <key>VideoPorts</key>
      <integer>YOUR_SECOND_CARD_PORTS</integer>
      <key>LoadVBios</key>
      <true/><!--YOUR_SECOND_CARD_LOADVBIOS-->
    </dict>
  </array>
    <key>ATI</key>
    <array>
      <dict>
        <key>Model</key>
        <string>ATI Radeon HD6670</string>
        <key>IOPCIPrimaryMatch</key>
        <string>0x6758</string>
        <key>IOPCISubDevId</key>
        <string>0x1342</string>
        <key>VRAM</key>
        <integer>2048</integer>
      </dict>
    </array>
  </dict>
*/

LIST_ENTRY  gCardList = INITIALIZE_LIST_HEAD_VARIABLE (gCardList);

VOID
AddCard (
  CONST CHAR8     *Model,
        UINT32    Id,
        UINT32    SubId,
        UINT64    VideoRam,
        UINTN     VideoPorts,
        BOOLEAN   LoadVBios
) {
  CARDLIST    *NewCard = AllocateZeroPool (sizeof (CARDLIST));

  if (NewCard) {
    NewCard->Signature = CARDLIST_SIGNATURE;
    NewCard->Id = Id;
    NewCard->SubId = SubId;
    NewCard->VideoRam = VideoRam;
    NewCard->VideoPorts = VideoPorts;
    NewCard->LoadVBios = LoadVBios;
    AsciiSPrint (NewCard->Model, ARRAY_SIZE (NewCard->Model), "%a", Model);
    InsertTailList (&gCardList, (LIST_ENTRY *)(((UINT8 *)NewCard) + OFFSET_OF (CARDLIST, Link)));
  }
}

CARDLIST *
FindCardWithIds (
  UINT32    Id,
  UINT32    SubId
) {
  LIST_ENTRY    *Link;
  CARDLIST      *Entry;

  if (!IsListEmpty (&gCardList)) {
    for (Link = gCardList.ForwardLink; Link != &gCardList; Link = Link->ForwardLink) {
      Entry = CR (Link, CARDLIST, Link, CARDLIST_SIGNATURE);
      if (Entry->Id == Id) {
        return Entry;
      }
    }
  }

  return NULL;
}

VOID
FillCardList (
  TagPtr    CfgDict
) {
  if (IsListEmpty (&gCardList) && (CfgDict != NULL)) {
    CHAR8     *VEN[] = { "NVIDIA",  "ATI" };
    INTN      Index, VenCount = ARRAY_SIZE (VEN);
    TagPtr    Prop;

    for (Index = 0; Index < VenCount; Index++) {
      CHAR8     *Key = VEN[Index];

      Prop = GetProperty (CfgDict, Key);

      if (Prop && (Prop->type == kTagTypeArray)) {
        INTN      i, Count = Prop->size;
        TagPtr    Element = 0, Prop2 = 0;

        for (i = 0; i < Count; i++) {
          CHAR8         *ModelName = NULL;
          UINT32        DevID = 0, SubDevID = 0;
          UINT64        VramSize  = 0;
          UINTN         VideoPorts  = 0;
          BOOLEAN       LoadVBios = FALSE;
          EFI_STATUS    Status = GetElement (Prop, i, Count, &Element);

          if (!EFI_ERROR (Status) && Element) {
            if ((Prop2 = GetProperty (Element, "Model")) != 0) {
              ModelName = Prop2->string;
            } else {
              ModelName = "VideoCard";
            }

            Prop2 = GetProperty (Element, "IOPCIPrimaryMatch");
            DevID = (UINT32)GetPropertyInteger (Prop2, 0);

            Prop2 = GetProperty (Element, "IOPCISubDevId");
            SubDevID = (UINT32)GetPropertyInteger (Prop2, 0);

            Prop2 = GetProperty (Element, "VRAM");
            VramSize = LShiftU64 ((UINTN)GetPropertyInteger (Prop2, (INTN)VramSize), 20); //Mb -> bytes

            Prop2 = GetProperty (Element, "VideoPorts");
            VideoPorts = (UINT16)GetPropertyInteger (Prop2, VideoPorts);

            Prop2 = GetProperty (Element, "LoadVBios");

            LoadVBios = GetPropertyBool (Prop2, FALSE);

            DBG ("FillCardList: %a | \"%a\" (%08x, %08x)\n", Key, ModelName, DevID, SubDevID);

            AddCard (ModelName, DevID, SubDevID, VramSize, VideoPorts, LoadVBios);
          }
        }
      }
    }
  }
}
