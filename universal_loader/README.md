# J5Hienloader - Architecture & Developer Guide

Tài liệu kỹ thuật chuyên sâu về cấu trúc Lõi Giả Lập C++20 (`core/`) và Tầng Giao Diện Đa Nền Tảng (`ui_app/`).

---

## 1. Cấu Trúc Thành Phần (Component Architecture)

```
universal_loader/
├── core/                                # Lõi thực thi C++20 độc lập nền tảng
│   ├── include/
│   │   ├── j2me_core.h                  # Hợp đồng C-ABI duy nhất (120+ native APIs)
│   │   ├── app_profile_config.h         # Model cấu hình Profile & 43 Resolution Presets
│   │   └── ...
│   ├── src/
│   │   ├── app/                         # AppItem, AppRepository, AppInstaller
│   │   ├── audio/                       # MMAPI Audio, Sonivox EAS Synthesizer, WAV Player
│   │   ├── config/                      # ProfilesManager, AppProfileConfig (Json Serialization)
│   │   ├── file/                        # JSR-75 FileConnection, FileSystemRegistry
│   │   ├── graphics3d/                  # Mascot Capsule Micro3D v3, M3G JSR-184, Z-Buffer
│   │   ├── input/                       # PhoneKeypad, KeyMapper (Nokia/Siemens/Moto)
│   │   ├── jvm/                         # CLDC 1.1 Bytecode VM, Safepoint, FullCanvas, Stdlib
│   │   ├── lcdui/                       # FrameBuffer, 2D Graphics, 8 Transforms, Font Engine
│   │   ├── messaging/                   # JSR-120 SMS Connection, Message Router
│   │   ├── network/                     # GCF Sockets, HttpBridge, Datagram UDP, Bluetooth
│   │   ├── oem/                         # Vendor Extensions (Nokia DirectGraphics, Vodafone, Siemens)
│   │   ├── platform/                    # Platform dialogs, DPI auto-window resize
│   │   ├── storage/                     # RMS Storage v3.0 (MIDRMS binary format & clone isolation)
│   │   └── j2me_core_api.cpp            # Điểm gắn kết C-ABI & quản lý vòng đời Engine
│   ├── test_assets/
│   │   └── DragonBoy.jar                # Tệp game J2ME thương mại thật (1.92 MB, 321 classes)
│   ├── CMakeLists.txt                   # Cấu hình biên dịch CMake đa nền tảng
│   └── test_main.cpp                    # Bộ 31 Test Suites tự động kiểm chứng 100% logic
└── ui_app/                              # Tầng giao diện người dùng Flutter
    ├── android/                         # Dự án Android NDK Native & J2meBackgroundService
    ├── ios/                             # Dự án iOS Runner & Metal rendering
    ├── windows/                         # Dự án Windows Win32 Runner
    ├── lib/
    │   ├── bridge/
    │   │   ├── j2me_ffi.dart            # Cầu nối Dart FFI kết nối trực tiếp j2me_core.dll / .so
    │   │   ├── http_bridge.dart         # Cầu nối mạng HTTP/HTTPS đa nền tảng
    │   │   └── platform_channel.dart    # Giao tiếp với Android/iOS host services
    │   ├── sessions/
    │   │   └── game_session.dart        # Quản lý đa tiến trình, clone slot, restart sạch
    │   ├── views/
    │   │   ├── emulator_screen.dart     # Màn hình chạy game J2ME với bàn phím & thanh công cụ
    │   │   ├── game_viewport.dart       # Trình kết xuất FrameBuffer thời gian thực (Zero-Copy)
    │   │   ├── virtual_keypad.dart      # Bàn phím ảo cảm ứng (LSK, RSK, D-Pad, Numpad, Split-Wing)
    │   │   ├── app_config_dialog.dart   # Bảng thiết lập Profile, 43 presets & Auto-Device Match
    │   │   ├── app_info_dialog.dart     # Bảng thông tin chi tiết ứng dụng JAR/JAD
    │   │   └── lcdui_screen_dialog.dart # Hộp thoại LCDUI High-Level (Form, TextBox, List, Alert)
    │   └── main.dart                    # Màn hình quản lý thư viện trò chơi & khởi chạy
    └── pubspec.yaml
```

---

## 2. Hợp Đồng C-ABI FFI (`j2me_core.h`)

Tất cả các nền tảng (Windows, macOS, iOS, Android, Linux) giao tiếp với Lõi thông qua tập hàm chuẩn C-ABI:

### Khởi tạo, Điều phối Vòng đời & Quản lý Luồng:
```c
J2ME_API J2meEngineInstance* j2me_core_create(const char* storage_root_dir);
J2ME_API bool j2me_core_load_jar_file(J2meEngineInstance* inst, const char* jar_file_path);
J2ME_API J2meEngineInstance* j2me_core_app_spawn(J2meEngineInstance* library, int app_id, int clone_slot);
J2ME_API void j2me_core_start(J2meEngineInstance* inst);
J2ME_API void j2me_core_pause(J2meEngineInstance* inst);
J2ME_API void j2me_core_resume(J2meEngineInstance* inst);
J2ME_API void j2me_core_stop(J2meEngineInstance* inst);
J2ME_API void j2me_core_destroy(J2meEngineInstance* inst);
```

### Bộ Đệm Hiển Thị Khung Hình (Zero-Copy FrameBuffer):
```c
J2ME_API const uint32_t* j2me_core_lock_framebuffer(J2meEngineInstance* inst, int* out_w, int* out_h, bool* out_dirty);
J2ME_API int j2me_core_copy_frame_rgba(J2meEngineInstance* inst, uint8_t* dst, size_t cap, int scale, bool force, int* out_w, int* out_h);
J2ME_API void j2me_core_unlock_framebuffer(J2meEngineInstance* inst);
J2ME_API void j2me_core_set_screen_dimensions(J2meEngineInstance* inst, int width, int height);
```

### Điều Khiển & Phím Bấm:
```c
J2ME_API void j2me_core_send_key(J2meEngineInstance* inst, int key_code, bool is_pressed);
J2ME_API void j2me_core_send_touch(J2meEngineInstance* inst, int action, int x, int y);
```

---

## 3. Các Cơ Chế Kỹ Thuật Đột Phá

### 1. Safepoint & Cơ Chế Dừng Luồng An Toàn Tuyệt Đối:
* Máy ảo JVM CLDC 1.1 tích hợp cờ `requestTerminate(true)` cùng ngoại lệ `VmTerminated{}` trên mọi vị trí safepoint, `monitorEnter()`, `monitorWait()`, `Thread.sleep()` (theo lát cắt 20ms) và các bước nhảy vòng lặp (`branch`).
* Đảm bảo giải phóng GIL (`m_gil`) ngay lập tức khi dừng phiên game, loại bỏ 100% tình trạng đơ/treo ứng dụng.

### 2. Tự Động Thích Ứng Màn Hình & Xoay Ngang Chia Cánh:
* Hỗ trợ bố cục **Split-Wing Gamepad** ở chế độ ngang: D-Pad bên trái, Numpad bên phải, Game Viewport ở giữa đạt 100% chiều cao màn hình.
* Tự động nhận diện hướng màn hình và kích thước thật của thiết bị di động (tỉ lệ 20:9, 19.5:9, 18:9, 16:9).
* Hỗ trợ nạp đầy đủ các lớp OEM như `com.nokia.mid.ui.FullCanvas` và `DirectGraphics`.

---

## 4. Quy Trình Kiểm Thử Tự Động (Quality Assurance)

Bộ kiểm thử `test_main.cpp` bao gồm 31 test suites kiểm chứng 100% logic:
* **Toán học & Rasterizer 3D**: Kiểm tra ma trận, Z-buffer, thuật toán khử mặt khuất.
* **RMS v3.0**: Kiểm tra tương thích nhị phân `MIDRMS` với file save từ Android.
* **Bytecode VM**: Nạp và kiểm tra 321 lớp bytecode Java thật từ `DragonBoy.jar`.
* **Mạng & SMS**: Kiểm thử Socket Loopback, Datagram UDP, HTTP parser, SMS intercept router.

Lệnh chạy kiểm tra:
```bash
cmake --build universal_loader/core/build --config Release
.\universal_loader\core\build\Release\test_core.exe
```
Kết quả đạt: `[SUCCESS] 100% Tat ca Unit Tests (31/31 Mo Dun) da vuot qua hoan hao!`
