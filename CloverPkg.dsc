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
  BUILD_TARGETS                  = RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = CloverPkg/CloverPkg.fdf

[LibraryClasses]
  #
  # Entry point
  #
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  #
  # Basic
  #
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  #
  # UEFI & PI
  #
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  !ifdef USB_SUPPORT
    HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
    ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
    UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
    UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  !endif
  #
  # Generic Modules
  #
  NetLib|MdeModulePkg/Library/DxeNetLib/DxeNetLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  #
  # Misc
  #
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  #
  # Our libs
  #
  CommonLib|CloverPkg/Module/CommonLib/CommonLib.inf
  DeviceTreeLib|CloverPkg/Module/DeviceTreeLib/DeviceTreeLib.inf
  MemLogLib|CloverPkg/Module/MemLogLibDefault/MemLogLibDefault.inf

[Components.X64]
  CloverPkg/Application/Clover/Clover.inf {
    <LibraryClasses>
      !ifdef EMBED_FSINJECT
        NULL|CloverPkg/Module/FSInject/FSInject_embed.inf
      !endif
      !ifdef EMBED_APTIOFIX
        NULL|CloverPkg/Module/OsxAptioFixDrv/OsxAptioFixDrv_embed.inf
      !endif
  }

  !ifdef BUILD_KERNEXTPATCHER
    CloverPkg/Module/KernextPatcher/KernextPatcher.inf
  !endif

  !ifndef EMBED_FSINJECT
    CloverPkg/Module/FSInject/FSInject.inf
  !endif

  !ifndef EMBED_APTIOFIX
    CloverPkg/Module/OsxAptioFixDrv/OsxAptioFixDrv.inf
  !endif

  #CloverPkg/Application/bdmesg_efi/bdmesg.inf
  #CloverPkg/Application/Dumpprot/Dumpprot.inf

  !ifdef USB_SUPPORT
    MdeModulePkg/Bus/Pci/EhciDxe/EhciDxe.inf
    MdeModulePkg/Bus/Pci/UhciDxe/UhciDxe.inf
    MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf
    MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
    MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
    MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
  !endif

[BuildOptions]
  DEFINE BUILD_OPTIONS_GEN = -D DISABLE_NEW_DEPRECATED_INTERFACES $(BUILD_OPTIONS)

  MSFT:*_*_*_CC_FLAGS  = /D MDEPKG_NDEBUG $(BUILD_OPTIONS_GEN) # /FAcs /GL-
  XCODE:*_*_*_CC_FLAGS = -D MDEPKG_NDEBUG $(BUILD_OPTIONS_GEN)
  GCC:*_*_*_CC_FLAGS   = -D MDEPKG_NDEBUG $(BUILD_OPTIONS_GEN)
  *_*_*_MTOC_PATH      = $(TOOLCHAIN_BIN)/mtoc
