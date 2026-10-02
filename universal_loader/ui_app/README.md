# J5Hienloader UI Application

Tầng giao diện người dùng đa nền tảng (Flutter Multiplatform) của trình giả lập **J5Hienloader**.

---

## 📱 Cấu Trúc Thành Phần Giao Diện

```
lib/
├── bridge/
│   ├── j2me_ffi.dart            # Định nghĩa Dart FFI tương ứng 1:1 với j2me_core.h
│   ├── http_bridge.dart         # Cầu nối mạng HTTP/HTTPS đa nền tảng
│   └── platform_channel.dart    # Giao tiếp với Android/iOS host services (Pick File, WakeLock, Foreground Service)
├── sessions/
│   └── game_session.dart        # Quản lý đa tiến trình (GameSessionManager), Clone Slot & Clean Re-spawn
├── views/
│   ├── emulator_screen.dart     # Màn hình giả lập game chính (AppBar, điều hướng, Menu In-Game)
│   ├── game_viewport.dart       # Trình kết xuất FrameBuffer thời gian thực (Zero-Copy Texture/RawImage)
│   ├── virtual_keypad.dart      # Bàn phím ảo cảm ứng (Portrait Bottom & Landscape Split-Wing)
│   ├── app_config_dialog.dart   # Hộp thoại cấu hình Profile & 43 Presets + Tự động khớp theo thiết bị
│   ├── app_info_dialog.dart     # Hộp thoại thông tin chi tiết ứng dụng JAR/JAD
│   └── lcdui_screen_dialog.dart # Hộp thoại hỗ trợ LCDUI Form, List, TextBox, Alert
└── main.dart                    # Màn hình chính Quản lý Kho game (Thêm, Xóa, Đổi tên, Tìm kiếm, Sắp xếp)
```

---

## 🚀 Hướng Dẫn Biên Dịch & Chạy

> ⚠️ **Lưu ý quan trọng**: Luôn kèm cờ `--no-tree-shake-icons` khi build Flutter để đảm bảo toàn bộ icon Material hiển thị đầy đủ và sắc nét.

### 1. Windows Desktop:
```bash
flutter build windows --release --no-tree-shake-icons
```
Chạy file thực thi:
```powershell
.\build\windows\x64\runner\Release\ui_app.exe
```

### 2. Android:
```bash
# Debug APK
flutter build apk --debug --no-tree-shake-icons

# Release APK
flutter build apk --release --no-tree-shake-icons
```

### 3. iOS / macOS:
```bash
flutter build ipa --release --no-tree-shake-icons
flutter build macos --release --no-tree-shake-icons
```

---

## 🎮 Các Tính Năng Nổi Bật Tầng UI

1. **Xoay Màn Hình Đa Hướng (Orientation Engine)**:
   - Tự động hoán đổi kích thước chuẩn khi xoay ngang/dọc và khởi động lại game sạch sẽ trong tích tắc.
   - Chế độ **Split-Wing** ở màn hình ngang: D-Pad bên trái, Numpad bên phải, khung game ở giữa mở rộng tối đa chiều cao.

2. **Ẩn / Hiện Bàn Phím 1-Chạm**:
   - Nút ẩn/hiện phím ảo trên thanh AppBar giúp game bung rộng tràn viền toàn màn hình 100% khi muốn chơi cảm ứng hoặc dùng tay cầm ngoài.

3. **Cấu Hình Màn Hình 43 Presets & Tự Động Nhận Diện**:
   - Tự động phát hiện tỷ lệ màn hình điện thoại thực tế (16:9, 18:9, 19.5:9, 20:9 FHD+ 2400x1080) và cấp phát góc nhìn siêu rộng cho game.
