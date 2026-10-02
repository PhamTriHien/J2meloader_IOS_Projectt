import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:ui_app/views/virtual_keypad.dart';

void main() {
  testWidgets('Held keys highlight independently and release on cancellation',
      (tester) async {
    final events = <(int, bool)>[];
    await tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: VirtualKeypad(
          enableHaptic: false,
          onKeyEvent: (code, pressed) => events.add((code, pressed)),
        ),
      ),
    ));

    Color? keyColor(String label) {
      final container = tester.widget<Container>(find.ancestor(
        of: find.text(label), matching: find.byType(Container)).first);
      return (container.decoration as BoxDecoration).color;
    }

    final idleColor = keyColor('1');
    final first = await tester.startGesture(tester.getCenter(find.text('1')),
        pointer: 1);
    final second = await tester.startGesture(tester.getCenter(find.text('2')),
        pointer: 2);
    await tester.pump();
    expect(keyColor('1'), const Color(0xFFFBBF24));
    expect(keyColor('2'), const Color(0xFFFBBF24));
    expect(keyColor('3'), idleColor);

    await first.cancel();
    await tester.pump();
    expect(keyColor('1'), idleColor);
    expect(keyColor('2'), const Color(0xFFFBBF24));
    await second.up();
    await tester.pump();
    expect(keyColor('2'), idleColor);
    expect(events, [(49, true), (50, true), (49, false), (50, false)]);
  });
}
