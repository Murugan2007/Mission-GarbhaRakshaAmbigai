import 'dart:async';
import 'dart:math';

import '../models/models.dart';

/// Simulated wearable so the app can be explored without a device.
/// The values are generated, not measured.
class DemoSource {
  Stream<BeatSummary> stream({Duration every = const Duration(seconds: 2)}) {
    final rnd = Random(7);
    var tick = 0;
    return Stream.periodic(every, (_) {
      tick++;
      final drift = -0.05 * tick; // slow shortening of the pulse arrival time
      final wobble = 2.0 * sin(tick / 6.0) + rnd.nextDouble() - 0.5;
      return BeatSummary(
        time: DateTime.now(),
        medianPatMs: 225 + drift + wobble,
        goodBeats: 15,
        rrMs: 820 + rnd.nextInt(40),
      );
    });
  }
}
