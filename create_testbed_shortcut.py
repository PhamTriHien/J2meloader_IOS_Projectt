import os, sys, win32com.client

desktop = os.path.join(os.environ['USERPROFILE'], 'Desktop')
target = sys.executable
args = r"C:\j2meloader\J2HienLoader_iOS_Test_Emulator.py"
icon = r"C:\j2meloader\desktop_runner\app_icon.ico"
shortcut_path = os.path.join(desktop, "J2HienLoader iOS Testbed.lnk")

shell = win32com.client.Dispatch("WScript.Shell")
shortcut = shell.CreateShortCut(shortcut_path)
shortcut.Targetpath = target
shortcut.Arguments = f'"{args}"'
shortcut.WorkingDirectory = r"C:\j2meloader"
shortcut.IconLocation = icon
shortcut.Description = "J2HienLoader - Công cụ Giả lập & Kiểm thử iOS"
shortcut.save()
print("Created Desktop shortcut for iOS Testbed at:", shortcut_path)
