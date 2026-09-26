import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

class VirtualKeypad extends StatelessWidget {
  final Function(int keyCode, bool isPressed) onKeyEvent;

  const VirtualKeypad({super.key, required this.onKeyEvent});

  Widget _buildKey(String label, int code, {Color? bg, Color? fg, double flex = 1.0}) {
    return Expanded(
      flex: (flex * 10).toInt(),
      child: Padding(
        padding: const EdgeInsets.all(2.5),
        child: Listener(
          onPointerDown: (_) {
            HapticFeedback.lightImpact();
            onKeyEvent(code, true);
          },
          onPointerUp: (_) => onKeyEvent(code, false),
          onPointerCancel: (_) => onKeyEvent(code, false),
          child: Container(
            height: 48,
            decoration: BoxDecoration(
              color: bg ?? const Color(0xFF1E293B),
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: const Color(0xFF334155), width: 1.2),
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withAlpha(76),
                  offset: const Offset(0, 2),
                  blurRadius: 2,
                )
              ],
            ),
            alignment: Alignment.center,
            child: Text(
              label,
              style: TextStyle(
                color: fg ?? const Color(0xFFE2E8F0),
                fontSize: 16,
                fontWeight: FontWeight.bold,
                letterSpacing: 0.5,
              ),
            ),
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      decoration: BoxDecoration(
        color: const Color(0xFF0F172A).withAlpha(242),
        borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
        border: const Border(top: BorderSide(color: Color(0xFF334155), width: 1.5)),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          // Softkeys & D-Pad Top Row
          Row(
            children: [
              _buildKey("LSK", -6, bg: const Color(0xFF334155), fg: const Color(0xFF38BDF8)),
              const Spacer(),
              _buildKey("▲", -1, bg: const Color(0xFF1E293B), fg: const Color(0xFF38BDF8)),
              const Spacer(),
              _buildKey("RSK", -7, bg: const Color(0xFF334155), fg: const Color(0xFF38BDF8)),
            ],
          ),
          // D-Pad Middle Row
          Row(
            children: [
              const Spacer(),
              _buildKey("◀", -3, bg: const Color(0xFF1E293B), fg: const Color(0xFF38BDF8)),
              _buildKey("OK", -5, bg: const Color(0xFF0284C7), fg: Colors.white, flex: 1.2),
              _buildKey("▶", -4, bg: const Color(0xFF1E293B), fg: const Color(0xFF38BDF8)),
              const Spacer(),
            ],
          ),
          // D-Pad Bottom Row
          Row(
            children: [
              const Spacer(),
              _buildKey("▼", -2, bg: const Color(0xFF1E293B), fg: const Color(0xFF38BDF8)),
              const Spacer(),
            ],
          ),
          const SizedBox(height: 6),
          // Numpad Row 1: 1, 2, 3
          Row(
            children: [
              _buildKey("1", 49),
              _buildKey("2", 50),
              _buildKey("3", 51),
            ],
          ),
          // Numpad Row 2: 4, 5, 6
          Row(
            children: [
              _buildKey("4", 52),
              _buildKey("5", 53),
              _buildKey("6", 54),
            ],
          ),
          // Numpad Row 3: 7, 8, 9
          Row(
            children: [
              _buildKey("7", 55),
              _buildKey("8", 56),
              _buildKey("9", 57),
            ],
          ),
          // Numpad Row 4: *, 0, #
          Row(
            children: [
              _buildKey("*", 42, fg: const Color(0xFFF59E0B)),
              _buildKey("0", 48),
              _buildKey("#", 35, fg: const Color(0xFFF59E0B)),
            ],
          ),
        ],
      ),
    );
  }
}
