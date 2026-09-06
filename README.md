# J2HienLoader for iOS & iPadOS (Native Port)

A high-performance, native J2ME (Java ME / MIDP 2.0 / CLDC 1.1) emulator ported for iOS, iPadOS, and Apple Silicon.

[![Build iOS IPA](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions/workflows/build-ios-ipa.yml/badge.svg)](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/actions/workflows/build-ios-ipa.yml)
[![Release](https://img.shields.io/github/v/release/PhamTriHien/J2meloader_IOS_Projectt?color=orange&label=B%E1%BA%A3n%20ph%C3%A1t%20h%C3%A0nh)](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/releases/latest)

---

## 🌟 Tính năng nổi bật (Key Features)

* **🔄 Tự động Cập nhật & Bản vá In-App (Auto-Updater)**:
  - Tự động kiểm tra bản cập nhật mới nhất từ GitHub Releases khi khởi động app hoặc khi mở lại có mạng.
  - Hộp thoại chi tiết nội dung bản vá (`UpdateModalView`), thanh tiến trình tải real-time.
  - Tích hợp 1 chạm cài đặt qua **ESign**, **TrollStore** hoặc mở liên kết tải trực tiếp qua **Safari**.
  - Tùy chọn bật/tắt tự động kiểm tra và nút "Kiểm tra bản cập nhật ngay" trong phần *Cài đặt chung*.

* **🚀 Core Máy Ảo JVM & Thực thi Bytecode thuần C++20**:
  - Tự động kích hoạt `<clinit>` static initializer trên mọi bytecode `NEW`, `GETSTATIC`, `PUTSTATIC`, `INVOKESTATIC`.
  - Hỗ trợ đầy đủ kiểu dữ liệu: `int`, `long`, `float`, `double`, exception handling `try/catch/finally`.
  - Hỗ trợ đa luồng thật (`Thread`, `Runnable`, `synchronized`, `wait`, `notify`).
  - Hệ thống API MIDP 2.0 & CLDC 1.1: `Image.createImage(InputStream/Image)`, `DataInputStream` đầy đủ, `Class.forName`, `Display.setCurrent` nhận diện canvas mã hóa obfuscated.

* **🎮 LCDUI 2D & Đồ họa Metal 3 (60 - 120 FPS)**:
  - Render Metal 3 siêu tốc với pixel format `bgra8Unorm` tối ưu Little Endian ARM64.
  - Bộ lọc Shaders cổ điển: Nearest Neighbor (Pixel Art sắc nét), Bilinear, CRT TV cổ điển, Lưới điểm ảnh LCD Nokia.
  - Full Canvas / GameCanvas, Graphics primitives, Sprite 8 hướng, TiledLayer, LayerManager.
  - Hỗ trợ bàn phím ảo chuẩn Nokia T9, D-Pad điều hướng, tùy biến độ trong suốt, rung phản hồi Haptic Touch.

* **🔋 Treo game ngầm 24/7 (Anti-Crash & Continuous Background)**:
  - Duy trì luồng xử lý và kết nối mạng TCP socket liên tục khi tắt màn hình hoặc chuyển app.
  - Cơ chế `UIBackgroundModes` + Audio Keep-Alive engine chống bị iOS đóng ứng dụng.

* **🎵 Âm thanh Sonivox EAS thực thụ**:
  - Bộ tổng hợp nhạc chuông MIDI, iMelody, RTTTL qua Sonivox EAS.
  - Phát âm thanh hiệu ứng WAV, MP3, AMR qua AudioBridge.

* **💾 Quản lý Lưu trữ RMS & Nhập Game Tiện lợi**:
  - Hệ thống RMS (Record Management System) lưu điểm cao và dữ liệu màn chơi bền vững trong Sandbox.
  - Nạp game `.jar` / `.jad` từ Files, iCloud Drive, AirDrop, Safari hoặc chuyển trực tiếp qua USB.

---

## 📲 Hướng dẫn Cài đặt & Sử dụng

### 1. Cài đặt qua TrollStore / ESign / Sideloadly
1. Tải file **`J2HienLoader.ipa`** từ [GitHub Releases](https://github.com/PhamTriHien/J2meloader_IOS_Projectt/releases/latest).
2. Cài đặt bằng các công cụ:
   - **TrollStore** (iOS 14.0 - 17.0): Cài vĩnh viễn không bao giờ bị thu hồi chứng chỉ (No Revoke).
   - **ESign / Scarlet / Feather / GBox**: Ký chứng chỉ cá nhân hoặc doanh nghiệp.
   - **Sideloadly / AltStore / SideStore**: Ký bằng Apple ID miễn phí trên PC/Mac.

### 2. Nạp game vào ứng dụng
1. Mở **J2HienLoader** trên iPhone / iPad.
2. Bấm nút **`+`** (Thêm game) ở góc trên bên phải màn hình.
3. Chọn file `.jar` hoặc `.jad` từ ứng dụng Tệp (Files).
4. Chọn game từ danh sách và thưởng thức!

---

## 🛠️ Biên dịch từ Mã nguồn (Build from Source)

### Yêu cầu hệ thống:
* macOS 14.0+ kèm Xcode 15.4+.
* Hoặc sử dụng GitHub Actions CI tự động build trên Cloud.

```bash
# Clone repository
git clone https://github.com/PhamTriHien/J2meloader_IOS_Projectt.git
cd J2meloader_IOS_Projectt/ios/J2MELoader-iOS

# Biên dịch ra file IPA
chmod +x build_ipa.sh
./build_ipa.sh
```

---

## 📄 Bản quyền & Đóng góp
* Dự án được phát triển và tối ưu cho cộng đồng người dùng iOS yêu thích các tựa game Java cổ điển.
* Giấy phép: GNU General Public License v3.0 (GPLv3).