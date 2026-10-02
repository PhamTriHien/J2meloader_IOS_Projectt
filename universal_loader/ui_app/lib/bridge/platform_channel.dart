import 'dart:io';
import 'package:flutter/services.dart';

/// Android / iOS host services from MainActivity.kt and AppDelegate.swift ("j2me/platform").
/// Windows uses the native core (j2me_core_platform_pick_file) instead.
class J2mePlatform {
  static const _channel = MethodChannel('j2me/platform');

  static bool get isMobile => Platform.isAndroid || Platform.isIOS;

  /// Writable per-app directory that holds universal_rms/ (installed games, RMS saves, configs)
  static Future<String?> dataDir() => _channel.invokeMethod<String>('dataDir');

  /// Opens the system document picker; returns a local copy of the chosen file, or null when cancelled
  static Future<String?> pickJar() => _channel.invokeMethod<String>('pickJar');

  static Future<String?> exportFile(String path, String name) =>
      _channel.invokeMethod<String>('exportFile', {'path': path, 'name': name});

  /// On mobile the process starts in "/", so every relative "./universal_rms" path (Dart and the
  /// native core alike) is rebased onto the app's private data directory.
  static Future<void> enterDataDir() async {
    if (!isMobile) return;
    final dir = await dataDir();
    if (dir == null || dir.isEmpty) return;
    await Directory('$dir/universal_rms').create(recursive: true);
    Directory.current = dir;
  }

  /// Updates the OS foreground service notification & wake lock for active game sessions
  static Future<void> updateBackgroundRunning(int count, String description) async {
    if (!Platform.isAndroid) return;
    try {
      await _channel.invokeMethod('updateBackgroundRunning', {
        'count': count,
        'description': description,
      });
    } catch (_) {}
  }
}
