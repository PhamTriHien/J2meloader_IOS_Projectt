# J2ME-Loader for iOS (Native Port)

A native J2ME (Java ME / MIDP 2.0 / CLDC 1.1) emulator for iOS and iPadOS. Focus: core game-running features (JVM + Canvas + audio + saves).

[![Build iOS IPA](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions/workflows/build-ios-ipa.yml/badge.svg)](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions/workflows/build-ios-ipa.yml)

---

## Core chạy game

* **JVM**: bytecode int/long/float/double, exception try/catch, `System.arraycopy` đủ kiểu, `Math` double, cooperative thread-stop (hết crash khi thoát/khởi động lại game).
* **LCDUI 2D**: Canvas/GameCanvas, Graphics, Image/PNG, Sprite 8 hướng + va chạm pixel, TiledLayer/LayerManager, Form/List/TextBox/Alert + CommandListener, font Unicode/Tiếng Việt (CoreText) + ASCII 8x8 retro.
* **3D cơ bản**: M3G parse mesh/texture thật, Micro3D MBAC parse thật, keyframe/animation cơ bản. File lạ không parse được sẽ màn đen (không hình giả).
* **Âm thanh**: Sonivox EAS (MIDI/iMelody) + WAV/MP3/AMR, tone Nokia/Samsung/Siemens, VolumeControl.
* **Lưu game**: RMS persistent trong sandbox.
* **Mạng cơ bản**: HTTP(S) tải thật, TCP socket + Datagram, FileConnection đọc JAR/local. Không hỗ trợ: BT classic nhiều người chơi, SMS nhà mạng ngầm.
* **Nhập game**: `.jar`/`.jad` từ Files, iCloud Drive, AirDrop, Safari (kèm `.jad` cùng tên để lấy metadata).
* **Điều khiển**: bàn phím Nokia T9 + D-Pad/OK/LSK/RSK, haptic, remap tay cầm MFi/Xbox/PlayStation/Switch, cảm ứng, chụp màn hình, tăng tốc 1x/2x/4x, tạm dừng.
* **Hiển thị**: Metal 60 FPS, shader Nearest/Bilinear/CRT/Nokia LCD, presets Nokia N73/6600, Sony Ericsson K750i, Motorola V3, Nokia 5800.

---

## Tương thích thực tế

* Game 2D Canvas/Sprite/RMS/lưu điểm: chơi được.
* Game 3D/animation phức tạp, online realtime, API hãng lạ: có thể lỗi/thiếu. Báo tên game + hiện tượng để sửa tiếp.

---

## Build & tải IPA

### Cách 1: Cloud Build (khuyên dùng)
1. Tab **Actions** → **"Build iOS IPA"** → **Run workflow** (hoặc push lên `main` tự build).
2. Tải artifact **`J2MELoader-iOS-ipa`**.

### Cách 2: Xcode local (macOS 14 + Xcode 15.4)
```bash
cd ios/J2MELoader-iOS
open J2MELoader-iOS.xcodeproj
```
Chọn Team ở Signing & Capabilities → Cmd+R.

### Cách 3: Script (macOS)
```bash
cd ios/J2MELoader-iOS
chmod +x build_ipa.sh
./build_ipa.sh
```

---

## Cài lên máy thật
* **TrollStore** (iOS 14.0–17.0, khuyên dùng): không revoke 7 ngày.
* **Sideloadly / AltStore / SideStore**: Apple ID free (revoke 7 ngày).