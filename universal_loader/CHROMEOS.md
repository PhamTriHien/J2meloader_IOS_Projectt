# ChromeOS Android build

J5Hienloader runs through the Android runtime on compatible Chromebooks.
This is an Android APK, not a browser extension or a ChromeOS native binary.
ChromeOS devices without Android app support are not covered.

- Touchscreen hardware is optional; physical keyboard and pointer input use the
  existing Flutter input handlers.
- The activity explicitly supports resizing and handles size, density, keyboard
  and orientation configuration changes.
- Intel/AMD devices use the x86_64 APK; ARM64 devices use the arm64 APK.
- Background execution uses the Android foreground service. Chromebook sleep,
  Android runtime shutdown and network loss can still interrupt execution.
- APKs currently use the project's debug signing key, including release builds.

Build from `universal_loader/ui_app`:

```powershell
flutter build apk --release --target-platform android-arm64,android-x64 --split-per-abi --no-tree-shake-icons
```

The release workflow publishes named ChromeOS APKs alongside Android artifacts.
Install using the Chromebook's supported Android installation or ADB procedure.

Before claiming device support, test on a physical Chromebook: import the bundled
and an external JAR, keyboard press/release, mouse drag, maximize/restore, repeated
window resizing, multiple sessions, switching apps, sleep/resume, and changing
networks. A successful APK build is not a ChromeOS runtime test.
