/*
 cpu.c
 implementation for cpu

 Remade by Slice 2011 based on Apple's XNU sources
 Portion copyright from Chameleon project. Thanks to all who involved to it.
 */
/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_CPU
#define DEBUG_CPU -1
#endif
#else
#ifdef DEBUG_CPU
#undef DEBUG_CPU
#endif
#define DEBUG_CPU DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_CPU, __VA_ARGS__)

#define VIRTUAL 0
#if VIRTUAL == 1
#define AsmReadMsr64(x) 0ULL
#define AsmWriteMsr64(m, x)
#endif

UINT8             gDefaultType;
CPU_STRUCTURE     gCPUStructure;
UINT64            TurboMsr;
BOOLEAN           NeedPMfix = FALSE;

//this must not be defined at LegacyBios calls
#define EAX 0
#define EBX 1
#define ECX 2
#define EDX 3

VOID
DoCpuid (
  UINT32  Selector,
  UINT32  *Data
) {
  AsmCpuid (Selector, Data, Data + 1, Data + 2, Data + 3);
}

//
// Should be used after PrepatchSmbios () but before users's config.plist reading
//
VOID
GetCPUProperties () {
  UINT32    Reg[4];

  UINT64    Msr = 0;
  CHAR8     Str[128];

  DbgHeader ("GetCPUProperties");

  //initial values
  gCPUStructure.MaxRatio = 10; //keep it as K * 10
  gCPUStructure.MinRatio = 10; //same
  gCPUStructure.SubDivider = 0;
  gSettings.CpuFreqMHz = 0;
  gCPUStructure.FSBFrequency = MultU64x32 (gCPUStructure.ExternalClock, kilo); //kHz -> Hz
  gCPUStructure.ProcessorInterconnectSpeed = 0;
  gCPUStructure.Mobile = FALSE; //not same as gMobile

  if (!gCPUStructure.CurrentSpeed) {
    gCPUStructure.CurrentSpeed = (UINT32)DivU64x32 (gCPUStructure.TSCCalibr + (Mega >> 1), Mega);
  }

  if (!gCPUStructure.MaxSpeed) {
    gCPUStructure.MaxSpeed = gCPUStructure.CurrentSpeed;
  }

  /* get CPUID Values */
  DoCpuid (0, gCPUStructure.CPUID[CPUID_0]);
  gCPUStructure.Vendor  = gCPUStructure.CPUID[CPUID_0][EBX];

  if (gCPUStructure.Vendor != CPU_VENDOR_INTEL) {
    DBG ("Unsupported CPU model\n");
  }

  /*
   * Get processor signature and decode
   * and bracket this with the approved procedure for reading the
   * the microcode version number a.k.a. signature a.k.a. BIOS ID
   */
  //if (gCPUStructure.Vendor == CPU_VENDOR_INTEL) {
    AsmWriteMsr64 (MSR_IA32_BIOS_SIGN_ID, 0);
  //}

  DoCpuid (1, gCPUStructure.CPUID[CPUID_1]);
  gCPUStructure.Signature = gCPUStructure.CPUID[CPUID_1][EAX];

  DBG ("CPU Vendor = %x Model=%x\n", gCPUStructure.Vendor, gCPUStructure.Signature);

  //if (gCPUStructure.Vendor == CPU_VENDOR_INTEL) {
    Msr = AsmReadMsr64 (MSR_IA32_BIOS_SIGN_ID);
    gCPUStructure.MicroCode = RShiftU64 (Msr, 32);
    /* Get "processor flag"; necessary for microcode update matching */
    gCPUStructure.ProcessorFlag = (RShiftU64 (AsmReadMsr64 (MSR_IA32_PLATFORM_ID), 50)) & 3;
  //}

  //  DoCpuid (2, gCPUStructure.CPUID[2]);

  DoCpuid (0x80000000, gCPUStructure.CPUID[CPUID_80]);
  if ((gCPUStructure.CPUID[CPUID_80][EAX] & 0x0000000f) >= 1) {
    DoCpuid (0x80000001, gCPUStructure.CPUID[CPUID_81]);
  }

  gCPUStructure.Stepping      = (UINT8)bitfield (gCPUStructure.Signature, 3, 0);
  gCPUStructure.Model         = (UINT8)bitfield (gCPUStructure.Signature, 7, 4);
  gCPUStructure.Family        = (UINT8)bitfield (gCPUStructure.Signature, 11, 8);
  gCPUStructure.Type          = (UINT8)bitfield (gCPUStructure.Signature, 13, 12);
  gCPUStructure.Extmodel      = (UINT8)bitfield (gCPUStructure.Signature, 19, 16);
  gCPUStructure.Extfamily     = (UINT8)bitfield (gCPUStructure.Signature, 27, 20);
  gCPUStructure.Features      = quad (gCPUStructure.CPUID[CPUID_1][ECX], gCPUStructure.CPUID[CPUID_1][EDX]);
  gCPUStructure.ExtFeatures   = quad (gCPUStructure.CPUID[CPUID_81][ECX], gCPUStructure.CPUID[CPUID_81][EDX]);

  /* Pack CPU Family and Model */
  if (gCPUStructure.Family == 0x0f) {
    gCPUStructure.Family += gCPUStructure.Extfamily;
  }

  gCPUStructure.Model += (gCPUStructure.Extmodel << 4);

  //Calculate Nr of Cores
  gCPUStructure.LogicalPerPackage = 1;
  if (gCPUStructure.Features & CPUID_FEATURE_HTT) {
    gCPUStructure.LogicalPerPackage = (UINT32)bitfield (gCPUStructure.CPUID[CPUID_1][EBX], 23, 16); //Atom330 = 4
  }

  //if (gCPUStructure.Vendor == CPU_VENDOR_INTEL) {
    DoCpuid (4, gCPUStructure.CPUID[CPUID_4]);
    if (gCPUStructure.CPUID[CPUID_4][EAX]) {
      gCPUStructure.CoresPerPackage =  (UINT32)bitfield (gCPUStructure.CPUID[CPUID_4][EAX], 31, 26) + 1; //Atom330 = 2
      DBG ("CPUID_4_eax=%x\n", gCPUStructure.CPUID[CPUID_4][EAX]);
      DoCpuid (4, gCPUStructure.CPUID[CPUID_4]);
      DBG ("CPUID_4_eax=%x\n", gCPUStructure.CPUID[CPUID_4][EAX]);
      DoCpuid (4, gCPUStructure.CPUID[CPUID_4]);
      DBG ("CPUID_4_eax=%x\n", gCPUStructure.CPUID[CPUID_4][EAX]);
    } else {
      gCPUStructure.CoresPerPackage = (UINT32)bitfield (gCPUStructure.CPUID[CPUID_1][EBX], 18, 16);
      DBG ("got cores from CPUID_1 = %d\n", gCPUStructure.CoresPerPackage);
    }
  //}

  CONSTRAIN_MIN (gCPUStructure.CoresPerPackage, 1);

  /* Fold in the Invariant TSC feature bit, if present */
  if (gCPUStructure.CPUID[CPUID_80][EAX] >= 0x80000007) {
    DoCpuid (0x80000007, gCPUStructure.CPUID[CPUID_87]);
    gCPUStructure.ExtFeatures |= gCPUStructure.CPUID[CPUID_87][EDX] & (UINT32)CPUID_EXTFEATURE_TSCI;
  }

  //if ((bit (9) & gCPUStructure.CPUID[CPUID_1][ECX]) != 0) {
  //  SSSE3 = TRUE;
  //}
  gCPUStructure.Turbo = FALSE;

  //if (gCPUStructure.Vendor == CPU_VENDOR_INTEL) {
    // Determine turbo boost support
    DoCpuid (6, gCPUStructure.CPUID[CPUID_6]);
    gCPUStructure.Turbo = ((gCPUStructure.CPUID[CPUID_6][EAX] & (1 << 1)) != 0);

    MsgLog ("CPU with turbo supported: %a\n", gCPUStructure.Turbo ? "Yes" : "No");

    gCPUStructure.Cores = 0;

    if (gCPUStructure.Model >= CPUID_MODEL_SANDYBRIDGE) {
      Msr = AsmReadMsr64 (MSR_CORE_THREAD_COUNT);  //0x35
      gCPUStructure.Cores   = (UINT8)bitfield ((UINT32)Msr, 31, 16);
      gCPUStructure.Threads = (UINT8)bitfield ((UINT32)Msr, 15,  0);
    }
  //}

  if (gCPUStructure.Cores == 0) {
    gCPUStructure.Cores   = (UINT8)(gCPUStructure.CoresPerPackage & 0xff);
    gCPUStructure.Threads = (UINT8)(gCPUStructure.LogicalPerPackage & 0xff);

    CONSTRAIN_MIN (gCPUStructure.Threads, gCPUStructure.Cores);
  }

  /* get BrandString (if supported) */
  if (gCPUStructure.CPUID[CPUID_80][EAX] >= 0x80000004) {
    CHAR8   *S;
    UINTN   Len;

    ZeroMem (Str, 128);

    /*
     * The BrandString 48 bytes (max), guaranteed to
     * be NULL terminated.
     */
    DoCpuid (0x80000002, Reg);
    CopyMem (&Str[0], (CHAR8 *)Reg,  16);
    DoCpuid (0x80000003, Reg);
    CopyMem (&Str[16], (CHAR8 *)Reg,  16);
    DoCpuid (0x80000004, Reg);
    CopyMem (&Str[32], (CHAR8 *)Reg,  16);

    for (S = Str; *S != '\0'; S++) {
      if (*S != ' ') break; //remove leading spaces
    }

    Len = ARRAY_SIZE (gCPUStructure.BrandString); //48
    AsciiStrnCpyS (gCPUStructure.BrandString, Len, S, Len);

    if (
      !AsciiStrnCmp (
        (CONST CHAR8 *)gCPUStructure.BrandString,
        (CONST CHAR8 *)CPU_STRING_UNKNOWN,
        AsciiTrimStrLen ((gCPUStructure.BrandString) + 1, Len)
      )
    ) {
      gCPUStructure.BrandString[0] = '\0';
    }

    gCPUStructure.BrandString[47] = '\0';
    MsgLog ("BrandString = %a\n", gCPUStructure.BrandString);
  }

  // workaround for Quad
  if (AsciiStrStr (gCPUStructure.BrandString, "Quad")) {
    gCPUStructure.Cores   = 4;
    gCPUStructure.Threads = 4;
  }

  // New for SkyLake 0x4E, 0x5E
  if (gCPUStructure.CPUID[CPUID_0][EAX] >= 0x15) {
    UINT32    Num, Denom;

    DoCpuid (0x15, gCPUStructure.CPUID[CPUID_15]);
    Num = gCPUStructure.CPUID[CPUID_15][EBX];
    Denom = gCPUStructure.CPUID[CPUID_15][EAX];

    DBG (" TSC/CCC Information Leaf:\n");
    DBG ("  numerator     : %d\n", Num);
    DBG ("  denominator   : %d\n", Denom);

    if (Num && Denom) {
      gCPUStructure.ARTFrequency = DivU64x32 (MultU64x32 (gCPUStructure.TSCCalibr, Denom), Num);
      DBG (" Calibrated ARTFrequency: %lld\n", gCPUStructure.ARTFrequency);
    }
  }

  if (
    //(gCPUStructure.Vendor == CPU_VENDOR_INTEL) &&
    //(
      ((gCPUStructure.Family == 0x06) && (gCPUStructure.Model >= 0x0c)) ||
      ((gCPUStructure.Family == 0x0f) && (gCPUStructure.Model >= 0x03))
    //)
  ) {
    if (gCPUStructure.Family == 0x06) {
      if (gCPUStructure.Model >= CPUID_MODEL_SANDYBRIDGE) {
        gCPUStructure.TSCFrequency = MultU64x32 (gCPUStructure.CurrentSpeed, Mega); //MHz -> Hz
        gCPUStructure.CPUFrequency = gCPUStructure.TSCFrequency;

        //----test C3 patch
        Msr = AsmReadMsr64 (MSR_PKG_CST_CONFIG_CONTROL); //0xE2
        DBG ("MSR 0xE2: %08x (before patch)\n", Msr);
        if (Msr & 0x8000) {
          DBG (" - is locked, PM patches will be turned on\n");
          NeedPMfix = TRUE;
        }

        //        AsmWriteMsr64 (MSR_PKG_CST_CONFIG_CONTROL, (Msr & 0x8000000ULL));
        //        Msr = AsmReadMsr64 (MSR_PKG_CST_CONFIG_CONTROL);
        //        MsgLog ("MSR 0xE2 after  patch %08x\n", Msr);
        Msr = AsmReadMsr64 (MSR_PMG_IO_CAPTURE_BASE);
        DBG ("MSR 0xE4: %08x\n", Msr);
        //------------
        Msr = AsmReadMsr64 (MSR_PLATFORM_INFO);       //0xCE
        DBG ("MSR 0xCE: %08x_%08x\n", (Msr>>32), Msr);
        gCPUStructure.MaxRatio = (UINT8)RShiftU64 (Msr, 8) & 0xff;
        gCPUStructure.MinRatio = (UINT8)MultU64x32 (RShiftU64 (Msr, 40) & 0xff, 10);
        Msr = AsmReadMsr64 (MSR_FLEX_RATIO);   //0x194

        if ((RShiftU64 (Msr, 16) & 0x01) != 0) {
          // bcc9 patch
          UINT8   FlexRatio = RShiftU64 (Msr, 8) & 0xff;

          DBG ("Non-usable FLEX_RATIO = %x\n", Msr);

          if (FlexRatio == 0) {
            AsmWriteMsr64 (MSR_FLEX_RATIO, (Msr & 0xFFFFFFFFFFFEFFFFULL));
            gBS->Stall (10);
            Msr = AsmReadMsr64 (MSR_FLEX_RATIO);
            MsgLog ("Corrected FLEX_RATIO = %x\n", Msr);
          }
          /*else {
           if (gCPUStructure.BusRatioMax > FlexRatio)
           gCPUStructure.BusRatioMax = (UINT8)FlexRatio;
           }*/
        }

        //if ((gCPUStructure.CPUID[CPUID_6][ECX] & (1 << 3)) != 0) {
        //  Msr = AsmReadMsr64 (IA32_ENERGY_PERF_BIAS); //0x1B0
        //  MsgLog ("MSR 0x1B0             %08x\n", Msr);
        //}

        if (gCPUStructure.MaxRatio) {
          gCPUStructure.FSBFrequency = DivU64x32 (gCPUStructure.TSCFrequency, gCPUStructure.MaxRatio);
        } else {
          gCPUStructure.FSBFrequency = 100000000ULL; //100 * Mega
        }

        Msr = AsmReadMsr64 (MSR_TURBO_RATIO_LIMIT);   //0x1AD
        gCPUStructure.Turbo1 = (UINT8)(RShiftU64 (Msr, 0) & 0xff);
        gCPUStructure.Turbo2 = (UINT8)(RShiftU64 (Msr, 8) & 0xff);
        gCPUStructure.Turbo3 = (UINT8)(RShiftU64 (Msr, 16) & 0xff);
        gCPUStructure.Turbo4 = (UINT8)(RShiftU64 (Msr, 24) & 0xff);

        if (gCPUStructure.Turbo4 == 0) {
          gCPUStructure.Turbo4 = gCPUStructure.Turbo1;

          if (gCPUStructure.Turbo4 == 0) {
            gCPUStructure.Turbo4 = (UINT16)gCPUStructure.MaxRatio;
          }
        }

        //Slice - we found that for some i5-2400 and i7-2600 MSR 1AD reports wrong turbo mult
        // another similar bug in i7-3820
        //MSR 000001AD  0000-0000-3B3B-3B3B - from AIDA64
        // so there is a workaround
        if ((gCPUStructure.Turbo4 == 0x3B) || (gCPUStructure.Turbo4 == 0x39)) {
          gCPUStructure.Turbo4 = (UINT16)gCPUStructure.MaxRatio + (gCPUStructure.Turbo ? 1 : 0);
        }

        gCPUStructure.MaxRatio *= 10;
        gCPUStructure.Turbo1 *= 10;
        gCPUStructure.Turbo2 *= 10;
        gCPUStructure.Turbo3 *= 10;
        gCPUStructure.Turbo4 *= 10;
      } else {
        gCPUStructure.TSCFrequency = MultU64x32 (gCPUStructure.CurrentSpeed, Mega); //MHz -> Hz
        gCPUStructure.CPUFrequency = gCPUStructure.TSCFrequency;
        gCPUStructure.MinRatio = 60;

        if (!gCPUStructure.FSBFrequency) {
          gCPUStructure.FSBFrequency = 100000000ULL; //100 * Mega
        }

        gCPUStructure.MaxRatio = (UINT32)(MultU64x32 (DivU64x64Remainder (gCPUStructure.TSCFrequency, gCPUStructure.FSBFrequency, NULL), 10));
        gCPUStructure.CPUFrequency = gCPUStructure.TSCFrequency;
      }
    }
  }
}

VOID
SyncCPUProperties () {
  UINT32    BusSpeed = 0; //units kHz
  UINT64    TmpFSB, ExternalClock = 0;

  // Check if QPI is used
  if (gSettings.QPI == 0xFFFF) {
    // Not used, quad-pumped FSB; multiply ExternalClock by 4
    ExternalClock = gCPUStructure.ExternalClock * 4;
  } else {
    // Used; keep the same value for ExternalClock
    ExternalClock = gCPUStructure.ExternalClock;
  }

  // DBG ("take FSB\n");
  TmpFSB = gCPUStructure.FSBFrequency;
  //  DBG ("divide by 1000\n");
  BusSpeed = (UINT32)DivU64x32 (TmpFSB, kilo); //Hz -> kHz
  DBG ("FSBFrequency=%dMHz DMIvalue=%dkHz\n", DivU64x32 (TmpFSB, Mega), gCPUStructure.ExternalClock);

  //now check if SMBIOS has ExternalClock = 4xBusSpeed
  if (
    (BusSpeed > 50 * kilo) &&
    ((/*gCPUStructure.*/ExternalClock > BusSpeed * 3) || (/*gCPUStructure.*/ExternalClock < 50 * kilo)) //khz
  ) {
    gCPUStructure.ExternalClock = BusSpeed;
  } else {
    TmpFSB = MultU64x32 (/*gCPUStructure.*/ExternalClock, kilo); //kHz -> Hz
    gCPUStructure.FSBFrequency = TmpFSB;
  }

  TmpFSB = gCPUStructure.FSBFrequency;
  DBG ("Corrected FSBFrequency=%dMHz\n", DivU64x32 (TmpFSB, Mega));

  gCPUStructure.ProcessorInterconnectSpeed = DivU64x32 (LShiftU64 (gCPUStructure.ExternalClock, 2), kilo); //kHz->MHz
  gCPUStructure.MaxSpeed = (UINT32)(DivU64x32 (MultU64x64 (gCPUStructure.FSBFrequency, gCPUStructure.MaxRatio), Mega * 10)); //kHz->MHz

  DBG ("Vendor/Model/Stepping: 0x%x/0x%x/0x%x\n", gCPUStructure.Vendor, gCPUStructure.Model, gCPUStructure.Stepping);
  DBG ("Family/ExtFamily: 0x%x/0x%x\n", gCPUStructure.Family, gCPUStructure.Extfamily);
  DBG ("MaxDiv/MinDiv: %d.%d/%d\n", gCPUStructure.MaxRatio/10, gCPUStructure.MaxRatio%10 , gCPUStructure.MinRatio/10);
  if (gCPUStructure.Turbo) {
    DBG ("Turbo: %d/%d/%d/%d\n", gCPUStructure.Turbo4/10, gCPUStructure.Turbo3/10, gCPUStructure.Turbo2/10, gCPUStructure.Turbo1/10);
  }
  DBG ("Features: 0x%08x\n", gCPUStructure.Features);

  MsgLog ("Threads: %d, Cores: %d, FSB: %d MHz, CPU: %d MHz, TSC: %d MHz, PIS: %d MHz\n",
    gCPUStructure.Threads,
    gCPUStructure.Cores,
    (INT32)(DivU64x32 (gCPUStructure.ExternalClock, kilo)),
    (INT32)(DivU64x32 (gCPUStructure.CPUFrequency, Mega)),
    (INT32)(DivU64x32 (gCPUStructure.TSCFrequency, Mega)),
    (INT32)gCPUStructure.ProcessorInterconnectSpeed
  );

  MsgLog ("Calibrated TSC frequency = %ld = %ldMHz\n", gCPUStructure.TSCCalibr, DivU64x32 (gCPUStructure.TSCCalibr, Mega));

  if (gCPUStructure.TSCCalibr > 200000000ULL) {  //200MHz
    gCPUStructure.TSCFrequency = gCPUStructure.TSCCalibr;
  }

  gCPUStructure.CPUFrequency  = gCPUStructure.TSCFrequency;
  gCPUStructure.FSBFrequency  = DivU64x32 (
                                  MultU64x32 (gCPUStructure.CPUFrequency, 10),
                                  (gCPUStructure.MaxRatio == 0) ? 1 : gCPUStructure.MaxRatio
                                );
  gCPUStructure.ExternalClock = (UINT32)DivU64x32 (gCPUStructure.FSBFrequency, kilo);
  gCPUStructure.MaxSpeed      = (UINT32)DivU64x32 (gCPUStructure.TSCFrequency + (Mega >> 1), Mega);

  // Check if QPI is used
  if (gSettings.QPI == 0xFFFF) {
    // Not used, quad-pumped FSB; divide ExternalClock by 4
    gCPUStructure.ExternalClock = gCPUStructure.ExternalClock / 4;
  }
}

VOID
SetCPUProperties () {
  /*
  UINT64    Msr = 0;

  if ((gCPUStructure.CPUID[CPUID_6][ECX] & (1 << 3)) != 0) {
    if (gSettings.SavingMode != 0xFF) {
      Msr = gSettings.SavingMode;
      AsmWriteMsr64 (IA32_ENERGY_PERF_BIAS, Msr);
      Msr = AsmReadMsr64 (IA32_ENERGY_PERF_BIAS); //0x1B0
      MsgLog ("MSR 0x1B0   set to        %08x\n", Msr);
    }
  }
  */
}

UINT16
GetStandardCpuType () {
  /*
  if (gCPUStructure.Threads >= 4) {
    return 0x402;   // Quad-Core Xeon
  } else if (gCPUStructure.Threads == 1) {
    return 0x201;   // Core Solo
  }

  return 0x301;   // Core 2 Duo
  */
  return (0x900 + BRIDGETYPE_SANDY_BRIDGE); // i3
}

UINT16
GetAdvancedCpuType () {
  if (
    /*(gCPUStructure.Vendor == CPU_VENDOR_INTEL) &&
    (*/ gCPUStructure.Family == 0x06 /*)*/
  ) {
    UINT8    CoreBridgeType = 0;

    switch (gCPUStructure.Model) {
      case CPUID_MODEL_KABYLAKE:
      case CPUID_MODEL_KABYLAKE_DT:
        CoreBridgeType = BRIDGETYPE_KABYLAKE;
        break;

      case CPUID_MODEL_SKYLAKE:
      case CPUID_MODEL_SKYLAKE_DT:
        CoreBridgeType = BRIDGETYPE_SKYLAKE;
        break;

      case CPUID_MODEL_BROADWELL:
      case CPUID_MODEL_BRYSTALWELL:
        CoreBridgeType = BRIDGETYPE_BROADWELL;
        break;

      case CPUID_MODEL_CRYSTALWELL:
      case CPUID_MODEL_HASWELL:
      case CPUID_MODEL_HASWELL_EP:
      case CPUID_MODEL_HASWELL_ULT:
        CoreBridgeType = BRIDGETYPE_HASWELL;
        break;

      case CPUID_MODEL_IVYBRIDGE:
      case CPUID_MODEL_IVYBRIDGE_EP:
        CoreBridgeType = BRIDGETYPE_IVY_BRIDGE;
        break;

      case CPUID_MODEL_SANDYBRIDGE:
      //case CPUID_MODEL_JAKETOWN:
        CoreBridgeType = BRIDGETYPE_SANDY_BRIDGE;
        break;
    }

    if (AsciiStrStr (gCPUStructure.BrandString, "Core(TM) i7")) {
      return (0x700 + CoreBridgeType);
    }
    else if (AsciiStrStr (gCPUStructure.BrandString, "Core(TM) i5")) {
      return (0x600 + CoreBridgeType);
    }
    else if (AsciiStrStr (gCPUStructure.BrandString, "Core(TM) i3")) {
      return (0x900 + CoreBridgeType);
    }
    else if (AsciiStrStr (gCPUStructure.BrandString, "Xeon(R)")) {
      return (0xA00 + CoreBridgeType);
    }
  }

  return GetStandardCpuType ();
}

/*
MACHINE_TYPES
GetModelByBrandString (
  MACHINE_TYPES   i3,
  MACHINE_TYPES   i5,
  MACHINE_TYPES   i7,
  MACHINE_TYPES   Other // Xeon?
) {
  MACHINE_TYPES   DefaultType = MinMachineType + 1;

  if (AsciiStrStr (gCPUStructure.BrandString, "i3")) {
    return i3;
  } else if (AsciiStrStr (gCPUStructure.BrandString, "i5")) {
    return i5;
  } else if (AsciiStrStr (gCPUStructure.BrandString, "i7")) {
    return i7;
  }

  return Other ? Other : DefaultType;
}
*/

MACHINE_TYPES
GetDefaultModel () {
  MACHINE_TYPES   DefaultType = MinMachineType + 1;

  //if (gCPUStructure.Vendor != CPU_VENDOR_INTEL) {
  //  return DefaultType;
  //}

  if (gMobile) {
    switch (gCPUStructure.Model) {
      case CPUID_MODEL_KABYLAKE:
      case CPUID_MODEL_KABYLAKE_DT:
        DefaultType = MacBookPro143;
        break;

      case CPUID_MODEL_SKYLAKE:
      case CPUID_MODEL_SKYLAKE_DT:
        DefaultType = MacBookPro133;
        break;

      case CPUID_MODEL_BROADWELL:
      case CPUID_MODEL_BRYSTALWELL:
        DefaultType = MacBookPro121;
        break;

      case CPUID_MODEL_CRYSTALWELL:
      case CPUID_MODEL_HASWELL:
      case CPUID_MODEL_HASWELL_EP:
      case CPUID_MODEL_HASWELL_ULT:
        DefaultType = MacBookPro115;
        break;

      case CPUID_MODEL_IVYBRIDGE:
      case CPUID_MODEL_IVYBRIDGE_EP:
        DefaultType = MacBookPro102;
        break;

      case CPUID_MODEL_SANDYBRIDGE:
      //case CPUID_MODEL_JAKETOWN:
      default:
        DefaultType = MacBookPro83;
        break;
    }
  } else {
    switch (gCPUStructure.Model) {
      case CPUID_MODEL_KABYLAKE:
      case CPUID_MODEL_KABYLAKE_DT:
        DefaultType = iMac183;
        break;

      case CPUID_MODEL_SKYLAKE:
      case CPUID_MODEL_SKYLAKE_DT:
        DefaultType = iMac171;
        break;

      case CPUID_MODEL_BROADWELL:
      case CPUID_MODEL_BRYSTALWELL:
        DefaultType = iMac162;
        break;

      case CPUID_MODEL_CRYSTALWELL:
      case CPUID_MODEL_HASWELL:
      case CPUID_MODEL_HASWELL_EP:
      case CPUID_MODEL_HASWELL_ULT:
        DefaultType = MacMini71;
        break;

      case CPUID_MODEL_IVYBRIDGE:
      case CPUID_MODEL_IVYBRIDGE_EP:
        DefaultType = MacMini62;
        break;

      case CPUID_MODEL_SANDYBRIDGE:
      //case CPUID_MODEL_JAKETOWN:
      default:
        DefaultType = MacMini53;
        break;
    }
  }

  return DefaultType;
}
