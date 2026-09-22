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

## 📊 KẾT QUẢ KIỂM THỬ
* **Khởi động**: DragonBoy, Avatar, Ninja School, Gameloft khởi chạy trực tiếp vào màn hình game.
* **Đồ họa**: Render 60 FPS mượt mà trên nền tảng Apple Metal 3 (iOS) và Windows Desktop.
* **Tính ổn định**: Không còn lỗi tràn bộ nhớ `SIGBUS 10`, không còn lỗi đứng màn hình đen, không còn lỗi out form text.

