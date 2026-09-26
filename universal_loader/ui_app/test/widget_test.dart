import 'package:flutter_test/flutter_test.dart';
import 'package:ui_app/main.dart';

void main() {
  testWidgets('UniversalJ2meApp smoke test', (WidgetTester tester) async {
    await tester.pumpWidget(const UniversalJ2meApp());
    expect(find.text('Universal J2ME Loader'), findsOneWidget);
  });
}
