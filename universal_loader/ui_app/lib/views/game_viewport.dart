import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';
import 'package:ffi/ffi.dart';
import '../bridge/j2me_ffi.dart';
import 'lcdui_screen_dialog.dart';
import '../sessions/emulator_settings.dart';
export '../sessions/emulator_settings.dart' show GameScaleMode;

class GameViewport extends StatefulWidget {
  final ffi.Pointer<ffi.Void> engineInstance;
  final bool filterPixelArt;
  final GameScaleMode scaleMode;
  final bool showPhoneFrame;
  final bool autoMatch;
  final bool touchInput;
  final bool showFps;
  final void Function(int width, int height)? onResolutionDetected;
  final void Function(int width, int height)? onCanvasSizeRequested;

  const GameViewport({
    super.key,
    required this.engineInstance,
    this.filterPixelArt = true,
    this.scaleMode = GameScaleMode.fit,
    this.showPhoneFrame = false,
    this.autoMatch = true,
    this.touchInput = true,
    this.showFps = false,
    this.onResolutionDetected,
    this.onCanvasSizeRequested,
  });

  @override
  State<GameViewport> createState() => _GameViewportState();
}

class _GameViewportState extends State<GameViewport> with SingleTickerProviderStateMixin {
  ui.Image? _currentFrame;
  // Game resolution (the decoded image may be prescaled)
  int _gameW = 0;
  int _gameH = 0;
  // Whole-number nearest-neighbour upscale done on the CPU before the GPU scales down to a
  // non-integer size ("sharp bilinear"): crisp pixels without uneven nearest-neighbour columns
  int _prescale = 1;
  bool _forceRedraw = false;
  bool _isDisposed = false;
  bool _isDecoding = false;
  bool _resolutionNotified = false;
  Size? _pendingCanvasSize;
  Timer? _resizeTimer;
  int _fps = 0;
  int _fpsFrames = 0;
  int _fpsEpochMs = 0;
  late final ffi.Pointer<ffi.Int32> _widthPtr;
  late final ffi.Pointer<ffi.Int32> _heightPtr;
  // Native RGBA staging buffer; only rewritten once the previous frame finished decoding
  ffi.Pointer<ffi.Uint8> _pixels = ffi.nullptr;
  int _pixelsCap = 0;
  // Polls for a new frame once per display vsync
  late final Ticker _ticker;
  // Last seen Form/TextBox serial of the engine and whether its dialog is open
  int _screenSerial = 0;
  bool _screenDialogOpen = false;

  @override
  void initState() {
    super.initState();
    _widthPtr = calloc<ffi.Int32>();
    _heightPtr = calloc<ffi.Int32>();

    _ticker = createTicker((elapsed) {
      _fetchNextFrame();
      final now = elapsed.inMilliseconds;
      if (widget.showFps && now - _fpsEpochMs >= 1000) {
        setState(() => _fps = (_fpsFrames * 1000 / (now - _fpsEpochMs)).round());
        _fpsFrames = 0;
        _fpsEpochMs = now;
      }
    })..start();
  }

  @override
  void dispose() {
    _isDisposed = true;
    _ticker.dispose();
    _resizeTimer?.cancel();
    calloc.free(_widthPtr);
    calloc.free(_heightPtr);
    if (!_isDecoding) _freePixels();
    _currentFrame?.dispose();
    super.dispose();
  }

  void _freePixels() {
    if (_pixelsCap > 0) malloc.free(_pixels);
    _pixels = ffi.nullptr;
    _pixelsCap = 0;
  }

  void _pollNativeScreen() {
    final serial = J2meBindings.instance.coreScreenSerial(widget.engineInstance);
    if (serial == _screenSerial) return;
    _screenSerial = serial;
    if (_screenDialogOpen) Navigator.of(context).pop();
    final screen = LcduiScreen.fetch(widget.engineInstance);
    if (screen == null) return;
    _screenDialogOpen = true;
    showLcduiScreenDialog(context, widget.engineInstance, screen).whenComplete(() => _screenDialogOpen = false);
  }

  void _fetchNextFrame() {
    if (_isDisposed) return;
    _pollNativeScreen();
    if (_isDecoding) return;

    // The core converts ARGB to RGBA and prescales in native code, straight into a reusable buffer
    final bindings = J2meBindings.instance;
    final k = _prescale;
    var rc = bindings.coreCopyFrameRgba(widget.engineInstance, _pixels, _pixelsCap, k,
        _forceRedraw || _currentFrame == null, _widthPtr, _heightPtr);
    if (rc < 0) {
      final need = _widthPtr.value * _heightPtr.value * k * k * 4;
      if (need <= 0) return;
      _freePixels();
      _pixels = malloc<ffi.Uint8>(need);
      _pixelsCap = need;
      rc = bindings.coreCopyFrameRgba(widget.engineInstance, _pixels, _pixelsCap, k, true, _widthPtr, _heightPtr);
    }
    if (rc <= 0) return;

    final width = _widthPtr.value;
    final height = _heightPtr.value;
    final outW = width * k;
    final outH = height * k;
    final byteData = _pixels.asTypedList(outW * outH * 4);
    _forceRedraw = false;

    if ((!_resolutionNotified || width != _gameW || height != _gameH) &&
        widget.onResolutionDetected != null) {
      _resolutionNotified = true;
      widget.onResolutionDetected!(width, height);
    }

    _isDecoding = true;
    ui.decodeImageFromPixels(
      byteData,
      outW,
      outH,
      ui.PixelFormat.rgba8888,
      (ui.Image image) {
        if (!_isDisposed) {
          _fpsFrames++;
          final old = _currentFrame;
          setState(() {
            _currentFrame = image;
            _gameW = width;
            _gameH = height;
          });
          old?.dispose();
        } else {
          image.dispose();
          _freePixels();
        }
        _isDecoding = false;
      },
    );
  }

  void _sendPointerEvent(PointerEvent event, int action, Size targetSize) {
    if (!widget.touchInput) return;
    if (_currentFrame == null || targetSize.width <= 0 || targetSize.height <= 0) return;
    final localX = event.localPosition.dx;
    final localY = event.localPosition.dy;
    if (localX >= 0 && localX < targetSize.width && localY >= 0 && localY < targetSize.height) {
      final fbX = (localX / targetSize.width * _gameW).floor().clamp(0, _gameW - 1);
      final fbY = (localY / targetSize.height * _gameH).floor().clamp(0, _gameH - 1);
      J2meBindings.instance.coreSendTouch(widget.engineInstance, action, fbX, fbY);
    }
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

    final frameW = _gameW.toDouble();
    final frameH = _gameH.toDouble();
    final dpr = MediaQuery.of(context).devicePixelRatio;

    return LayoutBuilder(
      builder: (context, constraints) {
        double displayW;
        double displayH;

        // Match the canvas to its actual viewport, after keypad and safe-area layout.
        if (widget.autoMatch &&
            widget.scaleMode == GameScaleMode.fit &&
            !_screenDialogOpen &&
            MediaQuery.viewInsetsOf(context).bottom == 0 &&
            (ModalRoute.of(context)?.isCurrent ?? true) &&
            !widget.showPhoneFrame &&
            constraints.maxWidth > 0 && constraints.maxHeight > 0 &&
            constraints.maxWidth.isFinite && constraints.maxHeight.isFinite) {
          final width = _gameW;
          final height = (width * constraints.maxHeight / constraints.maxWidth)
              .round().clamp(1, 4096);
          final target = Size(width.toDouble(), height.toDouble());
          if (height != _gameH && _pendingCanvasSize != target) {
            _pendingCanvasSize = target;
            _resizeTimer?.cancel();
            // Wait for rotation and window resizing to settle before restarting a MIDlet.
            _resizeTimer = Timer(const Duration(milliseconds: 250), () {
              if (_isDisposed || _pendingCanvasSize != target) return;
              if (_screenDialogOpen ||
                  MediaQuery.viewInsetsOf(context).bottom > 0 ||
                  !(ModalRoute.of(context)?.isCurrent ?? true)) {
                _pendingCanvasSize = null;
                return;
              }
              final resize = widget.onCanvasSizeRequested;
              if (resize != null) {
                resize(width, height);
              } else {
                J2meBindings.instance.coreSetScreenDimensions(
                    widget.engineInstance, width, height);
              }
              _forceRedraw = true;
            });
          } else if (height == _gameH) {
            _pendingCanvasSize = null;
            _resizeTimer?.cancel();
          }
        } else {
          _pendingCanvasSize = null;
          _resizeTimer?.cancel();
        }

        switch (widget.scaleMode) {
          case GameScaleMode.original1x:
            displayW = frameW;
            displayH = frameH;
            break;
          case GameScaleMode.scale2x:
            displayW = frameW * 2;
            displayH = frameH * 2;
            break;
          case GameScaleMode.scale3x:
            displayW = frameW * 3;
            displayH = frameH * 3;
            break;
          case GameScaleMode.fitWidth:
            displayW = constraints.maxWidth;
            displayH = (frameH * (constraints.maxWidth / frameW)).floorToDouble();
            break;
          case GameScaleMode.stretch:
            displayW = constraints.maxWidth;
            displayH = constraints.maxHeight;
            break;
          case GameScaleMode.fit:
            final containerW = constraints.maxWidth;
            final containerH = constraints.maxHeight;
            if (widget.autoMatch && !widget.showPhoneFrame) {
              displayW = containerW;
              displayH = containerH;
              break;
            }

            var scale = (containerW / frameW < containerH / frameH)
                ? containerW / frameW
                : containerH / frameH;

            // Snap to whole physical pixels when that costs little space
            final physScale = scale * dpr;
            if (physScale >= 1 && physScale - physScale.floorToDouble() < 0.15) scale = physScale.floorToDouble() / dpr;
            displayW = (frameW * scale).floorToDouble();
            displayH = (frameH * scale).floorToDouble();
            break;
        }

        // Integer physical scale: plain nearest-neighbour. Otherwise prescale and let the GPU filter down.
        final phys = displayW * dpr / frameW;
        final integral = (phys - phys.roundToDouble()).abs() < 0.01;
        final wantPrescale = (!widget.filterPixelArt || integral) ? 1 : phys.ceil().clamp(1, 4);
        if (wantPrescale != _prescale) {
          _prescale = wantPrescale;
          _forceRedraw = true;
        }
        final quality = !widget.filterPixelArt
            ? FilterQuality.medium
            : (integral ? FilterQuality.none : FilterQuality.low);

        final rawDisplay = SizedBox(
          width: displayW,
          height: displayH,
          child: Listener(
            onPointerDown: (e) => _sendPointerEvent(e, 0, Size(displayW, displayH)),
            onPointerUp: (e) => _sendPointerEvent(e, 1, Size(displayW, displayH)),
            onPointerMove: (e) => _sendPointerEvent(e, 2, Size(displayW, displayH)),
            child: RawImage(
              image: _currentFrame,
              width: displayW,
              height: displayH,
              filterQuality: quality,
              fit: BoxFit.fill,
            ),
          ),
        );

        Widget content;
        if (widget.showPhoneFrame) {
          content = Container(
            padding: const EdgeInsets.fromLTRB(14, 20, 14, 20),
            decoration: BoxDecoration(
              color: const Color(0xFF1E2430),
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: const Color(0xFF334155), width: 3),
              boxShadow: const [
                BoxShadow(
                  color: Colors.black54,
                  blurRadius: 16,
                  offset: Offset(0, 8),
                ),
              ],
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 44,
                  height: 4,
                  margin: const EdgeInsets.only(bottom: 12),
                  decoration: BoxDecoration(
                    color: const Color(0xFF475569),
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
                Container(
                  padding: const EdgeInsets.all(4),
                  decoration: BoxDecoration(
                    color: Colors.black,
                    borderRadius: BorderRadius.circular(4),
                    border: Border.all(color: const Color(0xFF0F172A), width: 2),
                  ),
                  child: rawDisplay,
                ),
              ],
            ),
          );
        } else {
          content = rawDisplay;
        }

        final viewport = Container(
          color: Colors.black,
          alignment: Alignment.center,
          child: widget.scaleMode == GameScaleMode.fit
              ? content
              : SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: SingleChildScrollView(
                    scrollDirection: Axis.vertical,
                    child: content,
                  ),
                ),
        );
        if (!widget.showFps) return viewport;
        return Stack(
          fit: StackFit.expand,
          children: [
            viewport,
            Positioned(
              top: 4,
              left: 4,
              child: IgnorePointer(
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 5, vertical: 2),
                  color: Colors.black.withAlpha(180),
                  child: Text('$_fps FPS',
                      style: const TextStyle(color: Colors.white, fontSize: 11)),
                ),
              ),
            ),
          ],
        );
      },
    );
  }
}
