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
      title: 'Universal J2ME Loader',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF0F172A),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF38BDF8),
          secondary: Color(0xFF0284C7),
          surface: Color(0xFF1E293B),
        ),
        fontFamily: 'Segoe UI',
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
  int _selectedLayout = 0; // 0: Nokia, 1: Siemens, 2: Motorola
  int _selectedFps = 60;
  int _selectedResolutionIndex = 5; // Default 240x320

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
        _games.add(GameItem(
          title: "Dragon Boy (Chú Bé Rồng)",
          vendor: "Team",
          version: "2.4.7",
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
        backgroundColor: const Color(0xFF1E293B),
        title: const Text("Nạp Tệp Game .JAR", style: TextStyle(color: Colors.white, fontSize: 16)),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              "Nhập đường dẫn đầy đủ đến tệp .JAR trên thiết bị:",
              style: TextStyle(color: Color(0xFF94A3B8), fontSize: 13),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              style: const TextStyle(color: Colors.white, fontSize: 14),
              decoration: const InputDecoration(
                hintText: r"C:\games\my_game.jar",
                hintStyle: TextStyle(color: Colors.white38),
                filled: true,
                fillColor: Color(0xFF0F172A),
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
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF0284C7)),
            onPressed: () {
              final path = controller.text.trim();
              if (path.isNotEmpty && File(path).existsSync()) {
                setState(() {
                  _games.add(GameItem(
                    title: path.split(RegExp(r'[\\/]')).last.replaceAll('.jar', ''),
                    vendor: "Custom",
                    version: "1.0",
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
            child: const Text("Nạp Game"),
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
          backgroundColor: const Color(0xFF1E293B),
          title: const Text("Cài Đặt Cấu Hình & Profile", style: TextStyle(color: Colors.white, fontSize: 16)),
          content: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text("Bố Cục Bàn Phím (KeyMapper):", style: TextStyle(color: Color(0xFF38BDF8), fontSize: 13, fontWeight: FontWeight.bold)),
                DropdownButton<int>(
                  value: _selectedLayout,
                  dropdownColor: const Color(0xFF1E293B),
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
                const Text("Giới Hạn Tốc Độ Khung Hình (FPS Limit):", style: TextStyle(color: Color(0xFF38BDF8), fontSize: 13, fontWeight: FontWeight.bold)),
                DropdownButton<int>(
                  value: _selectedFps,
                  dropdownColor: const Color(0xFF1E293B),
                  isExpanded: true,
                  items: const [
                    DropdownMenuItem(value: 30, child: Text("30 FPS (Tiết kiệm pin)", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 50, child: Text("50 FPS", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 60, child: Text("60 FPS (Chuẩn mượt)", style: TextStyle(color: Colors.white))),
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
                const Text("Độ Phân Giải Màn Hình:", style: TextStyle(color: Color(0xFF38BDF8), fontSize: 13, fontWeight: FontWeight.bold)),
                DropdownButton<int>(
                  value: _selectedResolutionIndex,
                  dropdownColor: const Color(0xFF1E293B),
                  isExpanded: true,
                  items: const [
                    DropdownMenuItem(value: 0, child: Text("128 x 128 (Nokia 7210)", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 1, child: Text("128 x 160 (Nokia 3110c)", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 3, child: Text("176 x 220 (Siemens / Moto)", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 5, child: Text("240 x 320 (Nokia S40/S60 Standard)", style: TextStyle(color: Colors.white))),
                    DropdownMenuItem(value: 6, child: Text("320 x 240 (Nokia E71 Landscape)", style: TextStyle(color: Colors.white))),
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
              style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF0284C7)),
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
        backgroundColor: const Color(0xFF1E293B),
        title: const Text("Đổi Tên Trò Chơi", style: TextStyle(color: Colors.white, fontSize: 16)),
        content: TextField(
          controller: controller,
          style: const TextStyle(color: Colors.white),
          decoration: const InputDecoration(
            hintText: "Nhập tên mới...",
            hintStyle: TextStyle(color: Colors.white38),
            filled: true,
            fillColor: Color(0xFF0F172A),
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text("Hủy", style: TextStyle(color: Colors.white60)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF0284C7)),
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
        backgroundColor: const Color(0xFF1E293B),
        title: const Text("Xóa Trò Chơi", style: TextStyle(color: Colors.white, fontSize: 16)),
        content: Text(
          "Bạn có chắc muốn xóa '${game.title}' khỏi danh sách?",
          style: const TextStyle(color: Color(0xFF94A3B8)),
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
        backgroundColor: const Color(0xFF1E293B),
        elevation: 2,
        title: Row(
          children: [
            const Icon(Icons.videogame_asset, color: Color(0xFF38BDF8), size: 26),
            const SizedBox(width: 10),
            const Text(
              "Universal J2ME Loader",
              style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18),
            ),
            const SizedBox(width: 8),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
              decoration: BoxDecoration(
                color: const Color(0xFF0284C7).withAlpha(76),
                borderRadius: BorderRadius.circular(4),
                border: Border.all(color: const Color(0xFF38BDF8), width: 0.8),
              ),
              child: const Text(
                "v2.0 Tri-Platform",
                style: TextStyle(fontSize: 10, color: Color(0xFF38BDF8), fontWeight: FontWeight.bold),
              ),
            ),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.folder_open, color: Color(0xFF38BDF8)),
            tooltip: "Nạp tệp .JAR",
            onPressed: _showAddGameDialog,
          ),
          IconButton(
            icon: const Icon(Icons.settings, color: Colors.white70),
            tooltip: "Cài đặt Profile",
            onPressed: _showSettingsDialog,
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  colors: [Color(0xFF1E293B), Color(0xFF0F172A)],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: const Color(0xFF334155), width: 1.2),
              ),
              child: Row(
                children: [
                  const Icon(Icons.devices, color: Color(0xFF38BDF8), size: 36),
                  const SizedBox(width: 14),
                  const Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          "Kiến trúc Thống nhất 1 Tầng UI",
                          style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold, color: Colors.white),
                        ),
                        SizedBox(height: 4),
                        Text(
                          "Chạy đồng thời trên iOS, Android, Windows & macOS với Lõi C++20 Zero-Overhead.",
                          style: TextStyle(fontSize: 12, color: Color(0xFF94A3B8)),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 20),
            const Text(
              "THƯ VIỆN TRÒ CHƠI J2ME",
              style: TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.bold,
                color: Color(0xFF64748B),
                letterSpacing: 1.2,
              ),
            ),
            const SizedBox(height: 12),
            Expanded(
              child: _games.isEmpty
                  ? Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: const [
                          Icon(Icons.sports_esports_outlined, size: 64, color: Color(0xFF475569)),
                          SizedBox(height: 12),
                          Text(
                            "Chưa có trò chơi nào",
                            style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Color(0xFF94A3B8)),
                          ),
                          SizedBox(height: 6),
                          Text(
                            "Nhấn nút dấu cộng (+) hoặc biểu tượng thư mục để nạp tệp .JAR",
                            style: TextStyle(fontSize: 13, color: Color(0xFF64748B)),
                          ),
                        ],
                      ),
                    )
                  : ListView.builder(
                      itemCount: _games.length,
                      itemBuilder: (context, index) {
                        final game = _games[index];
                        return _buildGameCard(
                          index: index,
                          game: game,
                          onTap: () => _launchGame(game),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
      // Nút Floating Action Button (+) chuẩn theo đặc tả gốc fragment_apps_list.xml
      floatingActionButton: FloatingActionButton(
        backgroundColor: const Color(0xFF0284C7),
        tooltip: "Thêm tệp game .JAR",
        onPressed: _showAddGameDialog,
        child: const Icon(Icons.add, color: Colors.white, size: 28),
      ),
    );
  }

  Widget _buildGameCard({
    required int index,
    required GameItem game,
    required VoidCallback onTap,
  }) {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      color: const Color(0xFF1E293B),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(10),
        side: const BorderSide(color: Color(0xFF334155), width: 1),
      ),
      clipBehavior: Clip.antiAlias,
      child: ListTile(
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        onTap: onTap,
        leading: Container(
          width: 48,
          height: 48,
          decoration: BoxDecoration(
            color: const Color(0xFF0F172A),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: const Color(0xFF38BDF8).withAlpha(128)),
          ),
          child: const Icon(Icons.sports_esports, color: Color(0xFF38BDF8), size: 28),
        ),
        title: Text(
          game.title,
          style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15, color: Colors.white),
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const SizedBox(height: 4),
            Text(
              "${game.vendor} • v${game.version} • ${game.resolution}",
              style: const TextStyle(fontSize: 12, color: Color(0xFF94A3B8)),
            ),
          ],
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            ElevatedButton(
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF0284C7),
                foregroundColor: Colors.white,
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(6)),
                padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
              ),
              onPressed: onTap,
              child: const Text("Chơi Ngay", style: TextStyle(fontWeight: FontWeight.bold, fontSize: 13)),
            ),
            // Menu ngữ cảnh gốc: Khởi chạy, Cấu hình, Đổi tên, Xóa
            PopupMenuButton<String>(
              icon: const Icon(Icons.more_vert, color: Colors.white70),
              color: const Color(0xFF1E293B),
              onSelected: (val) {
                if (val == 'run') {
                  onTap();
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
                  child: Row(
                    children: [
                      Icon(Icons.play_arrow, color: Color(0xFF38BDF8), size: 20),
                      SizedBox(width: 8),
                      Text("Khởi chạy", style: TextStyle(color: Colors.white)),
                    ],
                  ),
                ),
                const PopupMenuItem(
                  value: 'config',
                  child: Row(
                    children: [
                      Icon(Icons.tune, color: Colors.white70, size: 20),
                      SizedBox(width: 8),
                      Text("Cấu hình", style: TextStyle(color: Colors.white)),
                    ],
                  ),
                ),
                const PopupMenuItem(
                  value: 'rename',
                  child: Row(
                    children: [
                      Icon(Icons.edit, color: Colors.white70, size: 20),
                      SizedBox(width: 8),
                      Text("Đổi tên", style: TextStyle(color: Colors.white)),
                    ],
                  ),
                ),
                const PopupMenuItem(
                  value: 'delete',
                  child: Row(
                    children: [
                      Icon(Icons.delete_outline, color: Colors.redAccent, size: 20),
                      SizedBox(width: 8),
                      Text("Xóa", style: TextStyle(color: Colors.redAccent)),
                    ],
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
