## @file
#  An EFI/Framework Emulation Platform with UEFI HII interface supported.
#
#  Developer's UEFI Emulation. DUET provides an EFI/UEFI IA32/X64 environment on legacy BIOS,
#  to help developing and debugging native EFI/UEFI drivers.
#
#  Copyright (c) 2010 - 2011, Intel Corporation. All rights reserved.<BR>
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution. The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
##
[Defines]
  PLATFORM_NAME                  = Clover
  PLATFORM_GUID                  = 199E24E0-0989-42aa-87F2-611A8C397E72
  PLATFORM_VERSION               = 0.92
  DSC_SPECIFICATION              = 0x00010006
  OUTPUT_DIRECTORY               = Build/Clover
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  #
  # Entry point
  #
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  #
  # Basic
  #
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  #PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  #PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  #
  # UEFI & PI
  #
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  #
  # Generic Modules
  #
  NetLib|MdeModulePkg/Library/DxeNetLib/DxeNetLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  #
  # Misc
  #
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  #
  # Our libs
  #
  MemLogLib|CloverPkg/Modules/MemLogLibDefault/MemLogLibDefault.inf
  !ifdef EMBED_FSINJECT
    NULL|CloverPkg/Modules/FSInject/FSInject_internal.inf
  !endif
  !ifdef EMBED_APTIOFIX
    NULL|CloverPkg/Modules/OsxAptioFixDrv/OsxAptioFixDrv_internal.inf
  !endif
  CommonLib|CloverPkg/Modules/CommonLib/CommonLib.inf
  DeviceTreeLib|CloverPkg/Modules/DeviceTreeLib/DeviceTreeLib.inf

[Components.X64]
  # Misc
  !ifndef EMBED_FSINJECT
    CloverPkg/Modules/FSInject/FSInject.inf
  !endif
  !ifndef EMBED_APTIOFIX
    CloverPkg/Modules/OsxAptioFixDrv/OsxAptioFixDrv.inf
  !endif
  #CloverPkg/Modules/OsxFatBinaryDrv/OsxFatBinaryDrv.inf
  #CloverPkg/Applications/bdmesg_efi/bdmesg.inf
  CloverPkg/Applications/Clover/Clover.inf

[BuildOptions]
  !ifdef DISABLE_LTO
    DEFINE DISABLE_LTO_FLAG = -fno-lto
  !endif

  DEFINE BUILD_OPTIONS=-DMDEPKG_NDEBUG

  MSFT:*_*_*_CC_FLAGS  = /FAcs /FR$(@R).SBR $(BUILD_OPTIONS) -Dinline=__inline $(MSFT_GRUB_FLAG)

  XCODE:*_*_*_CC_FLAGS = -fno-unwind-tables -Os $(BUILD_OPTIONS) -Wno-msvc-include $(DISABLE_LTO_FLAG)
  GCC:*_*_*_CC_FLAGS   = $(BUILD_OPTIONS)
  #-Wunused-but-set-variable
  # -Os -fno-omit-frame-pointer -maccumulate-outgoing-args
