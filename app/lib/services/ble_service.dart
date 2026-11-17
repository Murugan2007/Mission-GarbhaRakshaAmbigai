import 'dart:async';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/models.dart';

/// BLE client for the wearable's summary service. See docs/BLE_PROTOCOL.md.
class BleService {
  static final Guid serviceUuid = Guid('6e1a0001-b5a3-f393-e0a9-e50e24dcca9e');
  static final Guid summaryUuid = Guid('6e1a0002-b5a3-f393-e0a9-e50e24dcca9e');

  BluetoothDevice? _device;
  StreamSubscription<List<int>>? _sub;
  final _controller = StreamController<BeatSummary>.broadcast();

  Stream<BeatSummary> get summaries => _controller.stream;

  /// Scans for the wearable, connects and starts listening for notifications.
  Future<void> connect({Duration timeout = const Duration(seconds: 10)}) async {
    final completer = Completer<BluetoothDevice>();
    final scanSub = FlutterBluePlus.scanResults.listen((results) {
      if (results.isNotEmpty && !completer.isCompleted) {
        completer.complete(results.first.device);
      }
    });
    await FlutterBluePlus.startScan(withServices: [serviceUuid], timeout: timeout);
    final device = await completer.future.timeout(timeout);
    await FlutterBluePlus.stopScan();
    await scanSub.cancel();

    await device.connect();
    _device = device;
    final services = await device.discoverServices();
    for (final s in services) {
      if (s.uuid != serviceUuid) continue;
      for (final c in s.characteristics) {
        if (c.uuid != summaryUuid) continue;
        await c.setNotifyValue(true);
        _sub = c.onValueReceived.listen((bytes) {
          final summary = BeatSummary.parse(bytes);
          if (summary != null) _controller.add(summary);
        });
      }
    }
  }

  Future<void> disconnect() async {
    await _sub?.cancel();
    await _device?.disconnect();
    _device = null;
  }
}
