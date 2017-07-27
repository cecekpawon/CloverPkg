/*
 * Copyright 2008 mackerintel
 * 2010 mojodojo, 2012 slice
 */

#include <Library/Platform/AmlGenerator.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_STATE_GEN
#define DEBUG_STATE_GEN -1
#endif
#else
#ifdef DEBUG_STATE_GEN
#undef DEBUG_STATE_GEN
#endif
#define DEBUG_STATE_GEN DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_STATE_GEN, __VA_ARGS__)

CONST UINT8 PssSsdtHeader[] = {
  0x53, 0x53, 0x44, 0x54, 0x7E, 0x00, 0x00, 0x00,   /* SSDT.... */
  0x01, 0x6A, 0x50, 0x6D, 0x52, 0x65, 0x66, 0x00,   /* ..PmRef. */
  0x43, 0x70, 0x75, 0x50, 0x6D, 0x00, 0x00, 0x00,   /* CpuPm... */
  0x00, 0x30, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x4C,   /* .0..INTL */
  0x20, 0x03, 0x12, 0x20                            /* 1.._   */
};

CHAR8 CstSsdtHeader[] = {
  0x53, 0x53, 0x44, 0x54, 0xE7, 0x00, 0x00, 0x00,   /* SSDT.... */
  0x01, 0x17, 0x50, 0x6D, 0x52, 0x65, 0x66, 0x41,   /* ..PmRefA */
  0x43, 0x70, 0x75, 0x43, 0x73, 0x74, 0x00, 0x00,   /* CpuCst.. */
  0x00, 0x30, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x4C,   /* ....INTL */
  0x20, 0x03, 0x12, 0x20                            /* 1.._   */
};

CHAR8 ResourceTemplateRegisterFixedHW[] = {
  0x11, 0x14, 0x0A, 0x11, 0x82, 0x0C, 0x00, 0x7F,
  0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x79, 0x00
};

CHAR8 PluginType[] = {
  0x14, 0x22, 0x5F, 0x44, 0x53, 0x4D, 0x04, 0xA0,
  0x09, 0x93, 0x6A, 0x00, 0xA4, 0x11, 0x03, 0x01,
  0x03, 0xA4, 0x12, 0x10, 0x02, 0x0D, 0x70, 0x6C,
  0x75, 0x67, 0x69, 0x6E, 0x2D, 0x74, 0x79, 0x70,
  0x65, 0x00,
};

EFI_ACPI_DESCRIPTION_HEADER *
GeneratePssSsdt (
  UINT8   FirstID,
  UINTN   Number
) {
  CHAR8     Name[31], Name1[31], Name2[31];
  P_STATE   Maximum, Minimum, PStates[64];
  UINT8     PStatesCount = 0;
  UINT16    RealMax, RealMin = 6, RealTurbo = 0, Apsn = 0, Aplf = 8;
  UINT32    i, j;

  if (gMobile) {
   Aplf = 4;
  }

  for (i = 0; i < 47; i++) {
    //ultra-mobile
    if (
      (gCPUStructure.BrandString[i] != 'P') &&
      (gCPUStructure.BrandString[i + 1] == 'U')
    ) {
      Aplf = 0;
      break;
    }
  }

  if (gSettings.MinMultiplier == 7) {
    Aplf++;
  }

  if (gCPUStructure.Model >= CPUID_MODEL_HASWELL) {
    Aplf = 0;
  }

  if (Number > 0) {
    // Retrieving P-States, ported from code by superhai (c)
    if (
      (gCPUStructure.Family == 0x06) &&
      (gCPUStructure.Model >= CPUID_MODEL_SANDYBRIDGE)
    ) {
      Maximum.Control.Control = RShiftU64 (AsmReadMsr64 (MSR_PLATFORM_INFO), 8) & 0xff;

      if (gSettings.MaxMultiplier) {
        DBG ("Using custom MaxMultiplier %d instead of automatic %d\n",
            gSettings.MaxMultiplier, Maximum.Control.Control);
        Maximum.Control.Control = gSettings.MaxMultiplier;
      }

      RealMax = Maximum.Control.Control;
      DBG ("Maximum control=0x%x\n", RealMax);

      if (gSettings.Turbo) {
        RealTurbo = (gCPUStructure.Turbo4 > gCPUStructure.Turbo1)
                      ? (gCPUStructure.Turbo4 / 10)
                      : (gCPUStructure.Turbo1 / 10);
        Maximum.Control.Control = RealTurbo;
        DBG ("Turbo control=0x%x\n", RealTurbo);
      }

      Apsn = (RealTurbo > RealMax) ? (RealTurbo - RealMax) : 0;
      RealMin =  RShiftU64 (AsmReadMsr64 (MSR_PLATFORM_INFO), 40) & 0xff;

      if (gSettings.MinMultiplier) {
        Minimum.Control.Control = gSettings.MinMultiplier;
        Aplf = (RealMin > Minimum.Control.Control) ? (RealMin - Minimum.Control.Control) : 0;
      } else {
        Minimum.Control.Control = RealMin;
      }

      DBG ("P-States: min 0x%x, max 0x%x\n", Minimum.Control.Control, Maximum.Control.Control);

      // Sanity check
      if (Maximum.Control.Control < Minimum.Control.Control) {
        DBG ("Insane control values!");
      } else {
        for (i = Maximum.Control.Control; i >= Minimum.Control.Control; i--) {
          j = i << 8;

          PStates[PStatesCount].Frequency = (UINT32)(100 * i);
          PStates[PStatesCount].Control.Control = (UINT16)j;
          PStates[PStatesCount].CID = j;

          if (!PStatesCount && gSettings.DoubleFirstState) {
            //double first state
            PStatesCount++;
            PStates[PStatesCount].Control.Control = (UINT16)j;
            PStates[PStatesCount].CID = j;
            PStates[PStatesCount].Frequency = (UINT32)(DivU64x32 (MultU64x32 (gCPUStructure.FSBFrequency, i), Mega)) - 1;
          }

          PStatesCount++;
        }
      }
    } else {
      MsgLog ("Unsupported CPU (0x%X): P-States not generated !!!\n", gCPUStructure.Family);
    }

    //
    // Generating SSDT
    //

    if (PStatesCount > 0) {
      EFI_ACPI_DESCRIPTION_HEADER   *Ssdt;
      AML_CHUNK                     *Scope, *Method, *Pack, *MetPSS, *MetPPC,
                                    *NamePCT, *PackPCT, *MetPCT, *Root = AmlCreateNode (NULL);

      AmlAddBuffer (Root, (CHAR8 *)&PssSsdtHeader[0], sizeof (PssSsdtHeader)); // SSDT header

      AsciiSPrint (Name, 31, "%a%4a", AcpiCPUScore, AcpiCPUName[0]);
      AsciiSPrint (Name1, 31, "%a%4aPSS_", AcpiCPUScore, AcpiCPUName[0]);
      AsciiSPrint (Name2, 31, "%a%4aPCT_", AcpiCPUScore, AcpiCPUName[0]);

      Scope = AmlAddScope (Root, Name);
      Method = AmlAddName (Scope, "PSS_");
      Pack = AmlAddPackage (Method);

      //for (i = gSettings.PLimitDict; i < PStatesCount; i++) {
      for (i = 0; i < PStatesCount; i++) {
        AML_CHUNK   *pstt = AmlAddPackage (Pack);

        AmlAddDword (pstt, PStates[i].Frequency);
        if (PStates[i].Control.Control < RealMin) {
          AmlAddDword (pstt, 0); //zero for power
        } else {
          AmlAddDword (pstt, PStates[i].Frequency<<3); // Power
        }

        AmlAddDword (pstt, 0x0000000A); // Latency
        AmlAddDword (pstt, 0x0000000A); // Latency
        AmlAddDword (pstt, PStates[i].Control.Control);
        AmlAddDword (pstt, PStates[i].Control.Control); // Status
      }

      MetPSS = AmlAddMethod (Scope, "_PSS", 0);
      AmlAddReturnName (MetPSS, "PSS_");
      //MetPSS = AmlAddMethod (Scope, "APSS", 0);
      //AmlAddReturnName (MetPSS, "PSS_");
      MetPPC = AmlAddMethod (Scope, "_PPC", 0);
      //AmlAddReturnByte (MetPPC, gSettings.PLimitDict);
      AmlAddReturnByte (MetPPC, 0);
      NamePCT = AmlAddName (Scope, "PCT_");
      PackPCT = AmlAddPackage (NamePCT);
      ResourceTemplateRegisterFixedHW[8] = 0x00;
      ResourceTemplateRegisterFixedHW[9] = 0x00;
      ResourceTemplateRegisterFixedHW[18] = 0x00;
      AmlAddBuffer (PackPCT, ResourceTemplateRegisterFixedHW, sizeof (ResourceTemplateRegisterFixedHW));
      AmlAddBuffer (PackPCT, ResourceTemplateRegisterFixedHW, sizeof (ResourceTemplateRegisterFixedHW));
      MetPCT = AmlAddMethod (Scope, "_PCT", 0);
      AmlAddReturnName (MetPCT, "PCT_");

      if (gSettings.PluginType) {
        AmlAddBuffer (Scope, PluginType, sizeof (PluginType));
        AmlAddByte (Scope, gSettings.PluginType);
      }

      if (gCPUStructure.Family >= 2) {
        AmlAddName (Scope, "APSN");
        AmlAddByte (Scope, (UINT8)Apsn);
        AmlAddName (Scope, "APLF");
        AmlAddByte (Scope, (UINT8)Aplf);
      }

      // Add CPUs
      for (i = 1; i < Number; i++) {
        AsciiSPrint (Name, 31, "%a%4a", AcpiCPUScore, AcpiCPUName[i]);
        Scope = AmlAddScope (Root, Name);
        MetPSS = AmlAddMethod (Scope, "_PSS", 0);
        AmlAddReturnName (MetPSS, Name1);
        //MetPSS = AmlAddMethod (Scope, "APSS", 0);
        //AmlAddReturnName (MetPSS, Name1);
        MetPPC = AmlAddMethod (Scope, "_PPC", 0);
        //AmlAddReturnByte (MetPPC, gSettings.PLimitDict);
        AmlAddReturnByte (MetPPC, 0);
        MetPCT = AmlAddMethod (Scope, "_PCT", 0);
        AmlAddReturnName (MetPCT, Name2);
      }

      AmlCalculateSize (Root);

      Ssdt = (EFI_ACPI_DESCRIPTION_HEADER *)AllocateZeroPool (Root->Size);
      AmlWriteNode (Root, (VOID *)Ssdt, 0);
      Ssdt->Length = Root->Size;
      Ssdt->Checksum = 0;
      Ssdt->Checksum = (UINT8)(256 - Checksum8 (Ssdt, Ssdt->Length));

      AmlDestroyNode (Root);

      MsgLog ("SSDT with CPU P-States generated successfully\n");

      return Ssdt;
    }
  } else {
    MsgLog ("ACPI CPUs not found: P-States not generated !!!\n");
  }

  return NULL;
}

EFI_ACPI_DESCRIPTION_HEADER *
GenerateCstSsdt (
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *Fadt,
  UINT8                                       FirstID,
  UINTN                                       Number
) {
  BOOLEAN                       C2Enabled, C3Enabled;
  UINT8                         CStatesCount;
  UINT32                        AcpiCPUPBlk;
  CHAR8                         Name2[31], Name0[31], Name1[31];
  AML_CHUNK                     *Root, *Scope, *Name, *Pack, *Tmpl, *Met;
  UINTN                         i;
  EFI_ACPI_DESCRIPTION_HEADER   *Ssdt;

  if (!Fadt) {
    return NULL;
  }

  AcpiCPUPBlk = Fadt->Pm1aEvtBlk + 0x10;
  C2Enabled = gSettings.EnableC2 || (Fadt->PLvl2Lat < 100);
  C3Enabled = (Fadt->PLvl3Lat < 1000);
  CStatesCount = 1 + (C2Enabled ? 1 : 0) + ((C3Enabled || gSettings.EnableC4)? 1 : 0)
                  + (gSettings.EnableC6 ? 1 : 0) + (gSettings.EnableC7 ? 1 : 0);

  Root = AmlCreateNode (NULL);
  AmlAddBuffer (Root, CstSsdtHeader, sizeof (CstSsdtHeader)); // SSDT header
  AsciiSPrint (Name0, 31, "%a%4a", AcpiCPUScore, AcpiCPUName[0]);
  AsciiSPrint (Name1, 31, "%a%4aCST_",  AcpiCPUScore, AcpiCPUName[0]);
  Scope = AmlAddScope (Root, Name0);
  Name = AmlAddName (Scope, "CST_");
  Pack = AmlAddPackage (Name);
  AmlAddByte (Pack, CStatesCount);

  Tmpl = AmlAddPackage (Pack);

  // C1
  ResourceTemplateRegisterFixedHW[8] = 0x01;
  ResourceTemplateRegisterFixedHW[9] = 0x02;
  //ResourceTemplateRegisterFixedHW[18] = 0x01;
  ResourceTemplateRegisterFixedHW[10] = 0x01;

  ResourceTemplateRegisterFixedHW[11] = 0x00; // C1

  AmlAddBuffer (Tmpl, ResourceTemplateRegisterFixedHW, ARRAY_SIZE (ResourceTemplateRegisterFixedHW));
  AmlAddByte (Tmpl, 0x01);     // C1
  AmlAddWord (Tmpl, 0x0001);     // Latency
  AmlAddDword (Tmpl, 0x000003e8);  // Power

  //ResourceTemplateRegisterFixedHW[18] = 0x03;
  ResourceTemplateRegisterFixedHW[10] = 0x03;

  if (C2Enabled) {         // C2
    Tmpl = AmlAddPackage (Pack);
    ResourceTemplateRegisterFixedHW[11] = 0x10; // C2
    AmlAddBuffer (Tmpl, ResourceTemplateRegisterFixedHW, ARRAY_SIZE (ResourceTemplateRegisterFixedHW));
    AmlAddByte (Tmpl, 0x02);     // C2
    AmlAddWord (Tmpl, 0x0040);     // Latency
    AmlAddDword (Tmpl, 0x000001f4);  // Power
  }

  if (gSettings.EnableC4) {         // C4
    Tmpl = AmlAddPackage (Pack);
    ResourceTemplateRegisterFixedHW[11] = 0x30; // C4
    AmlAddBuffer (Tmpl, ResourceTemplateRegisterFixedHW, ARRAY_SIZE (ResourceTemplateRegisterFixedHW));
    AmlAddByte (Tmpl, 0x04);     // C4
    AmlAddWord (Tmpl, 0x0080);     // Latency
    AmlAddDword (Tmpl, 0x000000C8);  // Power
  } else if (C3Enabled) {
    Tmpl = AmlAddPackage (Pack);
    ResourceTemplateRegisterFixedHW[11] = 0x20; // C3
    AmlAddBuffer (Tmpl, ResourceTemplateRegisterFixedHW, ARRAY_SIZE (ResourceTemplateRegisterFixedHW));
    AmlAddByte (Tmpl, 0x03);     // C3
    AmlAddWord (Tmpl, gSettings.C3Latency);      // Latency as in MacPro6,1 = 0x0043
    AmlAddDword (Tmpl, 0x000001F4);  // Power
  }

  if (gSettings.EnableC6) {     // C6
    Tmpl = AmlAddPackage (Pack);
    ResourceTemplateRegisterFixedHW[11] = 0x20; // C6
    AmlAddBuffer (Tmpl, ResourceTemplateRegisterFixedHW, ARRAY_SIZE (ResourceTemplateRegisterFixedHW));
    AmlAddByte (Tmpl, 0x06);     // C6
    AmlAddWord (Tmpl, gSettings.C3Latency + 3);      // Latency as in MacPro6,1 = 0x0046
    AmlAddDword (Tmpl, 0x0000015E);  // Power
  }

  if (gSettings.EnableC7) {
    Tmpl = AmlAddPackage (Pack);
    ResourceTemplateRegisterFixedHW[11] = 0x30; // C4 or C7
    AmlAddBuffer (Tmpl, ResourceTemplateRegisterFixedHW, ARRAY_SIZE (ResourceTemplateRegisterFixedHW));
    AmlAddByte (Tmpl, 0x07);     // C7
    AmlAddWord (Tmpl, 0xF5);     // Latency as in iMac14,1
    AmlAddDword (Tmpl, 0xC8);  // Power
  }

  Met = AmlAddMethod (Scope, "_CST", 0);
  AmlAddReturnName (Met, "CST_");
  //Met = AmlAddMethod (Scope, "ACST", 0);
  //ret = AmlAddReturnName (Met, "CST_");

  // Aliases
  for (i = 1; i < Number; i++) {
    AsciiSPrint (Name2, 31, "%a%4a",  AcpiCPUScore, AcpiCPUName[i]);

    Scope = AmlAddScope (Root, Name2);
    Met = AmlAddMethod (Scope, "_CST", 0);
    AmlAddReturnName (Met, Name1);
    //Met = AmlAddMethod (Scope, "ACST", 0);
    //ret = AmlAddReturnName (Met, Name1);
  }

  AmlCalculateSize (Root);

  Ssdt = (EFI_ACPI_DESCRIPTION_HEADER *)AllocateZeroPool (Root->Size);

  AmlWriteNode (Root, (VOID *)Ssdt, 0);

  Ssdt->Length = Root->Size;
  Ssdt->Checksum = 0;
  Ssdt->Checksum = (UINT8)(256 - Checksum8 ((VOID *)Ssdt, Ssdt->Length));

  AmlDestroyNode (Root);

  //dumpPhysAddr ("C-States SSDT content: ", Ssdt, Ssdt->Length);

  MsgLog ("SSDT with CPU C-States generated successfully\n");

  return Ssdt;
}
