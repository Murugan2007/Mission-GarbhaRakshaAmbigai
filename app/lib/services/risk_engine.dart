import '../models/models.dart';

enum RiskLevel { low, medium, high, urgent }

class RiskResult {
  const RiskResult({
    required this.level,
    required this.message,
    required this.rule,
    this.rise,
  });

  final RiskLevel level;
  final String message;

  /// Which rule fired (1-7), or 0 while the baseline is still being collected.
  final int rule;

  /// Estimated systolic rise over the personal baseline, in mmHg.
  final double? rise;
}

/// Readable fuzzy rules. The trend bands are engineering placeholders to be
/// tuned with data; the 140/90 and 160/110 cuff levels follow the alert levels
/// used in the 2024 pre-eclampsia watch paper (Munyao et al.).
///
///  1 stable, protein negative      -> Low
///  2 rising, protein negative/trace -> Medium
///  3 stable, protein 1+ or more     -> Medium
///  4 rising, protein 1+ or more     -> High
///  5 rising fast                    -> High
///  6 cuff >=140/90 twice, 4 h apart -> High
///  7 cuff >=160/110 or symptoms     -> Urgent
class RiskEngine {
  static const double stableMax = 5; // mmHg rise counted as fully "stable"
  static const double risingFull = 15; // mmHg rise counted as fully "rising"
  static const double fastFrom = 25; // placeholder for "rising fast"

  static double stableMembership(double rise) {
    if (rise <= stableMax) return 1;
    if (rise >= risingFull) return 0;
    return (risingFull - rise) / (risingFull - stableMax);
  }

  static double risingMembership(double rise) {
    if (rise <= stableMax) return 0;
    if (rise >= risingFull) return 1;
    return (rise - stableMax) / (risingFull - stableMax);
  }

  static bool twoElevatedFourHoursApart(List<CuffReading> cuffs) {
    final e = cuffs.where((c) => c.elevated).toList()
      ..sort((a, b) => a.time.compareTo(b.time));
    for (var i = 0; i < e.length; i++) {
      for (var j = i + 1; j < e.length; j++) {
        if (e[j].time.difference(e[i].time) >= const Duration(hours: 4)) return true;
      }
    }
    return false;
  }

  static RiskResult evaluate({
    double? riseMmHg,
    ProteinResult? protein,
    List<CuffReading> cuffs = const [],
    bool symptoms = false,
  }) {
    if (symptoms || cuffs.any((c) => c.severe)) {
      return RiskResult(
        level: RiskLevel.urgent,
        message: 'Go to hospital now',
        rule: 7,
        rise: riseMmHg,
      );
    }
    if (twoElevatedFourHoursApart(cuffs)) {
      return RiskResult(
        level: RiskLevel.high,
        message: 'Call your clinic today',
        rule: 6,
        rise: riseMmHg,
      );
    }
    if (riseMmHg == null) {
      return const RiskResult(
        level: RiskLevel.low,
        message: 'Building your personal baseline. Keep wearing',
        rule: 0,
      );
    }
    if (riseMmHg >= fastFrom) {
      return RiskResult(
        level: RiskLevel.high,
        message: 'Call your clinic today',
        rule: 5,
        rise: riseMmHg,
      );
    }
    final rising = risingMembership(riseMmHg) >= 0.5;
    final proteinPositive = protein?.positive ?? false;
    if (proteinPositive && rising) {
      return RiskResult(
        level: RiskLevel.high,
        message: 'Call your clinic today',
        rule: 4,
        rise: riseMmHg,
      );
    }
    if (proteinPositive) {
      return RiskResult(
        level: RiskLevel.medium,
        message: 'Repeat the strip, then tell your clinic',
        rule: 3,
        rise: riseMmHg,
      );
    }
    if (rising) {
      return RiskResult(
        level: RiskLevel.medium,
        message: 'Re-check with your cuff today',
        rule: 2,
        rise: riseMmHg,
      );
    }
    return RiskResult(
      level: RiskLevel.low,
      message: 'Nothing to do. Keep wearing',
      rule: 1,
      rise: riseMmHg,
    );
  }
}
