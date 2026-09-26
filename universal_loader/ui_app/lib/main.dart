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
  final int id;
  final String title;
  final String vendor;
  final String version;
  final String path;
  final String resolution;

  GameItem({
    required this.id,
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

    _loadInstalledApps();
  }

  void _loadInstalledApps() {
    _games.clear();
    if (_engine == null) return;

    final count = _bindings.appRepoGetCount(_engine!);
    final infoPtr = calloc<J2meAppItemInfoFFI>();

    for (int i = 0; i < count; ++i) {
      if (_bindings.appRepoGetItem(_engine!, i, infoPtr)) {
        final title = _arrayToString(infoPtr.ref.title, 128);
        final author = _arrayToString(infoPtr.ref.author, 128);
        final version = _arrayToString(infoPtr.ref.version, 32);
        final path = _arrayToString(infoPtr.ref.path, 128);

        _games.add(GameItem(
          id: infoPtr.ref.id,
          title: title.isNotEmpty ? title : "Game $i",
          vendor: author.isNotEmpty ? author : "J2ME",
          version: version.isNotEmpty ? version : "1.0",
          path: path,
          resolution: "240x320",
        ));
      }
    }

    calloc.free(infoPtr);
  }

  String _arrayToString(ffi.Array<ffi.Uint8> arr, int maxLen) {
    final bytes = <int>[];
    for (int i = 0; i < maxLen; ++i) {
      final b = arr[i];
      if (b == 0) break;
      bytes.add(b);
    }
    return String.fromCharCodes(bytes);
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

    bool ok = false;
    // 1. Nếu là ứng dụng đã cài đặt (id > 0), khởi chạy qua appLaunch (tự động cấu hình RMS, profile và app.jar)
    if (game.id > 0) {
      ok = _bindings.appLaunch(_engine!, game.id);
    }

    // 2. Dự phòng: Nếu chưa chạy được, phân giải đường dẫn thực tế trên đĩa và nạp trực tiếp
    if (!ok && game.path.isNotEmpty) {
      String resolvedPath = game.path;
      if (!File(resolvedPath).existsSync()) {
        final candidates = [
          "./universal_rms/apps/${game.path}/app.jar",
          "universal_rms/apps/${game.path}/app.jar",
          game.path,
        ];
        for (final c in candidates) {
          if (File(c).existsSync()) {
            resolvedPath = c;
            break;
          }
        }
      }

      if (File(resolvedPath).existsSync()) {
        final pathPtr = resolvedPath.toNativeUtf8();
        ok = _bindings.coreLoadJarFile(_engine!, pathPtr);
        calloc.free(pathPtr);
      }
    }

    if (!ok) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Không thể nạp tệp game: ${game.title}"),
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

  void _installJarPath(String path) {
    if (_engine == null) return;

    String cleanPath = path.trim();
    if (cleanPath.startsWith('"') && cleanPath.endsWith('"') && cleanPath.length >= 2) {
      cleanPath = cleanPath.substring(1, cleanPath.length - 1);
    }

    // Nếu người dùng chọn file .jad, tự động tìm file .jar cùng tên trong thư mục
    if (cleanPath.toLowerCase().endsWith('.jad')) {
      final jarCandidate = '${cleanPath.substring(0, cleanPath.length - 4)}.jar';
      if (File(jarCandidate).existsSync()) {
        cleanPath = jarCandidate;
      }
    }

    final pathPtr = cleanPath.toNativeUtf8();
    final errBuf = calloc<ffi.Uint8>(512).cast<Utf8>();
    final appId = _bindings.appInstallerInstall(_engine!, pathPtr, true, errBuf, 512);
    final errMsg = errBuf.toDartString();
    calloc.free(pathPtr);
    calloc.free(errBuf);

    if (appId > 0) {
      setState(() {
        _loadInstalledApps();
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Đã cài đặt thành công: ${cleanPath.split(RegExp(r'[\\/]')).last}"),
          backgroundColor: const Color(0xFF1E88E5),
        ),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Không thể cài đặt tệp: ${errMsg.isNotEmpty ? errMsg : cleanPath}"),
          backgroundColor: Colors.redAccent,
        ),
      );
    }
  }

  void _onAddGamePressed() {
    // 1. Trên Desktop (Windows, macOS, Linux): Tự động mở hộp thoại chọn tệp của hệ điều hành
    if (Platform.isWindows || Platform.isMacOS || Platform.isLinux) {
      final pathBuf = calloc<ffi.Uint8>(2048).cast<Utf8>();
      final ok = _bindings.platformPickFile(pathBuf, 2048);
      if (ok) {
        final pickedPath = pathBuf.toDartString();
        calloc.free(pathBuf);
        if (pickedPath.isNotEmpty && File(pickedPath).existsSync()) {
          _installJarPath(pickedPath);
          return;
        }
      } else {
        calloc.free(pathBuf);
      }
      return;
    }

    // 2. Trên Android, iOS hoặc duyệt thư mục bộ nhớ trên thiết bị:
    _showDeviceStorageBrowser();
  }

  void _showDeviceStorageBrowser() {
    showDialog(
      context: context,
      builder: (ctx) => DeviceStorageBrowserDialog(
        onFileSelected: (selectedPath) {
          _installJarPath(selectedPath);
        },
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
                    id: game.id,
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
              if (_engine != null && game.id > 0) {
                _bindings.appRepoDelete(_engine!, game.id);
              }
              setState(() {
                _loadInstalledApps();
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
        onPressed: _onAddGamePressed,
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

class DeviceStorageBrowserDialog extends StatefulWidget {
  final Function(String selectedPath) onFileSelected;

  const DeviceStorageBrowserDialog({super.key, required this.onFileSelected});

  @override
  State<DeviceStorageBrowserDialog> createState() => _DeviceStorageBrowserDialogState();
}

class _DeviceStorageBrowserDialogState extends State<DeviceStorageBrowserDialog> {
  late Directory _currentDir;
  final TextEditingController _pathController = TextEditingController();
  List<FileSystemEntity> _entries = [];
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _currentDir = Directory(_resolveInitialDirectory());
    _pathController.text = _currentDir.path;
    _refreshDirectory();
  }

  @override
  void dispose() {
    _pathController.dispose();
    super.dispose();
  }

  String _resolveInitialDirectory() {
    if (Platform.isAndroid) {
      const paths = ['/storage/emulated/0/Download', '/storage/emulated/0', '/sdcard/Download', '/sdcard'];
      for (final p in paths) {
        if (Directory(p).existsSync()) return p;
      }
    } else if (Platform.isWindows) {
      final userProfile = Platform.environment['USERPROFILE'];
      if (userProfile != null) {
        final downloads = '$userProfile\\Downloads';
        if (Directory(downloads).existsSync()) return downloads;
        return userProfile;
      }
    } else if (Platform.isMacOS || Platform.isLinux) {
      final home = Platform.environment['HOME'];
      if (home != null) {
        final downloads = '$home/Downloads';
        if (Directory(downloads).existsSync()) return downloads;
        return home;
      }
    }
    return Directory.current.path;
  }

  void _refreshDirectory() {
    setState(() => _isLoading = true);
    try {
      if (_currentDir.existsSync()) {
        final raw = _currentDir.listSync(followLinks: false);
        final dirs = raw.whereType<Directory>().toList()
          ..sort((a, b) => a.path.toLowerCase().compareTo(b.path.toLowerCase()));

        final files = raw.whereType<File>().where((f) {
          final p = f.path.toLowerCase();
          return p.endsWith('.jar') || p.endsWith('.jad');
        }).toList()
          ..sort((a, b) => a.path.toLowerCase().compareTo(b.path.toLowerCase()));

        _entries = [...dirs, ...files];
      } else {
        _entries = [];
      }
    } catch (_) {
      _entries = [];
    }
    setState(() => _isLoading = false);
  }

  void _navigateTo(Directory dir) {
    if (dir.existsSync()) {
      setState(() {
        _currentDir = dir;
        _pathController.text = dir.path;
      });
      _refreshDirectory();
    }
  }

  void _navigateUp() {
    final parent = _currentDir.parent;
    if (parent.path != _currentDir.path) {
      _navigateTo(parent);
    }
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      backgroundColor: const Color(0xFF1E1E1E),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      titlePadding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
      contentPadding: const EdgeInsets.symmetric(horizontal: 16),
      title: Row(
        children: [
          const Icon(Icons.folder_open, color: Color(0xFF1E88E5), size: 24),
          const SizedBox(width: 8),
          const Expanded(
            child: Text(
              "Chọn tệp JAR từ bộ nhớ",
              style: TextStyle(color: Colors.white, fontSize: 16, fontWeight: FontWeight.bold),
            ),
          ),
          IconButton(
            icon: const Icon(Icons.arrow_upward, color: Colors.white70, size: 20),
            tooltip: "Lên thư mục cha",
            onPressed: _navigateUp,
          ),
        ],
      ),
      content: SizedBox(
        width: 500,
        height: 420,
        child: Column(
          children: [
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              decoration: BoxDecoration(
                color: const Color(0xFF121212),
                borderRadius: BorderRadius.circular(6),
                border: Border.all(color: const Color(0xFF333333)),
              ),
              child: Row(
                children: [
                  Expanded(
                    child: Text(
                      _currentDir.path,
                      style: const TextStyle(color: Color(0xFF90CAF9), fontSize: 12, fontFamily: 'monospace'),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  IconButton(
                    icon: const Icon(Icons.refresh, color: Colors.white60, size: 16),
                    tooltip: "Tải lại",
                    onPressed: _refreshDirectory,
                    padding: EdgeInsets.zero,
                    constraints: const BoxConstraints(),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 8),
            Expanded(
              child: _isLoading
                  ? const Center(child: CircularProgressIndicator(color: Color(0xFF1E88E5)))
                  : _entries.isEmpty
                      ? const Center(
                          child: Text(
                            "Không tìm thấy tệp .jar trong thư mục này",
                            style: TextStyle(color: Colors.white38, fontSize: 13),
                          ),
                        )
                      : ListView.builder(
                          itemCount: _entries.length,
                          itemBuilder: (context, index) {
                            final entity = _entries[index];
                            final name = entity.path.split(RegExp(r'[\\/]')).last;
                            final isDir = entity is Directory;
                            final isJar = name.toLowerCase().endsWith('.jar');

                            return ListTile(
                              dense: true,
                              contentPadding: const EdgeInsets.symmetric(horizontal: 8),
                              leading: Icon(
                                isDir
                                    ? Icons.folder
                                    : isJar
                                        ? Icons.sports_esports
                                        : Icons.insert_drive_file,
                                color: isDir
                                    ? const Color(0xFFFFA726)
                                    : const Color(0xFF1E88E5),
                                size: 22,
                              ),
                              title: Text(
                                name,
                                style: TextStyle(
                                  color: isDir ? Colors.white : const Color(0xFFE0E0E0),
                                  fontWeight: isDir ? FontWeight.w500 : FontWeight.bold,
                                  fontSize: 13,
                                ),
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                              ),
                              subtitle: !isDir && entity is File
                                  ? FutureBuilder<int>(
                                      future: entity.length(),
                                      builder: (context, snapshot) {
                                        final bytes = snapshot.data ?? 0;
                                        final kb = (bytes / 1024).toStringAsFixed(1);
                                        return Text("$kb KB", style: const TextStyle(color: Colors.white38, fontSize: 11));
                                      },
                                    )
                                  : null,
                              onTap: () {
                                if (isDir) {
                                  _navigateTo(entity);
                                } else {
                                  Navigator.pop(context);
                                  widget.onFileSelected(entity.path);
                                }
                              },
                            );
                          },
                        ),
            ),
            const Divider(color: Color(0xFF333333)),
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _pathController,
                    style: const TextStyle(color: Colors.white, fontSize: 12),
                    decoration: const InputDecoration(
                      hintText: "Nhập đường dẫn tệp...",
                      hintStyle: TextStyle(color: Colors.white30, fontSize: 12),
                      isDense: true,
                      contentPadding: EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                      filled: true,
                      fillColor: Color(0xFF141414),
                      border: OutlineInputBorder(),
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                ElevatedButton(
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFF1E88E5),
                    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                  ),
                  onPressed: () {
                    final manualPath = _pathController.text.trim();
                    if (manualPath.isNotEmpty && File(manualPath).existsSync()) {
                      Navigator.pop(context);
                      widget.onFileSelected(manualPath);
                    } else if (manualPath.isNotEmpty && Directory(manualPath).existsSync()) {
                      _navigateTo(Directory(manualPath));
                    }
                  },
                  child: const Text("Mở", style: TextStyle(fontSize: 12)),
                ),
              ],
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text("Hủy", style: TextStyle(color: Colors.white60)),
        ),
      ],
    );
  }
}
