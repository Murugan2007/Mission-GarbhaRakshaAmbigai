import 'dart:async';

import 'package:flutter/foundation.dart';

import '../models/models.dart';
import 'ble_service.dart';
import 'calibration.dart';
import 'demo_source.dart';
import 'risk_engine.dart';
import 'store.dart';

/// Holds everything the screens show. No cloud: data stays on the phone.
class AppState extends ChangeNotifier {
  AppState(this._store);

  final Store _store;
  final BleService _ble = BleService();
  StreamSubscription<BeatSummary>? _sub;

  final List<BeatSummary> summaries = [];
  final List<CuffReading> cuffs = [];
  CalibrationModel? calibration;

  /// The first accepted reference is kept forever; the active one can be reset
  /// on purpose, and the original stays on file.
  double? originalBaselineSbp;
  double? activeBaselineSbp;

  bool symptoms = false;
  ProteinResult? protein;
  String source = 'Not connected';
  String? error;
  RiskResult risk = RiskEngine.evaluate();

  BeatSummary? get latest => summaries.isEmpty ? null : summaries.last;

  Future<void> init() async {
    await _store.open();
    summaries
      ..clear()
      ..addAll(await _store.summaries());
    cuffs
      ..clear()
      ..addAll(await _store.cuffs());
    originalBaselineSbp = double.tryParse(await _store.setting('baseline_original') ?? '');
    activeBaselineSbp = double.tryParse(await _store.setting('baseline_active') ?? '');
    _refit();
    _reevaluate();
    notifyListeners();
  }

  Future<void> startDemo() async {
    await _stop();
    source = 'Demo data (simulated)';
    _sub = DemoSource().stream().listen(_onSummary);
    notifyListeners();
  }

  Future<void> connectDevice() async {
    await _stop();
    try {
      error = null;
      source = 'Connecting...';
      notifyListeners();
      await _ble.connect();
      source = 'Wearable (BLE)';
      _sub = _ble.summaries.listen(_onSummary);
    } catch (e) {
      error = 'Could not connect: $e';
      source = 'Not connected';
    }
    notifyListeners();
  }

  Future<void> _stop() async {
    await _sub?.cancel();
    _sub = null;
    await _ble.disconnect();
  }

  Future<void> _onSummary(BeatSummary s) async {
    summaries.add(s);
    if (summaries.length > 5000) summaries.removeAt(0);
    await _store.addSummary(s);
    await _maybeSetBaseline();
    _reevaluate();
    notifyListeners();
  }

  Future<void> addCuff(int sys, int dia) async {
    final c = CuffReading(
      time: DateTime.now(),
      sys: sys,
      dia: dia,
      patMs: (latest != null && latest!.usable) ? latest!.medianPatMs : null,
    );
    cuffs.add(c);
    await _store.addCuff(c);
    _refit();
    await _maybeSetBaseline();
    _reevaluate();
    notifyListeners();
  }

  void setSymptoms(bool v) {
    symptoms = v;
    _reevaluate();
    notifyListeners();
  }

  void setProtein(ProteinResult? v) {
    protein = v;
    _reevaluate();
    notifyListeners();
  }

  /// Starts a fresh active reference. The original reference is never erased.
  Future<void> resetActiveBaseline() async {
    activeBaselineSbp = null;
    await _store.putSetting('baseline_active', '');
    await _maybeSetBaseline();
    _reevaluate();
    notifyListeners();
  }

  void _refit() {
    final pairs = cuffs.where((c) => c.patMs != null).toList();
    calibration = CalibrationModel.fit(
      [for (final c in pairs) c.patMs!],
      [for (final c in pairs) c.sys.toDouble()],
    );
  }

  double? get currentSbp {
    final m = calibration;
    final l = latest;
    if (m == null || l == null || !l.usable) return null;
    return m.predict(l.medianPatMs);
  }

  Future<void> _maybeSetBaseline() async {
    final m = calibration;
    if (m == null || activeBaselineSbp != null) return;
    final usable = summaries.where((s) => s.usable).toList();
    if (usable.length < 30) return;
    final first = usable.first.time;
    final week = usable
        .where((s) => s.time.difference(first) <= const Duration(days: 7))
        .map((s) => m.predict(s.medianPatMs))
        .toList();
    final mean = week.reduce((a, b) => a + b) / week.length;
    activeBaselineSbp = mean;
    await _store.putSetting('baseline_active', mean.toString());
    if (originalBaselineSbp == null) {
      originalBaselineSbp = mean;
      await _store.putSetting('baseline_original', mean.toString());
    }
  }

  void _reevaluate() {
    final now = DateTime.now();
    final recent = cuffs
        .where((c) => now.difference(c.time) <= const Duration(days: 7))
        .toList();
    final sbp = currentSbp;
    final base = activeBaselineSbp;
    risk = RiskEngine.evaluate(
      riseMmHg: (sbp != null && base != null) ? sbp - base : null,
      protein: protein,
      cuffs: recent,
      symptoms: symptoms,
    );
  }

  /// CSV of everything stored, for a clinician or for analysis.
  String exportCsv() {
    final b = StringBuffer('type,time_iso,value1,value2,value3,value4\n');
    for (final s in summaries) {
      b.writeln('summary,${s.time.toIso8601String()},${s.medianPatMs},${s.goodBeats},${s.rrMs},${s.flags}');
    }
    for (final c in cuffs) {
      b.writeln('cuff,${c.time.toIso8601String()},${c.sys},${c.dia},${c.patMs ?? ''},');
    }
    return b.toString();
  }

  @override
  void dispose() {
    _stop();
    super.dispose();
  }
}
