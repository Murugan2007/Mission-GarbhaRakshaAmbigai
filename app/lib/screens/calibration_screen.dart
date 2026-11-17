import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/app_state.dart';

class CalibrationScreen extends StatefulWidget {
  const CalibrationScreen({super.key, required this.state});
  final AppState state;

  @override
  State<CalibrationScreen> createState() => _CalibrationScreenState();
}

class _CalibrationScreenState extends State<CalibrationScreen> {
  final _sys = TextEditingController();
  final _dia = TextEditingController();

  @override
  void dispose() {
    _sys.dispose();
    _dia.dispose();
    super.dispose();
  }

  Future<void> _add() async {
    final sys = int.tryParse(_sys.text);
    final dia = int.tryParse(_dia.text);
    if (sys == null || dia == null || sys < 60 || sys > 260 || dia < 30 || dia > 160) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Enter a valid reading, e.g. 118 and 76')));
      return;
    }
    await widget.state.addCuff(sys, dia);
    _sys.clear();
    _dia.clear();
  }

  @override
  Widget build(BuildContext context) {
    final s = widget.state;
    final m = s.calibration;
    return ListView(padding: const EdgeInsets.all(16), children: [
      const Text(
        'Sit still for five minutes, take a reading on an upper-arm cuff, then enter it here '
        'straight away. Repeat across several days and times: at least 6 paired readings are needed.',
      ),
      const SizedBox(height: 12),
      Row(children: [
        Expanded(
          child: TextField(
            controller: _sys,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(labelText: 'Systolic', border: OutlineInputBorder()),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: TextField(
            controller: _dia,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(labelText: 'Diastolic', border: OutlineInputBorder()),
          ),
        ),
        const SizedBox(width: 12),
        FilledButton(onPressed: _add, child: const Text('Add')),
      ]),
      const SizedBox(height: 16),
      Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: m == null
              ? Text('Paired readings so far: ${s.cuffs.where((c) => c.patMs != null).length} of 6')
              : Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                  Text('SBP = ${m.a.toStringAsFixed(1)} + ${m.b.toStringAsFixed(0)} / PAT',
                      style: const TextStyle(fontWeight: FontWeight.w600)),
                  Text('${m.n} readings, r = ${m.r.toStringAsFixed(2)}, '
                      'fit error ${m.fitSd.toStringAsFixed(1)} mmHg'),
                  const SizedBox(height: 6),
                  const Text('A weak correlation means the timing is not yet tracking pressure for you. '
                      'Refit regularly.', style: TextStyle(fontSize: 12)),
                ]),
        ),
      ),
      Card(
        child: ListTile(
          title: const Text('Personal baseline'),
          subtitle: Text(
              'Original: ${s.originalBaselineSbp?.toStringAsFixed(0) ?? '-'} mmHg (kept on file)\n'
              'Active: ${s.activeBaselineSbp?.toStringAsFixed(0) ?? 'forming'} mmHg'),
          trailing: TextButton(
              onPressed: s.resetActiveBaseline, child: const Text('Start new active baseline')),
        ),
      ),
      OutlinedButton.icon(
        icon: const Icon(Icons.copy),
        label: const Text('Copy all data as CSV'),
        onPressed: () async {
          await Clipboard.setData(ClipboardData(text: s.exportCsv()));
          if (context.mounted) {
            ScaffoldMessenger.of(context)
                .showSnackBar(const SnackBar(content: Text('CSV copied to clipboard')));
          }
        },
      ),
    ]);
  }
}
