# GarbhaRaksha companion app (Flutter)

MVP companion app for the wearable.

- **Today:** colour-coded risk state with a plain-language message, median PAT, estimated systolic, rise over baseline, symptoms switch, optional urine strip result.
- **History:** PAT trend chart and cuff readings.
- **Calibrate:** enter cuff readings (paired with the device's current PAT), see the fitted line `SBP = a + b / PAT`, personal baseline (original kept on file, active resettable), copy all data as CSV.
- **BLE:** `flutter_blue_plus` client for the summary service (`docs/BLE_PROTOCOL.md`). **Demo** mode generates simulated summaries so the app can be explored without a device.
- **Rules:** `lib/services/risk_engine.dart` implements the seven fuzzy rules from `research/04_risk_rules.md`.
- **Privacy:** SQLite on the phone only; nothing is uploaded.

## Run

```bash
cd app
flutter create .          # generates android/ and ios/ platform folders once
flutter pub get
flutter test
flutter run
```

## Platform permissions (after `flutter create .`)

Android (`android/app/src/main/AndroidManifest.xml`):

```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
```

iOS (`ios/Runner/Info.plist`): add `NSBluetoothAlwaysUsageDescription` explaining that Bluetooth is used to read summaries from the wearable.

## Layout

```
lib/
  main.dart
  models/models.dart            BeatSummary (packet parser), CuffReading, ProteinResult
  services/
    risk_engine.dart            fuzzy rules
    calibration.dart            least-squares SBP = a + b / PAT
    ble_service.dart            BLE client
    demo_source.dart            simulated wearable
    store.dart                  SQLite storage
    app_state.dart              app state and logic
  screens/                      today, history, calibration, about
test/risk_engine_test.dart      rules, calibration, packet parser
```

Research prototype and screening aid. Not a medical device.
