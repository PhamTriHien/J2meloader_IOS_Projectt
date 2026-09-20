import os, sys, win32com.client

desktop = os.path.join(os.environ['USERPROFILE'], 'Desktop')
target = sys.executable
args = r"C:\j2meloader\auto_run_ios_emulator_room.py"
icon = r"C:\j2meloader\desktop_runner\app_icon.ico"
shortcut_path = os.path.join(desktop, "J2HienLoader iOS Room.lnk")

shell = win32com.client.Dispatch("WScript.Shell")
shortcut = shell.CreateShortCut(shortcut_path)
shortcut.Targetpath = target
shortcut.Arguments = f'"{args}"'
shortcut.WorkingDirectory = r"C:\j2meloader"
shortcut.IconLocation = icon
shortcut.Description = "J2HienLoader - Tự Động Tải & Chạy Phòng Giả Lập iOS"
shortcut.save()
print("Created Desktop shortcut for Auto iOS Room at:", shortcut_path)
