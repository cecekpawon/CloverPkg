**KernextPatcher** is an Darwin kernel & extensions patcher (part of my [Clover forked](https://github.com/cecekpawon/CloverPkg)) UEFI driver based on [Clover](https://sourceforge.net/p/cloverefiboot/) Memfix by dmazar (credits goes to him, rEFIt, Clover devs).

**Prerequisites & Tasks:**

- [x] Compile with `-D BUILD_KERNEXTPATCHER=1`.
- [x] Need to be loaded before boot manager in order to successfully hook ExitBootServices event.
- [x] Read patches property in `\EFI\KernextPatcher.plist`.
- [x] Create `\EFI\KernextPatcherLog.txt` log-file with supplied `-KernextPatcherLog` boot-args / `Preferences`->`SaveLogToFile`=`TRUE`.

**Known bugs:**

- [x] Sometimes, it will incorrectly parse BootArgs & set RelocBase.

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
    <key>SaveLogToFile</key>
    <true/>
  </dict>
</dict>
</plist>
```