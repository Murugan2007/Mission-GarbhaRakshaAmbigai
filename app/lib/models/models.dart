import 'dart:typed_data';

/// One summary packet from the wearable (see docs/BLE_PROTOCOL.md).
class BeatSummary {
  const BeatSummary({
    required this.time,
    required this.medianPatMs,
    required this.goodBeats,
    required this.rrMs,
    this.flags = 0,
  });

  final DateTime time;

  /// Rolling median pulse arrival time over the last good beats, in ms.
  final double medianPatMs;
  final int goodBeats;
  final int rrMs;

  /// bit0 leads off, bit1 no finger, bit2 motion.
  final int flags;

  bool get leadsOff => flags & 1 != 0;
  bool get noFinger => flags & 2 != 0;
  bool get motion => flags & 4 != 0;
  bool get usable => goodBeats >= 8 && flags == 0 && medianPatMs > 0;

  /// Parses the 10-byte little-endian v1 packet:
  /// u32 device_ms | u16 median_pat_x10 | u8 good_beats | u8 flags | u16 rr_ms
  static BeatSummary? parse(List<int> bytes, {DateTime? now}) {
    if (bytes.length < 10) return null;
    final bd = ByteData.sublistView(Uint8List.fromList(bytes));
    final patTenths = bd.getUint16(4, Endian.little);
    return BeatSummary(
      time: now ?? DateTime.now(),
      medianPatMs: patTenths / 10.0,
      goodBeats: bd.getUint8(6),
      flags: bd.getUint8(7),
      rrMs: bd.getUint16(8, Endian.little),
    );
  }
}

/// A manual upper-arm cuff reading, stored next to the device value at that moment.
class CuffReading {
  const CuffReading({
    required this.time,
    required this.sys,
    required this.dia,
    this.patMs,
  });

  final DateTime time;
  final int sys;
  final int dia;
  final double? patMs;

  bool get elevated => sys >= 140 || dia >= 90;
  bool get severe => sys >= 160 || dia >= 110;
}

enum ProteinResult { negative, trace, onePlus, twoPlus, threePlus }

extension ProteinResultX on ProteinResult {
  bool get positive => index >= ProteinResult.onePlus.index;
  String get label => const ['Negative', 'Trace', '1+', '2+', '3+'][index];
}
