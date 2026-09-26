import 'dart:ffi' as ffi;
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:ffi/ffi.dart';
import 'bridge/j2me_ffi.dart';
import 'views/emulator_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const UniversalJ2meApp());
}

class UniversalJ2meApp extends StatelessWidget {
  const UniversalJ2meApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'J2ME-Loader',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF121212),
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFF1F1F1F),
          elevation: 1,
          titleTextStyle: TextStyle(
            color: Colors.white,
            fontSize: 19,
            fontWeight: FontWeight.w600,
          ),
          iconTheme: IconThemeData(color: Colors.white),
        ),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF1E88E5),
          surface: Color(0xFF1F1F1F),
        ),
      ),
      home: const GameLibraryScreen(),
    );
  }
}

class GameItem {
  final String title;
  final String vendor;
  final String version;
  final String path;
  final String resolution;

  GameItem({
    required this.title,
    required this.vendor,
    required this.version,
    required this.path,
    required this.resolution,
  });
}

class GameLibraryScreen extends StatefulWidget {
  const GameLibraryScreen({super.key});

  @override
  State<GameLibraryScreen> createState() => _GameLibraryScreenState();
}

class _GameLibraryScreenState extends State<GameLibraryScreen> {
  ffi.Pointer<ffi.Void>? _engine;
  late final J2meBindings _bindings;
  final List<GameItem> _games = [];
  int _selectedLayout = 0;
  int _selectedFps = 60;
  int _selectedResolutionIndex = 5;

  @override
  void initState() {
    super.initState();
    _bindings = J2meBindings.instance;
    final storageDir = "./universal_rms".toNativeUtf8();
    _engine = _bindings.coreCreate(storageDir);
    calloc.free(storageDir);

    _scanDefaultGames();
  }

  void _scanDefaultGames() {
    const defaultPaths = [
      r"C:\j2meloader\universal_loader\core\test_assets\DragonBoy.jar",
      r"../core/test_assets/DragonBoy.jar",
      r"core/test_assets/DragonBoy.jar",
    ];

    for (final p in defaultPaths) {
      if (File(p).existsSync()) {
        String title = "Dragon Boy (Chú Bé Rồng)";
        String vendor = "Team";
        String version = "2.4.7";
        if (_engine != null) {
          final pathPtr = p.toNativeUtf8();
          if (_bindings.coreLoadJarFile(_engine!, pathPtr)) {
            final t = _bindings.coreGetAppTitle(_engine!).toDartString();
            final v = _bindings.coreGetAppVendor(_engine!).toDartString();
            final ver = _bindings.coreGetAppVersion(_engine!).toDartString();
            if (t.isNotEmpty) title = t;
            if (v.isNotEmpty) vendor = v;
            if (ver.isNotEmpty) version = ver;
          }
          calloc.free(pathPtr);
        }
        _games.add(GameItem(
          title: title,
          vendor: vendor,
          version: version,
          path: p,
          resolution: "240x320",
        ));
        break;
      }
    }
  }

  @override
  void dispose() {
    if (_engine != null) {
      _bindings.coreDestroy(_engine!);
      _engine = null;
    }
    super.dispose();
  }

  void _launchGame(GameItem game) {
    if (_engine == null) return;

    final pathPtr = game.path.toNativeUtf8();
    final ok = _bindings.coreLoadJarFile(_engine!, pathPtr);
    calloc.free(pathPtr);

    if (!ok) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Không thể nạp tệp game: ${game.path}"),
          backgroundColor: Colors.redAccent,
        ),
      );
      return;
    }

    final realTitlePtr = _bindings.coreGetAppTitle(_engine!);
    final realTitle = realTitlePtr.toDartString();

    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => EmulatorScreen(
          engineInstance: _engine!,
          title: realTitle.isNotEmpty ? realTitle : game.title,
        ),
      ),
    );
  }

  void _showAddGameDialog() {
    final controller = TextEditingController();
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1F1F1F),
        title: const Text("Chọn tệp JAR", style: TextStyle(color: Colors.white, fontSize: 16)),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              "Nhập đường dẫn đến tệp .JAR trên thiết bị:",
              style: TextStyle(color: Colors.white70, fontSize: 13),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              style: const TextStyle(color: Colors.white, fontSize: 14),
              decoration: const InputDecoration(
                hintText: r"C:\games\app.jar",
                hintStyle: TextStyle(color: Colors.white38),
                filled: true,
                fillColor: Color(0xFF141414),
                border: OutlineInputBorder(),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text("Hủy", style: TextStyle(color: Colors.white60)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF1E88E5)),
            onPressed: () {
              final path = controller.text.trim();
              if (path.isNotEmpty && File(path).existsSync()) {
                String title = path.split(RegExp(r'[\\/]')).last.replaceAll('.jar', '');
                String vendor = "J2ME";
                String version = "1.0.0";
                if (_engine != null) {
                  final pathPtr = path.toNativeUtf8();
                  if (_bindings.coreLoadJarFile(_engine!, pathPtr)) {
                    final t = _bindings.coreGetAppTitle(_engine!).toDartString();
                    final v = _bindings.coreGetAppVendor(_engine!).toDartString();
                    final ver = _bindings.coreGetAppVersion(_engine!).toDartString();
                    if (t.isNotEmpty) title = t;
                    if (v.isNotEmpty) vendor = v;
                    if (ver.isNotEmpty) version = ver;
                  }
                  calloc.free(pathPtr);
                }
                setState(() {
                  _games.add(GameItem(
                    title: title,
                    vendor: vendor,
                    version: version,
                    path: path,
                    resolution: "240x320",
                  ));
                });
                Navigator.pop(ctx);
              } else {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                    content: Text("Tệp không tồn tại hoặc đường dẫn không hợp lệ!"),
                    backgroundColor: Colors.redAccent,
                  ),
                );
              }
            },
            child: const Text("Cài đặt"),
          ),
        ],
      ),
    );
  }

  void _showSettingsDialog() {
    showDialog(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (context, setDialogState) => AlertDialog(
          backgroundColor: const Color(0xFF1F1F1F),
          title: const Text("Thiết lập", style: TextStyle(color: Colors.white, fontSize: 16)),
          content: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text("Bố cục phím (KeyMapper):", style: TextStyle(color: Color(0xFF64B5F6), fontSize: 13, fontWeight: FontWeight.bold)),
                DropdownButton<int>(
                  value: _selectedLayout,
                  dropdownColor: const Color(0xFF1F1F1F),
                  isExpanded: true,
                  items: const [
                    DropdownMenuItem(value: 0, child: Text("Nokia / Sony Ericsson (Mặc định)", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 1, child: Text("Siemens Layout", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 2, child: Text("Motorola Layout", style: TextStyle(color: Colors.white))),
                  ],
                  onChanged: (val) {
                    if (val != null) {
                      setDialogState(() => _selectedLayout = val);
                      setState(() => _selectedLayout = val);
                      _bindings.coreKeymapSetLayout(val);
                    }
                  },
                ),
                const SizedBox(height: 16),
                const Text("Giới hạn FPS:", style: TextStyle(color: Color(0xFF64B5F6), fontSize: 13, fontWeight: FontWeight.bold)),
                DropdownButton<int>(
                  value: _selectedFps,
                  dropdownColor: const Color(0xFF1F1F1F),
                  isExpanded: true,
                  items: const [
                    DropdownMenuItem(value: 30, child: Text("30 FPS", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 50, child: Text("50 FPS", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 60, child: Text("60 FPS", style: TextStyle(color: Colors.white))),
                  ],
                  onChanged: (val) {
                    if (val != null) {
                      setDialogState(() => _selectedFps = val);
                      setState(() => _selectedFps = val);
                      if (_engine != null) {
                        _bindings.coreSetFpsLimit(_engine!, val);
                      }
                    }
                  },
                ),
                const SizedBox(height: 16),
                const Text("Độ phân giải màn hình:", style: TextStyle(color: Color(0xFF64B5F6), fontSize: 13, fontWeight: FontWeight.bold)),
                DropdownButton<int>(
                  value: _selectedResolutionIndex,
                  dropdownColor: const Color(0xFF1F1F1F),
                  isExpanded: true,
                  items: const [
                    DropdownMenuItem(value: 0, child: Text("128 x 128", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 1, child: Text("128 x 160", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 3, child: Text("176 x 220", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 5, child: Text("240 x 320", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 6, child: Text("320 x 240", style: TextStyle(color: Colors.white))),
                  ],
                  onChanged: (val) {
                    if (val != null) {
                      setDialogState(() => _selectedResolutionIndex = val);
                      setState(() => _selectedResolutionIndex = val);
                      if (_engine != null) {
                        final outW = calloc<ffi.Int32>();
                        final outH = calloc<ffi.Int32>();
                        final outName = calloc<ffi.Uint8>(64).cast<Utf8>();
                        if (_bindings.coreGetPresetResolution(val, outW, outH, outName, 64)) {
                          _bindings.coreSetScreenDimensions(_engine!, outW.value, outH.value);
                        }
                        calloc.free(outW);
                        calloc.free(outH);
                        calloc.free(outName);
                      }
                    }
                  },
                ),
              ],
            ),
          ),
          actions: [
            ElevatedButton(
              style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF1E88E5)),
              onPressed: () => Navigator.pop(ctx),
              child: const Text("Đóng"),
            ),
          ],
        ),
      ),
    );
  }

  void _showRenameGameDialog(int index) {
    final game = _games[index];
    final controller = TextEditingController(text: game.title);
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1F1F1F),
        title: const Text("Đổi tên", style: TextStyle(color: Colors.white, fontSize: 16)),
        content: TextField(
          controller: controller,
          style: const TextStyle(color: Colors.white),
          decoration: const InputDecoration(
            hintText: "Nhập tên mới...",
            hintStyle: TextStyle(color: Colors.white38),
            filled: true,
            fillColor: Color(0xFF141414),
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text("Hủy", style: TextStyle(color: Colors.white60)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF1E88E5)),
            onPressed: () {
              final newName = controller.text.trim();
              if (newName.isNotEmpty) {
                setState(() {
                  _games[index] = GameItem(
                    title: newName,
                    vendor: game.vendor,
                    version: game.version,
                    path: game.path,
                    resolution: game.resolution,
                  );
                });
                Navigator.pop(ctx);
              }
            },
            child: const Text("Lưu"),
          ),
        ],
      ),
    );
  }

  void _deleteGame(int index) {
    final game = _games[index];
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1F1F1F),
        title: const Text("Xóa", style: TextStyle(color: Colors.white, fontSize: 16)),
        content: Text(
          "Bạn thực sự muốn xoá ứng dụng '${game.title}'?",
          style: const TextStyle(color: Colors.white70),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text("Hủy", style: TextStyle(color: Colors.white60)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: Colors.redAccent),
            onPressed: () {
              setState(() {
                _games.removeAt(index);
              });
              Navigator.pop(ctx);
            },
            child: const Text("Xóa"),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("J2ME-Loader"),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: "Thiết lập",
            onPressed: _showSettingsDialog,
          ),
        ],
      ),
      body: _games.isEmpty
          ? const Center(
              child: Text(
                "Không có dữ liệu",
                style: TextStyle(fontSize: 16, color: Colors.white54),
              ),
            )
          : ListView.separated(
              itemCount: _games.length,
              separatorBuilder: (context, index) => const Divider(
                height: 1,
                thickness: 1,
                color: Color(0xFF262626),
              ),
              itemBuilder: (context, index) {
                return _buildGameRow(index: index, game: _games[index]);
              },
            ),
      floatingActionButton: FloatingActionButton(
        backgroundColor: const Color(0xFF1E88E5),
        tooltip: "Chọn tệp JAR",
        onPressed: _showAddGameDialog,
        child: const Icon(Icons.add, color: Colors.white),
      ),
    );
  }

  Widget _buildGameRow({
    required int index,
    required GameItem game,
  }) {
    return ListTile(
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      onTap: () => _launchGame(game),
      leading: Container(
        width: 36,
        height: 36,
        decoration: BoxDecoration(
          color: const Color(0xFF262626),
          borderRadius: BorderRadius.circular(4),
        ),
        alignment: Alignment.center,
        child: const Icon(Icons.sports_esports, color: Color(0xFF1E88E5), size: 22),
      ),
      title: Text(
        game.title,
        style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15, color: Colors.white),
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
      ),
      subtitle: Padding(
        padding: const EdgeInsets.only(top: 2),
        child: Row(
          children: [
            Expanded(
              child: Text(
                game.vendor,
                style: const TextStyle(fontSize: 12, color: Colors.white60),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ),
            Text(
              game.version,
              style: const TextStyle(fontSize: 12, color: Colors.white60),
            ),
          ],
        ),
      ),
      trailing: PopupMenuButton<String>(
        icon: const Icon(Icons.more_vert, color: Colors.white70),
        color: const Color(0xFF262626),
        onSelected: (val) {
          if (val == 'run') {
            _launchGame(game);
          } else if (val == 'config') {
            _showSettingsDialog();
          } else if (val == 'rename') {
            _showRenameGameDialog(index);
          } else if (val == 'delete') {
            _deleteGame(index);
          }
        },
        itemBuilder: (context) => [
          const PopupMenuItem(
            value: 'run',
            child: Text("Khởi chạy"),
          ),
          const PopupMenuItem(
            value: 'config',
            child: Text("Thiết lập"),
          ),
          const PopupMenuItem(
            value: 'rename',
            child: Text("Đổi tên"),
          ),
          const PopupMenuItem(
            value: 'delete',
            child: Text("Xóa"),
          ),
        ],
      ),
    );
  }
}
