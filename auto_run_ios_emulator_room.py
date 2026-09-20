import os, sys, time, urllib.request, zipfile, subprocess, threading
import tkinter as tk
from tkinter import ttk, messagebox

ZIP_URL = "https://github.com/PhamTriHien/J2meloader_IOS_Projectt/releases/download/v1.8.3-j2hienloader/J2HienLoader-Simulator-iPad.zip"
TARGET_DIR = r"C:\j2meloader\ios_simulator_room"
ZIP_PATH = os.path.join(TARGET_DIR, "J2HienLoader-Simulator-iPad.zip")

os.makedirs(TARGET_DIR, exist_ok=True)

class IOSRoomDownloader(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("J2HienLoader - Tải & Khởi Chạy Phòng Giả Lập iOS")
        self.geometry("520x300")
        self.configure(bg="#0B0F19")
        self.resizable(False, False)

        lbl_title = tk.Label(self, text="📱 PHÒNG GIẢ LẬP iOS (J2HIENLOADER ROOM)", font=("Segoe UI", 12, "bold"), fg="#38BDF8", bg="#0B0F19")
        lbl_title.pack(pady=(20, 8))

        self.lbl_status = tk.Label(self, text="Đang chuẩn bị tải gói máy ảo iOS...", font=("Segoe UI", 9), fg="#E2E8F0", bg="#0B0F19")
        self.lbl_status.pack(pady=4)

        self.progress = ttk.Progressbar(self, mode="determinate", length=440)
        self.progress.pack(pady=14)

        self.lbl_details = tk.Label(self, text="Dung lượng: ~2.2 MB | Phiên bản: v1.8.3", font=("Segoe UI", 8), fg="#94A3B8", bg="#0B0F19")
        self.lbl_details.pack(pady=4)

        btn_box = tk.Frame(self, bg="#0B0F19")
        btn_box.pack(side=tk.BOTTOM, pady=20)

        self.btn_cancel = tk.Button(btn_box, text="Đóng", font=("Segoe UI", 9, "bold"), bg="#334155", fg="#FFF", relief=tk.FLAT, padx=16, pady=4, cursor="hand2", command=self.destroy)
        self.btn_cancel.pack(side=tk.LEFT, padx=6)

        threading.Thread(target=self.start_download_and_launch, daemon=True).start()

    def update_progress(self, block_num, block_size, total_size):
        if total_size > 0:
            downloaded = block_num * block_size
            percent = min(100, int(downloaded * 100 / total_size))
            self.progress['value'] = percent
            mb_down = downloaded / (1024 * 1024)
            mb_total = total_size / (1024 * 1024)
            self.lbl_status.config(text=f"Đang tải gói iOS Simulator: {percent}% ({mb_down:.1f} MB / {mb_total:.1f} MB)")
            self.update_idletasks()

    def start_download_and_launch(self):
        try:
            self.lbl_status.config(text="Đang kết nối đến GitHub Releases...")
            req = urllib.request.Request(ZIP_URL, headers={"User-Agent": "Mozilla/5.0"})
            
            with urllib.request.urlopen(req) as resp, open(ZIP_PATH, 'wb') as out_file:
                total_size = int(resp.info().get('Content-Length', -1))
                downloaded = 0
                block_size = 65536
                while True:
                    buf = resp.read(block_size)
                    if not buf:
                        break
                    out_file.write(buf)
                    downloaded += len(buf)
                    if total_size > 0:
                        percent = min(100, int(downloaded * 100 / total_size))
                        self.progress['value'] = percent
                        mb_down = downloaded / (1024 * 1024)
                        mb_total = total_size / (1024 * 1024)
                        self.lbl_status.config(text=f"Đang tải gói iOS Simulator: {percent}% ({mb_down:.1f} MB / {mb_total:.1f} MB)")

            self.lbl_status.config(text="Đang giải nén gói ứng dụng iOS...")
            self.progress['value'] = 100
            
            with zipfile.ZipFile(ZIP_PATH, 'r') as z:
                z.extractall(TARGET_DIR)

            self.lbl_status.config(text="Giải nén hoàn tất! Đang khởi chạy Phòng Giả Lập iOS...")
            time.sleep(1)

            # Launch iOS Emulator Testbed
            testbed_py = r"C:\j2meloader\J2HienLoader_iOS_Test_Emulator.py"
            subprocess.Popen([sys.executable, testbed_py], cwd=r"C:\j2meloader")

            time.sleep(0.5)
            self.destroy()

        except Exception as e:
            self.lbl_status.config(text=f"Lỗi: {e}")
            messagebox.showerror("Lỗi", f"Không thể tải hoặc giải nén gói iOS:\n{e}")

if __name__ == "__main__":
    app = IOSRoomDownloader()
    app.mainloop()
