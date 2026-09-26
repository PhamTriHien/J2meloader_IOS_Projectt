import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:ffi/ffi.dart';
import '../bridge/j2me_ffi.dart';

class GameViewport extends StatefulWidget {
  final ffi.Pointer<ffi.Void> engineInstance;
  final bool filterPixelArt;

  const GameViewport({
    super.key,
    required this.engineInstance,
    this.filterPixelArt = true,
  });

  @override
  State<GameViewport> createState() => _GameViewportState();
}

class _GameViewportState extends State<GameViewport> with SingleTickerProviderStateMixin {
  ui.Image? _currentFrame;
  bool _isDisposed = false;
  bool _isDecoding = false;
  late final ffi.Pointer<ffi.Int32> _widthPtr;
  late final ffi.Pointer<ffi.Int32> _heightPtr;
  late final ffi.Pointer<ffi.Bool> _dirtyPtr;
  Timer? _renderTimer;

  @override
  void initState() {
    super.initState();
    _widthPtr = calloc<ffi.Int32>();
    _heightPtr = calloc<ffi.Int32>();
    _dirtyPtr = calloc<ffi.Bool>();

    // 60 FPS Polling loop để lấy frame từ C++ Core
    _renderTimer = Timer.periodic(const Duration(milliseconds: 16), (_) => _fetchNextFrame());
  }

  @override
  void dispose() {
    _isDisposed = true;
    _renderTimer?.cancel();
    calloc.free(_widthPtr);
    calloc.free(_heightPtr);
    calloc.free(_dirtyPtr);
    _currentFrame?.dispose();
    super.dispose();
  }

  void _fetchNextFrame() {
    if (_isDisposed || _isDecoding) return;

    final bindings = J2meBindings.instance;
    final fbPtr = bindings.coreLockFramebuffer(widget.engineInstance, _widthPtr, _heightPtr, _dirtyPtr);

    if (fbPtr == ffi.nullptr) return;

    final width = _widthPtr.value;
    final height = _heightPtr.value;
    final dirty = _dirtyPtr.value;

    if (!dirty && _currentFrame != null) {
      bindings.coreUnlockFramebuffer(widget.engineInstance);
      return;
    }

    final totalPixels = width * height;
    if (totalPixels <= 0) {
      bindings.coreUnlockFramebuffer(widget.engineInstance);
      return;
    }

    // Sao chép buffer điểm ảnh sang Uint8List
    final uint32List = fbPtr.asTypedList(totalPixels);
    final byteData = Uint8List(totalPixels * 4);

    // Chuyển đổi định dạng điểm ảnh ARGB sang RGBA chuẩn cho Flutter Skia/Impeller
    for (int i = 0; i < totalPixels; ++i) {
      final p = uint32List[i];
      final a = (p >> 24) & 0xFF;
      final r = (p >> 16) & 0xFF;
      final g = (p >> 8) & 0xFF;
      final b = p & 0xFF;

      final offset = i * 4;
      byteData[offset] = r;
      byteData[offset + 1] = g;
      byteData[offset + 2] = b;
      byteData[offset + 3] = a;
    }

    bindings.coreUnlockFramebuffer(widget.engineInstance);

    _isDecoding = true;
    ui.decodeImageFromPixels(
      byteData,
      width,
      height,
      ui.PixelFormat.rgba8888,
      (ui.Image image) {
        if (!_isDisposed) {
          final old = _currentFrame;
          setState(() {
            _currentFrame = image;
          });
          old?.dispose();
        } else {
          image.dispose();
        }
        _isDecoding = false;
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_currentFrame == null) {
      return Container(
        color: const Color(0xFF0F172A),
        child: const Center(
          child: CircularProgressIndicator(color: Color(0xFF38BDF8)),
        ),
      );
    }

    return LayoutBuilder(
      builder: (context, constraints) {
        return Container(
          color: Colors.black,
          alignment: Alignment.center,
          child: AspectRatio(
            aspectRatio: _currentFrame!.width / _currentFrame!.height,
            child: RawImage(
              image: _currentFrame,
              filterQuality: widget.filterPixelArt ? FilterQuality.none : FilterQuality.medium,
              fit: BoxFit.contain,
            ),
          ),
        );
      },
    );
  }
}
