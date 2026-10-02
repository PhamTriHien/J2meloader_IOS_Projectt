import 'dart:ffi' as ffi;
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:ui_app/bridge/j2me_ffi.dart';
import 'package:ui_app/sessions/game_session.dart';

class FakeBindings implements J2meBindings {
  final calls = <String>[];
  int nextAddress = 10;
  bool failSpawn = false;
  final states = <int, int>{};
  @override
  J2meGetFpsDart get coreGetState => (p) => states[p.address] ?? 1;

  @override
  J2meAppSpawnDart get appSpawn => (_, app, slot) {
    calls.add('spawn:$app:$slot');
    return failSpawn ? ffi.nullptr : ffi.Pointer.fromAddress(nextAddress++);
  };
  @override
  J2meActionDart get coreStart => (p) => calls.add('start:${p.address}');
  @override
  J2meActionDart get coreStop => (p) => calls.add('stop:${p.address}');
  @override
  J2meDestroyDart get coreDestroy => (p) => calls.add('destroy:${p.address}');
  @override
  J2meActionDart get corePause => (p) {
    calls.add('pause:${p.address}');
    states[p.address] = 3;
  };
  @override
  J2meActionDart get coreResume => (p) {
    calls.add('resume:${p.address}');
    states[p.address] = 1;
  };
  @override
  void Function(ffi.Pointer<ffi.Void>, bool) get coreSetBackground =>
      (p, background) => calls.add('background:${p.address}:$background');
  @override
  J2meGetSpeedMultDart get coreGetSpeedMultiplier => (_) => 4;
  @override
  J2meSetSpeedMultDart get coreSetSpeedMultiplier => (p, speed) => calls.add('speed:${p.address}:$speed');
  @override
  J2meGetFpsDart get coreGetFpsLimit => (_) => 45;
  @override
  J2meSetFpsDart get coreSetFpsLimit => (p, fps) => calls.add('fps:${p.address}:$fps');
  @override
  J2meSetDimsDart get coreSetScreenDimensions => (p, w, h) => calls.add('size:${p.address}:$w:$h');
  @override
  dynamic noSuchMethod(Invocation invocation) => throw UnsupportedError('${invocation.memberName}');
}

void main() {
  late FakeBindings bindings;
  late GameSessionManager manager;
  late List<int> serviceCounts;
  GameSession launch({int app = 1, int slot = 0}) => manager.launchOrActivate(
      appId: app, appPath: 'game.jar', title: 'Game', cloneSlot: slot)!;

  setUp(() {
    bindings = FakeBindings();
    serviceCounts = [];
    manager = GameSessionManager.testing(bindings: bindings, isIOS: true,
        isMobile: true, backgroundUpdater: (count, _) => serviceCounts.add(count));
    manager.setLibraryEngine(ffi.Pointer.fromAddress(1));
  });
  tearDown(() async {
    manager.stopAll();
    await Future<void>.delayed(Duration.zero);
    manager.dispose();
  });

  test('Restart preserves FPS, dimensions and speed and pauses before starting', () {
    final original = launch();
    manager.setPaused(original, true);
    bindings.calls.clear();
    final restarted = manager.restartSession(original, screenWidth: 176, screenHeight: 220)!;
    expect(restarted.isPaused, isTrue);
    expect(bindings.calls, containsAll(['fps:11:45', 'speed:11:4', 'size:11:176:220']));
    expect(bindings.calls.indexOf('pause:11'), lessThan(bindings.calls.indexOf('start:11')));
    expect(bindings.calls.indexOf('destroy:10'), lessThan(bindings.calls.indexOf('start:11')));
    manager.setPaused(original, false);
    expect(restarted.isPaused, isFalse);
  });

  test('iOS resumes automatic pauses across restart but retains manual pause', () {
    final running = launch();
    final manual = launch(slot: 1);
    manager.setPaused(manual, true);
    manager.didChangeAppLifecycleState(AppLifecycleState.hidden);
    final restarted = manager.restartSession(running)!;
    manager.didChangeAppLifecycleState(AppLifecycleState.paused);
    manager.didChangeAppLifecycleState(AppLifecycleState.resumed);
    expect(restarted.isPaused, isFalse);
    expect(manual.isPaused, isTrue);
  });

  test('A new iOS session stays paused while suspended and resumes on return', () {
    manager.didChangeAppLifecycleState(AppLifecycleState.paused);
    final session = launch();
    expect(session.isPaused, isTrue);
    expect(bindings.calls.indexOf('pause:10'), lessThan(bindings.calls.indexOf('start:10')));
    manager.setPaused(session, false);
    expect(session.isPaused, isTrue);
    manager.didChangeAppLifecycleState(AppLifecycleState.resumed);
    expect(session.isPaused, isFalse);
  });

  test('A closing slot cannot be reopened by a synchronous listener', () async {
    final original = launch(slot: 1);
    GameSession? replacement;
    void listener() {
      replacement = manager.launchOrActivate(appId: 1, appPath: 'game.jar', title: 'Game', cloneSlot: 1);
    }
    manager.addListener(listener);
    manager.stopSession(original);
    manager.removeListener(listener);
    expect(replacement, isNull);
    expect(manager.getNextAvailableSlot(1), 2);
    await Future<void>.delayed(Duration.zero);
    replacement = launch(slot: 1);
    expect(replacement, isNotNull);
    expect(bindings.calls.indexOf('destroy:10'), lessThan(bindings.calls.lastIndexOf('spawn:1:1')));
  });

  test('Memory pressure blocks allocation until all native cleanup finishes', () async {
    final session = launch();
    manager.didHaveMemoryPressure();
    expect(manager.restartSession(session), isNull);
    manager.stopAll();
    expect(manager.launchOrActivate(appId: 2, appPath: 'game.jar', title: 'Game'), isNull);
    await Future<void>.delayed(Duration.zero);
    expect(launch(app: 2), isNotNull);
  });

  test('Failed restart leaves the original session running', () {
    final original = launch();
    bindings.failSpawn = true;
    bindings.calls.clear();
    expect(manager.restartSession(original), isNull);
    expect(manager.activeSession, same(original));
    expect(bindings.calls, isNot(contains('destroy:10')));
    expect(bindings.calls, isNot(contains('stop:10')));
  });

  test('Paused sessions are excluded from Android-style foreground counts', () {
    final first = launch();
    final second = launch(slot: 1);
    manager.setPaused(first, true);
    expect(serviceCounts.last, 1);
    manager.setPaused(second, true);
    expect(serviceCounts.last, 0);
    manager.setPaused(first, false);
    expect(serviceCounts.last, 1);
  });

  test('A naturally ended game is removed and the background service stops', () async {
    final session = launch();
    bindings.states[session.engineInstance.address] = 0;
    manager.refreshSessionStates();
    expect(manager.sessions, isEmpty);
    expect(serviceCounts.last, 0);
    await Future<void>.delayed(Duration.zero);
    expect(bindings.calls, contains('destroy:10'));
  });

  test('A pause requested by the MIDlet updates service state', () {
    final session = launch();
    bindings.states[session.engineInstance.address] = 3;
    manager.refreshSessionStates();
    expect(session.isPaused, isTrue);
    expect(serviceCounts.last, 0);
  });

  test('iOS does not auto-resume a MIDlet pause that arrived before polling', () {
    final session = launch();
    bindings.states[session.engineInstance.address] = 3;
    manager.didChangeAppLifecycleState(AppLifecycleState.hidden);
    manager.didChangeAppLifecycleState(AppLifecycleState.resumed);
    expect(session.isPaused, isTrue);
    expect(bindings.calls, isNot(contains('resume:10')));
  });
}
