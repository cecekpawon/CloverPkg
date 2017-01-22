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

CONST UINT8 pss_ssdt_header[] =
{
  0x53, 0x53, 0x44, 0x54, 0x7E, 0x00, 0x00, 0x00,   /* SSDT.... */
  0x01, 0x6A, 0x50, 0x6D, 0x52, 0x65, 0x66, 0x00,   /* ..PmRef. */
  0x43, 0x70, 0x75, 0x50, 0x6D, 0x00, 0x00, 0x00,   /* CpuPm... */
  0x00, 0x30, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x4C,   /* .0..INTL */
  0x20, 0x03, 0x12, 0x20                            /* 1.._   */
};


CHAR8 cst_ssdt_header[] =
{
  0x53, 0x53, 0x44, 0x54, 0xE7, 0x00, 0x00, 0x00,   /* SSDT.... */
  0x01, 0x17, 0x50, 0x6D, 0x52, 0x65, 0x66, 0x41,   /* ..PmRefA */
  0x43, 0x70, 0x75, 0x43, 0x73, 0x74, 0x00, 0x00,   /* CpuCst.. */
  0x00, 0x30, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x4C,   /* ....INTL */
  0x20, 0x03, 0x12, 0x20                            /* 1.._   */
};

CHAR8 resource_template_register_fixedhw[] =
{
  0x11, 0x14, 0x0A, 0x11, 0x82, 0x0C, 0x00, 0x7F,
  0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x79, 0x00
};

CHAR8 resource_template_register_systemio[] =
{
  0x11, 0x14, 0x0A, 0x11, 0x82, 0x0C, 0x00, 0x01,
  0x08, 0x00, 0x00, 0x15, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x79, 0x00,
};

CHAR8 plugin_type[] =
{
  0x14, 0x22, 0x5F, 0x44, 0x53, 0x4D, 0x04, 0xA0,
  0x09, 0x93, 0x6A, 0x00, 0xA4, 0x11, 0x03, 0x01,
  0x03, 0xA4, 0x12, 0x10, 0x02, 0x0D, 0x70, 0x6C,
  0x75, 0x67, 0x69, 0x6E, 0x2D, 0x74, 0x79, 0x70,
  0x65, 0x00,
};

SSDT_TABLE *
GeneratePssSsdt (
  UINT8   FirstID,
  UINTN   Number
) {
  CHAR8     name[31], name1[31], name2[31];
  P_STATE   maximum, minimum, p_states[64];
  UINT8     p_states_count = 0;
  UINT16    realMax, realMin = 6, realTurbo = 0, Apsn = 0, Aplf = 8;
  UINT32    i, j;

  if (gCPUStructure.Vendor != CPU_VENDOR_INTEL) {
    MsgLog ("Not an Intel platform: P-States will not be generated !!!\n");
    return NULL;
  }

  if (!(gCPUStructure.Features & CPUID_FEATURE_MSR)) {
    MsgLog ("Unsupported CPU: P-States will not be generated !!!\n");
    return NULL;
  }

  if (gMobile) {
   Aplf = 4;
  }

  for (i = 0; i < 47; i++) {
    //ultra-mobile
    if (
      (gCPUStructure.BrandString[i] != 'P') &&
      (gCPUStructure.BrandString[i+1] == 'U')
    ) {
      Aplf = 0;
      break;
    }
  }

  if (gSettings.MinMultiplier == 7) {
    Aplf++;
  }

  switch (gCPUStructure.Model) {
    case CPU_MODEL_HASWELL:
    case CPU_MODEL_IVY_BRIDGE_E5:
    case CPU_MODEL_HASWELL_E:
    case CPU_MODEL_HASWELL_ULT:
    case CPU_MODEL_CRYSTALWELL:
    case CPU_MODEL_HASWELL_U5:
    case CPU_MODEL_BROADWELL_HQ:
    case CPU_MODEL_SKYLAKE_U:
    case CPU_MODEL_SKYLAKE_S:
    case CPU_MODEL_ATOM_3700:
      Aplf = 0;
      break;
  }

  if (Number > 0)
  {
    // Retrieving P-States, ported from code by superhai (c)
    switch (gCPUStructure.Family) {
      case 0x06:
        switch (gCPUStructure.Model) {
          case CPU_MODEL_FIELDS:    // Intel Core i5, i7, Xeon X34xx LGA1156 (45nm)
          case CPU_MODEL_DALES:
          case CPU_MODEL_CLARKDALE: // Intel Core i3, i5 LGA1156 (32nm)
          case CPU_MODEL_NEHALEM:   // Intel Core i7, Xeon W35xx, Xeon X55xx, Xeon E55xx LGA1366 (45nm)
          case CPU_MODEL_NEHALEM_EX:  // Intel Xeon X75xx, Xeon X65xx, Xeon E75xx, Xeon E65x
          case CPU_MODEL_WESTMERE:  // Intel Core i7, Xeon X56xx, Xeon E56xx, Xeon W36xx LGA1366 (32nm) 6 Core
          case CPU_MODEL_WESTMERE_EX: // Intel Xeon E7
          case CPU_MODEL_SANDY_BRIDGE:    // Intel Core i3, i5, i7 LGA1155 (32nm)
          case CPU_MODEL_JAKETOWN:  // Intel Xeon E3
          case CPU_MODEL_IVY_BRIDGE:
          case CPU_MODEL_HASWELL:
          case CPU_MODEL_IVY_BRIDGE_E5:
          case CPU_MODEL_HASWELL_E:
          case CPU_MODEL_HASWELL_ULT:
          case CPU_MODEL_CRYSTALWELL:
          case CPU_MODEL_HASWELL_U5:
          case CPU_MODEL_BROADWELL_HQ:
          case CPU_MODEL_SKYLAKE_U:
          case CPU_MODEL_SKYLAKE_S:
          case CPU_MODEL_ATOM_3700:
            maximum.Control.Control = RShiftU64 (AsmReadMsr64 (MSR_PLATFORM_INFO), 8) & 0xff;

            if (gSettings.MaxMultiplier) {
              DBG ("Using custom MaxMultiplier %d instead of automatic %d\n",
                  gSettings.MaxMultiplier, maximum.Control.Control);
              maximum.Control.Control = gSettings.MaxMultiplier;
            }

            realMax = maximum.Control.Control;
            DBG ("Maximum control=0x%x\n", realMax);

            if (gSettings.Turbo) {
              realTurbo = (gCPUStructure.Turbo4 > gCPUStructure.Turbo1) ?
              (gCPUStructure.Turbo4 / 10) : (gCPUStructure.Turbo1 / 10);
              maximum.Control.Control = realTurbo;
              DBG ("Turbo control=0x%x\n", realTurbo);
            }

            Apsn = (realTurbo > realMax)?(realTurbo - realMax):0;
            realMin =  RShiftU64 (AsmReadMsr64 (MSR_PLATFORM_INFO), 40) & 0xff;

            if (gSettings.MinMultiplier) {
              minimum.Control.Control = gSettings.MinMultiplier;
              Aplf = (realMin > minimum.Control.Control)?(realMin - minimum.Control.Control):0;
            } else {
              minimum.Control.Control = realMin;
            }

            DBG ("P-States: min 0x%x, max 0x%x\n", minimum.Control.Control, maximum.Control.Control);

            // Sanity check
            if (maximum.Control.Control < minimum.Control.Control) {
              DBG ("Insane control values!");
              p_states_count = 0;
            } else {
              p_states_count = 0;

              for (i = maximum.Control.Control; i >= minimum.Control.Control; i--) {
                j = i;
                if (
                  (gCPUStructure.Model == CPU_MODEL_SANDY_BRIDGE) ||
                  (gCPUStructure.Model == CPU_MODEL_IVY_BRIDGE) ||
                  (gCPUStructure.Model == CPU_MODEL_HASWELL) ||
                  (gCPUStructure.Model == CPU_MODEL_HASWELL_E) ||
                  (gCPUStructure.Model == CPU_MODEL_HASWELL_ULT) ||
                  (gCPUStructure.Model == CPU_MODEL_CRYSTALWELL) ||
                  (gCPUStructure.Model == CPU_MODEL_IVY_BRIDGE_E5) ||
                  (gCPUStructure.Model == CPU_MODEL_HASWELL_U5) ||
                  (gCPUStructure.Model == CPU_MODEL_BROADWELL_HQ) ||
                  (gCPUStructure.Model == CPU_MODEL_SKYLAKE_U) ||
                  (gCPUStructure.Model == CPU_MODEL_SKYLAKE_S) ||
                  (gCPUStructure.Model == CPU_MODEL_JAKETOWN)
                ) {
                  j = i << 8;
                  p_states[p_states_count].Frequency = (UINT32)(100 * i);
                } else {
                  p_states[p_states_count].Frequency = (UINT32)(DivU64x32 (MultU64x32 (gCPUStructure.FSBFrequency, i), Mega));
                }

                p_states[p_states_count].Control.Control = (UINT16)j;
                p_states[p_states_count].CID = j;

                if (!p_states_count && gSettings.DoubleFirstState) {
                  //double first state
                  p_states_count++;
                  p_states[p_states_count].Control.Control = (UINT16)j;
                  p_states[p_states_count].CID = j;
                  p_states[p_states_count].Frequency = (UINT32)(DivU64x32 (MultU64x32 (gCPUStructure.FSBFrequency, i), Mega)) - 1;

                }

                p_states_count++;
              }
            }

            break;

          default:
            MsgLog ("Unsupported CPU (0x%X): P-States not generated !!!\n", gCPUStructure.Family);
            break;
        }

    }

    // Generating SSDT
    if (p_states_count > 0) {
      SSDT_TABLE    *ssdt;
      AML_CHUNK     *scop, *method, *pack, *metPSS, *metPPC,
                    *namePCT, *packPCT, *metPCT, *root = AmlCreateNode (NULL);

      AmlAddBuffer (root, (CHAR8 *)&pss_ssdt_header[0], sizeof (pss_ssdt_header)); // SSDT header

      AsciiSPrint (name, 31, "%a%4a", acpi_cpu_score, acpi_cpu_name[0]);
      AsciiSPrint (name1, 31, "%a%4aPSS_", acpi_cpu_score, acpi_cpu_name[0]);
      AsciiSPrint (name2, 31, "%a%4aPCT_", acpi_cpu_score, acpi_cpu_name[0]);

      scop = AmlAddScope (root, name);
      method = AmlAddName (scop, "PSS_");
      pack = AmlAddPackage (method);

      //for (i = gSettings.PLimitDict; i < p_states_count; i++) {
      for (i = 0; i < p_states_count; i++) {
        AML_CHUNK   *pstt = AmlAddPackage (pack);

        AmlAddDword (pstt, p_states[i].Frequency);
        if (p_states[i].Control.Control < realMin) {
          AmlAddDword (pstt, 0); //zero for power
        } else {
          AmlAddDword (pstt, p_states[i].Frequency<<3); // Power
        }

        AmlAddDword (pstt, 0x0000000A); // Latency
        AmlAddDword (pstt, 0x0000000A); // Latency
        AmlAddDword (pstt, p_states[i].Control.Control);
        AmlAddDword (pstt, p_states[i].Control.Control); // Status
      }

      metPSS = AmlAddMethod (scop, "_PSS", 0);
      AmlAddReturnName (metPSS, "PSS_");
      //metPSS = AmlAddMethod (scop, "APSS", 0);
      //AmlAddReturnName (metPSS, "PSS_");
      metPPC = AmlAddMethod (scop, "_PPC", 0);
      //AmlAddReturnByte (metPPC, gSettings.PLimitDict);
      AmlAddReturnByte (metPPC, 0);
      namePCT = AmlAddName (scop, "PCT_");
      packPCT = AmlAddPackage (namePCT);
      resource_template_register_fixedhw[8] = 0x00;
      resource_template_register_fixedhw[9] = 0x00;
      resource_template_register_fixedhw[18] = 0x00;
      AmlAddBuffer (packPCT, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
      AmlAddBuffer (packPCT, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
      metPCT = AmlAddMethod (scop, "_PCT", 0);
      AmlAddReturnName (metPCT, "PCT_");

      if (gSettings.PluginType) {
        AmlAddBuffer (scop, plugin_type, sizeof (plugin_type));
        AmlAddByte (scop, gSettings.PluginType);
      }

      if (gCPUStructure.Family >= 2) {
        AmlAddName (scop, "APSN");
        AmlAddByte (scop, (UINT8)Apsn);
        AmlAddName (scop, "APLF");
        AmlAddByte (scop, (UINT8)Aplf);
      }

      // Add CPUs
      for (i = 1; i < Number; i++) {
        AsciiSPrint (name, 31, "%a%4a", acpi_cpu_score, acpi_cpu_name[i]);
        scop = AmlAddScope (root, name);
        metPSS = AmlAddMethod (scop, "_PSS", 0);
        AmlAddReturnName (metPSS, name1);
        //metPSS = AmlAddMethod (scop, "APSS", 0);
        //AmlAddReturnName (metPSS, name1);
        metPPC = AmlAddMethod (scop, "_PPC", 0);
        //AmlAddReturnByte (metPPC, gSettings.PLimitDict);
        AmlAddReturnByte (metPPC, 0);
        metPCT = AmlAddMethod (scop, "_PCT", 0);
        AmlAddReturnName (metPCT, name2);

      }

      AmlCalculateSize (root);

      ssdt = (SSDT_TABLE *)AllocateZeroPool (root->Size);
      AmlWriteNode (root, (VOID *)ssdt, 0);
      ssdt->Length = root->Size;
      ssdt->Checksum = 0;
      ssdt->Checksum = (UINT8)(256 - Checksum8 (ssdt, ssdt->Length));

      AmlDestroyNode (root);

      MsgLog ("SSDT with CPU P-States generated successfully\n");
      return ssdt;
    }
  } else {
    MsgLog ("ACPI CPUs not found: P-States not generated !!!\n");
  }

  return NULL;
}

SSDT_TABLE *
GenerateCstSsdt (
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *fadt,
  UINT8                                       FirstID,
  UINTN                                       Number
) {
  BOOLEAN       c2_enabled = gSettings.EnableC2,
                c3_enabled,
                c4_enabled = gSettings.EnableC4 /*,
                //c6_enabled = gSettings.EnableC6,
                cst_using_systemio = gSettings.EnableISS */;
  UINT8         cstates_count; // p_blk_lo, p_blk_hi,
  UINT32        acpi_cpu_p_blk;
  CHAR8         name2[31], name0[31], name1[31];
  AML_CHUNK     *root, *scop, *name, *pack, *tmpl, *met;
  UINTN         i;
  SSDT_TABLE    *ssdt;

  if (!fadt) {
    return NULL;
  }

  acpi_cpu_p_blk = fadt->Pm1aEvtBlk + 0x10;
  c2_enabled = c2_enabled || (fadt->PLvl2Lat < 100);
  c3_enabled = (fadt->PLvl3Lat < 1000);
  cstates_count = 1 + (c2_enabled ? 1 : 0) + ((c3_enabled || c4_enabled)? 1 : 0)
                  + (gSettings.EnableC6 ? 1 : 0) + (gSettings.EnableC7 ? 1 : 0);

  root = AmlCreateNode (NULL);
  AmlAddBuffer (root, cst_ssdt_header, sizeof (cst_ssdt_header)); // SSDT header
  AsciiSPrint (name0, 31, "%a%4a", acpi_cpu_score, acpi_cpu_name[0]);
  AsciiSPrint (name1, 31, "%a%4aCST_",  acpi_cpu_score, acpi_cpu_name[0]);
  scop = AmlAddScope (root, name0);
  name = AmlAddName (scop, "CST_");
  pack = AmlAddPackage (name);
  AmlAddByte (pack, cstates_count);

  tmpl = AmlAddPackage (pack);

  // C1
  resource_template_register_fixedhw[8] = 0x01;
  resource_template_register_fixedhw[9] = 0x02;
  //resource_template_register_fixedhw[18] = 0x01;
  resource_template_register_fixedhw[10] = 0x01;

  resource_template_register_fixedhw[11] = 0x00; // C1

  AmlAddBuffer (tmpl, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
  AmlAddByte (tmpl, 0x01);     // C1
  AmlAddWord (tmpl, 0x0001);     // Latency
  AmlAddDword (tmpl, 0x000003e8);  // Power

  //resource_template_register_fixedhw[18] = 0x03;
  resource_template_register_fixedhw[10] = 0x03;

  if (c2_enabled) {         // C2
    tmpl = AmlAddPackage (pack);
    resource_template_register_fixedhw[11] = 0x10; // C2
    AmlAddBuffer (tmpl, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
    AmlAddByte (tmpl, 0x02);     // C2
    AmlAddWord (tmpl, 0x0040);     // Latency
    AmlAddDword (tmpl, 0x000001f4);  // Power
  }

  if (c4_enabled) {         // C4
    tmpl = AmlAddPackage (pack);
    resource_template_register_fixedhw[11] = 0x30; // C4
    AmlAddBuffer (tmpl, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
    AmlAddByte (tmpl, 0x04);     // C4
    AmlAddWord (tmpl, 0x0080);     // Latency
    AmlAddDword (tmpl, 0x000000C8);  // Power
  } else if (c3_enabled) {
    tmpl = AmlAddPackage (pack);
    resource_template_register_fixedhw[11] = 0x20; // C3
    AmlAddBuffer (tmpl, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
    AmlAddByte (tmpl, 0x03);     // C3
    AmlAddWord (tmpl, gSettings.C3Latency);      // Latency as in MacPro6,1 = 0x0043
    AmlAddDword (tmpl, 0x000001F4);  // Power
  }

  if (gSettings.EnableC6) {     // C6
    tmpl = AmlAddPackage (pack);
    resource_template_register_fixedhw[11] = 0x20; // C6
    AmlAddBuffer (tmpl, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
    AmlAddByte (tmpl, 0x06);     // C6
    AmlAddWord (tmpl, gSettings.C3Latency + 3);      // Latency as in MacPro6,1 = 0x0046
    AmlAddDword (tmpl, 0x0000015E);  // Power
  }

  if (gSettings.EnableC7) {
    tmpl = AmlAddPackage (pack);
    resource_template_register_fixedhw[11] = 0x30; // C4 or C7
    AmlAddBuffer (tmpl, resource_template_register_fixedhw, sizeof (resource_template_register_fixedhw));
    AmlAddByte (tmpl, 0x07);     // C7
    AmlAddWord (tmpl, 0xF5);     // Latency as in iMac14,1
    AmlAddDword (tmpl, 0xC8);  // Power
  }

  met = AmlAddMethod (scop, "_CST", 0);
  AmlAddReturnName (met, "CST_");
  //met = AmlAddMethod (scop, "ACST", 0);
  //ret = AmlAddReturnName (met, "CST_");

  // Aliases
  for (i = 1; i < Number; i++) {
    AsciiSPrint (name2, 31, "%a%4a",  acpi_cpu_score, acpi_cpu_name[i]);

    scop = AmlAddScope (root, name2);
    met = AmlAddMethod (scop, "_CST", 0);
    AmlAddReturnName (met, name1);
    //met = AmlAddMethod (scop, "ACST", 0);
    //ret = AmlAddReturnName (met, name1);
  }

  AmlCalculateSize (root);

  ssdt = (SSDT_TABLE *)AllocateZeroPool (root->Size);

  AmlWriteNode (root, (VOID *)ssdt, 0);

  ssdt->Length = root->Size;
  ssdt->Checksum = 0;
  ssdt->Checksum = (UINT8)(256 - Checksum8 ((VOID *)ssdt, ssdt->Length));

  AmlDestroyNode (root);

  //dumpPhysAddr ("C-States SSDT content: ", ssdt, ssdt->Length);

  MsgLog ("SSDT with CPU C-States generated successfully\n");

  return ssdt;
}
