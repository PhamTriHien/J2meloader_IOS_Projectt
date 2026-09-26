import 'dart:ffi' as ffi;
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:ffi/ffi.dart';
import '../bridge/j2me_ffi.dart';
import 'game_viewport.dart';
import 'virtual_keypad.dart';

class EmulatorScreen extends StatefulWidget {
  final ffi.Pointer<ffi.Void> engineInstance;
  final String title;

  const EmulatorScreen({
    super.key,
    required this.engineInstance,
    required this.title,
  });

  @override
  State<EmulatorScreen> createState() => _EmulatorScreenState();
}

class _EmulatorScreenState extends State<EmulatorScreen> {
  final FocusNode _focusNode = FocusNode();
  bool _isPaused = false;
  bool _pixelArtFilter = true;
  bool _showVirtualKeypad = true;
  int _speedMultiplier = 1;
  final J2meBindings _bindings = J2meBindings.instance;

  @override
  void initState() {
    super.initState();
    _bindings.coreStart(widget.engineInstance);
  }

  @override
  void dispose() {
    _focusNode.dispose();
    _bindings.coreStop(widget.engineInstance);
    super.dispose();
  }

  void _handleKeyEvent(int code, bool pressed) {
    _bindings.coreSendKey(widget.engineInstance, code, pressed);
  }

  KeyEventResult _handlePhysicalKey(FocusNode node, KeyEvent event) {
    int j2meCode = 0;

    // Ánh xạ phím vật lý bàn phím máy tính sang mã phím J2ME
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

  void _takeScreenshot() {
    final timestamp = DateTime.now().millisecondsSinceEpoch;
    final dir = Directory.current.path;
    final filePath = "$dir/screenshot_$timestamp.png";
    final pathPtr = filePath.toNativeUtf8();
    final ok = _bindings.coreCaptureScreenshotPng(widget.engineInstance, pathPtr);
    calloc.free(pathPtr);

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(ok ? "Đã chụp ảnh màn hình: $filePath" : "Chụp ảnh màn hình thất bại"),
        backgroundColor: ok ? const Color(0xFF0284C7) : Colors.redAccent,
        duration: const Duration(seconds: 2),
      ),
    );
  }

  void _cycleSpeed() {
    setState(() {
      if (_speedMultiplier == 1) {
        _speedMultiplier = 2;
      } else if (_speedMultiplier == 2) {
        _speedMultiplier = 4;
      } else {
        _speedMultiplier = 1;
      }
      _bindings.coreSetSpeedMultiplier(widget.engineInstance, _speedMultiplier);
    });
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Tốc độ mô phỏng: ${_speedMultiplier}x"),
        duration: const Duration(milliseconds: 700),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Focus(
      focusNode: _focusNode,
      autofocus: true,
      onKeyEvent: _handlePhysicalKey,
      child: Scaffold(
        backgroundColor: const Color(0xFF0F172A),
        appBar: AppBar(
          backgroundColor: const Color(0xFF1E293B),
          elevation: 0,
          title: Text(
            widget.title,
            style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.white),
          ),
          actions: [
            // Nút điều chỉnh tốc độ mô phỏng (Speed Multiplier 1x, 2x, 4x)
            TextButton(
              onPressed: _cycleSpeed,
              child: Text(
                "${_speedMultiplier}x",
                style: const TextStyle(
                  color: Color(0xFF38BDF8),
                  fontWeight: FontWeight.bold,
                  fontSize: 15,
                ),
              ),
            ),
            // Nút chụp ảnh màn hình Game PNG (Screenshot)
            IconButton(
              icon: const Icon(Icons.camera_alt_outlined, color: Colors.white),
              tooltip: "Chụp ảnh màn hình (PNG)",
              onPressed: _takeScreenshot,
            ),
            // Nút ẩn / hiện Bàn phím ảo (Virtual Keypad Toggle)
            IconButton(
              icon: Icon(
                _showVirtualKeypad ? Icons.keyboard : Icons.keyboard_hide,
                color: _showVirtualKeypad ? const Color(0xFF38BDF8) : Colors.grey,
              ),
              tooltip: _showVirtualKeypad ? "Ẩn bàn phím ảo" : "Hiện bàn phím ảo",
              onPressed: () {
                setState(() {
                  _showVirtualKeypad = !_showVirtualKeypad;
                });
              },
            ),
            // Nút bộ lọc điểm ảnh Pixel Art sắc nét
            IconButton(
              icon: Icon(
                _pixelArtFilter ? Icons.blur_off : Icons.blur_on,
                color: _pixelArtFilter ? const Color(0xFF38BDF8) : Colors.grey,
              ),
              tooltip: "Chế độ Pixel Art sắc nét",
              onPressed: () {
                setState(() {
                  _pixelArtFilter = !_pixelArtFilter;
                });
              },
            ),
            // Nút Tạm dừng / Tiếp tục
            IconButton(
              icon: Icon(_isPaused ? Icons.play_arrow : Icons.pause, color: Colors.white),
              tooltip: _isPaused ? "Tiếp tục" : "Tạm dừng",
              onPressed: () {
                setState(() {
                  _isPaused = !_isPaused;
                  if (_isPaused) {
                    _bindings.corePause(widget.engineInstance);
                  } else {
                    _bindings.coreResume(widget.engineInstance);
                  }
                });
              },
            ),
          ],
        ),
        body: Column(
          children: [
            // Màn hình hiển thị Game J2ME Viewport
            Expanded(
              child: GameViewport(
                engineInstance: widget.engineInstance,
                filterPixelArt: _pixelArtFilter,
              ),
            ),
            // Bàn phím ảo (Hiển thị khi được bật)
            if (_showVirtualKeypad)
              VirtualKeypad(
                onKeyEvent: _handleKeyEvent,
              ),
          ],
        ),
      ),
    );
  }
}
