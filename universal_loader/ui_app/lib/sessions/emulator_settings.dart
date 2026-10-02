enum GameScaleMode { fit, fitWidth, stretch, original1x, scale2x, scale3x }

class EmulatorSettings {
  final Map<String, dynamic> values;
  EmulatorSettings(this.values);

  int number(String key, int fallback) {
    final value = values[key];
    return value is num && value.isFinite ? value.toInt() : fallback;
  }
  bool flag(String key, bool fallback) {
    final value = values[key];
    return value is bool ? value : fallback;
  }
  int get width => number('ScreenWidth', 240).clamp(1, 4096);
  int get height => number('ScreenHeight', 320).clamp(1, 4096);
  int get fps => number('FpsLimit', 60).clamp(0, 240);
  int get orientation => number('Orientation', 0).clamp(0, 3);
  bool get autoMatch => flag('ScreenAutoMatch', true);

  GameScaleMode get scaleMode {
    final name = values['UiScaleMode'];
    for (final mode in GameScaleMode.values) {
      if (mode.name == name) return mode;
    }
    if (!flag('ScreenScaleToFit', true) || number('ScreenScaleType', 1) == 0) {
      return GameScaleMode.original1x;
    }
    if (!flag('ScreenKeepAspectRatio', true) || number('ScreenScaleType', 1) == 2) {
      return GameScaleMode.stretch;
    }
    return GameScaleMode.fit;
  }

  static Map<String, dynamic> scaleValues(GameScaleMode mode) => {
    'UiScaleMode': mode.name,
    'ScreenScaleToFit': mode == GameScaleMode.fit ||
        mode == GameScaleMode.fitWidth || mode == GameScaleMode.stretch,
    'ScreenKeepAspectRatio': mode != GameScaleMode.stretch,
    'ScreenScaleType': switch (mode) {
      GameScaleMode.stretch => 2,
      GameScaleMode.fit || GameScaleMode.fitWidth => 1,
      _ => 0,
    },
  };
}
