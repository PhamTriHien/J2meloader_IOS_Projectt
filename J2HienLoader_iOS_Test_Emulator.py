"""
J2HienLoader iOS Emulator & Testing Tool
Trình Giả Lập & Kiểm Thử Môi Trường iOS cho J2HienLoader (iPhone / iPad)
Phát triển bởi Phạm Trí Hiện
"""

import os, sys, time, json, zipfile, subprocess, threading, webbrowser, urllib.request
import ctypes

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    try:
        ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from PIL import Image, ImageTk, ImageDraw, ImageFont

COLOR_BG = "#0B0F19"
COLOR_CARD = "#151C2C"
COLOR_ACCENT = "#38BDF8"
COLOR_PHONE_BODY = "#1E293B"
COLOR_SCREEN_BG = "#000000"
COLOR_TEXT = "#F8FAFC"
COLOR_TEXT_MUTED = "#94A3B8"
COLOR_KEYPAD_BG = "#1A2234"
COLOR_KEYPAD_BTN = "#253146"
COLOR_KEYPAD_ACTIVE = "#38BDF8"
COLOR_BTN_LSK = "#3B82F6"
COLOR_BTN_RSK = "#EF4444"
COLOR_BTN_OK = "#0EA5E9"

FREEJ2ME_JAR = r"C:\j2meloader\desktop_runner\freej2me.jar"
SYSTEM_DIR = r"C:\j2meloader\desktop_runner\freej2me_system"

class IOSTestEmulatorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        try:
            dpi = self.winfo_fpixels('1i')
            self.tk.call('tk', 'scaling', dpi / 72.0)
        except Exception:
            pass

        self.title("J2HienLoader - iOS Simulator Test & Debugging Tool")
        self.geometry("980x860")
        self.minsize(880, 720)
        self.configure(bg=COLOR_BG)

        self.current_game_path = None
        self.current_game_title = "Chưa chọn Game"
        self.game_proc = None
        self.fps_val = 60
        self.speed_multiplier = 1
        self.shader_mode = "Nearest"
        self.is_running = False

        self.setup_ui()

    def setup_ui(self):
        # Header Toolbar
        hdr = tk.Frame(self, bg=COLOR_CARD, height=54)
        hdr.pack(fill=tk.X, side=tk.TOP)
        hdr.pack_propagate(False)

        lbl_logo = tk.Label(hdr, text="📱 J2HienLoader iOS Emulator Testbed", font=("Segoe UI", 12, "bold"), fg=COLOR_TEXT, bg=COLOR_CARD)
        lbl_logo.pack(side=tk.LEFT, padx=16)

        btn_appetize = tk.Button(hdr, text="🌐 Mở Appetize.io Web Simulator", font=("Segoe UI", 9, "bold"), bg="#8B5CF6", fg="#FFF", activebackground="#7C3AED", activeforeground="#FFF", relief=tk.FLAT, padx=12, pady=4, cursor="hand2", command=self.open_appetize)
        btn_appetize.pack(side=tk.RIGHT, padx=12, pady=10)

        btn_download_zip = tk.Button(hdr, text="📥 Tải iOS Simulator .ZIP Mới Nhất", font=("Segoe UI", 9, "bold"), bg="#0284C7", fg="#FFF", activebackground="#0369A1", activeforeground="#FFF", relief=tk.FLAT, padx=12, pady=4, cursor="hand2", command=self.download_latest_simulator_zip)
        btn_download_zip.pack(side=tk.RIGHT, padx=6, pady=10)

        btn_load_jar = tk.Button(hdr, text="📂 Chọn Game (.JAR)", font=("Segoe UI", 9, "bold"), bg=COLOR_ACCENT, fg="#0F172A", activebackground="#0284C7", activeforeground="#FFF", relief=tk.FLAT, padx=12, pady=4, cursor="hand2", command=self.browse_jar)
        btn_load_jar.pack(side=tk.RIGHT, padx=6, pady=10)

        # Main Body: Left Phone Frame, Right Debug & Control Panel
        body = tk.Frame(self, bg=COLOR_BG)
        body.pack(fill=tk.BOTH, expand=True, padx=16, pady=12)

        # Left: iOS Phone Enclosure
        self.phone_container = tk.Frame(body, bg=COLOR_BG, width=460)
        self.phone_container.pack(side=tk.LEFT, fill=tk.BOTH, expand=False, padx=(0, 12))

        self.phone_bezel = tk.Frame(self.phone_container, bg=COLOR_PHONE_BODY, bd=0, highlightthickness=3, highlightbackground="#334155")
        self.phone_bezel.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Dynamic Island / Status Bar
        status_bar = tk.Frame(self.phone_bezel, bg=COLOR_PHONE_BODY, height=36)
        status_bar.pack(fill=tk.X, side=tk.TOP)
        status_bar.pack_propagate(False)

        lbl_time = tk.Label(status_bar, text="9:41", font=("Segoe UI", 9, "bold"), fg=COLOR_TEXT, bg=COLOR_PHONE_BODY)
        lbl_time.pack(side=tk.LEFT, padx=18)

        island = tk.Frame(status_bar, bg="#000000", width=90, height=20)
        island.pack(side=tk.TOP, pady=6)

        lbl_icons = tk.Label(status_bar, text="5G  🔋 100%", font=("Segoe UI", 8, "bold"), fg=COLOR_TEXT, bg=COLOR_PHONE_BODY)
        lbl_icons.pack(side=tk.RIGHT, padx=16)

        # In-Game Top Nav Bar (mimics GameScreenView.swift)
        self.nav_bar = tk.Frame(self.phone_bezel, bg="#111827", height=40)
        self.nav_bar.pack(fill=tk.X, side=tk.TOP)
        self.nav_bar.pack_propagate(False)

        self.lbl_game_title = tk.Label(self.nav_bar, text=self.current_game_title, font=("Segoe UI", 9, "bold"), fg=COLOR_TEXT, bg="#111827")
        self.lbl_game_title.pack(side=tk.LEFT, padx=12)

        self.lbl_fps_badge = tk.Label(self.nav_bar, text="60 FPS", font=("Consolas", 8, "bold"), fg="#4ADE80", bg="#000000", padx=4, pady=1)
        self.lbl_fps_badge.pack(side=tk.RIGHT, padx=12)

        # Game Canvas Screen Area
        self.canvas_frame = tk.Frame(self.phone_bezel, bg=COLOR_SCREEN_BG, height=320, width=240)
        self.canvas_frame.pack(fill=tk.BOTH, expand=True, padx=12, pady=6)

        self.game_display = tk.Label(self.canvas_frame, bg=COLOR_SCREEN_BG, text="Chưa khởi động Game\nBấm [Chọn Game (.JAR)] để chạy", font=("Segoe UI", 10), fg=COLOR_TEXT_MUTED)
        self.game_display.pack(fill=tk.BOTH, expand=True)

        # Virtual iOS Keypad (mimics VirtualKeypadView.swift)
        self.setup_ios_keypad()

        # Right: Testing & Diagnosis Panel
        right_panel = tk.Frame(body, bg=COLOR_CARD)
        right_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        lbl_dbg_title = tk.Label(right_panel, text="🛠️ Bảng Điều Khiển & Nhật Ký Kiểm Thử (Live Test Logs)", font=("Segoe UI", 11, "bold"), fg=COLOR_TEXT, bg=COLOR_CARD)
        lbl_dbg_title.pack(anchor=tk.W, padx=14, pady=10)

        # Controls Row
        ctrl_box = tk.Frame(right_panel, bg=COLOR_CARD)
        ctrl_box.pack(fill=tk.X, padx=14, pady=4)

        btn_run = tk.Button(ctrl_box, text="▶ Khởi Chạy Lại", font=("Segoe UI", 9, "bold"), bg="#10B981", fg="#FFF", relief=tk.FLAT, padx=10, pady=4, cursor="hand2", command=self.restart_current_game)
        btn_run.pack(side=tk.LEFT, padx=(0, 6))

        btn_stop = tk.Button(ctrl_box, text="⏹ Dừng Game", font=("Segoe UI", 9, "bold"), bg="#EF4444", fg="#FFF", relief=tk.FLAT, padx=10, pady=4, cursor="hand2", command=self.stop_current_game)
        btn_stop.pack(side=tk.LEFT, padx=6)

        btn_native_input = tk.Button(ctrl_box, text="⌨️ Test Prompt iOS Native Input", font=("Segoe UI", 9, "bold"), bg="#6366F1", fg="#FFF", relief=tk.FLAT, padx=10, pady=4, cursor="hand2", command=self.test_native_ios_input_modal)
        btn_native_input.pack(side=tk.LEFT, padx=6)

        btn_shot = tk.Button(ctrl_box, text="📸 Chụp Màn Hình", font=("Segoe UI", 9, "bold"), bg="#475569", fg="#FFF", relief=tk.FLAT, padx=10, pady=4, cursor="hand2", command=self.take_test_screenshot)
        btn_shot.pack(side=tk.LEFT, padx=6)

        # Log Console
        self.txt_log = tk.Text(right_panel, bg="#0A0F1D", fg="#E2E8F0", font=("Consolas", 9), bd=0, wrap=tk.WORD)
        self.txt_log.pack(fill=tk.BOTH, expand=True, padx=14, pady=10)

        # Auto select DragonBoy1.jar if exists
        default_jar = r"C:\Users\PhamTriHien\Downloads\DragonBoy1.jar"
        if os.path.exists(default_jar):
            self.load_game(default_jar)

    def setup_ios_keypad(self):
        kp = tk.Frame(self.phone_bezel, bg=COLOR_KEYPAD_BG, height=270)
        kp.pack(fill=tk.X, side=tk.BOTTOM, padx=8, pady=(0, 8))

        # Softkeys + D-Pad Row
        row1 = tk.Frame(kp, bg=COLOR_KEYPAD_BG)
        row1.pack(fill=tk.X, pady=4)

        btn_lsk = tk.Button(row1, text="LSK (F1)", font=("Segoe UI", 9, "bold"), bg=COLOR_BTN_LSK, fg="#FFF", relief=tk.FLAT, width=8, pady=4, cursor="hand2", command=lambda: self.send_key_press(112))
        btn_lsk.pack(side=tk.LEFT, padx=6)

        dpad_frame = tk.Frame(row1, bg=COLOR_KEYPAD_BG)
        dpad_frame.pack(side=tk.LEFT, expand=True)

        btn_up = tk.Button(dpad_frame, text="▲", font=("Segoe UI", 9, "bold"), bg=COLOR_KEYPAD_BTN, fg="#FFF", relief=tk.FLAT, width=4, command=lambda: self.send_key_press(38))
        btn_up.grid(row=0, column=1, padx=2, pady=1)

        btn_left = tk.Button(dpad_frame, text="◀", font=("Segoe UI", 9, "bold"), bg=COLOR_KEYPAD_BTN, fg="#FFF", relief=tk.FLAT, width=4, command=lambda: self.send_key_press(37))
        btn_left.grid(row=1, column=0, padx=2, pady=1)

        btn_ok = tk.Button(dpad_frame, text="OK", font=("Segoe UI", 8, "bold"), bg=COLOR_BTN_OK, fg="#FFF", relief=tk.FLAT, width=4, command=lambda: self.send_key_press(10))
        btn_ok.grid(row=1, column=1, padx=2, pady=1)

        btn_right = tk.Button(dpad_frame, text="▶", font=("Segoe UI", 9, "bold"), bg=COLOR_KEYPAD_BTN, fg="#FFF", relief=tk.FLAT, width=4, command=lambda: self.send_key_press(39))
        btn_right.grid(row=1, column=2, padx=2, pady=1)

        btn_down = tk.Button(dpad_frame, text="▼", font=("Segoe UI", 9, "bold"), bg=COLOR_KEYPAD_BTN, fg="#FFF", relief=tk.FLAT, width=4, command=lambda: self.send_key_press(40))
        btn_down.grid(row=2, column=1, padx=2, pady=1)

        btn_rsk = tk.Button(row1, text="RSK (F2)", font=("Segoe UI", 9, "bold"), bg=COLOR_BTN_RSK, fg="#FFF", relief=tk.FLAT, width=8, pady=4, cursor="hand2", command=lambda: self.send_key_press(113))
        btn_rsk.pack(side=tk.RIGHT, padx=6)

        # Numpad 4x3 Grid
        numpad_frame = tk.Frame(kp, bg=COLOR_KEYPAD_BG)
        numpad_frame.pack(pady=4)

        keys = [
            ("1 ._@", 49), ("2 abc", 50), ("3 def", 51),
            ("4 ghi", 52), ("5 jkl", 53), ("6 mno", 54),
            ("7 pqrs", 55), ("8 tuv", 56), ("9 wxyz", 57),
            ("*", 106), ("0 +", 48), ("#", 520)
        ]

        for i, (label, code) in enumerate(keys):
            r, c = divmod(i, 3)
            b = tk.Button(numpad_frame, text=label, font=("Segoe UI", 8, "bold"), bg=COLOR_KEYPAD_BTN, fg=COLOR_TEXT, relief=tk.FLAT, width=9, pady=3, cursor="hand2", command=lambda cd=code: self.send_key_press(cd))
            b.grid(row=r, column=c, padx=3, pady=2)

    def log(self, text):
        self.txt_log.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] {text}\n")
        self.txt_log.see(tk.END)

    def browse_jar(self):
        f = filedialog.askopenfilename(title="Chọn Game Java (.jar)", filetypes=[("Java Archive", "*.jar"), ("All Files", "*.*")])
        if f:
            self.load_game(f)

    def load_game(self, jar_path):
        self.current_game_path = jar_path
        self.current_game_title = os.path.basename(jar_path)
        self.lbl_game_title.config(text=self.current_game_title)
        self.log(f"Đã nạp file game: {jar_path}")
        self.restart_current_game()

    def restart_current_game(self):
        if not self.current_game_path or not os.path.exists(self.current_game_path):
            messagebox.showwarning("Cảnh báo", "Vui lòng chọn file game .jar trước!")
            return

        self.stop_current_game()
        self.log("Đang khởi tạo môi trường máy ảo iOS...")

        def _run():
            try:
                cmd = [
                    "java",
                    "-Dsun.java2d.dpiaware=true",
                    "-Dfile.encoding=ISO_8859_1",
                    "-jar",
                    FREEJ2ME_JAR,
                    self.current_game_path
                ]
                self.game_proc = subprocess.Popen(cmd, cwd=os.path.dirname(FREEJ2ME_JAR), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
                self.is_running = True
                self.log("Game đã chạy thành công trong môi trường máy ảo!")
                for line in self.game_proc.stdout:
                    if line.strip():
                        self.log(line.strip())
            except Exception as e:
                self.log(f"Lỗi khởi chạy: {e}")

        threading.Thread(target=_run, daemon=True).start()

    def stop_current_game(self):
        if self.game_proc:
            try:
                self.game_proc.kill()
                self.log("Đã dừng game.")
            except Exception:
                pass
            self.game_proc = None
            self.is_running = False

    def send_key_press(self, vk):
        self.log(f"Gửi sự kiện phím VK_{vk}")
        try:
            import win32gui, win32con
            hwnd = None
            def enum_cb(h, extra):
                nonlocal hwnd
                t = win32gui.GetWindowText(h)
                if "FreeJ2ME" in t or (self.current_game_title and self.current_game_title[:8] in t):
                    hwnd = h
            win32gui.EnumWindows(enum_cb, None)
            if hwnd:
                user32.keybd_event(vk, 0, 0, 0)
                time.sleep(0.05)
                user32.keybd_event(vk, 0, win32con.KEYEVENTF_KEYUP, 0)
        except Exception as e:
            self.log(f"Lỗi gửi phím: {e}")

    def test_native_ios_input_modal(self):
        # Native iOS UIAlertController Simulator
        modal = tk.Toplevel(self)
        modal.title("iOS Native Input")
        modal.geometry("340x220")
        modal.configure(bg="#22272E")
        modal.resizable(False, False)
        modal.transient(self)
        modal.grab_set()

        lbl_t = tk.Label(modal, text="Nhập văn bản", font=("Segoe UI", 12, "bold"), fg="#FFF", bg="#22272E")
        lbl_t.pack(pady=(16, 4))

        lbl_m = tk.Label(modal, text="Nhập nội dung bằng bàn phím hệ thống iOS:", font=("Segoe UI", 9), fg="#94A3B8", bg="#22272E")
        lbl_m.pack(pady=(0, 12))

        txt_in = tk.Entry(modal, font=("Segoe UI", 11), bg="#161B22", fg="#FFF", insertbackground="#38BDF8", bd=1, relief=tk.SOLID)
        txt_in.pack(fill=tk.X, padx=24, pady=8)
        txt_in.focus_set()

        btn_box = tk.Frame(modal, bg="#22272E")
        btn_box.pack(fill=tk.X, side=tk.BOTTOM, padx=24, pady=16)

        def on_done():
            val = txt_in.get()
            self.log(f"iOS Native Keyboard Input: '{val}'")
            modal.destroy()

        btn_cancel = tk.Button(btn_box, text="Hủy", font=("Segoe UI", 9, "bold"), bg="#475569", fg="#FFF", relief=tk.FLAT, padx=12, pady=4, cursor="hand2", command=modal.destroy)
        btn_cancel.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(0, 6))

        btn_submit = tk.Button(btn_box, text="Xong", font=("Segoe UI", 9, "bold"), bg="#38BDF8", fg="#0F172A", relief=tk.FLAT, padx=12, pady=4, cursor="hand2", command=on_done)
        btn_submit.pack(side=tk.RIGHT, expand=True, fill=tk.X, padx=(6, 0))

    def take_test_screenshot(self):
        os.makedirs(r"C:\j2meloader\test_screenshots", exist_ok=True)
        out = os.path.join(r"C:\j2meloader\test_screenshots", f"ios_test_{int(time.time())}.png")
        try:
            from PIL import ImageGrab
            img = ImageGrab.grab()
            img.save(out)
            self.log(f"Đã lưu ảnh chụp màn hình: {out}")
            messagebox.showinfo("Thành công", f"Đã lưu ảnh chụp màn hình vào:\n{out}")
        except Exception as e:
            self.log(f"Lỗi chụp ảnh: {e}")

    def download_latest_simulator_zip(self):
        url = "https://github.com/PhamTriHien/J2meloader_IOS_Projectt/releases/download/v1.8.3-j2hienloader/J2HienLoader-Simulator-iPad.zip"
        self.log(f"Mở liên kết tải iOS Simulator Bundle: {url}")
        webbrowser.open(url)

    def open_appetize(self):
        self.log("Mở trang web Appetize.io để upload và chạy thử iOS App trên trình duyệt...")
        webbrowser.open("https://appetize.io/upload")

if __name__ == "__main__":
    app = IOSTestEmulatorApp()
    app.mainloop()
