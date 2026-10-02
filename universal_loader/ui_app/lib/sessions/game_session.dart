import 'dart:ffi' as ffi;
import 'dart:io';
import 'package:flutter/widgets.dart';
import '../bridge/j2me_ffi.dart';
import '../bridge/platform_channel.dart';

class GameSession {
  final String id;
  final int appId;
  final String appPath;
  final String title;
  final int cloneSlot; // 0 = bản chính, 1..N = bản sao (clone)
  final ffi.Pointer<ffi.Void> engineInstance;
  final DateTime startedAt;
  bool isPaused;

  GameSession({
    required this.id,
    required this.appId,
    required this.appPath,
    required this.title,
    required this.cloneSlot,
    required this.engineInstance,
    required this.startedAt,
    this.isPaused = false,
  });

  String get displayName {
    if (cloneSlot == 0) {
      return title;
    }
    return "$title (Bản sao $cloneSlot)";
  }

  String get slotLabel {
    if (cloneSlot == 0) {
      return "Bản chính";
    }
    return "Bản sao $cloneSlot";
  }
}

class GameSessionManager extends ChangeNotifier with WidgetsBindingObserver {
  static final GameSessionManager instance = GameSessionManager._internal();
  GameSessionManager._internal() {
    WidgetsBinding.instance.addObserver(this);
  }

  final Set<String> _suspendedSessions = {};

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // iOS suspends the process in the background; keep VM pause state consistent.
    if (!Platform.isIOS) return;
    if (state == AppLifecycleState.paused) {
      for (final session in _sessions) {
        if (!session.isPaused) {
          _suspendedSessions.add(session.id);
          _bindings.corePause(session.engineInstance);
          session.isPaused = true;
        }
      }
      notifyListeners();
    } else if (state == AppLifecycleState.resumed) {
      for (final session in _sessions) {
        if (_suspendedSessions.remove(session.id)) {
          _bindings.coreResume(session.engineInstance);
          session.isPaused = false;
        }
      }
      notifyListeners();
    }
  }

  void setPaused(GameSession session, bool paused) {
    if (!_sessions.contains(session)) return;
    _suspendedSessions.remove(session.id);
    if (paused) {
      _bindings.corePause(session.engineInstance);
    } else {
      _bindings.coreResume(session.engineInstance);
    }
    session.isPaused = paused;
    notifyListeners();
  }

  final List<GameSession> _sessions = [];
  GameSession? _activeSession;
  ffi.Pointer<ffi.Void>? _libraryEngine;
  final J2meBindings _bindings = J2meBindings.instance;

  List<GameSession> get sessions => List.unmodifiable(_sessions);
  GameSession? get activeSession => _activeSession;
  int get activeCount => _sessions.length;

  void setLibraryEngine(ffi.Pointer<ffi.Void> engine) {
    _libraryEngine = engine;
  }

  void setActiveSession(GameSession? session) {
    if (_activeSession != session) {
      _activeSession = session;
      notifyListeners();
    }
  }

  GameSession? findSession(int appId, int cloneSlot) {
    try {
      return _sessions.firstWhere((s) => s.appId == appId && s.cloneSlot == cloneSlot);
    } catch (_) {
      return null;
    }
  }

  List<GameSession> sessionsForApp(int appId) {
    return _sessions.where((s) => s.appId == appId).toList();
  }

  int getNextAvailableSlot(int appId) {
    final existingSlots = _sessions
        .where((s) => s.appId == appId)
        .map((s) => s.cloneSlot)
        .toSet();
    int slot = 1;
    while (existingSlots.contains(slot)) {
      slot++;
    }
    return slot;
  }

  GameSession? launchOrActivate({
    required int appId,
    required String appPath,
    required String title,
    int cloneSlot = 0,
  }) {
    final existing = findSession(appId, cloneSlot);
    if (existing != null) {
      _activeSession = existing;
      notifyListeners();
      return existing;
    }

    if (_libraryEngine == null) return null;

    final spawnedEngine = _bindings.appSpawn(_libraryEngine!, appId, cloneSlot);
    if (spawnedEngine == ffi.nullptr || spawnedEngine.address == 0) {
      return null;
    }

    // Khởi chạy vòng lặp game loop trên luồng nền của engine mới
    _bindings.coreStart(spawnedEngine);

    final sessionId = "${appId}_slot_${cloneSlot}_${DateTime.now().millisecondsSinceEpoch}";
    final session = GameSession(
      id: sessionId,
      appId: appId,
      appPath: appPath,
      title: title,
      cloneSlot: cloneSlot,
      engineInstance: spawnedEngine,
      startedAt: DateTime.now(),
    );

    _sessions.add(session);
    _activeSession = session;
    _updateBackgroundService();
    notifyListeners();
    return session;
  }

  GameSession? cloneSession(GameSession source) {
    final nextSlot = getNextAvailableSlot(source.appId);
    return launchOrActivate(
      appId: source.appId,
      appPath: source.appPath,
      title: source.title,
      cloneSlot: nextSlot,
    );
  }

  GameSession? restartSession(GameSession session, {int? screenWidth, int? screenHeight}) {
    final idx = _sessions.indexWhere((s) => s.id == session.id);
    if (idx < 0) return null;

    final target = _sessions[idx];
    final speed = _bindings.coreGetSpeedMultiplier(target.engineInstance);
    if (_libraryEngine == null) return null;
    final spawnedEngine = _bindings.appSpawn(_libraryEngine!, target.appId, target.cloneSlot);
    if (spawnedEngine == ffi.nullptr || spawnedEngine.address == 0) return null;
    try {
      _bindings.coreStop(target.engineInstance);
      _bindings.coreDestroy(target.engineInstance);
    } catch (_) {}

    if (screenWidth != null && screenHeight != null) {
      _bindings.coreSetScreenDimensions(spawnedEngine, screenWidth, screenHeight);
    }
    _bindings.coreSetSpeedMultiplier(spawnedEngine, speed);
    _bindings.coreStart(spawnedEngine);
    if (target.isPaused) _bindings.corePause(spawnedEngine);

    final newSession = GameSession(
      id: target.id,
      appId: target.appId,
      appPath: target.appPath,
      title: target.title,
      cloneSlot: target.cloneSlot,
      engineInstance: spawnedEngine,
      startedAt: DateTime.now(),
      isPaused: target.isPaused,
    );

    _sessions[idx] = newSession;
    if (_activeSession?.id == target.id) _activeSession = newSession;
    _updateBackgroundService();
    notifyListeners();
    return newSession;
  }

  void stopSession(GameSession session) {
    final idx = _sessions.indexWhere((s) => s.id == session.id);
    if (idx < 0) return;

    final target = _sessions.removeAt(idx);
    _suspendedSessions.remove(target.id);

    if (_activeSession?.id == target.id) {
      _activeSession = _sessions.isNotEmpty ? _sessions.last : null;
    }

    _updateBackgroundService();
    notifyListeners();

    // Dừng và giải phóng engine
    Future.microtask(() {
      try {
        _bindings.coreStop(target.engineInstance);
        _bindings.coreDestroy(target.engineInstance);
      } catch (_) {}
    });
  }

  void stopAll() {
    final toStop = List<GameSession>.from(_sessions);
    _sessions.clear();
    _suspendedSessions.clear();
    _activeSession = null;
    _updateBackgroundService();
    notifyListeners();

    Future.microtask(() {
      for (final s in toStop) {
        try {
          _bindings.coreStop(s.engineInstance);
          _bindings.coreDestroy(s.engineInstance);
        } catch (_) {}
      }
    });
  }

  void _updateBackgroundService() {
    if (J2mePlatform.isMobile) {
      final count = _sessions.length;
      final desc = count > 0
          ? _sessions.map((s) => s.displayName).join(", ")
          : "";
      J2mePlatform.updateBackgroundRunning(count, desc);
    }
  }
}
