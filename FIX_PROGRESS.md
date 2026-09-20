# TIẾN TRÌNH KHẮC PHỤC LỖI & GIẢI PHÁP KỸ THUẬT (J2HIENLOADER)

Tài liệu chi tiết về toàn bộ các lỗi phát hiện, nguyên nhân gốc rễ và giải pháp kỹ thuật đã triển khai trên cả 2 nền tảng **iOS (iPhone/iPad)** và **Desktop (Windows)**.

---

## 📑 MỤC LỤC

1. [Sập bộ nhớ Stack trên iOS (Crash SIGBUS 10 / EXC_BAD_ACCESS)](#1-sập-bộ-nhớ-stack-trên-ios)
2. [Lỗi Treo màn hình đen khi mở game (Black Screen Freeze)](#2-lỗi-treo-màn-hình-đen-khi-mở-game)
3. [Lệch bảng Hằng số Kế thừa (Constant Pool Inheritance Mismatch)](#3-lệch-bảng-hằng-số-kế-thừa-constant-pool-mismatch)
4. [Lỗi Font chữ 8x8 bị rụng ký tự (Missing ASCII Glyphs)](#4-lỗi-font-chữ-8x8-bị-rụng-ký-tự)
5. [Loại bỏ toàn bộ chữ Demo & Thông báo che màn hình](#5-loại-bỏ-chữ-demo--thông-báo-che-màn-hình)
6. [Hệ thống Nhập Form Văn bản Chuẩn Native OS (Desktop & Mobile)](#6-hệ-thống-nhập-form-văn-bản-chuẩn-native-os)
7. [Bàn phím Máy tính Windows không nhận phím & Mờ UI](#7-bàn-phím-máy-tính-windows--độ-nét-ui-hidpi)
8. [Lỗi Giới hạn truy vấn tạm thời khi Cập nhật (GitHub Rate Limit 403)](#8-lỗi-giới-hạn-truy-vấn-tạm-thời-khi-cập-nhật)
9. [Cảm ứng Vuốt/Kéo & Lặp chu kỳ TimerTask](#9-cảm-ứng-vuốtkéo--lặp-chu-kỳ-timertask)

---

### 1. Sập bộ nhớ Stack trên iOS (Crash SIGBUS 10 / EXC_BAD_ACCESS)
* **Hiện tượng**: Mở các game Java có cấu trúc lồng hàm sâu hoặc nhiều luồng (DragonBoy, Ninja School) trên iPhone/iPad bị văng ứng dụng ngay lập tức với mã lỗi `EXC_BAD_ACCESS (SIGBUS 10)` tại `___chkstk_darwin`.
* **Nguyên nhân**: Trên iOS / Darwin, luồng phụ `pthread` mặc định chỉ được hệ điều hành cấp phát **512 KB** stack. Khi máy ảo JVM đệ quy qua các khung hàm Java lồng nhau, bộ nhớ chạm vào vùng bảo vệ Stack Guard.
* **Giải pháp**:
  - Xây dựng lớp trừu tượng `JvmThread` gán cứng `pthread_attr_setstacksize(&attr, 4 * 1024 * 1024)` cấp phát **4 MB Stack** riêng biệt cho luồng JVM chính và các luồng game con (`Thread.start`, `Runnable`, `TimerTask`).
  - Giới hạn độ sâu đệ quy gọi hàm lồng `t_callDepth` an toàn ở mức 128 khung hàm.

---

### 2. Lỗi Treo màn hình đen khi mở game (Black Screen Freeze)
* **Hiện tượng**: Mở game lên chỉ thấy màn hình đen, game không vẽ bất kỳ hình ảnh nào.
* **Nguyên nhân**:
  1. **Không nhận diện Canvas mã hóa (Obfuscated Canvas)**: Trong các game Java thương mại, tên lớp Canvas thường bị đổi thành `a`, `b`, `c` và kế thừa từ `javax.microedition.lcdui.Canvas`. Bộ phân tích trước đó chỉ kiểm tra nếu tên lớp `c->thisClassName` chứa chữ `"Canvas"`, bỏ qua lớp cha `c->superClassName` và phương thức `paint(Graphics g)`. Do đó, lệnh `Display.setCurrent(canvas)` bị bỏ qua và Canvas không bao giờ được gán vào luồng vẽ.
  2. **Nghẽn luồng (Thread Thrashing)**: Vòng lặp 60 FPS liên tục gọi hàm `startRunnableThread()` mỗi 16ms khi chưa nhận Canvas, tạo ra hàng trăm luồng `run()` chạy đè lên nhau gây tê liệt CPU.
* **Giải pháp**:
  - Mở rộng thuật toán nhận diện Canvas: Kiểm tra đệ quy cả lớp cha `superClassName` (`Canvas`, `GameCanvas`, `FullCanvas`) và quét sự tồn tại của phương thức `paint:(Ljavax/microedition/lcdui/Graphics;)V`.
  - Khóa luồng `startRunnableThread()`: Chỉ kích hoạt luồng trò chơi đúng 1 lần duy nhất khi Canvas được bind.
  - Tự động gọi `showNotify()` theo đúng đặc tả J2ME khi Canvas được kích hoạt.

---

### 3. Lệch bảng Hằng số Kế thừa (Constant Pool Mismatch)
* **Hiện tượng**: Một số game load tài nguyên bị sai, gọi hàm sai hoặc đứng ở màn hình khởi động.
* **Nguyên nhân**: Khi một lớp con gọi phương thức được định nghĩa ở lớp cha (`curCls`), các chỉ mục opcode bytecode trong phương thức đó trỏ vào bảng hằng số (`Constant Pool`) của lớp cha. Trình thông dịch trước đó dùng nhầm bảng Constant Pool của lớp con (`cls`), dẫn đến việc đọc sai chuỗi, sai tên hàm và sai định danh class.
* **Giải pháp**:
  - Gán `cls = curCls;` ngay khi tìm thấy phương thức trong cây kế thừa để đảm bảo 100% các opcode `LDC`, `INVOKEVIRTUAL`, `INVOKESPECIAL`, `INVOKESTATIC`, `GETFIELD`, `PUTFIELD` đọc đúng bảng Constant Pool của lớp sở hữu mã lệnh.

---

### 4. Lỗi Font chữ 8x8 bị rụng ký tự (Missing ASCII Glyphs)
* **Hiện tượng**: Khi in chữ trên màn hình (như tên game, splash), màn hình chỉ hiện các chữ in hoa rời rạc (ví dụ: `DragonBoy` chỉ hiện `D   J`, `J2HienLoader` chỉ hiện `J2H  L`).
* **Nguyên nhân**: Mảng bitmap `font8x8_basic` trong `lcdui_display.cpp` chỉ có 65 phần tử (từ ký tự 32 đến 96), thiếu hoàn toàn 26 chữ cái thường `a – z` (mã 97–122) và `{ | } ~` (mã 123–126).
* **Giải pháp**:
  - Bổ sung trọn vẹn đầy đủ toàn bộ 95 ký tự ASCII chuẩn (`32 – 126`) vào bảng font bitmap.

---

### 5. Loại bỏ chữ Demo & Thông báo che màn hình
* **Hiện tượng**: Khi chạy game có chữ demo *"Dang tai game Java / J2HienLoader"* và khung thông báo đen *"Game treo (không vẽ) — thử game khác"* che mất đồ họa game.
* **Giải pháp**:
  - Xóa toàn bộ việc vẽ chữ demo ở màn hình chờ trong `jvm_interpreter.cpp`; giữ màn hình đen sạch sẽ nguyên bản của máy ảo J2ME.
  - Gỡ bỏ hoàn toàn biến `bootDiag`, `diagTimer` và khung toast đen đè lên màn hình trong `GameScreenView.swift`.

---

### 6. Hệ thống Nhập Form Văn bản Chuẩn Native OS (Desktop & Mobile)
* **Hiện tượng**: Khung nhập text (`TextBox` / `TextField` như Số di động/Email, Tên nhân vật) bị kẹt ở ô chọn từng chữ `^ a v [Lat]` cổ điển, hoặc gõ nhanh bị chèn dấu `#######` và văng form.
* **Giải pháp**:
  - **Trên Desktop (Windows)**:
    * Gõ chữ và số trực tiếp vào ô form bằng bàn phím máy tính PC thật.
    * Hỗ trợ đầy đủ: `Backspace` (xóa), mũi tên `← / →` (di chuyển con trỏ), nhấp chuột kéo rê chọn vùng (bôi đen), `Ctrl+A` (chọn tất cả), `Ctrl+C` (sao chép), `Ctrl+V` (dán).
    * Khóa phím tắt game khi đang trong ô văn bản: gõ nhanh các chữ `e, q, w, a, s, d` không bao giờ bị văng form.
    * Bấm `Enter` / `F1` để **OK**; bấm `Esc` / `F2` để **Hủy**.
  - **Trên iOS (iPhone/iPad)**:
    * Tích hợp `native_prompt_text_input` kích hoạt bàn phím hệ thống iOS gốc (`UIAlertController` + `UITextField`), gõ tiếng Việt có dấu, gợi ý từ, dán clipboard mượt mà.

---

### 7. Bàn phím Máy tính Windows & Độ nét UI (HiDPI)
* **Hiện tượng**: Bản Windows không nhận dãy phím số trên cùng `0–9`, phím mũi tên và giao diện bị mờ hạt.
* **Giải pháp**:
  - Tự động cấu hình `freej2me.conf` map đầy đủ 100% bàn phím PC: Dãy phím số `0–9`, Numpad, phím mũi tên `↑ ↓ ← →`, `WASD`, `Enter`, `Space`, `F1`, `F2`, `Backspace`.
  - Kích hoạt **True Per-Monitor HiDPI Awareness V2** (`SetProcessDpiAwareness(2)`): Giao diện ứng dụng và cửa sổ game hiển thị sắc nét từng pixel trên mọi màn hình Full HD, 2K, 4K.

---

### 8. Lỗi Giới hạn truy vấn tạm thời khi Cập nhật (GitHub Rate Limit 403)
* **Hiện tượng**: Bấm kiểm tra cập nhật trên app báo *"GitHub giới hạn truy vấn tạm thời, thử lại sau vài phút."*
* **Nguyên nhân**: Máy chủ `api.github.com` giới hạn 60 lượt gọi/giờ cho mỗi IP công cộng (thường bị nghẽn khi dùng 4G/WiFi chung).
* **Giải pháp**:
  - Tạo file `version.json` trên nhánh `main` và tích hợp kênh CDN dự phòng qua `raw.githubusercontent.com`. Kênh CDN không có giới hạn 60 req/giờ, đảm bảo tính năng kiểm tra cập nhật hoạt động liên tục 24/7.

---

### 9. Cảm ứng Vuốt/Kéo & Lặp chu kỳ TimerTask
* **Hiện tượng**: Game cảm ứng không vuốt/kéo màn hình được; một số game bị dừng vòng lặp cập nhật.
* **Giải pháp**:
  - Cập nhật sự kiện cảm ứng 3 trạng thái: `0 = pointerPressed`, `1 = pointerDragged`, `2 = pointerReleased` trong `jvm_interpreter.cpp`.
  - Bổ sung cơ chế luồng lặp chu kỳ độc lập cho `java.util.Timer` khi `period > 0`.
  - Cập nhật `Object.getClass()` gán đúng chuỗi `className` phục vụ cơ chế Reflection nạp file trong JAR.

---

## 📊 KẾT QUẢ KIỂM THỬ
* **Khởi động**: DragonBoy, Avatar, Ninja School, Gameloft khởi chạy trực tiếp vào màn hình game.
* **Đồ họa**: Render 60 FPS mượt mà trên nền tảng Apple Metal 3 (iOS) và Windows Desktop.
* **Tính ổn định**: Không còn lỗi tràn bộ nhớ `SIGBUS 10`, không còn lỗi đứng màn hình đen, không còn lỗi out form text.
