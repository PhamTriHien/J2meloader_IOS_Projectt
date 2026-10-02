import 'dart:async';
import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:ffi/ffi.dart';
import '../bridge/j2me_ffi.dart';
import '../bridge/platform_channel.dart';
import '../sessions/game_session.dart';
import '../sessions/emulator_settings.dart';
import 'game_viewport.dart';
import 'virtual_keypad.dart';
import 'app_config_dialog.dart';
import 'lcdui_screen_dialog.dart';

const int kEmuBarHeight = 40;

enum ScreenOrientationMode {
  auto,      // Tự động theo tỉ lệ game / thiết bị
  portrait,  // Màn hình dọc
  landscape, // Màn hình ngang (chia 2 cánh D-Pad & Numpad)
}

class EmulatorScreen extends StatefulWidget {
  final ffi.Pointer<ffi.Void>? engineInstance;
  final String title;
  final String appPath;
  final GameSession? initialSession;

  const EmulatorScreen({
    super.key,
    this.engineInstance,
    this.title = "",
    this.appPath = "",
    this.initialSession,
  });

  @override
  State<EmulatorScreen> createState() => _EmulatorScreenState();
}

class _EmulatorScreenState extends State<EmulatorScreen> {
  final FocusNode _focusNode = FocusNode();
  bool _isPaused = false;
  bool _pixelArtFilter = true;
  bool _showVirtualKeypad = true;
  bool _showPhoneFrame = false;
  bool _autoMatch = true;
  bool _touchInput = true;
  bool _showFps = false;
  bool _keypadFeedback = true;
  KeypadButtonShape _keypadShape = KeypadButtonShape.roundRect;
  String? _loadedSessionId;
  ScreenOrientationMode _orientationMode = ScreenOrientationMode.auto;
  GameScaleMode _scaleMode = GameScaleMode.fit;
  KeypadType _keypadType = KeypadType.full;
  double _keypadOpacity = 0.95;
  int _speedMultiplier = 1;
  int _phoneWidth = 240;
  int _phoneHeight = 320;
  final J2meBindings _bindings = J2meBindings.instance;
  Timer? _commandsTimer;
  int _commandsVersion = -1;
  List<LcduiCommand> _canvasCommands = const [];

  GameSessionManager get _sessionManager => GameSessionManager.instance;

  GameSession? get _currentSession {
    if (widget.initialSession != null && _sessionManager.sessions.contains(widget.initialSession)) {
      return _sessionManager.activeSession ?? widget.initialSession;
    }
    return _sessionManager.activeSession ?? _sessionManager.sessions.firstOrNull;
  }

  ffi.Pointer<ffi.Void> get _currentEngine {
    final sess = _currentSession;
    if (sess != null) return sess.engineInstance;
    return widget.engineInstance ?? ffi.nullptr;
  }

  String get _currentTitle {
    final sess = _currentSession;
    if (sess != null) return sess.title;
    return widget.title;
  }

  String get _currentAppPath {
    final sess = _currentSession;
    if (sess != null) return sess.appPath;
    return widget.appPath;
  }

  @override
  void initState() {
    super.initState();
    if (widget.initialSession != null) {
      _sessionManager.setActiveSession(widget.initialSession);
    }
    _sessionManager.addListener(_onSessionsChanged);

    _loadAppConfigAndApply(restart: false);

    final eng = _currentEngine;
    if (eng != ffi.nullptr && eng.address != 0) {
      _bindings.coreStart(eng);
    }

    _commandsTimer = Timer.periodic(const Duration(milliseconds: 250), (_) => _pollCanvasCommands());
    if (Platform.isWindows) {
      _applyWindowDimensions(_phoneWidth, _phoneHeight, _scaleMode, _showVirtualKeypad);
    }
  }

  void _loadAppConfigAndApply({bool restart = false}) {
    if (_currentAppPath.isEmpty) return;
    final file = File('./universal_rms/configs/$_currentAppPath/config.json');
    {
      try {
        final json = file.existsSync()
            ? jsonDecode(file.readAsStringSync()) as Map<String, dynamic>
            : <String, dynamic>{};
        final settings = EmulatorSettings(json);
        final w = settings.width;
        final h = settings.height;
        final fps = settings.fps;
        final showKb = json['ShowKeyboard'] as bool? ?? true;
        final vkType = (json['VirtualKeyboardType'] as num?)?.toInt() ?? 0;
        final vkAlpha = (json['VirtualKeyboardAlpha'] as num?)?.toInt() ?? 95;

        final isMobile = Platform.isAndroid || Platform.isIOS;
        final ScreenOrientationMode initialOrientation = (w > h) ? ScreenOrientationMode.landscape : ScreenOrientationMode.portrait;

        setState(() {
          _phoneWidth = w;
          _phoneHeight = h;
          _showVirtualKeypad = showKb;
          _keypadType = (vkType == 1) ? KeypadType.dpadOnly : (vkType == 2 ? KeypadType.numpadOnly : KeypadType.full);
          _keypadOpacity = (vkAlpha / 100.0).clamp(0.1, 1.0);
          _scaleMode = settings.scaleMode;
          _autoMatch = settings.autoMatch;
          _pixelArtFilter = settings.flag('ScreenFilter', true);
          _touchInput = settings.flag('TouchInput', true);
          _showFps = settings.flag('ShowFps', false);
          _keypadFeedback = settings.flag('VirtualKeyboardFeedback', true);
          _keypadShape = KeypadButtonShape.values[
              settings.number('ButtonShape', 0).clamp(0, 2)];
          _orientationMode = switch (settings.orientation) {
            1 => ScreenOrientationMode.auto,
            2 => ScreenOrientationMode.portrait,
            3 => ScreenOrientationMode.landscape,
            _ => initialOrientation,
          };
          _speedMultiplier = _currentEngine != ffi.nullptr
              ? _bindings.coreGetSpeedMultiplier(_currentEngine) : 1;
          _isPaused = _currentSession?.isPaused ?? false;
        });

        if (isMobile) {
          _applySystemOrientation(_orientationMode);
        }

        final eng = _currentEngine;
        if (eng != ffi.nullptr && eng.address != 0) {
          // Spawn already applies the native profile before startApp.
          _bindings.coreSetFpsLimit(eng, fps);
          if (restart) {
            _restartGame();
          }
        }
        if (Platform.isWindows) {
          _applyWindowDimensions(w, h, _scaleMode, _showVirtualKeypad);
        }
      } catch (_) {}
    }
    _loadedSessionId = _currentSession?.id;
  }

  void _saveUiSettings(Map<String, dynamic> changes) {
    if (_currentAppPath.isEmpty) return;
    final file = File('./universal_rms/configs/$_currentAppPath/config.json');
    try {
      final json = file.existsSync()
          ? jsonDecode(file.readAsStringSync()) as Map<String, dynamic>
          : <String, dynamic>{};
      json.addAll(changes);
      file.parent.createSync(recursive: true);
      file.writeAsStringSync(const JsonEncoder.withIndent('  ').convert(json));
    } catch (error) {
      debugPrint('Could not save emulator settings: $error');
    }
  }

  void _onSessionsChanged() {
    if (!mounted) return;
    if (_sessionManager.sessions.isEmpty) {
      Navigator.of(context).maybePop();
      return;
    }
    if (_currentSession?.id != _loadedSessionId) {
      _loadAppConfigAndApply();
    }
    _isPaused = _currentSession?.isPaused ?? false;
    setState(() {});
  }

  @override
  void dispose() {
    if (Platform.isAndroid || Platform.isIOS) {
      SystemChrome.setPreferredOrientations([
        DeviceOrientation.portraitUp,
        DeviceOrientation.portraitDown,
        DeviceOrientation.landscapeLeft,
        DeviceOrientation.landscapeRight,
      ]);
    }
    _sessionManager.removeListener(_onSessionsChanged);
    if (_sessionManager.activeSession?.id == _loadedSessionId) {
      _sessionManager.setActiveSession(null);
    }
    _commandsTimer?.cancel();
    _focusNode.dispose();
    super.dispose();
  }

  void _applySystemOrientation(ScreenOrientationMode mode) {
    if (!Platform.isAndroid && !Platform.isIOS) return;
    switch (mode) {
      case ScreenOrientationMode.auto:
        SystemChrome.setPreferredOrientations([
          DeviceOrientation.portraitUp,
          DeviceOrientation.portraitDown,
          DeviceOrientation.landscapeLeft,
          DeviceOrientation.landscapeRight,
        ]);
        break;
      case ScreenOrientationMode.portrait:
        SystemChrome.setPreferredOrientations([
          DeviceOrientation.portraitUp,
          DeviceOrientation.portraitDown,
        ]);
        break;
      case ScreenOrientationMode.landscape:
        SystemChrome.setPreferredOrientations([
          DeviceOrientation.landscapeLeft,
          DeviceOrientation.landscapeRight,
        ]);
        break;
    }
  }

  void _toggleOrientation() {
    switch (_orientationMode) {
      case ScreenOrientationMode.auto:
        _setOrientation(ScreenOrientationMode.landscape);
        break;
      case ScreenOrientationMode.landscape:
        _setOrientation(ScreenOrientationMode.portrait);
        break;
      case ScreenOrientationMode.portrait:
        _setOrientation(ScreenOrientationMode.landscape);
        break;
    }
  }

  void _setOrientation(ScreenOrientationMode mode, {bool autoRestart = true}) {
    int targetW = _phoneWidth;
    int targetH = _phoneHeight;
    bool needsRestart = false;
    if ((mode == ScreenOrientationMode.landscape && targetW < targetH) ||
        (mode == ScreenOrientationMode.portrait && targetW > targetH)) {
      targetW = _phoneHeight;
      targetH = _phoneWidth;
      needsRestart = true;
    }

    setState(() {
      _orientationMode = mode;
      _phoneWidth = targetW;
      _phoneHeight = targetH;
    });

    // Cập nhật cấu hình vào file config.json
    _saveUiSettings({
      'ScreenWidth': targetW,
      'ScreenHeight': targetH,
      'Orientation': mode == ScreenOrientationMode.landscape ? 3
          : (mode == ScreenOrientationMode.portrait ? 2 : 1),
    });

    _applySystemOrientation(mode);

    final sess = _currentSession;
    if (needsRestart && autoRestart && sess != null) {
      final restarted = _sessionManager.restartSession(sess,
          screenWidth: targetW, screenHeight: targetH);
      if (restarted != null) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              mode == ScreenOrientationMode.landscape
                  ? "🔄 Đã xoay Ngang & Khởi động lại: $targetW x $targetH"
                  : "🔄 Đã xoay Dọc & Khởi động lại: $targetW x $targetH",
            ),
            backgroundColor: const Color(0xFF0284C7),
            duration: const Duration(milliseconds: 1000),
          ),
        );
      }
    } else if (needsRestart) {
      final eng = _currentEngine;
      if (eng != ffi.nullptr && eng.address != 0) {
        _bindings.coreSetScreenDimensions(eng, targetW, targetH);
      }
    }

    if (Platform.isWindows) {
      _applyWindowDimensions(targetW, targetH, _scaleMode, _showVirtualKeypad);
    }
  }

  void _toggleVirtualKeypad() {
    final nextState = !_showVirtualKeypad;
    setState(() {
      _showVirtualKeypad = nextState;
    });
    _saveUiSettings({'ShowKeyboard': nextState});
    if (Platform.isWindows) {
      _applyWindowDimensions(_phoneWidth, _phoneHeight, _scaleMode, nextState);
    }
  }

  // Canvas Commands (Displayable.addCommand)
  void _pollCanvasCommands() {
    final eng = _currentEngine;
    if (eng == ffi.nullptr || eng.address == 0) return;
    final v = _bindings.coreCanvasCommandsVersion(eng);
    if (v == _commandsVersion) return;
    _commandsVersion = v;
    final cmds = fetchCanvasCommands(eng) ?? const <LcduiCommand>[];
    if (mounted) setState(() => _canvasCommands = cmds);
  }

  void _onResolutionDetected(int width, int height) {
    setState(() {
      _phoneWidth = width;
      _phoneHeight = height;
    });
  }

  void _resizeCanvasToViewport(int width, int height) {
    if (!mounted) return;
    final session = _currentSession;
    if (session != null) {
      // Some MIDlets cache Canvas dimensions in startApp, so resize before start.
      _sessionManager.restartSession(session,
          screenWidth: width, screenHeight: height);
    } else {
      _bindings.coreSetScreenDimensions(_currentEngine, width, height);
    }
    setState(() {
      _phoneWidth = width;
      _phoneHeight = height;
    });
  }

  int _keypadHeightFor(KeypadType type) {
    final isMobile = Platform.isAndroid || Platform.isIOS;
    if (isMobile) {
      switch (type) {
        case KeypadType.full:
          return 290;
        case KeypadType.dpadOnly:
          return 128;
        case KeypadType.numpadOnly:
          return 168;
      }
    }
    switch (type) {
      case KeypadType.full:
        return 170;
      case KeypadType.dpadOnly:
        return 75;
      case KeypadType.numpadOnly:
        return 99;
    }
  }

  void _applyWindowDimensions(int phoneW, int phoneH, GameScaleMode mode, bool showKeypad) {
    if (!Platform.isWindows || phoneW <= 0 || phoneH <= 0) return;
    const appBarHeight = kEmuBarHeight;
    final isLandscape = _orientationMode == ScreenOrientationMode.landscape ||
        (_orientationMode == ScreenOrientationMode.auto && phoneW > phoneH);
    final display = WidgetsBinding.instance.platformDispatcher.displays.first;
    final dpr = display.devicePixelRatio;
    final displayH = display.size.height / dpr;
    final displayW = display.size.width / dpr;

    double scale;
    int targetClientW;
    int targetClientH;

    if (isLandscape) {
      final wingsWidth = showKeypad ? 280 : 0; // 140 left wing + 140 right wing
      final maxGameH = (displayH * 0.78 - appBarHeight).clamp(180.0, displayH - appBarHeight - 60);
      final maxGameW = (displayW * 0.82 - wingsWidth).clamp(240.0, displayW - wingsWidth - 40);

      switch (mode) {
        case GameScaleMode.original1x:
          scale = 1.0;
          break;
        case GameScaleMode.scale2x:
          scale = 2.0;
          break;
        case GameScaleMode.scale3x:
          scale = 3.0;
          break;
        case GameScaleMode.fit:
        case GameScaleMode.fitWidth:
        case GameScaleMode.stretch:
          final rawScale = (maxGameH / phoneH < maxGameW / phoneW)
              ? (maxGameH / phoneH)
              : (maxGameW / phoneW);
          scale = rawScale.clamp(0.2, 4.0);
          final phys = scale * dpr;
          if (phys - phys.floorToDouble() < 0.15) scale = phys.floorToDouble() / dpr;
          break;
      }
      targetClientW = (phoneW * scale).round() + wingsWidth;
      targetClientH = (phoneH * scale).round() + appBarHeight;
    } else {
      final keypadHeight = showKeypad ? _keypadHeightFor(_keypadType) : 0;
      final maxAllowedTotalH = (displayH * 0.82).floor();
      final maxGameH = (maxAllowedTotalH - keypadHeight - appBarHeight).clamp(160.0, displayH);
      final maxGameW = (displayW * 0.75).clamp(200.0, displayW - 40);

      switch (mode) {
        case GameScaleMode.original1x:
          scale = 1.0;
          break;
        case GameScaleMode.scale2x:
          scale = 2.0;
          break;
        case GameScaleMode.scale3x:
          scale = 3.0;
          break;
        case GameScaleMode.fit:
        case GameScaleMode.fitWidth:
        case GameScaleMode.stretch:
          final rawScale = (maxGameH / phoneH < maxGameW / phoneW)
              ? (maxGameH / phoneH)
              : (maxGameW / phoneW);
          scale = rawScale.clamp(0.2, 4.0);
          final phys = scale * dpr;
          if (phys - phys.floorToDouble() < 0.15) scale = phys.floorToDouble() / dpr;
          break;
      }
      targetClientW = (phoneW * scale).round();
      if (targetClientW < 240) targetClientW = 240;
      targetClientH = (phoneH * scale).round() + keypadHeight + appBarHeight;
    }

    final w = targetClientW, h = targetClientH;
    Timer.run(() => _bindings.platformSetWindowSize(w, h));
  }

  void _handleKeyEvent(int code, bool pressed) {
    final eng = _currentEngine;
    if (eng != ffi.nullptr && eng.address != 0) {
      _bindings.coreSendKey(eng, code, pressed);
    }
  }

  KeyEventResult _handlePhysicalKey(FocusNode node, KeyEvent event) {
    int j2meCode = 0;

    if (event.logicalKey == LogicalKeyboardKey.escape && event is KeyDownEvent) {
      _showInGameMenu();
      return KeyEventResult.handled;
    }

    if (event.logicalKey == LogicalKeyboardKey.arrowUp || event.logicalKey == LogicalKeyboardKey.keyW) {
      j2meCode = -1;
    } else if (event.logicalKey == LogicalKeyboardKey.arrowDown || event.logicalKey == LogicalKeyboardKey.keyS) {
      j2meCode = -2;
    } else if (event.logicalKey == LogicalKeyboardKey.arrowLeft || event.logicalKey == LogicalKeyboardKey.keyA) {
      j2meCode = -3;
    } else if (event.logicalKey == LogicalKeyboardKey.arrowRight || event.logicalKey == LogicalKeyboardKey.keyD) {
      j2meCode = -4;
    } else if (event.logicalKey == LogicalKeyboardKey.enter ||
               event.logicalKey == LogicalKeyboardKey.space ||
               event.logicalKey == LogicalKeyboardKey.keyJ ||
               event.logicalKey == LogicalKeyboardKey.keyK ||
               event.logicalKey == LogicalKeyboardKey.keyZ ||
               event.logicalKey == LogicalKeyboardKey.keyX) {
      j2meCode = -5;
    } else if (event.logicalKey == LogicalKeyboardKey.f1) {
      j2meCode = -6;
    } else if (event.logicalKey == LogicalKeyboardKey.f2) {
      j2meCode = -7;
    } else if (event.logicalKey == LogicalKeyboardKey.backspace) {
      j2meCode = -8;
    } else if (event.logicalKey == LogicalKeyboardKey.digit0 || event.logicalKey == LogicalKeyboardKey.numpad0) {
      j2meCode = 48;
    } else if (event.logicalKey == LogicalKeyboardKey.digit1 || event.logicalKey == LogicalKeyboardKey.numpad1) {
      j2meCode = 49;
    } else if (event.logicalKey == LogicalKeyboardKey.digit2 || event.logicalKey == LogicalKeyboardKey.numpad2) {
      j2meCode = 50;
    } else if (event.logicalKey == LogicalKeyboardKey.digit3 || event.logicalKey == LogicalKeyboardKey.numpad3) {
      j2meCode = 51;
    } else if (event.logicalKey == LogicalKeyboardKey.digit4 || event.logicalKey == LogicalKeyboardKey.numpad4) {
      j2meCode = 52;
    } else if (event.logicalKey == LogicalKeyboardKey.digit5 || event.logicalKey == LogicalKeyboardKey.numpad5) {
      j2meCode = 53;
    } else if (event.logicalKey == LogicalKeyboardKey.digit6 || event.logicalKey == LogicalKeyboardKey.numpad6) {
      j2meCode = 54;
    } else if (event.logicalKey == LogicalKeyboardKey.digit7 || event.logicalKey == LogicalKeyboardKey.numpad7) {
      j2meCode = 55;
    } else if (event.logicalKey == LogicalKeyboardKey.digit8 || event.logicalKey == LogicalKeyboardKey.numpad8) {
      j2meCode = 56;
    } else if (event.logicalKey == LogicalKeyboardKey.digit9 || event.logicalKey == LogicalKeyboardKey.numpad9) {
      j2meCode = 57;
    }

    if (j2meCode != 0) {
      if (event is KeyDownEvent) {
        _handleKeyEvent(j2meCode, true);
        return KeyEventResult.handled;
      } else if (event is KeyUpEvent) {
        _handleKeyEvent(j2meCode, false);
        return KeyEventResult.handled;
      }
    }
    return KeyEventResult.ignored;
  }

  Future<void> _takeScreenshot() async {
    final eng = _currentEngine;
    if (eng == ffi.nullptr || eng.address == 0) return;
    final timestamp = DateTime.now().millisecondsSinceEpoch;
    final dir = Directory.current.path;
    final filePath = "$dir/screenshot_$timestamp.png";
    final pathPtr = filePath.toNativeUtf8();
    final ok = _bindings.coreCaptureScreenshotPng(eng, pathPtr);
    calloc.free(pathPtr);
    if (ok && J2mePlatform.isMobile) {
      try {
        final exported = await J2mePlatform.exportFile(filePath, 'screenshot_$timestamp.png');
        if (!mounted || exported == null) return;
      } catch (error) {
        if (!mounted) return;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Xuất ảnh thất bại: $error')),
        );
        return;
      }
    }
    if (!mounted) return;

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(ok ? (J2mePlatform.isMobile ? 'Đã xuất ảnh màn hình' : "Đã chụp ảnh màn hình: $filePath") : "Chụp ảnh màn hình thất bại"),
        backgroundColor: ok ? const Color(0xFF0284C7) : Colors.redAccent,
        duration: const Duration(seconds: 2),
      ),
    );
  }

  void _cycleSpeed() {
    final eng = _currentEngine;
    if (eng == ffi.nullptr || eng.address == 0) return;
    setState(() {
      if (_speedMultiplier == 1) {
        _speedMultiplier = 2;
      } else if (_speedMultiplier == 2) {
        _speedMultiplier = 4;
      } else {
        _speedMultiplier = 1;
      }
      _bindings.coreSetSpeedMultiplier(eng, _speedMultiplier);
    });
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Tốc độ mô phỏng: ${_speedMultiplier}x"),
        duration: const Duration(milliseconds: 700),
      ),
    );
  }

  void _changeScaleMode(GameScaleMode mode) {
    setState(() {
      _scaleMode = mode;
    });
    _saveUiSettings(EmulatorSettings.scaleValues(mode));
    _applyWindowDimensions(_phoneWidth, _phoneHeight, mode, _showVirtualKeypad);
  }

  void _setPaused(bool paused) {
    final session = _currentSession;
    if (session != null) {
      _sessionManager.setPaused(session, paused);
    } else if (_currentEngine != ffi.nullptr) {
      if (paused) {
        _bindings.corePause(_currentEngine);
      } else {
        _bindings.coreResume(_currentEngine);
      }
    }
    if (mounted) setState(() => _isPaused = paused);
  }

  void _restartGame() {
    final sess = _currentSession;
    if (sess != null) {
      final restarted = _sessionManager.restartSession(sess);
      if (restarted != null) {
        _loadAppConfigAndApply(restart: false);
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text("Đã khởi động lại game với cấu hình mới!"),
            backgroundColor: Color(0xFF0284C7),
            duration: Duration(milliseconds: 1000),
          ),
        );
        return;
      }
    }
    final eng = _currentEngine;
    if (eng == ffi.nullptr || eng.address == 0) return;
    _bindings.coreStop(eng);
    _bindings.coreStart(eng);
    _loadAppConfigAndApply(restart: false);
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text("Đã khởi động lại game!"),
        backgroundColor: Color(0xFF0284C7),
        duration: Duration(milliseconds: 900),
      ),
    );
  }

  void _stopCurrentSession() {
    final sess = _currentSession;
    if (sess != null) {
      _sessionManager.stopSession(sess);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Đã dừng: ${sess.displayName}"),
          backgroundColor: Colors.redAccent,
          duration: const Duration(seconds: 1),
        ),
      );
    } else if (widget.engineInstance != null) {
      _bindings.coreStop(widget.engineInstance!);
      _bindings.coreDestroy(widget.engineInstance!);
      Navigator.of(context).maybePop();
    }
  }

  void _cloneCurrentSession() {
    final sess = _currentSession;
    if (sess == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Không tìm thấy thông tin phiên game để nhân bản"),
          backgroundColor: Colors.redAccent,
        ),
      );
      return;
    }
    final cloned = _sessionManager.cloneSession(sess);
    if (cloned != null) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Đã mở bản sao mới: ${cloned.displayName}"),
          backgroundColor: const Color(0xFF0284C7),
          duration: const Duration(seconds: 2),
        ),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("Không thể nhân bản phiên game này"),
          backgroundColor: Colors.redAccent,
        ),
      );
    }
  }

  void _showTabsMenu() {
    final sessions = _sessionManager.sessions;
    final active = _currentSession;

    showModalBottomSheet(
      context: context,
      backgroundColor: const Color(0xFF131B2E),
      shape: const RoundedRectangleBorder(borderRadius: BorderRadius.vertical(top: Radius.circular(16))),
      builder: (ctx) => StatefulBuilder(
        builder: (context, setSheetState) => Padding(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Center(
                  child: Container(
                    width: 32,
                    height: 3,
                    decoration: BoxDecoration(
                      color: const Color(0xFF334155),
                      borderRadius: BorderRadius.circular(2),
                    ),
                  ),
                ),
                const SizedBox(height: 10),
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Text(
                      "DANH SÁCH GAME ĐANG CHẠY (TABS)",
                      style: TextStyle(color: Colors.white, fontSize: 12, fontWeight: FontWeight.bold, letterSpacing: 0.3),
                    ),
                    Text(
                      "${sessions.length} tab",
                      style: const TextStyle(color: Color(0xFF38BDF8), fontSize: 11.5, fontWeight: FontWeight.bold),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                for (final s in sessions)
                  Container(
                    margin: const EdgeInsets.only(bottom: 6),
                    decoration: BoxDecoration(
                      color: s.id == active?.id ? const Color(0xFF1E293B) : const Color(0xFF0F172A),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                        color: s.id == active?.id ? const Color(0xFF0284C7) : const Color(0xFF1E293B),
                        width: 1.2,
                      ),
                    ),
                    child: ListTile(
                      dense: true,
                      contentPadding: const EdgeInsets.symmetric(horizontal: 10, vertical: 0),
                      visualDensity: VisualDensity.compact,
                      leading: Icon(
                        Icons.sports_esports,
                        color: s.id == active?.id ? const Color(0xFF38BDF8) : Colors.white60,
                        size: 18,
                      ),
                      title: Row(
                        children: [
                          Flexible(
                            child: Text(
                              s.title,
                              overflow: TextOverflow.ellipsis,
                              style: TextStyle(
                                color: Colors.white,
                                fontSize: 12,
                                fontWeight: s.id == active?.id ? FontWeight.bold : FontWeight.normal,
                              ),
                            ),
                          ),
                          const SizedBox(width: 6),
                          Container(
                            padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 1),
                            decoration: BoxDecoration(
                              color: s.cloneSlot == 0 ? const Color(0xFF0284C7) : const Color(0xFFEAB308),
                              borderRadius: BorderRadius.circular(4),
                            ),
                            child: Text(
                              s.slotLabel,
                              style: TextStyle(
                                color: s.cloneSlot == 0 ? Colors.white : Colors.black,
                                fontSize: 9,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ],
                      ),
                      subtitle: Text(
                        s.id == active?.id ? "Đang hiển thị" : "Đang chạy nền",
                        style: TextStyle(
                          color: s.id == active?.id ? const Color(0xFF38BDF8) : Colors.white54,
                          fontSize: 10,
                        ),
                      ),
                      trailing: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          if (s.id != active?.id)
                            TextButton(
                              style: TextButton.styleFrom(
                                foregroundColor: const Color(0xFF38BDF8),
                                padding: const EdgeInsets.symmetric(horizontal: 6),
                                visualDensity: VisualDensity.compact,
                              ),
                              onPressed: () {
                                Navigator.pop(ctx);
                                _sessionManager.setActiveSession(s);
                              },
                              child: const Text("Chuyển", style: TextStyle(fontSize: 11)),
                            ),
                          IconButton(
                            visualDensity: VisualDensity.compact,
                            padding: EdgeInsets.zero,
                            constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
                            icon: const Icon(Icons.close, color: Colors.redAccent, size: 16),
                            tooltip: "Dừng game này",
                            onPressed: () {
                              _sessionManager.stopSession(s);
                              setSheetState(() {});
                            },
                          ),
                        ],
                      ),
                      onTap: () {
                        Navigator.pop(ctx);
                        _sessionManager.setActiveSession(s);
                      },
                    ),
                  ),
                const Divider(color: Color(0xFF334155), height: 16),
                Wrap(
                  spacing: 6,
                  runSpacing: 6,
                  children: [
                    ElevatedButton.icon(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF0284C7),
                        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                        visualDensity: VisualDensity.compact,
                      ),
                      icon: const Icon(Icons.control_point_duplicate, size: 15),
                      label: const Text("Nhân bản game (+)", style: TextStyle(fontSize: 11.5)),
                      onPressed: () {
                        Navigator.pop(ctx);
                        _cloneCurrentSession();
                      },
                    ),
                    OutlinedButton.icon(
                      style: OutlinedButton.styleFrom(
                        foregroundColor: Colors.white,
                        side: const BorderSide(color: Color(0xFF334155)),
                        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                        visualDensity: VisualDensity.compact,
                      ),
                      icon: const Icon(Icons.grid_view, size: 15),
                      label: const Text("Về thư viện", style: TextStyle(fontSize: 11.5)),
                      onPressed: () {
                        Navigator.pop(ctx);
                        Navigator.pop(context);
                      },
                    ),
                  ],
                ),
                const SizedBox(height: 4),
              ],
            ),
          ),
        ),
      ),
    );
  }

  void _showInGameMenu() {
    final wasPaused = _isPaused;
    _setPaused(true);

    showModalBottomSheet(
      context: context,
      backgroundColor: const Color(0xFF131B2E),
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(borderRadius: BorderRadius.vertical(top: Radius.circular(16))),
      builder: (ctx) => StatefulBuilder(
        builder: (context, setSheetState) => ConstrainedBox(
          constraints: BoxConstraints(
            maxHeight: MediaQuery.of(context).size.height * 0.88,
          ),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
            child: SingleChildScrollView(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Center(
                    child: Container(
                      width: 32,
                      height: 3,
                      decoration: BoxDecoration(
                        color: const Color(0xFF334155),
                        borderRadius: BorderRadius.circular(2),
                      ),
                    ),
                  ),
                  const SizedBox(height: 8),
                  // Header: Game Title & Close Button
                  Row(
                    children: [
                      const Icon(Icons.sports_esports, color: Color(0xFF38BDF8), size: 18),
                      const SizedBox(width: 6),
                      Flexible(
                        child: Text(
                          _currentTitle,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(color: Colors.white, fontSize: 13.5, fontWeight: FontWeight.bold),
                        ),
                      ),
                      if (_currentSession != null) ...[
                        const SizedBox(width: 6),
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 1),
                          decoration: BoxDecoration(
                            color: _currentSession!.cloneSlot == 0 ? const Color(0xFF0284C7) : const Color(0xFFEAB308),
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: Text(
                            _currentSession!.slotLabel,
                            style: TextStyle(
                              color: _currentSession!.cloneSlot == 0 ? Colors.white : Colors.black,
                              fontSize: 9,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ),
                      ],
                      const Spacer(),
                      IconButton(
                        visualDensity: VisualDensity.compact,
                        padding: EdgeInsets.zero,
                        constraints: const BoxConstraints(minWidth: 26, minHeight: 26),
                        icon: const Icon(Icons.close, color: Colors.white60, size: 18),
                        onPressed: () => Navigator.pop(ctx),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  // 2 Primary action buttons: Tiếp tục & Khởi động lại
                  Row(
                    children: [
                      Expanded(
                        child: ElevatedButton.icon(
                          style: ElevatedButton.styleFrom(
                            backgroundColor: const Color(0xFF0284C7),
                            padding: const EdgeInsets.symmetric(vertical: 7),
                            visualDensity: VisualDensity.compact,
                          ),
                          icon: const Icon(Icons.play_arrow, size: 16),
                          label: const Text("Tiếp tục", style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold)),
                          onPressed: () {
                            Navigator.pop(ctx);
                            _setPaused(false);
                          },
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: OutlinedButton.icon(
                          style: OutlinedButton.styleFrom(
                            foregroundColor: const Color(0xFFF59E0B),
                            side: const BorderSide(color: Color(0xFFF59E0B)),
                            padding: const EdgeInsets.symmetric(vertical: 7),
                            visualDensity: VisualDensity.compact,
                          ),
                          icon: const Icon(Icons.restart_alt, size: 16),
                          label: const Text("Khởi động lại", style: TextStyle(fontSize: 12)),
                          onPressed: () {
                            Navigator.pop(ctx);
                            _restartGame();
                          },
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  // Quick Adjustments Box (Tốc độ, Tỉ lệ, Kiểu phím, Độ mờ)
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                    decoration: BoxDecoration(
                      color: const Color(0xFF0F172A),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: const Color(0xFF1E293B)),
                    ),
                    child: Column(
                      children: [
                        // Tốc độ mô phỏng
                        Row(
                          children: [
                            const Icon(Icons.speed, color: Color(0xFF38BDF8), size: 15),
                            const SizedBox(width: 6),
                            const Text("Tốc độ:", style: TextStyle(color: Colors.white70, fontSize: 11.5)),
                            const Spacer(),
                            for (final spd in [1, 2, 4]) ...[
                              Padding(
                                padding: const EdgeInsets.only(left: 4),
                                child: InkWell(
                                  onTap: () {
                                    setSheetState(() => _speedMultiplier = spd);
                                    setState(() {
                                      _speedMultiplier = spd;
                                      if (_currentEngine != ffi.nullptr) {
                                        _bindings.coreSetSpeedMultiplier(_currentEngine, spd);
                                      }
                                    });
                                  },
                                  borderRadius: BorderRadius.circular(4),
                                  child: Container(
                                    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                                    decoration: BoxDecoration(
                                      color: _speedMultiplier == spd ? const Color(0xFF0284C7) : const Color(0xFF1E293B),
                                      borderRadius: BorderRadius.circular(4),
                                    ),
                                    child: Text(
                                      "${spd}x",
                                      style: TextStyle(
                                        color: _speedMultiplier == spd ? Colors.white : Colors.white60,
                                        fontSize: 11,
                                        fontWeight: _speedMultiplier == spd ? FontWeight.bold : FontWeight.normal,
                                      ),
                                    ),
                                  ),
                                ),
                              ),
                            ],
                          ],
                        ),
                        const Divider(color: Color(0xFF1E293B), height: 12),
                        // Hướng màn hình (Orientation)
                        Row(
                          children: [
                            const Icon(Icons.screen_rotation, color: Color(0xFF38BDF8), size: 15),
                            const SizedBox(width: 6),
                            const Text("Hướng màn hình:", style: TextStyle(color: Colors.white70, fontSize: 11.5)),
                            const Spacer(),
                            DropdownButton<ScreenOrientationMode>(
                              value: _orientationMode,
                              isDense: true,
                              dropdownColor: const Color(0xFF1E293B),
                              style: const TextStyle(color: Colors.white, fontSize: 11.5),
                              items: const [
                                DropdownMenuItem(value: ScreenOrientationMode.auto, child: Text("Tự động (Auto)", style: TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: ScreenOrientationMode.portrait, child: Text("Màn hình Dọc", style: TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: ScreenOrientationMode.landscape, child: Text("Màn hình Ngang", style: TextStyle(fontSize: 11.5))),
                              ],
                              onChanged: (v) {
                                if (v != null) {
                                  setSheetState(() => _orientationMode = v);
                                  _setOrientation(v);
                                }
                              },
                            ),
                          ],
                        ),
                        const Divider(color: Color(0xFF1E293B), height: 12),
                        // Tỉ lệ màn hình
                        Row(
                          children: [
                            const Icon(Icons.aspect_ratio, color: Color(0xFF38BDF8), size: 15),
                            const SizedBox(width: 6),
                            const Text("Tỉ lệ:", style: TextStyle(color: Colors.white70, fontSize: 11.5)),
                            const Spacer(),
                            DropdownButton<GameScaleMode>(
                              value: _scaleMode,
                              isDense: true,
                              dropdownColor: const Color(0xFF1E293B),
                              style: const TextStyle(color: Colors.white, fontSize: 11.5),
                              items: [
                                const DropdownMenuItem(value: GameScaleMode.fit, child: Text("Vừa màn hình (Fit)", style: TextStyle(fontSize: 11.5))),
                                const DropdownMenuItem(value: GameScaleMode.fitWidth, child: Text("Tràn viền ngang (Fit Width)", style: TextStyle(fontSize: 11.5))),
                                const DropdownMenuItem(value: GameScaleMode.stretch, child: Text("Giãn đầy (Stretch)", style: TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: GameScaleMode.original1x, child: Text("1x Gốc (${_phoneWidth}x$_phoneHeight)", style: const TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: GameScaleMode.scale2x, child: Text("2x Phóng to (${_phoneWidth * 2}x${_phoneHeight * 2})", style: const TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: GameScaleMode.scale3x, child: Text("3x Phóng to (${_phoneWidth * 3}x${_phoneHeight * 3})", style: const TextStyle(fontSize: 11.5))),
                              ],
                              onChanged: (v) {
                                if (v != null) {
                                  setSheetState(() => _scaleMode = v);
                                  _changeScaleMode(v);
                                }
                              },
                            ),
                          ],
                        ),
                        const Divider(color: Color(0xFF1E293B), height: 12),
                        // Kiểu phím ảo
                        Row(
                          children: [
                            const Icon(Icons.grid_view, color: Color(0xFF38BDF8), size: 15),
                            const SizedBox(width: 6),
                            const Text("Kiểu phím:", style: TextStyle(color: Colors.white70, fontSize: 11.5)),
                            const Spacer(),
                            DropdownButton<KeypadType>(
                              value: _keypadType,
                              isDense: true,
                              dropdownColor: const Color(0xFF1E293B),
                              style: const TextStyle(color: Colors.white, fontSize: 11.5),
                              items: const [
                                DropdownMenuItem(value: KeypadType.full, child: Text("Đầy đủ 12 phím", style: TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: KeypadType.dpadOnly, child: Text("Chỉ D-Pad", style: TextStyle(fontSize: 11.5))),
                                DropdownMenuItem(value: KeypadType.numpadOnly, child: Text("Chỉ số 0-9", style: TextStyle(fontSize: 11.5))),
                              ],
                              onChanged: (v) {
                                if (v != null) {
                                  setSheetState(() => _keypadType = v);
                                  setState(() => _keypadType = v);
                                  _saveUiSettings({'VirtualKeyboardType': v.index});
                                  _applyWindowDimensions(_phoneWidth, _phoneHeight, _scaleMode, _showVirtualKeypad);
                                }
                              },
                            ),
                          ],
                        ),
                        const Divider(color: Color(0xFF1E293B), height: 12),
                        // Độ mờ bàn phím
                        Row(
                          children: [
                            const Icon(Icons.opacity, color: Color(0xFF38BDF8), size: 15),
                            const SizedBox(width: 6),
                            Text("Độ mờ: ${(_keypadOpacity * 100).toInt()}%", style: const TextStyle(color: Colors.white70, fontSize: 11.5)),
                            Expanded(
                              child: SliderTheme(
                                data: SliderTheme.of(context).copyWith(
                                  trackHeight: 2,
                                  thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 6),
                                  overlayShape: const RoundSliderOverlayShape(overlayRadius: 12),
                                ),
                                child: Slider(
                                  value: _keypadOpacity,
                                  min: 0.1,
                                  max: 1.0,
                                  divisions: 18,
                                  activeColor: const Color(0xFF38BDF8),
                                  onChanged: (v) {
                                    setSheetState(() => _keypadOpacity = v);
                                    setState(() => _keypadOpacity = v);
                                    _saveUiSettings({'VirtualKeyboardAlpha': (v * 100).round()});
                                  },
                                ),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 10),
                  // Quick Features (Bàn phím, Xoay màn hình, Khung máy, Pixel Art, Nhân bản, Chụp ảnh, Cấu hình)
                  Wrap(
                    spacing: 6,
                    runSpacing: 6,
                    children: [
                      _buildQuickChip(
                        icon: _showVirtualKeypad ? Icons.keyboard : Icons.keyboard_hide,
                        label: _showVirtualKeypad ? "Bàn phím: Bật" : "Bàn phím: Tắt",
                        active: _showVirtualKeypad,
                        onTap: () {
                          _toggleVirtualKeypad();
                          setSheetState(() {});
                        },
                      ),
                      _buildQuickChip(
                        icon: _orientationMode == ScreenOrientationMode.landscape
                            ? Icons.stay_current_landscape
                            : (_orientationMode == ScreenOrientationMode.portrait
                                ? Icons.stay_current_portrait
                                : Icons.screen_rotation),
                        label: _orientationMode == ScreenOrientationMode.landscape
                            ? "Màn hình: Ngang"
                            : (_orientationMode == ScreenOrientationMode.portrait
                                ? "Màn hình: Dọc"
                                : "Màn hình: Tự động"),
                        active: _orientationMode != ScreenOrientationMode.auto,
                        onTap: () {
                          _toggleOrientation();
                          setSheetState(() {});
                        },
                      ),
                      _buildQuickChip(
                        icon: Icons.smartphone,
                        label: _showPhoneFrame ? "Khung máy: Bật" : "Khung máy: Tắt",
                        active: _showPhoneFrame,
                        onTap: () {
                          setSheetState(() => _showPhoneFrame = !_showPhoneFrame);
                          setState(() => _showPhoneFrame = !_showPhoneFrame);
                        },
                      ),
                      _buildQuickChip(
                        icon: Icons.blur_off,
                        label: _pixelArtFilter ? "Pixel Art: Sắc nét" : "Pixel Art: Mịn",
                        active: _pixelArtFilter,
                        onTap: () {
                          final next = !_pixelArtFilter;
                          setSheetState(() => _pixelArtFilter = next);
                          setState(() => _pixelArtFilter = next);
                          _saveUiSettings({'ScreenFilter': next});
                        },
                      ),
                      _buildQuickChip(
                        icon: Icons.control_point_duplicate,
                        label: "Nhân bản (Clone)",
                        active: false,
                        onTap: () {
                          Navigator.pop(ctx);
                          _cloneCurrentSession();
                        },
                      ),
                      _buildQuickChip(
                        icon: Icons.camera_alt,
                        label: "Chụp ảnh",
                        active: false,
                        onTap: () {
                          Navigator.pop(ctx);
                          _takeScreenshot();
                        },
                      ),
                      if (_currentAppPath.isNotEmpty)
                        _buildQuickChip(
                          icon: Icons.tune,
                          label: "Cấu hình Game",
                          active: false,
                          onTap: () {
                            Navigator.pop(ctx);
                            showDialog(
                              context: context,
                              builder: (c) => AppConfigDialog(
                                appPath: _currentAppPath,
                                appTitle: _currentTitle,
                                onSaved: () {
                                  _loadAppConfigAndApply(restart: true);
                                },
                              ),
                            );
                          },
                        ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  const Divider(color: Color(0xFF334155), height: 10),
                  const SizedBox(height: 4),
                  // Footer buttons: Về Thư viện & Đóng game
                  Row(
                    children: [
                      Expanded(
                        child: OutlinedButton.icon(
                          style: OutlinedButton.styleFrom(
                            foregroundColor: const Color(0xFF38BDF8),
                            side: const BorderSide(color: Color(0xFF334155)),
                            padding: const EdgeInsets.symmetric(vertical: 6),
                            visualDensity: VisualDensity.compact,
                          ),
                          icon: const Icon(Icons.grid_view, size: 14),
                          label: const Text("Về Thư viện (Chạy nền)", style: TextStyle(fontSize: 11)),
                          onPressed: () {
                            Navigator.pop(ctx);
                            Navigator.pop(context);
                          },
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: ElevatedButton.icon(
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.redAccent.shade700,
                            padding: const EdgeInsets.symmetric(vertical: 6),
                            visualDensity: VisualDensity.compact,
                          ),
                          icon: const Icon(Icons.power_settings_new, size: 14),
                          label: const Text("Đóng game hoàn toàn", style: TextStyle(fontSize: 11)),
                          onPressed: () {
                            Navigator.pop(ctx);
                            _stopCurrentSession();
                          },
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 6),
                ],
              ),
            ),
          ),
        ),
      ),
    ).then((_) {
      if (_isPaused) {
        _setPaused(wasPaused);
      }
    });
  }

  Widget _buildQuickChip({
    required IconData icon,
    required String label,
    required bool active,
    required VoidCallback onTap,
  }) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(6),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
        decoration: BoxDecoration(
          color: active ? const Color(0xFF0284C7) : const Color(0xFF1E293B),
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: active ? const Color(0xFF38BDF8) : const Color(0xFF334155), width: 1.0),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 13, color: active ? Colors.white : const Color(0xFF38BDF8)),
            const SizedBox(width: 5),
            Text(
              label,
              style: TextStyle(
                color: Colors.white,
                fontSize: 10.5,
                fontWeight: active ? FontWeight.bold : FontWeight.normal,
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final session = _currentSession;
    final engine = _currentEngine;
    final totalSessions = _sessionManager.sessions.length;
    final screenWidth = MediaQuery.of(context).size.width;
    final isMobile = Platform.isAndroid || Platform.isIOS;

    return Focus(
      focusNode: _focusNode,
      autofocus: true,
      onKeyEvent: _handlePhysicalKey,
      child: Scaffold(
        resizeToAvoidBottomInset: false,
        backgroundColor: const Color(0xFF0F172A),
        appBar: AppBar(
          backgroundColor: const Color(0xFF1E293B),
          elevation: 0,
          toolbarHeight: isMobile ? 44.0 : kEmuBarHeight.toDouble(),
          leadingWidth: isMobile ? 36 : 32,
          leading: IconButton(
            icon: Icon(Icons.arrow_back, size: isMobile ? 20 : 18),
            padding: EdgeInsets.zero,
            visualDensity: VisualDensity.compact,
            tooltip: "Về Thư viện (Game tiếp tục chạy nền)",
            onPressed: () => Navigator.maybePop(context),
          ),
          titleSpacing: 0,
          title: InkWell(
            onTap: _showTabsMenu,
            borderRadius: BorderRadius.circular(6),
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 2),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Flexible(
                    child: Text(
                      _currentTitle,
                      overflow: TextOverflow.ellipsis,
                      maxLines: 1,
                      style: TextStyle(fontSize: isMobile ? 14 : 13, fontWeight: FontWeight.bold, color: Colors.white),
                    ),
                  ),
                  if (session != null) ...[
                    const SizedBox(width: 4),
                    Container(
                      padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 1),
                      decoration: BoxDecoration(
                        color: session.cloneSlot == 0 ? const Color(0xFF0284C7) : const Color(0xFFEAB308),
                        borderRadius: BorderRadius.circular(4),
                      ),
                      child: Text(
                        session.slotLabel,
                        style: TextStyle(
                          color: session.cloneSlot == 0 ? Colors.white : Colors.black,
                          fontSize: 9,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                  ],
                  if (totalSessions > 1) ...[
                    const SizedBox(width: 2),
                    Icon(Icons.arrow_drop_down, color: const Color(0xFF38BDF8), size: isMobile ? 20 : 18),
                  ],
                ],
              ),
            ),
          ),
          iconTheme: IconThemeData(size: isMobile ? 20 : 18),
          actionsIconTheme: IconThemeData(size: isMobile ? 20 : 18),
          actions: [
            // Nút Dừng Game (Nhấn giữ để dừng & đóng hoàn toàn)
            Tooltip(
              message: "Nhấn giữ để dừng game hoàn toàn",
              child: InkWell(
                onTap: () {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                      content: Text("ℹ️ Nhấn GIỮ biểu tượng Dừng (màu đỏ) để tắt game hoàn toàn"),
                      duration: Duration(milliseconds: 1400),
                      backgroundColor: Color(0xFF1E293B),
                    ),
                  );
                },
                onLongPress: _stopCurrentSession,
                borderRadius: BorderRadius.circular(16),
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 6),
                  child: Icon(Icons.power_settings_new, color: Colors.redAccent, size: isMobile ? 20 : 18),
                ),
              ),
            ),
            if (_canvasCommands.isNotEmpty && engine != ffi.nullptr)
              PopupMenuButton<int>(
                padding: EdgeInsets.zero,
                iconSize: isMobile ? 20 : 18,
                tooltip: "Lệnh của game",
                icon: const Icon(Icons.more_vert, color: Color(0xFF38BDF8)),
                color: const Color(0xFF1E293B),
                onSelected: (i) => _bindings.coreCanvasCommand(engine, i),
                itemBuilder: (_) => [
                  for (int i = 0; i < _canvasCommands.length; ++i)
                    PopupMenuItem(value: i, child: Text(_canvasCommands[i].label)),
                ],
              ),
            // Hiển thị thêm các phím tắt nhanh trên màn hình rộng
            if (screenWidth >= 460) ...[
              // Nút chuyển đổi tỉ lệ màn hình
              PopupMenuButton<GameScaleMode>(
                padding: EdgeInsets.zero,
                iconSize: isMobile ? 20 : 18,
                tooltip: "Tỉ lệ màn hình",
                icon: const Icon(Icons.aspect_ratio, color: Color(0xFF38BDF8)),
                color: const Color(0xFF1E293B),
                onSelected: _changeScaleMode,
                itemBuilder: (context) => [
                  CheckedPopupMenuItem(
                    checked: _scaleMode == GameScaleMode.fit,
                    value: GameScaleMode.fit,
                    child: const Text("Vừa màn hình (Fit)", style: TextStyle(color: Colors.white)),
                  ),
                  CheckedPopupMenuItem(
                    checked: _scaleMode == GameScaleMode.fitWidth,
                    value: GameScaleMode.fitWidth,
                    child: const Text("Tràn viền ngang (Fit Width)", style: TextStyle(color: Colors.white)),
                  ),
                  CheckedPopupMenuItem(
                    checked: _scaleMode == GameScaleMode.stretch,
                    value: GameScaleMode.stretch,
                    child: const Text("Giãn đầy (Stretch)", style: TextStyle(color: Colors.white)),
                  ),
                  CheckedPopupMenuItem(
                    checked: _scaleMode == GameScaleMode.original1x,
                    value: GameScaleMode.original1x,
                    child: Text("1x Gốc (${_phoneWidth}x$_phoneHeight)", style: const TextStyle(color: Colors.white)),
                  ),
                  CheckedPopupMenuItem(
                    checked: _scaleMode == GameScaleMode.scale2x,
                    value: GameScaleMode.scale2x,
                    child: Text("2x Phóng to (${_phoneWidth * 2}x${_phoneHeight * 2})", style: const TextStyle(color: Colors.white)),
                  ),
                  CheckedPopupMenuItem(
                    checked: _scaleMode == GameScaleMode.scale3x,
                    value: GameScaleMode.scale3x,
                    child: Text("3x Phóng to (${_phoneWidth * 3}x${_phoneHeight * 3})", style: const TextStyle(color: Colors.white)),
                  ),
                ],
              ),
              // Nút điều chỉnh tốc độ mô phỏng
              TextButton(
                style: TextButton.styleFrom(
                  minimumSize: Size(isMobile ? 32 : 28, isMobile ? 32 : 28),
                  padding: EdgeInsets.zero,
                  visualDensity: VisualDensity.compact,
                ),
                onPressed: _cycleSpeed,
                child: Text(
                  "${_speedMultiplier}x",
                  style: TextStyle(
                    color: const Color(0xFF38BDF8),
                    fontWeight: FontWeight.bold,
                    fontSize: isMobile ? 13 : 12,
                  ),
                ),
              ),
              // Nút Tạm dừng / Tiếp tục
              IconButton(
                visualDensity: VisualDensity.compact,
                constraints: BoxConstraints(minWidth: isMobile ? 32 : 28, minHeight: isMobile ? 32 : 28),
                padding: EdgeInsets.zero,
                icon: Icon(_isPaused ? Icons.play_arrow : Icons.pause, color: Colors.white, size: isMobile ? 20 : 18),
                tooltip: _isPaused ? "Tiếp tục" : "Tạm dừng",
                onPressed: () {
                  _setPaused(!_isPaused);
                },
              ),
            ],
            // Nút Đổi hướng màn hình (Xoay Ngang / Dọc / Tự động - Luôn hiển thị)
            IconButton(
              visualDensity: VisualDensity.compact,
              constraints: BoxConstraints(minWidth: isMobile ? 32 : 28, minHeight: isMobile ? 32 : 28),
              padding: EdgeInsets.zero,
              icon: Icon(
                _orientationMode == ScreenOrientationMode.landscape
                    ? Icons.stay_current_landscape
                    : (_orientationMode == ScreenOrientationMode.portrait
                        ? Icons.stay_current_portrait
                        : Icons.screen_rotation),
                color: _orientationMode != ScreenOrientationMode.auto ? const Color(0xFF38BDF8) : Colors.white70,
                size: isMobile ? 20 : 18,
              ),
              tooltip: _orientationMode == ScreenOrientationMode.landscape
                  ? "Hướng: Màn hình ngang (Chạm để chuyển Dọc)"
                  : (_orientationMode == ScreenOrientationMode.portrait
                      ? "Hướng: Màn hình dọc (Chạm để Tự động)"
                      : "Xoay màn hình (Tự động - Chạm để xoay Ngang)"),
              onPressed: _toggleOrientation,
            ),
            // Nút Ẩn / Hiện Bàn phím ảo (Luôn hiển thị trên mọi kích thước)
            IconButton(
              visualDensity: VisualDensity.compact,
              constraints: BoxConstraints(minWidth: isMobile ? 32 : 28, minHeight: isMobile ? 32 : 28),
              padding: EdgeInsets.zero,
              icon: Icon(
                _showVirtualKeypad ? Icons.keyboard : Icons.keyboard_hide,
                color: _showVirtualKeypad ? const Color(0xFF38BDF8) : Colors.grey,
                size: isMobile ? 20 : 18,
              ),
              tooltip: _showVirtualKeypad ? "Ẩn bàn phím ảo" : "Hiện bàn phím ảo",
              onPressed: _toggleVirtualKeypad,
            ),
            // Nút Menu In-Game (Có trên mọi kích thước màn hình)
            IconButton(
              visualDensity: VisualDensity.compact,
              constraints: BoxConstraints(minWidth: isMobile ? 32 : 28, minHeight: isMobile ? 32 : 28),
              padding: EdgeInsets.zero,
              icon: Icon(Icons.menu, color: const Color(0xFF38BDF8), size: isMobile ? 20 : 18),
              tooltip: "Menu giả lập (Esc)",
              onPressed: _showInGameMenu,
            ),
          ],
        ),
        body: engine == ffi.nullptr || engine.address == 0
            ? const Center(child: Text("Không có phiên game nào đang chạy", style: TextStyle(color: Colors.white70)))
            : SafeArea(
                top: false,
                left: true,
                right: true,
                bottom: true,
                child: LayoutBuilder(
                  builder: (context, constraints) {
                    final isLandscape = _orientationMode == ScreenOrientationMode.landscape ||
                        (_orientationMode == ScreenOrientationMode.auto && constraints.maxWidth > constraints.maxHeight * 1.15);

                    if (isLandscape) {
                      final totalW = constraints.maxWidth;
                      final totalH = constraints.maxHeight;
                      final isMobile = Platform.isAndroid || Platform.isIOS;
                      // Stable flank widths keep canvas resizing from feeding back into layout.
                      final wingWidth = !_showVirtualKeypad ? 0.0
                          : (totalW * 0.18).clamp(80.0, isMobile ? 190.0 : 160.0)
                              .clamp(0.0, totalW * 0.3);

                      final dynamicWingKeyH = ((totalH - 40) / 5).clamp(isMobile ? 32.0 : 20.0, isMobile ? 48.0 : 32.0);

                      return Row(
                        children: [
                          if (_showVirtualKeypad && (_keypadType == KeypadType.full || _keypadType == KeypadType.dpadOnly))
                            SizedBox(
                              width: wingWidth,
                              height: totalH,
                              child: VirtualKeypad(
                                onKeyEvent: _handleKeyEvent,
                                enableHaptic: _keypadFeedback,
                                buttonShape: _keypadShape,
                                opacity: _keypadOpacity,
                                layoutMode: KeypadLayoutMode.leftWing,
                                customKeyHeight: dynamicWingKeyH,
                              ),
                            ),
                          Expanded(
                            child: GameViewport(
                              key: ValueKey('${engine.address}_${session?.startedAt.microsecondsSinceEpoch}'),
                              engineInstance: engine,
                              filterPixelArt: _pixelArtFilter,
                              autoMatch: _autoMatch,
                              touchInput: _touchInput,
                              showFps: _showFps,
                              scaleMode: _scaleMode,
                              showPhoneFrame: _showPhoneFrame,
                              onResolutionDetected: _onResolutionDetected,
                              onCanvasSizeRequested: _resizeCanvasToViewport,
                            ),
                          ),
                          if (_showVirtualKeypad && (_keypadType == KeypadType.full || _keypadType == KeypadType.numpadOnly))
                            SizedBox(
                              width: wingWidth,
                              height: totalH,
                              child: VirtualKeypad(
                                onKeyEvent: _handleKeyEvent,
                                enableHaptic: _keypadFeedback,
                                buttonShape: _keypadShape,
                                opacity: _keypadOpacity,
                                layoutMode: KeypadLayoutMode.rightWing,
                                customKeyHeight: dynamicWingKeyH,
                              ),
                            ),
                        ],
                      );
                    }

                    if (!_showVirtualKeypad) {
                      return Container(
                        color: Colors.black,
                        alignment: Alignment.center,
                        child: GameViewport(
                          key: ValueKey('${engine.address}_${session?.startedAt.microsecondsSinceEpoch}'),
                          engineInstance: engine,
                          filterPixelArt: _pixelArtFilter,
                          autoMatch: _autoMatch,
                          touchInput: _touchInput,
                          showFps: _showFps,
                          scaleMode: _scaleMode,
                          showPhoneFrame: _showPhoneFrame,
                          onResolutionDetected: _onResolutionDetected,
                          onCanvasSizeRequested: _resizeCanvasToViewport,
                        ),
                      );
                    }

                    return Column(
                      children: [
                        Expanded(
                          child: Container(
                            color: Colors.black,
                            alignment: Alignment.center,
                            child: GameViewport(
                              key: ValueKey('${engine.address}_${session?.startedAt.microsecondsSinceEpoch}'),
                              engineInstance: engine,
                              filterPixelArt: _pixelArtFilter,
                              autoMatch: _autoMatch,
                              touchInput: _touchInput,
                              showFps: _showFps,
                              scaleMode: _scaleMode,
                              showPhoneFrame: _showPhoneFrame,
                              onResolutionDetected: _onResolutionDetected,
                              onCanvasSizeRequested: _resizeCanvasToViewport,
                            ),
                          ),
                        ),
                        VirtualKeypad(
                          onKeyEvent: _handleKeyEvent,
                          enableHaptic: _keypadFeedback,
                          buttonShape: _keypadShape,
                          opacity: _keypadOpacity,
                          keypadType: _keypadType,
                        ),
                      ],
                    );
                  },
                ),
              ),
      ),
    );
  }
}
