import 'package:flutter_test/flutter_test.dart';
import 'package:garbha_raksha_app/models/models.dart';
import 'package:garbha_raksha_app/services/calibration.dart';
import 'package:garbha_raksha_app/services/risk_engine.dart';

void main() {
  group('RiskEngine', () {
    test('builds baseline when there is no trend yet', () {
      final r = RiskEngine.evaluate();
      expect(r.level, RiskLevel.low);
      expect(r.rule, 0);
    });

    test('stable and no protein is low', () {
      expect(RiskEngine.evaluate(riseMmHg: 2).rule, 1);
    });

    test('rising is medium, rising with protein is high', () {
      expect(RiskEngine.evaluate(riseMmHg: 14).level, RiskLevel.medium);
      final r = RiskEngine.evaluate(riseMmHg: 14, protein: ProteinResult.onePlus);
      expect(r.level, RiskLevel.high);
      expect(r.rule, 4);
    });

    test('stable with protein is medium (rule 3)', () {
      final r = RiskEngine.evaluate(riseMmHg: 1, protein: ProteinResult.twoPlus);
      expect(r.rule, 3);
      expect(r.level, RiskLevel.medium);
    });

    test('rising fast is high', () {
      expect(RiskEngine.evaluate(riseMmHg: 30).rule, 5);
    });

    test('two elevated cuffs 4 hours apart is high', () {
      final t = DateTime(2026, 1, 1, 8);
      final r = RiskEngine.evaluate(cuffs: [
        CuffReading(time: t, sys: 142, dia: 88),
        CuffReading(time: t.add(const Duration(hours: 4)), sys: 138, dia: 92),
      ]);
      expect(r.rule, 6);
    });

    test('two elevated cuffs closer than 4 hours do not trigger rule 6', () {
      final t = DateTime(2026, 1, 1, 8);
      final r = RiskEngine.evaluate(cuffs: [
        CuffReading(time: t, sys: 142, dia: 88),
        CuffReading(time: t.add(const Duration(hours: 2)), sys: 145, dia: 92),
      ]);
      expect(r.rule, isNot(6));
    });

    test('severe cuff or symptoms is urgent', () {
      final t = DateTime(2026, 1, 1);
      expect(RiskEngine.evaluate(cuffs: [CuffReading(time: t, sys: 165, dia: 100)]).level,
          RiskLevel.urgent);
      expect(RiskEngine.evaluate(symptoms: true).level, RiskLevel.urgent);
    });

    test('fuzzy memberships overlap between 5 and 15 mmHg', () {
      expect(RiskEngine.stableMembership(10), closeTo(0.5, 1e-9));
      expect(RiskEngine.risingMembership(10), closeTo(0.5, 1e-9));
    });
  });

  group('CalibrationModel', () {
    test('recovers a known line', () {
      final pat = [190.0, 200, 210, 220, 230, 240, 250];
      final sbp = [for (final p in pat) 50 + 15000 / p];
      final m = CalibrationModel.fit(pat, sbp)!;
      expect(m.a, closeTo(50, 0.01));
      expect(m.b, closeTo(15000, 1));
      expect(m.predict(220), closeTo(50 + 15000 / 220, 0.01));
    });

    test('needs enough points', () {
      expect(CalibrationModel.fit([200, 210], [120, 118]), isNull);
    });
  });

  test('BeatSummary parses the v1 packet', () {
    // device_ms=1000, pat=221.5 ms (2215), good=15, flags=0, rr=820
    final bytes = [0xE8, 0x03, 0, 0, 0xA7, 0x08, 15, 0, 0x34, 0x03];
    final s = BeatSummary.parse(bytes)!;
    expect(s.medianPatMs, closeTo(221.5, 1e-9));
    expect(s.goodBeats, 15);
    expect(s.rrMs, 820);
    expect(s.usable, isTrue);
  });
}
