import 'package:flutter/material.dart';

class AboutScreen extends StatelessWidget {
  const AboutScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return ListView(padding: const EdgeInsets.all(16), children: const [
      Text('Mission GarbhaRaksha', style: TextStyle(fontSize: 20, fontWeight: FontWeight.w700)),
      SizedBox(height: 8),
      Text(
        'Pre-eclampsia can develop between check-ups. This wearable and app watch for a worsening '
        'trend against your own normal and prompt you to check sooner. They do not diagnose.',
      ),
      SizedBox(height: 16),
      Text('How it works', style: TextStyle(fontWeight: FontWeight.w600)),
      Text(
        '1. The wearable times each heartbeat\'s pulse and keeps a rolling median.\n'
        '2. You calibrate once against an upper-arm cuff.\n'
        '3. The app compares today with your own baseline and applies readable rules.\n'
        '4. Low: keep wearing. Medium: re-check with your cuff today. High: call your clinic today. '
        'Urgent: go to hospital now.',
      ),
      SizedBox(height: 16),
      Text('Your data', style: TextStyle(fontWeight: FontWeight.w600)),
      Text('Everything is stored on this phone. Nothing is uploaded.'),
      SizedBox(height: 16),
      Text('Warning signs: get care at once', style: TextStyle(fontWeight: FontWeight.w600)),
      Text(
        'Severe headache that will not ease, blurred vision or flashing lights, pain in the upper '
        'right belly, sudden swelling of the face or hands, or shortness of breath.',
      ),
      SizedBox(height: 16),
      Text(
        'Research prototype. Not a medical device. Not a substitute for professional care.',
        style: TextStyle(fontSize: 12, color: Colors.black54),
      ),
    ]);
  }
}
