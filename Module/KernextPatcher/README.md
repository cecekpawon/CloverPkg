**KernextPatcher** is an Darwin kernel & extensions patcher (part of my [Clover forked](https://github.com/cecekpawon/CloverPkg)) UEFI driver based on [Clover](https://sourceforge.net/p/cloverefiboot/) Memfix by dmazar (credits goes to him, rEFIt, Clover devs).

## Prerequisites & Tasks:

- [x] Compile with `-D BUILD_KERNEXTPATCHER=1`.
- [x] ~~Need to be loaded before boot manager in order to successfully hook ExitBootServices event.~~
- [x] Read patches property in `\EFI\KernextPatcher.plist`.

## Known bugs:

- [x] ~~Sometimes, it will incorrectly parse BootArgs & set RelocBase.~~

## Installation:

### \#1. Install driver with BCFG

**Start shell and type following commands (as needed):**

```
# list existing drivers

bcfg driver dump

Option: 00. Variable: Driver0000
  Desc    - KernextPatcher
  DevPath - PciRoot(0x0)/Pci(0x1d,0x0)/USB(0x1,0x0)/USB(0x5,0x0)/HD(1,MBR,0x00045ef5,0x1000,0x64000)/\EFI\KernextPatcher.efi
  Optional- N
Option: 01. Variable: Driver0001
  Desc    - Ozmosis
  DevPath - PciRoot(0x0)/Pci(0x1d,0x0)/USB(0x1,0x0)/USB(0x5,0x0)/HD(1,MBR,0x00045ef5,0x1000,0x64000)/\EFI\Ozmosis.efi
  Optional- N

# reorder drivers

bcfg driver mv 0 1

# remove existing drivers

bcfg driver rm 0
...

# register drivers

fs0:
cd EFI
bcfg driver add 0 KernextPatcher.efi "KernextPatcher"
bcfg driver add 1 Ozmosis.efi "Ozmosis"
```
### \#2. Install driver into Firmware

Integrate `99665243-5AED-4D57-92AF-8C785FBC7558.ffs` into your UEFI firmware using [UEFITool](https://github.com/LongSoft/UEFITool) or any other suitable software.

**Drivers list from Shell**
```
108 00000000 ? N N   0   0 Ozmosis Platform Driver      MemoryMapped(0xb,0xcdc77000,0xce0c6fff)/FvFile(aae65279-0761-41d1-ba13-4a3c1383603f)
...
166 0000000A ? N N   0   0 KernextPatcher (for Darwin)  MemoryMapped(0xb,0xcdc77000,0xce0c6fff)/FvFile(99665243-5aed-4d57-92af-8c785fbc7558)
```

## Directory structure:
```
.
└── EFI
    ├── KernextPatcher.efi
    ├── KernextPatcher.plist
    ├── KernextPatcherLog.txt
    └── Ozmosis.efi
```

## Preferences:

| Boot argument | Key | Default | Description |
| --- | --- | --- | --- |
| `-KernextPatcherDbg` | Debug | FALSE | Advanced logging |
| `-KernextPatcherLog` | SaveLogToFile | TRUE | Save log to `\EFI\KernextPatcherLog.txt` |
| `-KernextPatcherOff` | Off | FALSE | Disable patcher |
|| KernelPatchesWholePrelinked | FALSE | KernelToPatch the whole prelinked / just kernel |

**Sample** of `\EFI\KernextPatcher.plist` ([KextsToPatch & KernelToPatch references](https://github.com/cecekpawon/CloverPkg/wiki/Config)):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>KernextPatches</key>
  <dict>
    <key>KextsToPatch</key>
    <array>
      <dict>
        <key>Comment</key>
        <string>ALC892 - 1</string>
        <key>Find</key>
        <data>
        ixnUEQ==
        </data>
        <key>Name</key>
        <string>com.apple.driver.AppleHDA</string>
        <key>Replace</key>
        <data>
        kgjsEA==
        </data>
        <key>MatchOS</key>
        <string>10.9-10.12</string>
      </dict>
      <dict>
        <key>Comment</key>
        <string>ALC892 - 2</string>
        <key>Find</key>
        <data>
        ihnUEQ==
        </data>
        <key>Name</key>
        <string>com.apple.driver.AppleHDA</string>
        <key>Replace</key>
        <data>
        AAAAAA==
        </data>
        <key>MatchOS</key>
        <string>10.12</string>
      </dict>
    </array>
    <key>KernelToPatch</key>
    <array>
      <dict>
        <key>Comment</key>
        <string>startupExt</string>
        <key>Disabled</key>
        <true/>
        <key>Find</key>
        <data>
        voQAAQAxwOibEdH/
        </data>
        <key>MatchOS</key>
        <string>10.12</string>
        <key>Replace</key>
        <data>
        kJCQkEiJ3+gbBQAA
        </data>
      </dict>
    </array>
  </dict>
  <key>Preferences</key>
  <dict>
    <key>Debug</key>
    <false/>
    <key>SaveLogToFile</key>
    <true/>
    <key>Off</key>
    <false/>
    <key>KernelPatchesWholePrelinked</key>
    <false/>
  </dict>
</dict>
</plist>
```

## Download:

Precompiled binaries always can be found [here](https://1drv.ms/f/s!AjxLshYT0HDug0JUmVUbr1B-r0rh).