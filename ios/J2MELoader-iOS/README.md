# J2ME-Loader for iOS (Native Port)

A high-performance, native J2ME (Java ME / MIDP 2.0 / CLDC 1.1) emulator ported for iOS, iPadOS, and Apple Silicon.

---

## 🌟 Key Features

* **Native SwiftUI & MetalKit UI**: Fluid 60/120 FPS rendering with hardware-accelerated shaders (Pixel-art Nearest Neighbor, Bilinear, CRT Scanlines, and Nokia LCD Subpixel Grid).
* **Direct ROM Importer**: Import `.jar` and `.jad` games directly from iOS Files app, iCloud Drive, Safari downloads, or AirDrop.
* **Customizable Virtual Keypads**:
  - Classic Retro Phone (Soft 1, Soft 2, D-Pad, 0-9, \*, #, Call, End).
  - Modern D-Pad + Action buttons.
  - Transparent overlay mode.
  - Direct touchscreen canvas support.
* **Haptic Touch Feedback**: Realistic tactile button clicks via `UIImpactFeedbackGenerator`.
* **Audio & MIDI Synthesizer**: Native CoreAudio / AVAudioEngine integration with MIDI playback and tone generation.
* **RMS (Record Management System)**: Persistent save states and high scores stored securely in the app sandbox.
* **Aspect Ratios & Resolutions**: Support for 128x128, 128x160, 176x208, 176x220, 240x320 (QVGA), 320x240 (Landscape), 360x640, and custom dimensions.

---

## 🏗️ Architecture Overview

```
ios/J2MELoader-iOS/
├── J2MELoader-iOS.xcodeproj/    # Xcode project configuration
└── J2MELoader-iOS/
    ├── App/                     # SwiftUI App Lifecycle (J2MELoaderApp.swift)
    ├── Models/                  # Data Models (GameItem, EmulatorConfig, KeyMapping, GameManager)
    ├── Views/                   # UI Screens (LibraryView, GameScreenView, VirtualKeypadView, SettingsView, MetalView)
    ├── Bridge/                  # Objective-C++ Bridges (J2MEBridge, AudioBridge, Bridging Header)
    ├── Core/                    # C++ J2ME Engine (jar_loader, lcdui_display, rms_storage, jvm_interpreter)
    ├── Shaders/                 # Metal Vertex & Fragment Shaders (Shaders.metal)
    └── Resources/               # Info.plist & Assets.xcassets
```

---

## 🚀 Building & Running on macOS / Xcode

### Prerequisites
* macOS 12.0+ with Xcode 14.0+ installed.
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

# 1. Archive build
xcodebuild -project J2MELoader-iOS.xcodeproj \
           -scheme J2MELoader-iOS \
           -configuration Release \
           -archivePath build/J2MELoader-iOS.xcarchive \
           archive

# 2. Export to IPA
xcodebuild -exportArchive \
           -archivePath build/J2MELoader-iOS.xcarchive \
           -exportPath build/ipa \
           -exportOptionsPlist ExportOptions.plist
```

---

## 📲 Sideloading onto iPhone / iPad

* **AltStore / SideStore**: Open AltStore on your device, tap `+`, select `J2MELoader-iOS.ipa`.
* **Sideloadly**: Connect your iPhone to PC/Mac via USB, drag `J2MELoader-iOS.ipa` into Sideloadly and enter your Apple ID.
* **TrollStore** (Jailbreak / CoreTrust bypass): Install IPA directly without revokes.

---

## 🎮 How to Load Games

1. Open **J2ME Loader** on your iOS device.
2. Tap the **`+`** icon at the top-right corner.
3. Select any `.jar` or `.jad` file from your iCloud Drive / On My iPhone / Downloads folder.
4. The app automatically extracts metadata (Game Name, Vendor, Version, Icon) and adds it to your library.
5. Tap on the game card to start playing!