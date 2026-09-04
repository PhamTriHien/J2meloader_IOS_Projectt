# J2ME-Loader Desktop Simulator
import os, sys, time, math, zipfile, threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog, simpledialog

try:
    import winsound
    HAS_SOUND = True
except ImportError:
    HAS_SOUND = False

COLOR_BG_DARK = "#121212"
COLOR_TOOLBAR = "#212121"
COLOR_TOOLBAR_BORDER = "#333333"
COLOR_FAB = "#FF2E51"
COLOR_FAB_PRESSED = "#D81B3F"
COLOR_TEXT_PRIMARY = "#FFFFFF"
COLOR_TEXT_SECONDARY = "#AAAAAA"
COLOR_TEXT_MUTED = "#777777"
COLOR_ITEM_HOVER = "#242424"
COLOR_KEYPAD_BG = "#1A1A1A"
COLOR_KEYPAD_BTN = "#2D2D2D"
COLOR_KEYPAD_BTN_ACTIVE = "#FF2E51"
COLOR_KEYPAD_TEXT = "#E0E0E0"
COLOR_KEYPAD_SUB = "#888888"

def play_tone(freq=520, duration_ms=60):
    if not HAS_SOUND: return
    def _run():
        try: winsound.Beep(max(37, min(32767, int(freq))), max(10, duration_ms))
        except: pass
    threading.Thread(target=_run, daemon=True).start()

class GameModel:
    def __init__(self, title, vendor, version, icon_color="#FF2E51", jar_path=None):
        self.title = title
        self.vendor = vendor
        self.version = version
        self.icon_color = icon_color
        self.jar_path = jar_path
        self.width = 240
        self.height = 320
        self.shader = "Nearest (Pixel Sharp)"
        self.fps_limit = 60
        self.speed = 1.0
        self.keypad_layout = "Classic Phone (3x4 + D-Pad)"
        self.sound_enabled = True

class J2MELoaderApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("J2ME-Loader (iOS / Android 100% UI Preview)")
        self.geometry("440x860")
        self.minsize(400, 700)
        self.configure(bg=COLOR_BG_DARK)

        self.games = [
            GameModel("Bounce Tales", "Nokia", "2.0.26", "#FF3B30"),
            GameModel("Diamond Rush", "Gameloft", "1.1.8", "#007AFF"),
            GameModel("Tower Bloxx", "Digital Chocolate", "1.0.4", "#FF9500"),
            GameModel("Asphalt 3: 3D", "Gameloft", "1.2.0", "#FF2D55"),
            GameModel("Doom RPG", "id Software / EA", "1.0.9", "#5856D6"),
            GameModel("Sonic The Hedgehog", "Sega", "1.0.0", "#34C759"),
            GameModel("Ninja School", "Teamobi", "1.4.2", "#AF52DE"),
            GameModel("Avatar 2D", "Teamobi", "2.5.0", "#5AC8FA"),
            GameModel("Khí Phách Anh Hùng", "Teamobi", "1.7.5", "#FFCC00"),
        ]
        self.filtered_games = list(self.games)
        self.current_game = None
        self.is_search_open = False
        self.search_query = ""

        self.container = tk.Frame(self, bg=COLOR_BG_DARK)
        self.container.pack(fill="both", expand=True)
        self.show_library_view()

    def clear_container(self):
        for widget in self.container.winfo_children():
            widget.destroy()

    def show_library_view(self):
        self.clear_container()
        self.current_game = None

        toolbar = tk.Frame(self.container, bg=COLOR_TOOLBAR, height=56)
        toolbar.pack(fill="x", side="top")
        toolbar.pack_propagate(False)

        if not self.is_search_open:
            lbl_title = tk.Label(toolbar, text="J2ME-Loader", font=("Segoe UI", 14, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR)
            lbl_title.pack(side="left", padx=16, pady=12)

            btn_menu = tk.Button(toolbar, text="⋮", font=("Segoe UI", 14, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, activebackground="#333333", activeforeground=COLOR_TEXT_PRIMARY, relief="flat", bd=0, command=self.show_overflow_menu, cursor="hand2")
            btn_menu.pack(side="right", padx=12)

            btn_sort = tk.Button(toolbar, text="⇅", font=("Segoe UI", 13, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, activebackground="#333333", activeforeground=COLOR_TEXT_PRIMARY, relief="flat", bd=0, command=self.sort_games, cursor="hand2")
            btn_sort.pack(side="right", padx=6)

            btn_search = tk.Button(toolbar, text="🔍", font=("Segoe UI", 11), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, activebackground="#333333", activeforeground=COLOR_TEXT_PRIMARY, relief="flat", bd=0, command=self.toggle_search, cursor="hand2")
            btn_search.pack(side="right", padx=6)
        else:
            btn_back_search = tk.Button(toolbar, text="←", font=("Segoe UI", 13, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, relief="flat", bd=0, command=self.toggle_search, cursor="hand2")
            btn_back_search.pack(side="left", padx=12)

            self.ent_search = tk.Entry(toolbar, font=("Segoe UI", 12), bg="#2D2D2D", fg=COLOR_TEXT_PRIMARY, insertbackground=COLOR_TEXT_PRIMARY, relief="flat")
            self.ent_search.pack(side="left", fill="x", expand=True, padx=8, pady=10)
            self.ent_search.insert(0, self.search_query)
            self.ent_search.focus_set()
            self.ent_search.bind("<KeyRelease>", self.on_search_change)

            btn_clear = tk.Button(toolbar, text="✕", font=("Segoe UI", 11), fg=COLOR_TEXT_SECONDARY, bg=COLOR_TOOLBAR, relief="flat", bd=0, command=self.clear_search, cursor="hand2")
            btn_clear.pack(side="right", padx=12)

        div = tk.Frame(self.container, bg=COLOR_TOOLBAR_BORDER, height=1)
        div.pack(fill="x", side="top")

        list_container = tk.Frame(self.container, bg=COLOR_BG_DARK)
        list_container.pack(fill="both", expand=True)

        canvas = tk.Canvas(list_container, bg=COLOR_BG_DARK, highlightthickness=0, bd=0)
        scrollbar = ttk.Scrollbar(list_container, orient="vertical", command=canvas.yview)
        self.scroll_content = tk.Frame(canvas, bg=COLOR_BG_DARK)

        self.scroll_content.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas_window = canvas.create_window((0, 0), window=self.scroll_content, anchor="nw")
        canvas.bind("<Configure>", lambda event: canvas.itemconfig(canvas_window, width=event.width))
        canvas.bind_all("<MouseWheel>", lambda event: canvas.yview_scroll(int(-1*(event.delta/120)), "units"))

        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        self.populate_game_list()

        fab = tk.Button(self.container, text="+", font=("Segoe UI", 24, "bold"), fg="#FFFFFF", bg=COLOR_FAB, activebackground=COLOR_FAB_PRESSED, activeforeground="#FFFFFF", relief="flat", bd=0, cursor="hand2", command=self.import_jar_file)
        fab.place(relx=0.84, rely=0.91, width=58, height=58, anchor="center")

    def populate_game_list(self):
        for w in self.scroll_content.winfo_children(): w.destroy()

        if not self.filtered_games:
            lbl_empty = tk.Label(self.scroll_content, text="No apps in library.\nTap '+' to install .jar or .jad file.", font=("Segoe UI", 11), fg=COLOR_TEXT_MUTED, bg=COLOR_BG_DARK, pady=60)
            lbl_empty.pack(fill="x")
            return

        for idx, game in enumerate(self.filtered_games):
            row = tk.Frame(self.scroll_content, bg=COLOR_BG_DARK, cursor="hand2")
            row.pack(fill="x", padx=6, pady=2)

            icon_canvas = tk.Canvas(row, width=38, height=38, bg=COLOR_BG_DARK, highlightthickness=0)
            icon_canvas.pack(side="left", padx=(10, 12), pady=8)
            icon_canvas.create_oval(2, 2, 36, 36, fill=game.icon_color, outline="")
            letter = game.title[0].upper() if game.title else "J"
            icon_canvas.create_text(19, 19, text=letter, fill="#FFFFFF", font=("Segoe UI", 14, "bold"))

            text_frame = tk.Frame(row, bg=COLOR_BG_DARK)
            text_frame.pack(side="left", fill="x", expand=True, pady=6)

            lbl_title = tk.Label(text_frame, text=game.title, font=("Segoe UI", 11, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_BG_DARK, anchor="w")
            lbl_title.pack(fill="x")

            sub_frame = tk.Frame(text_frame, bg=COLOR_BG_DARK)
            sub_frame.pack(fill="x")

            lbl_vendor = tk.Label(sub_frame, text=game.vendor, font=("Segoe UI", 9), fg=COLOR_TEXT_SECONDARY, bg=COLOR_BG_DARK, anchor="w")
            lbl_vendor.pack(side="left")

            lbl_ver = tk.Label(sub_frame, text=f"v{game.version}", font=("Segoe UI", 9), fg=COLOR_TEXT_MUTED, bg=COLOR_BG_DARK, anchor="e")
            lbl_ver.pack(side="right", padx=(0, 12))

            def _bind_all(widget, g=game, r=row):
                widget.bind("<Button-1>", lambda e, cur_g=g: self.launch_game(cur_g))
                widget.bind("<Button-3>", lambda e, cur_g=g: self.show_context_menu(e, cur_g))
                widget.bind("<Enter>", lambda e, cur_r=r: cur_r.configure(bg=COLOR_ITEM_HOVER))
                widget.bind("<Leave>", lambda e, cur_r=r: cur_r.configure(bg=COLOR_BG_DARK))

            for w in [row, icon_canvas, text_frame, lbl_title, sub_frame, lbl_vendor, lbl_ver]:
                _bind_all(w)

            sep = tk.Frame(self.scroll_content, bg="#1E1E1E", height=1)
            sep.pack(fill="x", padx=16)

    def toggle_search(self):
        self.is_search_open = not self.is_search_open
        if not self.is_search_open:
            self.search_query = ""
            self.filtered_games = list(self.games)
        self.show_library_view()

    def on_search_change(self, event):
        q = self.ent_search.get().strip().lower()
        self.search_query = q
        self.filtered_games = [g for g in self.games if q in g.title.lower() or q in g.vendor.lower()] if q else list(self.games)
        self.populate_game_list()

    def clear_search(self):
        if hasattr(self, 'ent_search'):
            self.ent_search.delete(0, "end")
            self.search_query = ""
            self.filtered_games = list(self.games)
            self.populate_game_list()

    def sort_games(self):
        self.games.sort(key=lambda g: g.title.lower())
        self.filtered_games = list(self.games)
        self.populate_game_list()
        play_tone(600, 40)

    def show_context_menu(self, event, game):
        menu = tk.Menu(self, tearoff=0, bg="#262626", fg="#FFFFFF", activebackground=COLOR_FAB, activeforeground="#FFFFFF")
        menu.add_command(label="▶  Start", command=lambda: self.launch_game(game))
        menu.add_command(label="⚙  Settings", command=lambda: self.show_settings_dialog(game))
        menu.add_command(label="✏  Rename", command=lambda: self.rename_game(game))
        menu.add_command(label="🗑  Clear Saved Data (RMS)", command=lambda: self.clear_rms(game))
        menu.add_separator()
        menu.add_command(label="❌ Delete", command=lambda: self.delete_game(game))
        try: menu.tk_popup(event.x_root, event.y_root)
        finally: menu.grab_release()

    def show_overflow_menu(self):
        menu = tk.Menu(self, tearoff=0, bg="#262626", fg="#FFFFFF", activebackground=COLOR_FAB, activeforeground="#FFFFFF")
        menu.add_command(label="⚙  Global Settings", command=lambda: self.show_settings_dialog(None))
        menu.add_command(label="❓  Help", command=self.show_help_dialog)
        menu.add_command(label="ℹ️  About", command=self.show_about_dialog)
        try: menu.tk_popup(self.winfo_rootx() + self.winfo_width() - 150, self.winfo_rooty() + 60)
        finally: menu.grab_release()

    def rename_game(self, game):
        new_name = simpledialog.askstring("Rename", f"Enter new name for '{game.title}':", initialvalue=game.title, parent=self)
        if new_name and new_name.strip():
            game.title = new_name.strip()
            self.populate_game_list()

    def clear_rms(self, game):
        if messagebox.askyesno("Clear RMS", f"Delete all saved data and high scores for '{game.title}'?", parent=self):
            play_tone(400, 80)
            messagebox.showinfo("Success", f"RMS storage for '{game.title}' was cleared.", parent=self)

    def delete_game(self, game):
        if messagebox.askyesno("Delete App", f"Are you sure you want to delete '{game.title}'?", parent=self):
            if game in self.games: self.games.remove(game)
            if game in self.filtered_games: self.filtered_games.remove(game)
            self.populate_game_list()
            play_tone(350, 90)

    def show_about_dialog(self):
        msg = "J2ME-Loader for iOS & Desktop\nVersion 1.8.2\n\nOriginal Android App:\nNikita Shakarun (PlaySoftware)\n\nHigh-performance J2ME / MIDP 2.0 emulator powered by Swift, Metal, CoreAudio & Sonivox EAS.\n\nLicensed under Apache-2.0"
        messagebox.showinfo("About J2ME-Loader", msg, parent=self)

    def show_help_dialog(self):
        msg = "HOW TO USE J2ME-LOADER:\n\n1. Installing Games:\n   - Tap '+' in bottom-right corner.\n   - Select any .jar or .jad file from your PC.\n\n2. Controls:\n   - Touch / Mouse on screen.\n   - Virtual Keypad on bottom.\n   - PC Keyboard: Arrow keys, Enter (OK), F1/F2, 0-9.\n\n3. Game Settings:\n   - Right click any game -> Settings."
        messagebox.showinfo("J2ME-Loader Help", msg, parent=self)

    def import_jar_file(self):
        file_path = filedialog.askopenfilename(title="Select Java Game (.jar / .jad)", filetypes=[("Java MIDlet Archive", "*.jar;*.jad"), ("All Files", "*.* ")], parent=self)
        if not file_path: return
        title = os.path.splitext(os.path.basename(file_path))[0]
        vendor, version = "Unknown Vendor", "1.0.0"
        if file_path.lower().endswith(".jar") and zipfile.is_zipfile(file_path):
            try:
                with zipfile.ZipFile(file_path, 'r') as z:
                    if "META-INF/MANIFEST.MF" in z.namelist():
                        manifest = z.read("META-INF/MANIFEST.MF").decode("utf-8", errors="ignore")
                        for line in manifest.splitlines():
                            if line.startswith("MIDlet-Name:"): title = line.split(":", 1)[1].strip()
                            elif line.startswith("MIDlet-Vendor:"): vendor = line.split(":", 1)[1].strip()
                            elif line.startswith("MIDlet-Version:"): version = line.split(":", 1)[1].strip()
            except: pass
        new_game = GameModel(title, vendor, version, icon_color="#34C759", jar_path=file_path)
        self.games.insert(0, new_game)
        self.filtered_games = list(self.games)
        self.populate_game_list()
        play_tone(880, 100)
        messagebox.showinfo("Installed", f"Successfully installed '{title}'!", parent=self)

    def show_settings_dialog(self, game):
        dlg = tk.Toplevel(self)
        target_name = game.title if game else "Default App Settings"
        dlg.title(f"Settings - {target_name}")
        dlg.geometry("380x600")
        dlg.configure(bg=COLOR_BG_DARK)
        dlg.transient(self)
        dlg.grab_set()

        h = tk.Frame(dlg, bg=COLOR_TOOLBAR, height=50)
        h.pack(fill="x")
        h.pack_propagate(False)
        tk.Label(h, text=f"Settings: {target_name}", font=("Segoe UI", 12, "bold"), fg="#FFFFFF", bg=COLOR_TOOLBAR).pack(side="left", padx=16)

        body = tk.Frame(dlg, bg=COLOR_BG_DARK, padx=16, pady=12)
        body.pack(fill="both", expand=True)

        tk.Label(body, text="Device Profile Preset", font=("Segoe UI", 10, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_BG_DARK).pack(anchor="w", pady=(6, 2))
        preset_var = tk.StringVar(value="Nokia N73 (240x320)")
        cbo_preset = ttk.Combobox(body, textvariable=preset_var, state="readonly", values=["Nokia N73 (240x320)", "Sony Ericsson K750i (176x220)", "Nokia 6600 (176x208)", "Nokia 5800 (360x640)", "Custom Resolution"])
        cbo_preset.pack(fill="x", pady=2)

        res_frame = tk.Frame(body, bg=COLOR_BG_DARK)
        res_frame.pack(fill="x", pady=4)
        tk.Label(res_frame, text="Width:", fg=COLOR_TEXT_SECONDARY, bg=COLOR_BG_DARK).pack(side="left")
        w_var = tk.StringVar(value=str(game.width if game else 240))
        ent_w = tk.Entry(res_frame, textvariable=w_var, width=6, bg="#2A2A2A", fg="#FFF", insertbackground="#FFF")
        ent_w.pack(side="left", padx=(4, 16))
        tk.Label(res_frame, text="Height:", fg=COLOR_TEXT_SECONDARY, bg=COLOR_BG_DARK).pack(side="left")
        h_var = tk.StringVar(value=str(game.height if game else 320))
        ent_h = tk.Entry(res_frame, textvariable=h_var, width=6, bg="#2A2A2A", fg="#FFF", insertbackground="#FFF")
        ent_h.pack(side="left", padx=4)

        def on_preset_selected(event):
            p = preset_var.get()
            if "240x320" in p: w_var.set("240"); h_var.set("320")
            elif "176x220" in p: w_var.set("176"); h_var.set("220")
            elif "176x208" in p: w_var.set("176"); h_var.set("208")
            elif "360x640" in p: w_var.set("360"); h_var.set("640")
        cbo_preset.bind("<<ComboboxSelected>>", on_preset_selected)

        tk.Label(body, text="Graphics Filter / Shader", font=("Segoe UI", 10, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_BG_DARK).pack(anchor="w", pady=(10, 2))
        shader_var = tk.StringVar(value=game.shader if game else "Nearest (Pixel Sharp)")
        cbo_shader = ttk.Combobox(body, textvariable=shader_var, state="readonly", values=["Nearest (Pixel Sharp)", "Bilinear (Smooth)", "CRT Scanlines", "Nokia LCD Subpixel Grid"])
        cbo_shader.pack(fill="x", pady=2)

        tk.Label(body, text="FPS Limit", font=("Segoe UI", 10, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_BG_DARK).pack(anchor="w", pady=(10, 2))
        fps_var = tk.StringVar(value=f"{game.fps_limit if game else 60} FPS")
        cbo_fps = ttk.Combobox(body, textvariable=fps_var, state="readonly", values=["30 FPS", "60 FPS", "Unlimited"])
        cbo_fps.pack(fill="x", pady=2)

        tk.Label(body, text="Virtual Keypad Layout", font=("Segoe UI", 10, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_BG_DARK).pack(anchor="w", pady=(10, 2))
        layout_var = tk.StringVar(value=game.keypad_layout if game else "Classic Phone (3x4 + D-Pad)")
        cbo_layout = ttk.Combobox(body, textvariable=layout_var, state="readonly", values=["Classic Phone (3x4 + D-Pad)", "Gamepad D-Pad Only", "Hidden (Touchscreen only)"])
        cbo_layout.pack(fill="x", pady=2)

        sound_var = tk.BooleanVar(value=game.sound_enabled if game else True)
        chk_sound = tk.Checkbutton(body, text="Enable Sonivox EAS Audio Synthesizer", variable=sound_var, font=("Segoe UI", 10), fg="#FFFFFF", bg=COLOR_BG_DARK, selectcolor="#2A2A2A", activebackground=COLOR_BG_DARK, activeforeground="#FFF")
        chk_sound.pack(anchor="w", pady=(12, 4))

        def save():
            if game:
                try: game.width, game.height = int(w_var.get()), int(h_var.get())
                except: pass
                game.shader = shader_var.get()
                game.fps_limit = 60 if "60" in fps_var.get() else (30 if "30" in fps_var.get() else 0)
                game.keypad_layout = layout_var.get()
                game.sound_enabled = sound_var.get()
            play_tone(660, 60)
            dlg.destroy()

        btn_save = tk.Button(dlg, text="Save & Apply", font=("Segoe UI", 11, "bold"), fg="#FFFFFF", bg=COLOR_FAB, activebackground=COLOR_FAB_PRESSED, relief="flat", bd=0, cursor="hand2", pady=8, command=save)
        btn_save.pack(fill="x", padx=16, pady=16)

    def launch_game(self, game):
        self.current_game = game
        self.clear_container()

        bar = tk.Frame(self.container, bg=COLOR_TOOLBAR, height=48)
        bar.pack(fill="x", side="top")
        bar.pack_propagate(False)

        btn_back = tk.Button(bar, text="‹ Back", font=("Segoe UI", 11, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, relief="flat", bd=0, command=self.show_library_view, cursor="hand2")
        btn_back.pack(side="left", padx=10)

        lbl_game = tk.Label(bar, text=game.title, font=("Segoe UI", 11, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR)
        lbl_game.pack(side="left", padx=6)

        self.btn_speed = tk.Button(bar, text=f"{int(game.speed)}x", font=("Segoe UI", 9, "bold"), fg=COLOR_FAB, bg="#333333", relief="flat", bd=0, command=self.cycle_speed, cursor="hand2")
        self.btn_speed.pack(side="right", padx=8)

        self.is_paused = False
        self.btn_pause = tk.Button(bar, text="❚❚", font=("Segoe UI", 10), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, relief="flat", bd=0, command=self.toggle_pause, cursor="hand2")
        self.btn_pause.pack(side="right", padx=6)

        btn_restart = tk.Button(bar, text="↺", font=("Segoe UI", 12, "bold"), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, relief="flat", bd=0, command=self.restart_game, cursor="hand2")
        btn_restart.pack(side="right", padx=6)

        btn_settings = tk.Button(bar, text="⚙", font=("Segoe UI", 11), fg=COLOR_TEXT_PRIMARY, bg=COLOR_TOOLBAR, relief="flat", bd=0, command=lambda: self.show_settings_dialog(game), cursor="hand2")
        btn_settings.pack(side="right", padx=6)

        canvas_holder = tk.Frame(self.container, bg="#000000", height=320)
        canvas_holder.pack(fill="x", side="top")
        canvas_holder.pack_propagate(False)

        self.game_canvas = tk.Canvas(canvas_holder, bg="#000000", highlightthickness=0, bd=0)
        self.game_canvas.pack(fill="both", expand=True)

        self.ball_x = 120.0
        self.ball_y = 200.0
        self.ball_vx = 0.0
        self.ball_vy = 0.0
        self.ball_radius = 14
        self.score = 0
        self.rings = [(50, 180), (120, 120), (190, 180), (80, 240), (160, 240)]
        self.collected_rings = set()
        self.frame_count = 0
        self.fps_display = 60
        self.last_fps_time = time.time()
        self.keys_held = set()

        self.game_canvas.bind("<Button-1>", self.on_canvas_touch)
        self.game_canvas.bind("<B1-Motion>", self.on_canvas_touch)

        if game.keypad_layout != "Hidden (Touchscreen only)":
            self.build_keypad()

        self.bind("<KeyPress>", self.on_key_down)
        self.bind("<KeyRelease>", self.on_key_up)

        self.running_loop = True
        self.after(16, self.game_loop_step)

    def cycle_speed(self):
        speeds = [1.0, 2.0, 4.0]
        cur_idx = speeds.index(self.current_game.speed) if self.current_game.speed in speeds else 0
        self.current_game.speed = speeds[(cur_idx + 1) % len(speeds)]
        self.btn_speed.config(text=f"{int(self.current_game.speed)}x")
        play_tone(700, 40)

    def toggle_pause(self):
        self.is_paused = not self.is_paused
        self.btn_pause.config(text="▶" if self.is_paused else "❚❚")
        play_tone(500, 40)

    def restart_game(self):
        self.ball_x = 120.0
        self.ball_y = 200.0
        self.ball_vx = 0.0
        self.ball_vy = 0.0
        self.score = 0
        self.collected_rings.clear()
        play_tone(800, 80)

    def build_keypad(self):
        keypad_frame = tk.Frame(self.container, bg=COLOR_KEYPAD_BG)
        keypad_frame.pack(fill="both", expand=True, side="bottom", padx=8, pady=4)

        soft_row = tk.Frame(keypad_frame, bg=COLOR_KEYPAD_BG)
        soft_row.pack(fill="x", pady=2)

        btn_lsk = tk.Button(soft_row, text="— LSK —", font=("Segoe UI", 9, "bold"), fg=COLOR_KEYPAD_TEXT, bg=COLOR_KEYPAD_BTN, relief="flat", bd=0, height=1, cursor="hand2")
        btn_lsk.pack(side="left", fill="x", expand=True, padx=4)
        btn_lsk.bind("<ButtonPress-1>", lambda e: self.press_key("LSK"))
        btn_lsk.bind("<ButtonRelease-1>", lambda e: self.release_key("LSK"))

        btn_rsk = tk.Button(soft_row, text="— RSK —", font=("Segoe UI", 9, "bold"), fg=COLOR_KEYPAD_TEXT, bg=COLOR_KEYPAD_BTN, relief="flat", bd=0, height=1, cursor="hand2")
        btn_rsk.pack(side="right", fill="x", expand=True, padx=4)
        btn_rsk.bind("<ButtonPress-1>", lambda e: self.press_key("RSK"))
        btn_rsk.bind("<ButtonRelease-1>", lambda e: self.release_key("RSK"))

        dpad_frame = tk.Frame(keypad_frame, bg=COLOR_KEYPAD_BG)
        dpad_frame.pack(pady=4)

        def make_dpad_btn(parent, text, key, w=5, h=1):
            b = tk.Button(parent, text=text, font=("Segoe UI", 9, "bold"), fg=COLOR_KEYPAD_TEXT, bg=COLOR_KEYPAD_BTN, relief="flat", bd=0, width=w, height=h, cursor="hand2")
            b.bind("<ButtonPress-1>", lambda e: self.press_key(key))
            b.bind("<ButtonRelease-1>", lambda e: self.release_key(key))
            return b

        btn_up = make_dpad_btn(dpad_frame, "▲", "UP"); btn_up.grid(row=0, column=1, padx=2, pady=2)
        btn_left = make_dpad_btn(dpad_frame, "◀", "LEFT"); btn_left.grid(row=1, column=0, padx=2, pady=2)
        btn_ok = make_dpad_btn(dpad_frame, "OK", "FIRE"); btn_ok.config(fg=COLOR_FAB); btn_ok.grid(row=1, column=1, padx=2, pady=2)
        btn_right = make_dpad_btn(dpad_frame, "▶", "RIGHT"); btn_right.grid(row=1, column=2, padx=2, pady=2)
        btn_down = make_dpad_btn(dpad_frame, "▼", "DOWN"); btn_down.grid(row=2, column=1, padx=2, pady=2)

        num_frame = tk.Frame(keypad_frame, bg=COLOR_KEYPAD_BG)
        num_frame.pack(fill="x", pady=2)

        num_keys = [
            ("1", "."), ("2", "abc"), ("3", "def"),
            ("4", "ghi"), ("5", "jkl"), ("6", "mno"),
            ("7", "pqrs"), ("8", "tuv"), ("9", "wxyz"),
            ("*", ""), ("0", "␣"), ("#", "⇧")
        ]

        for idx, (num, sub) in enumerate(num_keys):
            r, c = idx // 3, idx % 3
            cell = tk.Frame(num_frame, bg=COLOR_KEYPAD_BTN, cursor="hand2", padx=2, pady=2)
            cell.grid(row=r, column=c, padx=3, pady=2, sticky="nsew")
            num_frame.columnconfigure(c, weight=1)

            lbl_n = tk.Label(cell, text=num, font=("Segoe UI", 10, "bold"), fg=COLOR_KEYPAD_TEXT, bg=COLOR_KEYPAD_BTN)
            lbl_n.pack(side="left", padx=4)
            if sub:
                lbl_s = tk.Label(cell, text=sub, font=("Segoe UI", 7), fg=COLOR_KEYPAD_SUB, bg=COLOR_KEYPAD_BTN)
                lbl_s.pack(side="right", padx=4)

            def _bind_num(w, k=num, p_cell=cell):
                w.bind("<ButtonPress-1>", lambda e: (p_cell.config(bg=COLOR_FAB), self.press_key(k)))
                w.bind("<ButtonRelease-1>", lambda e: (p_cell.config(bg=COLOR_KEYPAD_BTN), self.release_key(k)))

            for w in [cell, lbl_n]: _bind_num(w)

    def press_key(self, key):
        self.keys_held.add(key)
        if self.current_game and self.current_game.sound_enabled:
            tone_map = {
                "1": 697, "2": 770, "3": 852,
                "4": 697, "5": 770, "6": 852,
                "7": 697, "8": 770, "9": 852,
                "UP": 587, "DOWN": 493, "LEFT": 523, "RIGHT": 659, "FIRE": 880,
                "LSK": 440, "RSK": 440
            }
            play_tone(tone_map.get(key, 500), 50)

    def release_key(self, key):
        self.keys_held.discard(key)

    def on_key_down(self, event):
        key_map = {"Up": "UP", "Down": "DOWN", "Left": "LEFT", "Right": "RIGHT", "Return": "FIRE", "space": "FIRE", "F1": "LSK", "F2": "RSK", "z": "LSK", "x": "RSK"}
        k = key_map.get(event.keysym, event.char)
        if k: self.press_key(k.upper())

    def on_key_up(self, event):
        key_map = {"Up": "UP", "Down": "DOWN", "Left": "LEFT", "Right": "RIGHT", "Return": "FIRE", "space": "FIRE", "F1": "LSK", "F2": "RSK", "z": "LSK", "x": "RSK"}
        k = key_map.get(event.keysym, event.char)
        if k: self.release_key(k.upper())

    def on_canvas_touch(self, event):
        self.ball_x, self.ball_y = float(event.x), float(event.y)
        self.ball_vx, self.ball_vy = 0, 0
        play_tone(620, 30)

    def game_loop_step(self):
        if not self.current_game or not hasattr(self, 'game_canvas'): return

        cw = self.game_canvas.winfo_width() or 240
        ch = self.game_canvas.winfo_height() or 320

        if not self.is_paused:
            speed = self.current_game.speed
            if "LEFT" in self.keys_held or "4" in self.keys_held: self.ball_vx -= 0.8 * speed
            if "RIGHT" in self.keys_held or "6" in self.keys_held: self.ball_vx += 0.8 * speed
            if "UP" in self.keys_held or "2" in self.keys_held or "FIRE" in self.keys_held or "5" in self.keys_held:
                if self.ball_y >= ch - self.ball_radius - 2:
                    self.ball_vy = -8.5 * speed
                    play_tone(900, 40)

            self.ball_vy += 0.35 * speed
            self.ball_x += self.ball_vx * speed
            self.ball_y += self.ball_vy * speed
            self.ball_vx *= 0.92

            if self.ball_x - self.ball_radius < 0:
                self.ball_x = self.ball_radius
                self.ball_vx = -self.ball_vx * 0.6
            elif self.ball_x + self.ball_radius > cw:
                self.ball_x = cw - self.ball_radius
                self.ball_vx = -self.ball_vx * 0.6

            if self.ball_y + self.ball_radius > ch:
                self.ball_y = ch - self.ball_radius
                self.ball_vy = -self.ball_vy * 0.5

            for idx, (rx, ry) in enumerate(self.rings):
                if idx not in self.collected_rings:
                    dist = math.hypot(self.ball_x - rx, self.ball_y - ry)
                    if dist < self.ball_radius + 12:
                        self.collected_rings.add(idx)
                        self.score += 100
                        play_tone(1200, 80)

        self.frame_count += 1
        now = time.time()
        if now - self.last_fps_time >= 1.0:
            self.fps_display = self.frame_count
            self.frame_count = 0
            self.last_fps_time = now

        self.game_canvas.delete("all")

        self.game_canvas.create_rectangle(0, 0, cw, ch * 0.65, fill="#0F172A", outline="")
        self.game_canvas.create_rectangle(0, ch * 0.65, cw, ch, fill="#1E293B", outline="")
        
        for y in range(int(ch * 0.65), ch, 18): self.game_canvas.create_line(0, y, cw, y, fill="#334155")
        for x in range(0, cw, 24): self.game_canvas.create_line(x, int(ch * 0.65), x, ch, fill="#334155")

        for idx, (rx, ry) in enumerate(self.rings):
            if idx not in self.collected_rings:
                pulse = math.sin(time.time() * 6 + idx) * 2
                self.game_canvas.create_oval(rx - 10 - pulse, ry - 10 - pulse, rx + 10 + pulse, ry + 10 + pulse, outline="#FFD700", width=3)
                self.game_canvas.create_oval(rx - 4, ry - 4, rx + 4, ry + 4, fill="#FFF8DC", outline="")

        bx, by, r = self.ball_x, self.ball_y, self.ball_radius
        self.game_canvas.create_oval(bx - r, by - r, bx + r, by + r, fill="#FF2E51", outline="#990022", width=2)
        self.game_canvas.create_oval(bx - r * 0.5, by - r * 0.6, bx, by - r * 0.1, fill="#FFA0B0", outline="")

        self.game_canvas.create_text(8, 12, text=f"SCORE: {self.score}", fill="#FFFFFF", font=("Segoe UI", 9, "bold"), anchor="w")
        self.game_canvas.create_text(cw - 8, 12, text=f"{self.fps_display} FPS", fill="#34C759", font=("Segoe UI", 9, "bold"), anchor="e")

        shader = self.current_game.shader
        if shader == "CRT Scanlines":
            for sy in range(0, ch, 4): self.game_canvas.create_line(0, sy, cw, sy, fill="#000000", stipple="gray25")
        elif shader == "Nokia LCD Subpixel Grid":
            for sy in range(0, ch, 3): self.game_canvas.create_line(0, sy, cw, sy, fill="#112211")

        if self.is_paused:
            self.game_canvas.create_rectangle(0, 0, cw, ch, fill="#000000", stipple="gray50")
            self.game_canvas.create_text(cw // 2, ch // 2, text="PAUSED", fill="#FFFFFF", font=("Segoe UI", 16, "bold"))

        self.after(16, self.game_loop_step)

if __name__ == "__main__":
    app = J2MELoaderApp()
    app.mainloop()
