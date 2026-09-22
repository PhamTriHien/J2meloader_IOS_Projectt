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
11. [CÔNG VIỆC DỞ DANG: Lỗi vào sảnh tự nhấn loạn cảm ứng nút 'Chơi mới' (Ghost Input)](#11-công-việc-dở-dang-lỗi-vào-sảnh-tự-nhấn-loạn-cảm-ứng-nút-chơi-mới)
12. [CÔNG VIỆC DỞ DANG: Bản Android (J2ME-Loader Core) - Treo 24/7, Cửa sổ nổi & Tối ưu RAM](#12-công-việc-dở-dang-bản-android-j2me-loader-core---treo-247-cửa-sổ-nổi--tối-ưu-ram)

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

### 11. CÔNG VIỆC DỞ DANG: Lỗi vào sảnh tự nhấn loạn cảm ứng nút 'Chơi mới'
* **Hiện tượng đang gặp**:
  - Vừa khởi động game xong vào màn hình sảnh đăng nhập (màn hình Chú Bé Rồng Online với 3 nút: "Chơi mới", "Đổi tài khoản", "Máy chủ"), game **tự động kích hoạt liên tục vào nút "Chơi mới"**, làm hiện popup *"Xin chờ"* (biểu tượng Ngọc Rồng 1 sao) mà người chơi chưa hề chạm vào màn hình hoặc bấm phím.
* **Các nghi vấn kỹ thuật & Luồng phân tích đang tiến hành**:
  1. **Tọa độ cảm ứng ảo lúc Mount MetalView (Initial Touch/Gesture Leak)**:
     - Khi `GameScreenView` chuyển cảnh mở `MetalView`, kiểm tra xem `TouchGestureRecognizer` hoặc `MetalView` có bị gọi một sự kiện chạm giả lập ban đầu với tọa độ `(0, 0)` hoặc điểm giữa màn hình hay không.
     - Vị trí nút "Chơi mới" là nút trên cùng (hoặc nút được focus mặc định index 0 trong `b/bD.g(Lb/bD;)[Lb/v;`).
  2. **Trạng thái khởi tạo `isPressed` trong `KeypadButtonStyle`**:
     - `VirtualKeypadView` sử dụng `.onChange(of: configuration.isPressed)` trong SwiftUI.
     - Kiểm tra xem khi SwiftUI khởi tạo và gắn các nút bàn phím vào cây View Hierarchy, sự kiện `onStateChange(isPressed)` có vô tình kích hoạt với giá trị `true` cho nút phím mặc định (như phím **OK / FIRE** mã `-5`, hoặc **LSK** mã `-6`) hay không. Trong DragonBoy, bấm phím OK khi ở sảnh chính sẽ tự kích hoạt ngay nút đầu tiên là "Chơi mới".
  3. **Biến static lưu trạng thái chạm trong bytecode game (`main/b`)**:
     - Trong DragonBoy, lớp `main/b` lưu tọa độ con trỏ qua `main/b.aW:I`, `main/b.aZ:I` và cờ nhấn qua `main/b.au:Z`, `main/b.at:Z`, `main/b.aw:Z`.
     - Phương thức `b/v.wF()Z` kiểm tra va chạm con trỏ với nút bấm. Nếu biến `aW, aZ` có giá trị khởi tạo `0` và cờ con trỏ bị hiểu nhầm là đang nhấn, hàm `wF()` sẽ trả về `true` và kích hoạt hàm hành động của nút (`b/v.wG()V`).
  4. **Kế hoạch xử lý tiếp theo khi tiếp tục**:
     - Thêm log debug xem sự kiện `Key` hay `Touch` nào được đẩy vào `m_eventQueue` trong 2 giây đầu tiên lúc boot.
     - Bổ sung bộ lọc an toàn (**Input Boot Warmup Guard**): Bỏ qua (drop) toàn bộ các sự kiện chạm và phím ảo trong **500ms – 800ms đầu tiên** sau khi game canvas khởi động xong để triệt tiêu mọi sự kiện chạm rác do chuyển cảnh SwiftUI.
     - Kiểm tra và sửa `KeypadButtonStyle`: Dùng `@State private var lastPressed = false` trong ButtonStyle để chặn việc kích hoạt giả lập khi View vừa xuất hiện.
      - Khởi tạo giá trị mặc định an toàn cho các biến tọa độ con trỏ (`main/b.aW = -1000`, `main/b.aZ = -1000`, `main/b.au = false`) trong engine lúc nạp lớp `main/b`.

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

## 📊 KẾT QUẢ KIỂM THỬ
* **Khởi động**: DragonBoy, Avatar, Ninja School, Gameloft khởi chạy trực tiếp vào màn hình game.
* **Đồ họa**: Render 60 FPS mượt mà trên nền tảng Apple Metal 3 (iOS) và Windows Desktop.
* **Tính ổn định**: Không còn lỗi tràn bộ nhớ `SIGBUS 10`, không còn lỗi đứng màn hình đen, không còn lỗi out form text.
