import 'package:flutter/material.dart';

import '../services/app_state.dart';

class HistoryScreen extends StatelessWidget {
  const HistoryScreen({super.key, required this.state});
  final AppState state;

  @override
  Widget build(BuildContext context) {
    final pts = state.summaries.where((s) => s.usable).toList();
    final shown = pts.length > 200 ? pts.sublist(pts.length - 200) : pts;
    return ListView(padding: const EdgeInsets.all(16), children: [
      const Text('Pulse arrival time trend (ms)',
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
      const SizedBox(height: 8),
      SizedBox(
        height: 180,
        child: shown.length < 2
            ? const Center(child: Text('Not enough data yet'))
            : CustomPaint(
                painter: _LinePainter([for (final s in shown) s.medianPatMs],
                    Theme.of(context).colorScheme.primary),
                size: Size.infinite,
              ),
      ),
      const SizedBox(height: 16),
      Text('Cuff readings (${state.cuffs.length})',
          style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
      for (final c in state.cuffs.reversed)
        ListTile(
          dense: true,
          title: Text('${c.sys}/${c.dia} mmHg'),
          subtitle: Text(c.time.toLocal().toString().substring(0, 16)),
          trailing: Text(c.patMs == null ? '' : '${c.patMs!.toStringAsFixed(1)} ms'),
        ),
    ]);
  }
}

class _LinePainter extends CustomPainter {
  _LinePainter(this.values, this.color);
  final List<double> values;
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    var lo = values.first, hi = values.first;
    for (final v in values) {
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    if (hi - lo < 1) {
      hi += 0.5;
      lo -= 0.5;
    }
    final p = Paint()
      ..color = color
      ..strokeWidth = 2
      ..style = PaintingStyle.stroke;
    final path = Path();
    for (var i = 0; i < values.length; i++) {
      final x = size.width * i / (values.length - 1);
      final y = size.height * (1 - (values[i] - lo) / (hi - lo));
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    canvas.drawPath(path, p);
  }

  @override
  bool shouldRepaint(covariant _LinePainter old) => old.values != values;
}
