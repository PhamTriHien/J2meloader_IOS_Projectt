# J2HienLoader for iOS & iPadOS (Native Port)

A high-performance, native J2ME (Java ME / MIDP 2.0 / CLDC 1.1) emulator ported for iOS, iPadOS, and Apple Silicon.

---

## 🌟 Key Features

* **Native SwiftUI & MetalKit UI**: Fluid 60/120 FPS rendering with hardware-accelerated shaders (Pixel-art Nearest Neighbor, Bilinear, CRT Scanlines, and Nokia LCD Subpixel Grid).
* **🔄 In-App Auto-Updater & Patch Manager**:
  - Automatic GitHub Releases API checker on application launch and resume.
  - Interactive Release Notes & Patch Details dialog (`UpdateModalView`).
  - Real-time in-app IPA download progress.
  - Direct 1-click install via **ESign** / **TrollStore** / **Share Sheet** or Safari direct link.
  - Manual check button in General Settings.
* **Direct ROM Importer**: Import `.jar` and `.jad` games directly from iOS Files app, iCloud Drive, Safari downloads, AirDrop, or USB.
* **🔋 24/7 Continuous Background Engine**:
  - Keep game loop and network TCP/UDP sockets alive when the device is locked or app is in the background.
  - Powered by `UIBackgroundModes` + Audio Keep-Alive Session.
* **Customizable Virtual Keypads**:
  - Classic Retro Phone (Soft 1, Soft 2, D-Pad, 0-9, \*, #, Call, End).
  - Modern D-Pad + Action buttons.
  - Transparent overlay mode.
  - Direct touchscreen canvas support.
* **Haptic Touch Feedback**: Realistic tactile button clicks via `UIImpactFeedbackGenerator`.
* **Audio**: Sonivox EAS (MIDI/iMelody/RTTTL) + WAV/MP3/AMR via `AVAudioPlayer`, tone Nokia/Samsung/Siemens.
* **RMS (Record Management System)**: Persistent save states and high scores stored securely in the app sandbox.
* **Aspect Ratios & Resolutions**: Support for 128x128, 128x160, 176x208, 176x220, 240x320 (QVGA), 320x240 (Landscape), 360x640, and custom dimensions.
* **Unicode Font**: Full Vietnamese and multilingual rendering via CoreText, ASCII retro 8x8 bitmap font.

---

## 🏗️ Architecture Overview

```
ios/J2MELoader-iOS/
├── J2MELoader-iOS.xcodeproj/    # Xcode project configuration
└── J2MELoader-iOS/
    ├── App/                     # SwiftUI App Lifecycle (J2MELoaderApp.swift)
    ├── Models/                  # Data Models (GameItem, EmulatorConfig, KeyMapping, GameManager, AppUpdateManager)
    ├── Views/                   # UI Screens (LibraryView, GameScreenView, VirtualKeypadView, SettingsView, MetalView, UpdateModalView)
    ├── Bridge/                  # Obj-C++ Bridges (J2MEBridge, AudioBridge, NativeExtBridge, NativeFontBridge)
    ├── Core/                    # C++ J2ME Engine (jvm_bytecode/interpreter, j2me_full_apis, m3g/micro3d, lcdui, rms)
    ├── Audio/                   # Sonivox EAS engine + eas_engine_bridge
    ├── Shaders/                 # Metal Vertex & Fragment Shaders (Shaders.metal)
    └── Resources/               # Info.plist & Assets.xcassets
```

---

## 🚀 Building & Running on macOS / Xcode

### Prerequisites
* macOS 14.0+ with Xcode 15.4+ installed.
* iOS Device running iOS 15.0+ or iOS Simulator.

### Steps to Build in Xcode:
1. Open the project in Xcode:
   ```bash
   open ios/J2MELoader-iOS/J2MELoader-iOS.xcodeproj
   ```
2. Select your development team in **Signing & Capabilities** (Personal Team or Apple Developer Account).
3. Select your target device (iPhone / iPad or Simulator).
4. Press **Cmd + R** to build and run.

### Steps to Build `.ipa` via Terminal:
```bash
cd ios/J2MELoader-iOS
chmod +x build_ipa.sh
./build_ipa.sh
```

---

## 📲 Sideloading onto iPhone / iPad

* **TrollStore** (iOS 14.0–17.0, Recommended): Permanent installation without certificate revokes.
* **ESign / Scarlet / Feather / GBox**: Direct on-device signing and installation.
* **Sideloadly / AltStore / SideStore**: Free Apple ID sideloading via PC/Mac.

---

## 🎮 How to Load Games

1. Open **J2HienLoader** on your iOS device.
2. Tap the **`+`** icon at the top-right corner.
3. Select any `.jar` or `.jad` file from your iCloud Drive / On My iPhone / Downloads folder.
4. The app automatically extracts metadata (Game Name, Vendor, Version, Icon) and adds it to your library.
5. Tap on the game card to start playing!