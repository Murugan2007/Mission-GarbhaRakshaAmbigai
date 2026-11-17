import 'package:flutter/material.dart';

import '../models/models.dart';
import '../services/app_state.dart';
import '../services/risk_engine.dart';

class TodayScreen extends StatelessWidget {
  const TodayScreen({super.key, required this.state});
  final AppState state;

  static Color colorFor(RiskLevel l) {
    switch (l) {
      case RiskLevel.low:
        return Colors.green.shade600;
      case RiskLevel.medium:
        return Colors.amber.shade700;
      case RiskLevel.high:
        return Colors.deepOrange.shade600;
      case RiskLevel.urgent:
        return Colors.red.shade700;
    }
  }

  static String titleFor(RiskLevel l) {
    switch (l) {
      case RiskLevel.low:
        return 'LOW';
      case RiskLevel.medium:
        return 'MEDIUM';
      case RiskLevel.high:
        return 'HIGH';
      case RiskLevel.urgent:
        return 'URGENT';
    }
  }

  @override
  Widget build(BuildContext context) {
    final r = state.risk;
    final l = state.latest;
    final c = colorFor(r.level);
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Card(
          color: c,
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(titleFor(r.level),
                    style: const TextStyle(
                        color: Colors.white, fontSize: 14, letterSpacing: 2)),
                const SizedBox(height: 8),
                Text(r.message,
                    style: const TextStyle(
                        color: Colors.white, fontSize: 24, fontWeight: FontWeight.w600)),
              ],
            ),
          ),
        ),
        const SizedBox(height: 12),
        Card(
          child: ListTile(
            title: const Text('Data source'),
            subtitle: Text(state.source),
            trailing: Wrap(spacing: 8, children: [
              OutlinedButton(onPressed: state.startDemo, child: const Text('Demo')),
              FilledButton(onPressed: state.connectDevice, child: const Text('Connect')),
            ]),
          ),
        ),
        if (state.error != null)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 8),
            child: Text(state.error!, style: TextStyle(color: Colors.red.shade700)),
          ),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                _row('Median pulse arrival time',
                    l == null ? '-' : '${l.medianPatMs.toStringAsFixed(1)} ms'),
                _row('Good beats in median', l == null ? '-' : '${l.goodBeats}'),
                _row('Estimated systolic (calibrated)',
                    state.currentSbp == null ? 'Calibrate first' : '${state.currentSbp!.toStringAsFixed(0)} mmHg'),
                _row('Rise over your baseline',
                    r.rise == null ? '-' : '${r.rise!.toStringAsFixed(1)} mmHg'),
                if (l != null && l.flags != 0)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Text(
                      [
                        if (l.leadsOff) 'Chest electrodes off',
                        if (l.noFinger) 'Finger not on sensor',
                        if (l.motion) 'Too much movement',
                      ].join(' | '),
                      style: TextStyle(color: Colors.orange.shade800),
                    ),
                  ),
              ],
            ),
          ),
        ),
        Card(
          child: Column(children: [
            SwitchListTile(
              title: const Text('I have warning symptoms'),
              subtitle: const Text(
                  'Severe headache, blurred vision, upper right belly pain, sudden swelling, shortness of breath'),
              value: state.symptoms,
              onChanged: state.setSymptoms,
            ),
            ListTile(
              title: const Text('Urine protein strip (optional)'),
              trailing: DropdownButton<ProteinResult?>(
                value: state.protein,
                hint: const Text('Not tested'),
                items: [
                  for (final p in ProteinResult.values)
                    DropdownMenuItem(value: p, child: Text(p.label)),
                ],
                onChanged: state.setProtein,
              ),
            ),
          ]),
        ),
        const Padding(
          padding: EdgeInsets.all(8),
          child: Text(
            'A screening aid, not a medical device. It prompts a check; it does not diagnose. '
            'If you feel unwell, contact your clinic.',
            style: TextStyle(fontSize: 12, color: Colors.black54),
          ),
        ),
      ],
    );
  }

  Widget _row(String k, String v) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 4),
        child: Row(mainAxisAlignment: MainAxisAlignment.spaceBetween, children: [
          Flexible(child: Text(k)),
          Text(v, style: const TextStyle(fontWeight: FontWeight.w600)),
        ]),
      );
}
