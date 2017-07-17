> _First of all, Im just PHP coder, with poor knowledge of C. Thats why I cannot accept any issues on this repo regarding all modificatons that been made, so feel free to add yours on your own. Not for production / daily usage. This project is intended for general use and no warranty is implied for suitability to any given task. **I hold no responsibility for your setup or any damage done while using/installing/modifying current sources / supplied binaries**._

[![](../../wiki/assets/main-1.gif)](../../wiki)

I create this fork (start on mid 2016 based on revision [3884](https://sourceforge.net/p/cloverefiboot/code/3884/tree/)) with the following changes:

### Goals:

The main goals is to bring some of outstanding Ozmosis features into Clover. Strip down all legacy support including all deprecated code. Let user to inject their own properties from config without guessing it as much as possible to prevent lack of updates.

### Requirements:

- [x] 64bit UEFI.
- [x] Native NVRAM support.
- [x] Darwin 13 (OS 10.9 / Mavericks) & up.

### Tasks:

- [x] EDK2 patches free.
- [x] Simplify all internal patches & tweaks.
- [x] Simplify GUI & tweaks (including useful GUI combo keys, templating entries title, and splash screen).
- [x] Embeddable FSInject & Aptiofix module.
- [x] Porting plist lib, taken from last BootX based on previous code.
- [x] Included LZVN en/decoder port & Base64Encode.
- [x] Patcher with Wildcard support.
- [x] More configurable advanced logs.

### Limitations / Cut-OFF:

- [x] No GUI mouse support.
- [x] Remove Clang compiler support.
- [x] Detect major Linux distros only.
- [x] Detect .png icons only.
- [x] No 'OEM' setting folder support.
- [x] Remove huge built-in injection properties.
- [x] No FileVault support.
- [x] No AMD CPU support.

### Migrations:

Check [wiki](../../wiki) for directory, property, format changes.

### Tested:

With GCC49, GCC5, XCODE5, ~~XCLANG~~ & VS2015x86 toolchain, on Darwin & Windows platform.

### Thanks to:

[Clover](https://sourceforge.net/p/cloverefiboot/) and all projects below for lines of code & ideas:

[EDK2](https://github.com/tianocore/edk2) / [rEFInd](https://sourceforge.net/projects/refind/) / [Bareboot](https://github.com/SunnyKi/bareBoot) / [Revoboot](https://github.com/Piker-Alpha/RevoBoot) / [KernelPatcher](https://public.xzenue.com/diffusion/K/repository/master/) / [Chameleon](http://forge.voodooprojects.org/p/chameleon/) / Ozmosis / [CupertinoNet](https://github.com/CupertinoNet) / [UEFI-Bootkit](https://github.com/dude719/UEFI-Bootkit).


Enjoy, [wiki](../../wiki) to follow!
