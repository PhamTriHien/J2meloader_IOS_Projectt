# J2ME-Loader for iOS (Native Full Prototype Port)

A high-performance, native J2ME (Java ME / MIDP 2.0 / CLDC 1.1) emulator for iOS and iPadOS.

[![Build iOS IPA](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions/workflows/build-ios-ipa.yml/badge.svg)](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions/workflows/build-ios-ipa.yml)

---

## 🌟 Key Features

* **Native SwiftUI & Metal Engine**: 60/120 FPS hardware-accelerated rendering with custom Shaders (Nearest Neighbor, Bilinear, CRT Scanlines, Nokia LCD Subpixel Grid).
* **Sonivox EAS MIDI Synthesizer**: Original EAS MIDI sound engine providing authentic retro phone music and sound effects.
* **Full 2D & 3D Graphics**:
  - Full LCDUI Canvas, GameCanvas, Sprite (8 transform directions), and Pixel-Perfect Collision Detection.
  - M3G (JSR 184) 3D Graphics pipeline (Z-Buffer, Gouraud Shading, Lighting, Textures).
  - MascotCapsule Micro3D v3 3D Engine.
* **Direct ROM Importer**: Open `.jar` and `.jad` games from Files, iCloud Drive, AirDrop, or Safari.
* **Retro Virtual Keypads**: Authentic Nokia T9 keypad with D-Pad, Softkeys, and Haptic Feedback.
* **Key Mapper**: Remap any button to external Bluetooth controllers (MFi, Xbox, PlayStation DualSense, Nintendo Switch).
* **Shader Tune**: Real-time adjustment for CRT scanlines, LCD subpixel grid, and ghosting persistence.
* **Device Profiles**: Built-in presets for Nokia N73, Nokia 6600, Sony Ericsson K750i, Motorola V3, and Nokia 5800 Touch.

---

## 🚀 Building & Downloading IPA

### Method 1: Automatic Cloud Build (GitHub Actions)
1. Go to the **[Actions](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions)** tab.
2. Select **"Build iOS IPA"** and click **"Run workflow"**.
3. Download the generated **`J2MELoader-iOS-ipa`** artifact.

### Method 2: Local Build via Xcode
```bash
cd ios/J2MELoader-iOS
open J2MELoader-iOS.xcodeproj
```

### Method 3: Local Script via Terminal (macOS)
```bash
cd ios/J2MELoader-iOS
chmod +x build_ipa.sh
./build_ipa.sh
```

---

## 📲 Installation on iOS (Sideloading)
* **TrollStore** (Recommended for iOS 14.0 - 17.0): Install without 7-day revokes.
* **Sideloadly / AltStore / SideStore**: Sideload with free Apple ID.