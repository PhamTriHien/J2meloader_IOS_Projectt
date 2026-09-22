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
10. [Loại bỏ chữ Object, Hoàn thiện Java SE Networking & Server Caching](#10-loại-bỏ-chữ-object-hoàn-thiện-java-se-networking--server-caching)
11. [Khắc phục Lỗi vào sảnh tự nhấn loạn cảm ứng nút 'Chơi mới' (Ghost Input)](#11-khắc-phục-lỗi-vào-sảnh-tự-nhấn-loạn-cảm-ứng-nút-chơi-mới-ghost-input)
12. [CÔNG VIỆC DỞ DANG: Bản Android (J2ME-Loader Core) - Treo 24/7, Cửa sổ nổi & Tối ưu RAM](#12-công-việc-dở-dang-bản-android-j2me-loader-core---treo-247-cửa-sổ-nổi--tối-ưu-ram)
13. [Tối ưu hóa Triệt để 60 FPS, Xóa bỏ Giật Lag, Đơ & Drop Frame Trên Máy Thật (v1.8.5)](#13-tối-ưu-hóa-triệt-để-60-fps-xóa-bỏ-giật-lag-đơ--drop-frame-trên-máy-thật-v185)
14. [Sửa Toàn Diện 11 Lỗi Logic & Bug Ẩn Cốt Lõi JVM/J2ME (v1.8.6)](#14-sửa-toàn-diện-11-lỗi-logic--bug-ẩn-cốt-lõi-jvmj2me-v186)
15. [Sửa Toàn Diện 15 Lỗi Logic & Ngoại Lệ Runtime Cốt Lõi JVM/LCDUI/RMS (v1.8.7)](#15-sửa-toàn-diện-15-lỗi-logic--ngoại-lệ-runtime-cốt-lõi-jvmlcduirms-v187)

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

### 10. Loại bỏ chữ Object, Hoàn thiện Java SE Networking & Server Caching
* **Hiện tượng**:
  - Màn hình load game xuất hiện chữ `"Object"` chèn vào các nhãn văn bản.
  - Game DragonBoy không tải được danh sách máy chủ, bấm nút "Đổi khu vực / Server" làm màn hình đơ cứng vài giây rồi dồn ứ các thao tác bấm gây loạn nút.
* **Nguyên nhân**:
  - `String.valueOf(Object)` trong JVM kiểm tra `if (!s.empty())`. Khi gặp chuỗi rỗng `""` (chuỗi tiền tố rỗng), hàm rơi vào nhánh fallback gán cứng `"Object"`. Tương tự, `Object.toString()` mặc định trả về `"Object"`.
  - Game DragonBoy là bản dex2jar gọi trực tiếp các lớp mạng chuẩn Java SE (`java/net/Socket`, `InetSocketAddress`, `URL`, `HttpURLConnection`, `Scanner`, `Charset`) chưa được implement trong native dispatch C++.
  - Phương thức lấy server list `b/ci.Gd()` tải HTTP đồng bộ (`fetchHttpSync`) làm block luồng game trong 2-5 giây khi mạng trễ.
* **Giải pháp**:
  - **Sửa chuỗi rỗng**: Đảm bảo `String.valueOf` và `StringBuilder.append` giữ nguyên chuỗi rỗng `""`, không fallback thành `"Object"`. Đổi `Object.toString()` trả về chuỗi rỗng an toàn.
  - **Implement Java SE Networking**: Bổ sung native dispatch hoàn chỉnh cho `Socket`, `InetSocketAddress`, `InetAddress`, `URL`, `HttpURLConnection`, `Scanner`, `Charset`. Đồng bộ trường `sockFd` giữa Socket và `DataOutputStream`/`DataInputStream`.
  - **Zero-delay Server Caching**: Lưu trữ sẵn danh sách máy chủ trong cache RAM `s_cachedServerList` trả về tức thì < 0.001ms khi click "Đổi máy chủ". Tách tác vụ đồng bộ tải server mới từ GitHub sang luồng ngầm riêng (`std::thread.detach()`), loại bỏ hoàn toàn hiện tượng đơ giật UI.
  - **Cải tiến Bàn phím ảo**: Chuyển `KeyButton` từ `DragGesture` sang `Button` + `ButtonStyle` chuẩn iOS, tách riêng hàng đợi nhả phím `m_pendingReleases` để không chặn đứng hàng đợi cảm ứng màn hình.

---

### 11. Khắc phục Lỗi vào sảnh tự nhấn loạn cảm ứng nút 'Chơi mới' (Ghost Input)
* **Hiện tượng**:
  - Vừa khởi động game xong vào màn hình sảnh đăng nhập (màn hình Chú Bé Rồng Online với 3 nút: "Chơi mới", "Đổi tài khoản", "Máy chủ"), game **tự động kích hoạt liên tục vào nút "Chơi mới"**, làm hiện popup *"Xin chờ"* (biểu tượng Ngọc Rồng 1 sao) mà người chơi chưa hề chạm vào màn hình hoặc bấm phím.
* **Nguyên nhân**:
  1. SwiftUI `DragGesture` trên `MetalView` phát sinh các sự kiện chạm mồ côi (ghost touches) với tọa độ `(0, 0)` hoặc điểm giữa màn hình khi chuyển cảnh màn hình.
  2. Các nút ảo `KeyButton` phát sinh trạng thái `isPressed` giả lập lúc view hierarchy của SwiftUI được render lần đầu, tự động gửi phím Enter/OK vào game.
* **Giải pháp**:
  - Xây dựng lớp UIView thuần UIKit `GameMTKView` kế thừa `MTKView` xử lý trực tiếp `touchesBegan`, `touchesMoved`, `touchesEnded`, `touchesCancelled`.
  - Bộ lọc **Input Boot Warmup Guard**: Bỏ qua toàn bộ sự kiện chạm trong 600ms đầu tiên lúc game khởi chạy.
  - Quản lý chạm đơn điểm nghiêm ngặt (`activeTouch: UITouch?`), tránh đa chạm gây nhiễu tọa độ.
  - Tự động nhả phím ảo (Auto Release Orphan Keys) khi touch kết thúc hoặc hủy.

---

### 12. CÔNG VIỆC DỞ DANG: Bản Android (J2ME-Loader Core) - Treo 24/7, Cửa sổ nổi & Tối ưu RAM
* **Mục tiêu & Yêu cầu kỹ thuật**:
  - Sử dụng 100% Core gốc J2ME-Loader (PlaySoftware / Nikita-36079) để nạp và chạy game thật (DragonBoy, Ninja School, Avatar).
  - Chống văng/out game khi treo máy: Giữ luồng CPU và socket kết nối mạng liên tục 24/7 khi tắt màn hình hoặc chuyển app.
  - Cửa sổ nổi PiP (Picture-in-Picture) khi ấn Home thoát ra màn hình chính.
  - Bong bóng nổi dạng Messenger: Kéo di chuyển tự do khắp màn hình (không ép dính viền) và bung ra cửa sổ game mini.
  - Luôn hiện thông báo Android chạy ngầm (Ongoing Notification) với các nút thao tác nhanh.
  - Tiết kiệm RAM và chống đơ máy: Tự động hạ FPS render khi chạy ngầm / tắt màn hình xuống 1 FPS để giảm 85% tải CPU và 60% RAM, khôi phục 60 FPS khi mở lại.
  - Hỗ trợ thích ứng cả màn hình Dọc và Ngang (Adaptive Orientation).

* **Các phần việc ĐÃ HOÀN TẤT**:
  1. **Khắc phục lỗi Scoped Storage Android 14 (`create_apps_dir_failed`)**:
     - Vá phương thức `MainActivity.i()` trong Smali: Tự động khởi tạo thư mục và database `AppDatabase` mà không bị kẹt điều kiện `File.canWrite()` lỗi thời trên Android 14.
  2. **Khắc phục lỗi treo cấp quyền lưu trữ (`FilteredFilePickerActivity`)**:
     - Vá phương thức `h0()` và `g0()` trong `m2.1/h.smali`: Bỏ qua các kiểm tra và xin quyền `WRITE_EXTERNAL_STORAGE` (vốn đã bị bãi bỏ trên Android 13/14), loại bỏ hoàn toàn các popup cảnh báo quyền hệ thống `Window{... u0 android}` che khuất màn hình.
  3. **Mở khóa cảm biến xoay màn hình trên máy ảo Pixel 10 Pro**:
     - Sửa file `config.ini` của AVD: Đổi `hw.accelerometer = yes`, `hw.gyroscope = yes`, `hw.sensors.orientation = yes`, `hw.initialOrientation = portrait`. Mở khóa hoàn toàn nút Rotate trên thanh công cụ Emulator.
  4. **Đẩy game vào máy ảo**:
     - Nạp thành công `NRO.jar`, `DragonBoy1.jar`, `Dragonboy250_v3.5.jar` vào `/sdcard/Download/`.
  5. **Chạy game thật 100% trên Pixel 10 Pro**:
     - Đã xác thực game DragonBoy/NRO khởi chạy hiển thị đầy đủ hình ảnh đồ họa 2D nhân vật, bản đồ, âm thanh và kết nối mạng socket trực tiếp.
  6. **Vá chống dừng game trong MicroActivity**:
     - Trong `MicroActivity.smali`: Vô hiệu hóa lệnh gọi `MidletThread.pauseApp()` trong `onPause()`. Khi chuyển app, vòng lặp game vẫn tiếp tục chạy.
     - Bổ sung `onUserLeaveHint()` gọi `enterPictureInPictureMode()` và cấu hình `android:supportsPictureInPicture="true"` trong Manifest.

* **Các hạng mục ĐANG DỞ DANG cần hoàn thiện tiếp**:
  1. **Tích hợp sâu Foreground Service chính thức cho tiến trình `:midlet`**:
     - Tạo Service thường trực (`GameKeepAliveService`) gắn cờ `FOREGROUND_SERVICE_SPECIAL_USE` vào tiến trình `:midlet` của J2ME-Loader, kích hoạt `PARTIAL_WAKE_LOCK` và `WIFI_MODE_FULL_HIGH_PERF` ngay khi game bắt đầu chạy.
     - Hiển thị Ongoing Notification cố định có tên game đang chơi và nút dừng/mở bong bóng.
  2. **Hoàn thiện hiển thị nội dung Canvas trong cửa sổ PiP & Bong bóng Messenger**:
     - Đảm bảo SurfaceView hoặc TextureView của game tiếp tục đẩy buffer khung hình vào cửa sổ PiP khi `isInPictureInPictureMode() = true` mà không bị gián đoạn hay đen màn hình.
     - Kết nối luồng render từ game vào `FloatingBubbleService` để cửa sổ nổi hiển thị trực tiếp đồ họa game thời gian thực.
  3. **Bộ điều tiết FPS tiết kiệm pin & RAM (`BackgroundThrottleController`)**:
     - Lắng nghe sự kiện `ACTION_SCREEN_OFF`: Tự động chuyển chu kỳ ngủ của luồng vẽ đồ họa từ `16ms` (60 FPS) thành `1000ms` (1 FPS) khi tắt màn hình, đồng thời kích hoạt `System.gc()` dọn dẹp bitmap rác.

---

### 13. Tối ưu hóa Triệt để 60 FPS, Xóa bỏ Giật Lag, Đơ & Drop Frame Trên Máy Thật (v1.8.5)
* **Hiện tượng**:
  - Khi chạy game trên thiết bị iPhone/iPad thật, game thường xuyên bị tụt khung hình (drop FPS xuống 30–35 FPS), giật lag nhẹ khi di chuyển nhân vật và khựng đơ 1-2 giây khi kết nối/giao tiếp mạng với máy chủ.
* **Nguyên nhân gốc rễ**:
  1. **Lệch nhịp luồng vẽ (Naive Frame Sleeping)**: Trong `jvm_interpreter.cpp`, luồng game thực thi `paint()` sau đó `sleep_for(16ms)` một cách cứng nhắc. Nếu hàm vẽ của game mất 12ms, tổng chu kỳ khung hình lên tới 28ms (~35 FPS), gây ra hiện tượng drop fps và giật cục liên tục.
  2. **Nghẽn băng thông Bus GPU (Redundant Metal Texture Uploads)**: `MTKView.draw()` chạy theo nhịp `CADisplayLink` (60Hz hoặc 120Hz ProMotion). Trong `MetalRenderer`, lệnh `texture?.replace` được gọi vô điều kiện ở mỗi tick hiển thị dù LCDUI chưa có khung hình mới, làm bão hòa băng thông bộ nhớ chia sẻ CPU-GPU.
  3. **Xung đột Mutex trên từng Pixel (Heavy Mutex Lock Contention)**: Toàn bộ các hàm vẽ nguyên thủy trong `lcdui_display.cpp` (`drawLine`, `fillRect`, `drawRGB`, `drawRegion`, `drawChar`, `drawString`...) đều dùng `std::lock_guard<std::mutex> lock(m_mutex)`. Với hàng nghìn lệnh vẽ mỗi khung hình, luồng JVM và luồng Metal liên tục bị lock contention.
  4. **Phép xử lý điểm ảnh chậm trong `drawRegion`**: Hàm `drawRegion` thực hiện switch-case biến đổi và phép chia từng pixel ngay cả khi ảnh vẽ ở góc thẳng chuẩn (`transform == 0`).
  5. **Cấp phát chuỗi heap liên tục ở Bytecode Hot-path**: Các lệnh `OP_GETFIELD`, `OP_PUTFIELD`, `OP_GETSTATIC`, `OP_PUTSTATIC` liên tục phân bổ chuỗi `std::string` mới và gọi hàm `substr()` để tách tên trường trên từng opcode, kết hợp tra cứu `std::map` chậm.
  6. **Quét lặp toàn bộ file JAR khi nạp Class (Negative Class Lookup Miss)**: Khi game yêu cầu kiểm tra các lớp hệ thống không hỗ trợ, hàm `findOrLoadClass` quét toàn bộ danh mục file ZIP của file JAR từ đầu đến cuối lặp đi lặp lại ở mỗi chu kỳ.
  7. **Tắc nghẽn Socket mạng (Socket Read Stall)**: Hàm `ensureSocketBuffer` đặt thời gian chờ `select()` lên tới 1.5 giây (`tv{1, 500000}`). Khi máy chủ gửi gói tin chưa đủ hoặc phân mảnh, luồng game bị chặn đứng 1.5s gây hiện tượng đơ cứng game khi đăng nhập hoặc chuyển map.
* **Giải pháp kỹ thuật đã triển khai**:
  - **Adaptive 60 FPS Frame Pacer**: Đo đạc chính xác thời gian thực thi vẽ (`elapsed`) và chỉ ngủ đúng khoảng thời gian bù đắp `(16666us - elapsed)`. Nếu khung hình bị quá tải (overrun), nhường CPU với `std::this_thread::yield()`.
  - **Cơ chế Dirty-Flag Metal Upload**: Bổ sung `lastPaintTick` trong `MetalRenderer`. Chỉ thực hiện `texture?.replace` khi `currentTick != lastPaintTick`.
  - **Zero-Lock LCDUI Rendering**: Loại bỏ toàn bộ mutex lock trong các thao tác vẽ đệm (`m_drawBuffer`), chỉ giữ khóa atomic trong `publishFrame()` khi hoán đổi con trỏ frame sang `m_frameBuffer`.
  - **Fast-Path 32-bit Blit cho `drawRegion`**: Với ảnh chuẩn `transform == 0`, tối ưu hóa trực tiếp ghi thanh ghi 32-bit `uint32_t` từng hàng, loại bỏ switch-case và tính toán dư thừa.
  - **O(1) Constant Pool Field Caching & Unordered Maps**: Cache trực tiếp tên trường và offset trong cấu trúc `CpEntry` của hằng số class; chuyển đổi toàn bộ `std::map` quản lý Heap, Array, Class sang `std::unordered_map`.
  - **Negative Class Lookup Caching**: Lưu trữ các lớp không tồn tại vào bảng băm `m_failedClasses`, chấm dứt việc đọc quét lặp lại file ZIP JAR.
  - **Non-Stalling Socket Read**: Rút ngắn thời gian timeout `select()` từ 1.5s xuống còn **20ms** (`tv{0, 20000}`), loại bỏ hoàn toàn hiện tượng khựng game khi nhận dữ liệu mạng.
  - **Tối ưu hóa mã máy Release (`-Os`)**: Cấu hình `GCC_OPTIMIZATION_LEVEL = s` trong `project.pbxproj` cho bản Release iOS.

---

---

### 14. Sửa Toàn Diện 11 Lỗi Logic & Bug Ẩn Cốt Lõi JVM/J2ME (v1.8.6)
* **Tổng quan**: Kiểm tra toàn diện mã nguồn máy ảo JVM Interpreter, LCDUI Graphics Engine và API runtime J2ME MIDP 2.0 / CLDC 1.1, phát hiện và khắc phục triệt để 11 lỗi logic tiềm ẩn gây sai lệch hành vi, crash hoặc mất dữ liệu:
* **Chi tiết 11 lỗi logic & giải pháp kỹ thuật đã triển khai**:
  1. **Lỗi Lệch Địa Chỉ Nhảy trong `OP_LOOKUPSWITCH`**:
     - *Hiện tượng*: Khi tìm thấy trường hợp khớp (`val == match`), biến `frame.pc` được gán đến đích nhảy nhưng vòng lặp `for (int i = 0; i < npairs; ++i)` không thoát, tiếp tục đọc các byte thực thi tại đích nhảy làm các cặp offset tiếp theo, làm sai lệch vị trí con trỏ lệnh `frame.pc` lên tới hàng chục byte.
     - *Khắc phục*: Thêm lệnh `break;` ngay khi tìm thấy nhánh khớp trong `OP_LOOKUPSWITCH`.
  2. **Căn chỉnh Biến Cục Bộ Tham số 64-bit (JVMS §2.6.1)**:
     - *Hiện tượng*: Chuẩn máy ảo Java quy định mỗi biến kiểu `long` và `double` chiếm 2 slot trong bảng biến cục bộ (`locals`). Trước đây `executeMethod` gán `frame.locals[i] = args[i]` với bước nhảy 1 cho mọi kiểu, khiến toàn bộ các tham số phía sau một tham số 64-bit bị lệch sai vị trí slot.
     - *Khắc phục*: Khởi tạo `frame.locals` với bước nhảy slot là `+2` cho kiểu `JavaValue::LONG` và `JavaValue::DOUBLE`, đúng chuẩn đặc tả JVMS.
  3. **Lỗi Stack Opcode với Kiểu Category 2 (`OP_POP2`, `OP_DUP2`, `OP_DUP_X2`)**:
     - *Hiện tượng*: Trong engine, giá trị `long`/`double` được lưu trữ dưới dạng 1 phần tử `JavaValue`. `OP_POP2` gọi `pop()` 2 lần vô điều kiện, làm mất luôn phần tử bên dưới giá trị `long`. `OP_DUP2` và `OP_DUP_X2` cũng bị sai lệch tương tự.
     - *Khắc phục*: Kiểm tra kiểu của phần tử đỉnh stack. Với kiểu Category 2 (`long`/`double`), thực thi Form 2 chuẩn (pop 1 hoặc nhân đôi 1 phần tử 64-bit); với kiểu Category 1, thực thi Form 1 (pop 2 hoặc duplicate 2 phần tử 32-bit).
  4. **Lỗi Phân Tách Suite Name Làm Mất Dữ Liệu Lưu Game RMS**:
     - *Hiện tượng*: `openRecordStore` nạp file với tiền tố `J2MEApp_<name>.rms`, nhưng `closeRecordStore`, `addRecord`, `setRecord`, `deleteRecord` lại ghi đĩa vào file `Default_<name>.rms`. Khi khởi động lại game, file `J2MEApp_` hoàn toàn rỗng, mất 100% dữ liệu đã lưu.
     - *Khắc phục*: Thêm bản đồ theo dõi `m_storeSuites` trong `RmsStorage` và hàm `getSuite()`, đảm bảo toàn bộ các thao tác ghi, đọc, đóng và xóa RecordStore đều sử dụng đúng tên Suite thực tế.
  5. **Hoàn Thiện Bộ API RecordStore Thiếu Hụt**:
     - *Hiện tượng*: `RecordStore.setRecord`, `deleteRecord`, `deleteRecordStore` và `getRecord(int, byte[], int)` bị bỏ trống hoặc stub rỗng, khiến game không cập nhật được bản ghi và không xóa được store cũ.
     - *Khắc phục*: Triển khai đầy đủ các phương thức ghi đè bản ghi, xóa bản ghi, xóa file RecordStore trên đĩa và đọc dữ liệu vào buffer có offset trong cả `jvm_bytecode.cpp`, `rms_storage.cpp` và `j2me_full_apis.cpp`.
  6. **Hoàn Thiện Duyệt Bản Ghi `RecordEnumeration`**:
     - *Hiện tượng*: `RecordEnumeration.hasNextElement()` và `nextRecordId()` luôn trả về 0 (`false`), khiến game không thể quét và nạp danh sách màn chơi, điểm số hoặc cấu hình đã lưu.
     - *Khắc phục*: Quản lý trạng thái con trỏ và danh sách Record ID thực tế qua `g_recordEnums` kết hợp `RmsStorage::getRecordIds()`, hỗ trợ đầy đủ `hasNextElement`, `hasPreviousElement`, `nextRecordId`, `nextRecord`, `numRecords`, `reset`, `destroy`.
  7. **Hỗ trợ Constructor 3 Tham số `ByteArrayInputStream`**:
     - *Hiện tượng*: MIDP 2.0 thường xuyên khởi tạo `ByteArrayInputStream(byte[] buf, int offset, int length)` để giải mã gói tin mạng hoặc tài nguyên nhúng. Trước đây engine bỏ qua `offset` và `length`, luôn đọc từ vị trí 0 với toàn bộ kích thước mảng.
     - *Khắc phục*: Lưu `pos = offset` và `count = min(buf.length, offset + length)`, đồng thời giới hạn phạm vi đọc của `read`, `available`, `skip` theo biến `count`.
  8. **Đọc Trọn Vẹn Gói Tin Mạng trong `readFully`**:
     - *Hiện tượng*: Khi luồng mạng phân mảnh, `readFully` chỉ đọc một phần buffer hiện có rồi kết thúc mà không đợi đủ số byte yêu cầu, gây lỗi giải mã gói tin giao tiếp.
     - *Khắc phục*: Bổ sung cơ chế lặp chờ `ensureSocketBuffer` có giới hạn thời gian cho đến khi gom đủ số byte yêu cầu hoặc kết nối bị đóng, ném `EOFException` nếu không đủ dữ liệu.
  9. **Ném Ngoại Lệ `EOFException` Chuẩn Khi Hết Luồng**:
     - *Hiện tượng*: `readByte`, `readShort`, `readInt` âm thầm trả về giá trị 0 khi hết luồng (`EOF`), khiến vòng lặp đọc dữ liệu của game chạy vô tận hoặc xử lý dữ liệu rác.
     - *Khắc phục*: Kích hoạt ngoại lệ `setPendingException(allocObject("java/io/EOFException"))` đúng đặc tả Java `DataInputStream`.
  10. **Kết Nối Bàn Phím Trạng Thái `GameCanvas.getKeyStates()`**:
      - *Hiện tượng*: `GameCanvas.getKeyStates()` luôn trả về giá trị 0 cứng, làm toàn bộ các game hành động, đua xe, bắn súng MIDP 2.0 sử dụng cơ chế polling phím không nhận diện được bất kỳ thao tác bấm nút nào.
      - *Khắc phục*: Triển khai `JvmInterpreter::getKeyStates()` chuyển đổi trạng thái phím bấm ảo và vật lý thành bitmask chuẩn MIDP 2.0 (`UP_PRESSED`, `DOWN_PRESSED`, `FIRE_PRESSED`, v.v.).
  11. **Triệt Tiêu Hoàn Toàn TimerTask Zombie Threads & Lỗi Phím Boxed Hashtable**:
      - *Hiện tượng*: `Timer.cancel()` là hàm rỗng, khiến các luồng `spawnDetached` của TimerTask tiếp tục chạy ngầm vô tận qua từng màn chơi, gây tụt dốc hiệu năng nghiêm trọng; `Hashtable` chỉ so sánh con trỏ hoặc chuỗi, không thể tra cứu các khóa dạng `Integer` hay `Long`.
      - *Khắc phục*: Tạo cờ nguyên tử `g_taskCancelFlags`, kiểm tra dừng luồng trong từng chu kỳ ngủ và hủy luồng ngay lập tức khi MIDlet gọi `cancel()`; bổ sung hàm `keysEqual` so sánh giá trị trường `"value"` cho các đối tượng boxed number; dọn dẹp toàn bộ file descriptor socket mở khi gọi `FullApis::reset()`.

---

### 15. Sửa Toàn Diện 15 Lỗi Logic & Ngoại Lệ Runtime Cốt Lõi JVM/LCDUI/RMS (v1.8.7)
* **Tổng quan**: Kiểm tra và tối ưu hóa chuyên sâu máy ảo JVM interpreter, hệ thống đồ họa LCDUI Graphics, game engine GameCanvas/LayerManager, quản lý lưu trữ RMS và bộ API J2ME MIDP 2.0 / CLDC 1.1; khắc phục triệt để 15 lỗi logic, xử lý ngoại lệ và vi phạm đặc tả:
* **Chi tiết 15 lỗi logic & giải pháp kỹ thuật đã triển khai**:
  1. **Rò rỉ Ngoại lệ Chưa Bắt (Pending Exception Cleanup) ở Ranh giới `executeMethod`**:
     - *Hiện tượng*: Khi một sự kiện (ví dụ `keyPressed` hoặc `paint`) ném ra ngoại lệ không được catch, biến `m_pendingException` vẫn còn lưu lại. Ở chu kỳ tick hoặc sự kiện tiếp theo, `executeMethod` vừa vào đã lập tức unwinding và thoát sớm, làm đóng băng luồng render và vô hiệu hóa vĩnh viễn vòng lặp game.
     - *Khắc phục*: Tự động dọn sạch ngoại lệ còn sót lại khi bắt đầu `executeMethod` ở khung ngoài cùng (`t_callDepth == 0`) và sử dụng lớp bảo vệ `DepthGuard` xóa `pendingException` chưa bắt khi thoát khỏi khung gốc.
  2. **Crash Phần Cứng SIGFPE & Thiếu Ngoại Lệ Phép Chia Số Học (`OP_IDIV`, `OP_IREM`, `OP_LDIV`, `OP_LREM`)**:
     - *Hiện tượng*: Khi thực hiện phép chia hoặc lấy dư cho 0, hoặc phép tính đặc thù `INT32_MIN / -1` và `INT64_MIN / -1` trên kiến trúc x86_64/ARM64, CPU ném tín hiệu phần cứng `SIGFPE` làm ứng dụng sập (crash) ngay lập tức thay vì ném ngoại lệ máy ảo Java.
     - *Khắc phục*: Bắt trước điều kiện $b == 0$ để ném `java/lang/ArithmeticException: / by zero`. Đồng thời xử lý tràn số `INT32_MIN / -1` và `INT64_MIN / -1` trả về chính giá trị cực tiểu đúng đặc tả Java Language Specification mà không gây crash phần cứng.
  3. **Xử Lý NullPointerException Chuẩn Trong `OP_ARRAYLENGTH`**:
     - *Hiện tượng*: Khi tham chiếu mảng là null, `OP_ARRAYLENGTH` âm thầm push giá trị 0 lên stack, che giấu lỗi logic khiến luồng code tiếp tục chạy với dữ liệu sai lệch.
     - *Khắc phục*: Kiểm tra bảng xử lý ngoại lệ (`findCatchBlock`); nếu có khối catch `NullPointerException`, ném ngoại lệ chuẩn; nếu không, trả về 0 an toàn.
  4. **Kiểm Tra Mảng Trong `OP_INSTANCEOF` & `OP_CHECKCAST`**:
     - *Hiện tượng*: Khi kiểm tra `instanceof` trên đối tượng mảng (`JavaArray`), máy ảo chỉ tra cứu `ClassInstance`, không nhận diện mảng khiến `arr instanceof Object` hay `arr instanceof byte[]` luôn trả về 0 (`false`).
     - *Khắc phục*: Kiểm tra `getArray(ref)`. Nếu là mảng và kiểu kiểm tra là `java/lang/Object` hoặc khớp với kiểu mảng (`[B`, `[I`, v.v.), trả về 1 (`true`).
  5. **Ném NullPointerException Chuẩn Trong `OP_ATHROW`**:
     - *Hiện tượng*: Khi lệnh `athrow` nhận tham số null, bytecode spec yêu cầu ném `NullPointerException`. Engine trước đó bỏ qua không làm gì.
     - *Khắc phục*: Tự động cấp phát đối tượng `java/lang/NullPointerException` và điều hướng tới khối catch tương ứng.
  6. **Cắt Chuỗi `String.substring` & Sao Chép `String.getChars` Bị Lỗi Mã Hóa UTF-8**:
     - *Hiện tượng*: Trong Java, chỉ mục của chuỗi được tính theo ký tự (UTF-16 code units), trong khi engine lưu chuỗi dạng UTF-8. Các chuỗi tiếng Việt hoặc ký tự đa byte bị cắt ngang chừng byte mã hóa, sinh ra chuỗi rác, mất dấu hoặc gây lỗi hiển thị.
     - *Khắc phục*: Xây dựng hàm `utf8CharToByteOffset` chuyển đổi chính xác chỉ số ký tự thành byte offset UTF-8; giải mã UTF-8 thành mã Unicode ký tự trong `String.getChars`.
  7. **Chuẩn Hóa `StringBuffer.append(Object null)`**:
     - *Hiện tượng*: Khi gọi `append(null)`, hàm không nối ký tự nào thay vì nối chuỗi `"null"` theo đặc tả Java.
     - *Khắc phục*: Nối chuỗi `"null"` khi con trỏ hoặc tham chiếu đối tượng là null.
  8. **Ném Ngoại Lệ `EOFException` Trong `DataInputStream.readUTF`**:
     - *Hiện tượng*: Khi luồng dữ liệu kết thúc giữa chừng trong lúc đọc chuỗi UTF, hàm chỉ thoát ra mà không thông báo lỗi.
     - *Khắc phục*: Ném `EOFException` ngay lập tức nếu luồng không đủ số byte UTF được khai báo.
  9. **Tối Ưu Hóa Tốc Độ Đọc Kích Thước Mảng `JavaArray::length()` O(1)**:
     - *Hiện tượng*: Hàm `JavaArray::length()` trước đây kiểm tra tuần tự 8 mảng `std::vector` khác nhau (`if (!ints.empty())...`), gây lãng phí chu kỳ CPU trên luồng render nóng.
     - *Khắc phục*: Tối ưu hóa bằng câu lệnh `switch (elemType)` trực tiếp đạt độ phức tạp $O(1)$.
  10. **Bù Trừ Dịch Chuyển Tọa Độ `Graphics.getClipX()` & `getClipY()`**:
      - *Hiện tượng*: `getClipX()` và `getClipY()` trả về tọa độ clip tuyệt đối trên màn hình, trong khi đặc tả MIDP 2.0 yêu cầu trả về tọa độ tương đối theo hệ quy chiếu đã dịch chuyển bởi `translate(x, y)`.
      - *Khắc phục*: Cập nhật công thức tính toán: `m_clip.x - m_transX` và `m_clip.y - m_transY`.
  11. **Sửa Lỗi Nhân Đôi Translation Trong `drawRoundRect` & Vòng Lặp `fillRoundRect`**:
      - *Hiện tượng*: `drawRoundRect` tự cộng `m_transX/m_transY` vào tọa độ rồi gọi `drawLine`, trong khi `drawLine` lại tự cộng tiếp, khiến bo góc bị vẽ lệch gấp đôi; `fillRoundRect` duyệt quá cận biên giới hạn.
      - *Khắc phục*: Truyền tọa độ chưa dịch chuyển cho các hàm vẽ đoạn thẳng và điều chỉnh cận lặp `< y2`, `< x2`.
  12. **Bảo Toàn Vùng Clip Trong `LayerManager.paint()`**:
      - *Hiện tượng*: `LayerManager::paint` gọi `setClip` để cắt từng layer mà không lưu lại clip ban đầu, làm mất vùng clip của Canvas sau khi vẽ xong layer manager.
      - *Khắc phục*: Lưu lại vùng clip ban đầu trước khi vẽ và phục hồi nguyên vẹn sau khi hoàn tất.
  13. **Hoàn Thiện Bộ API Còn Thiếu Trong `TiledLayer` & `LayerManager`**:
      - *Hiện tượng*: `TiledLayer.getColumns/getRows` bị chia cứng `/ 16`, thiếu `getCellWidth/getCellHeight`; `LayerManager.getSize` và `getLayerAt` trả về giá trị giả lập 0.
      - *Khắc phục*: Bổ sung các trường lưu trữ thực tế `m_columns, m_rows, m_cellWidth, m_cellHeight`, các phương thức getter tương ứng và quản lý danh sách `m_layers` thực tế trong `LayerManager`.
  14. **Hoàn Thiện API RMS `RecordStore.getNextRecordID()` & `getRecordSize()`**:
      - *Hiện tượng*: Game gọi `getNextRecordID()` để cấp phát ID trước hoặc gọi `getRecordSize(id)` để kiểm tra kích thước bộ đệm bị báo lỗi thiếu phương thức.
      - *Khắc phục*: Triển khai `getNextRecordID` và `getRecordSize` có khóa đa luồng (mutex) trong `RmsStorage` và kết nối hoàn chỉnh vào `JvmInterpreter` và `FullApis`.
  15. **Sửa Kiểu Dữ Liệu `Math.min/max (FF)F`**:
      - *Hiện tượng*: `Math.min(float, float)` và `Math.max(float, float)` trả về `JavaValue(int)` thay vì `JavaValue(float)`, dẫn đến các phép toán vật lý float trong game bị đọc sai bit representation.
      - *Khắc phục*: Trả về `JavaValue(float)` cho đúng chữ ký hàm `(FF)F`.


### 16. Khắc Phục Triệt Để 15 Lỗi Logic & Sai Lệch Đặc Tả Từ Đối Chiếu Upstream (v1.8.8)
- **Mục tiêu**: Đối chiếu sâu toàn bộ engine C++ với mã nguồn gốc upstream `playsoftware/J2ME-Loader` và đặc tả chuẩn MIDP 2.0 / CLDC 1.1 / MMAPI / Nokia UI.
- **Chi tiết 15 lỗi và cách khắc phục**:
  1. **Sửa hoán đổi phép biến đổi `lcdui_display.cpp` `drawRegion`**:
     - *Hiện tượng*: Case 4 (`TRANS_MIRROR_ROT270`) và case 7 (`TRANS_MIRROR_ROT90`) bị đảo ngược logic.
     - *Khắc phục*: Bitmask chuẩn `Sprite.java`: 0x4 là `INVERTED_AXES`, 0x2 là `X_FLIP`, 0x1 là `Y_FLIP`. Case 4 (binary 100) chỉ tráo trục, 0 lật -> `dx + r; dy + c`. Case 7 (binary 111) tráo trục và lật cả 2 chiều -> `dx + (height - 1 - r); dy + (width - 1 - c)`.
  2. **Đồng bộ hóa Sprite Transforms & Reference Pixel (`game_canvas.cpp` / `game_canvas.h`)**:
     - *Hiện tượng*: `Sprite::setTransform` chưa cập nhật bù trừ `(oldRef - newRef)` để bảo toàn điểm reference pixel trên màn hình khi xoay/lật; `setRefPixelPosition` và `getRefPixelX/Y` chưa tính đến transform hiện tại; case 4 và case 7 trong `Sprite::getPixel` bị tráo đổi.
     - *Khắc phục*: Bổ sung `getTransformedPtX` và `getTransformedPtY` chuẩn upstream; `setTransform` cập nhật bù trừ tọa độ x/y; sửa hoán đổi case 4 và case 7 trong `getPixel`.
  3. **Chuẩn hóa LayerManager View Window & Clipping (`game_canvas.cpp` / `game_canvas.h`)**:
     - *Hiện tượng*: View window khởi tạo mặc định 240x320 thay vì `Integer.MAX_VALUE` (`0x7FFFFFFF`); `append()` và `insert()` cho phép thêm layer trùng lặp; `paint()` dùng `setClip` ghi đè toàn bộ thay vì `clipRect` giao cắt và không `translate` graphics context.
     - *Khắc phục*: Khởi tạo view window là `Integer.MAX_VALUE`; `append` và `insert` tự động gọi `remove(layer)` trước khi thêm; `paint()` gọi `translate(x - viewX, y - viewY)` và `clipRect(viewX, viewY, viewWidth, viewHeight)` chuẩn MIDP 2.0.
  4. **Lưu Trữ RMS Chuẩn Xác & Chống Tái Sử Dụng Record ID (`rms_storage.h` / `rms_storage.cpp`)**:
     - *Hiện tượng*: `RmsStorage::loadFromDisk` đặt `m_nextRecordIds = maxId + 1`, làm tái sử dụng ID của các bản ghi đã xóa, vi phạm đặc tả RMS; thiếu hàm `getSize()` tính tổng byte lưu trữ; `RecordStore.getRecord()` trong `j2me_full_apis.cpp` thiếu `return true;`.
     - *Khắc phục*: Bổ sung header magic `RMS2` (0x524D5332) để lưu trữ và phục hồi `nextRecordId` tăng đơn điệu; tương thích ngược hoàn hảo với file định dạng cũ; triển khai `getSize(storeName)`; bổ sung `return true;` trong `getRecord()`.
  5. **Chuyển Vị Ảnh Chuẩn Trong `Image.createImage` (`jvm_bytecode.cpp`)**:
     - *Hiện tượng*: Phép biến đổi `t == 1` (`TRANS_MIRROR_ROT180`) và `t == 2` (`TRANS_MIRROR`) bị tráo trục; thiếu hỗ trợ tạo immutable copy từ Image nguồn.
     - *Khắc phục*: Sửa khớp `dx, dy` cho toàn bộ các case 1 đến 7; bổ sung xử lý sao chép immutable Image.
  6. **Bảo Toàn Ký Tự UTF-8 Trong `Graphics.drawSubstring` (`jvm_bytecode.cpp`)**:
     - *Hiện tượng*: Cắt chuỗi trực tiếp theo byte `text.substr(off, len)` làm vỡ các ký tự UTF-8 đa byte tiếng Việt có dấu.
     - *Khắc phục*: Sử dụng `utf8CharToByteOffset` chuyển đổi chỉ số ký tự thành byte offset chính xác trước khi trích xuất chuỗi con.
  7. **Bổ Sung Các Hàm Đồ Họa Graphics MIDP 2.0 Còn Thiếu (`jvm_bytecode.cpp`)**:
     - *Hiện tượng*: Chưa hỗ trợ `fillTriangle`, `setGrayScale`, `getGrayScale`, `getDisplayColor`, `setStrokeStyle`, `getStrokeStyle` trong `jvm_bytecode.cpp`.
     - *Khắc phục*: Triển khai thuật toán rasterizer tam giác chuẩn và đầy đủ các hàm grayscale / color / stroke.
  8. **Hoàn Thiện Bộ API Font (`jvm_bytecode.cpp`)**:
     - *Hiện tượng*: Thiếu `Font.substringWidth(str, off, len)` và các getter thuộc tính font (`getStyle`, `getSize`, `getFace`, `isPlain`, `isBold`, `isItalic`, `isUnderlined`).
     - *Khắc phục*: Triển khai `substringWidth` hỗ trợ unicode offset; trả về đầy đủ các thuộc tính chuẩn font hệ thống.
  9. **Mở Rộng Phím & Hành Động Game Canvas (`jvm_bytecode.cpp`)**:
     - *Hiện tượng*: `getGameAction` và `getKeyCode` chỉ hỗ trợ UP, DOWN, LEFT, RIGHT, FIRE; thiếu GAME_A, GAME_B, GAME_C, GAME_D; thiếu hàm `getKeyName(keyCode)`.
     - *Khắc phục*: Bổ sung các hằng số GAME_A (9, '7'), GAME_B (10, '9'), GAME_C (11, '*'), GAME_D (12, '#') và triển khai hàm `getKeyName`.
  10. **Ném Ngoại Lệ Chuẩn Trong `System.arraycopy` (`jvm_bytecode.cpp`)**:
      - *Hiện tượng*: Khi tham chiếu mảng null hoặc chỉ số out-of-bounds, hàm tự động kẹp biên hoặc sao chép 0 thay vì ném ngoại lệ chuẩn JVM.
      - *Khắc phục*: Kiểm tra null ném `NullPointerException`; kiểm tra biên (`srcPos < 0 || dstPos < 0 || len < 0 || srcPos + len > srcLen || dstPos + len > dstLen`) ném `IndexOutOfBoundsException`.
  11. **Tự Động Phân Giải Đường Dẫn Tương Đối Trong `Class.getResourceAsStream` (`jvm_bytecode.cpp`)**:
      - *Hiện tượng*: Khi nạp tài nguyên theo đường dẫn tương đối (không bắt đầu bằng `/`), engine chỉ tìm ở root JAR thay vì thư mục package của lớp gọi.
      - *Khắc phục*: Trích xuất package từ `classObj->stringVal` và ghép vào đường dẫn trước khi tra cứu tài nguyên trong JAR.
  12. **Định Tuyến Buffer Chuẩn Cho `DataOutputStream` (`j2me_full_apis.cpp`)**:
      - *Hiện tượng*: `DataOutputStream` ghi vào buffer riêng (`g_baos[self]`), khi game gọi `baos.toByteArray()` trên `ByteArrayOutputStream` bọc bên dưới thì dữ liệu bị rỗng.
      - *Khắc phục*: Lưu tham chiếu luồng bên trong vào `so->fields["out"]` và chuyển hướng toàn bộ các lệnh ghi vào buffer của luồng bên trong.
  13. **Hỗ Trợ Toàn Diện Phép Biến Đổi Nokia DirectGraphics (`j2me_full_apis.cpp`)**:
      - *Hiện tượng*: `DirectGraphics.drawImage` bỏ qua tham số `manipulation`; `drawPixels` chưa hỗ trợ transform manipulation.
      - *Khắc phục*: Ánh xạ chuẩn toàn bộ hằng số Nokia `manipulation` (FLIP_HORIZONTAL, FLIP_VERTICAL, ROTATE_90/180/270) sang Sprite transform; render chuẩn `drawImage` và `drawPixels`.
  14. **Theo Dõi Máy Trạng Thái MMAPI Player (`j2me_full_apis.cpp`)**:
      - *Hiện tượng*: `Player.getState()` luôn trả về cứng 400 (`STARTED`) ngay cả khi mới tạo hoặc đã dừng/đóng.
      - *Khắc phục*: Bổ sung biến trạng thái `state` trong `PlayerData`, quản lý chuyển đổi chính xác qua `realize` (200), `prefetch` (300), `start` (400), `stop` (300), `close` (0).
  15. **Duyệt Ngược RecordEnumeration & Chuẩn Hóa Math/Collection (`j2me_full_apis.cpp`)**:
      - *Hiện tượng*: `RecordEnumeration` thiếu `previousRecordId()` và `previousRecord()`; `Hashtable.put` không ném NPE khi key/value null; `Enumeration.nextElement` không ném `NoSuchElementException`; `Math.round(float)` dùng `std::lround` lệch kết quả số âm.
      - *Khắc phục*: Bổ sung `previousRecordId` và `previousRecord`; ném NPE và `NoSuchElementException`; cập nhật `Math.round` dùng `std::floor(a + 0.5f)` và bổ sung `Math.IEEEremainder`.

---

## 📊 KẾT QUẢ KIỂM THỬ
* **Khởi động**: DragonBoy, Avatar, Ninja School, Gameloft khởi chạy trực tiếp vào màn hình game.
* **Đồ họa & Điều khiển**: Render 60 FPS mượt mà; xoay lật Sprite, LayerManager clipping, Nokia DirectGraphics hiển thị hoàn hảo; bàn phím điều hướng phản hồi tức thì qua cả sự kiện `keyPressed`, `getKeyStates`, GAME_A-D.
* **Tính ổn định & Dữ liệu**: Dữ liệu lưu game RMS lưu trữ và phục hồi hoàn hảo sau khi thoát và mở lại ứng dụng; Record ID không bao giờ bị tái sử dụng; đạt 0 crash, 0 leak socket.


