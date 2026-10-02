import 'package:flutter_test/flutter_test.dart';
import 'package:ui_app/sessions/emulator_settings.dart';

void main() {
  test('Legacy core scale values preserve their original meaning', () {
    expect(EmulatorSettings({'ScreenScaleType': 1}).scaleMode, GameScaleMode.fit);
    expect(EmulatorSettings({'ScreenScaleType': 2}).scaleMode, GameScaleMode.stretch);
    expect(EmulatorSettings({'ScreenScaleType': 0}).scaleMode, GameScaleMode.original1x);
    expect(EmulatorSettings({'ScreenKeepAspectRatio': false}).scaleMode,
        GameScaleMode.stretch);
    expect(EmulatorSettings({'ScreenScaleToFit': false}).scaleMode,
        GameScaleMode.original1x);
  });

  test('Every UI scale mode survives config serialization', () {
    for (final mode in GameScaleMode.values) {
      final config = EmulatorSettings.scaleValues(mode);
      expect(EmulatorSettings(config).scaleMode, mode);
      expect(config['ScreenKeepAspectRatio'], mode != GameScaleMode.stretch);
    }
  });

  test('Defaults and invalid dimensions are safe on all platforms', () {
    final defaults = EmulatorSettings({});
    expect(defaults.width, 240);
    expect(defaults.height, 320);
    expect(defaults.autoMatch, isTrue);
    final invalid = EmulatorSettings({
      'ScreenWidth': -1, 'ScreenHeight': 10000, 'FpsLimit': 0,
      'ScreenAutoMatch': false, 'ScreenFilter': 'invalid',
    });
    expect(invalid.width, 1);
    expect(invalid.height, 4096);
    expect(invalid.fps, 0);
    expect(invalid.autoMatch, isFalse);
    expect(invalid.flag('ScreenFilter', true), isTrue);
  });
}
