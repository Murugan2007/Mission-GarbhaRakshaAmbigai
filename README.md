# Pre-eclampsia Early-Warning Wearable — Research Prototype

A low-cost embedded-systems prototype exploring whether **ECG and PPG signal trends** can be captured together and studied as pulse-arrival-time (PAT) features.

This repository is a firmware development scaffold for an ESP32-C3 SuperMini, an AD8232 ECG module and a MAX30102 optical sensor. It is intended for learning, signal-acquisition experiments and supervised research—not clinical use.

> **Important health and safety note**
>
> This project does not measure blood pressure, diagnose pre-eclampsia, predict pregnancy complications, or rule out disease. PAT is not the same as pulse transit time: it includes the pre-ejection period and is affected by several physiological and measurement factors. A validated upper-arm blood-pressure monitor and qualified clinical assessment remain essential. Do not use this prototype to make care decisions or delay medical attention.
>
> Do not test this wearable on pregnant people without appropriate clinical oversight, ethics approval and a validated study protocol. Never connect ECG electrodes to a person while the ESP32 is connected to a mains-powered computer, charger or other non-isolated equipment. Human-connected tests require an appropriately designed, battery-powered, isolated setup and competent supervision.

## What's in this repository?

```text
PreEclampsia_Wearable/
├── PreEclampsia_Wearable.ino
├── README.md
├── LICENSE
├── .gitignore
├── library.properties
├── src/
│   ├── device_config.h
│   ├── button_control.h
│   ├── max30102_sensor.h
│   ├── ppg_processing.h
│   ├── ecg_sensor.h
│   ├── ecg_processing.h
│   ├── pat_engine.h
│   ├── signal_quality.h
│   └── calibration.h
└── docs/
    └── DEVELOPMENT_NOTES.md
```

The code is organised as small modules so each part can be tested separately. **This is not yet a validated, end-to-end wearable firmware.** The modules are a starting point; in particular, the pulse-foot detector, R-peak detector, signal-quality scoring and clock synchronisation still need bench testing and refinement.

## Hardware currently planned

- ESP32-C3 SuperMini
- AD8232 ECG front-end module
- MAX30102 red/infrared PPG module
- 4-pin RGB LED module (one resistor per colour channel)
- Momentary push button
- Three 1 kΩ resistors for RGB LED channels

A thermistor and urine-strip colour reader are intentionally out of the current build scope.

## Proposed pin map

| Function | ESP32-C3 GPIO | Notes |
|---|---:|---|
| AD8232 OUTPUT | GPIO3 | ADC input; confirm the exact board pinout |
| AD8232 LO+ | GPIO4 | Lead-off digital input |
| AD8232 LO− | GPIO10 | Lead-off digital input |
| MAX30102 SDA | GPIO6 | I²C data |
| MAX30102 SCL | GPIO7 | I²C clock |
| MAX30102 INT | Not connected | Optional; FIFO is polled in the current scaffold |
| Push button | GPIO5 | Button to GND; uses `INPUT_PULLUP` |
| RGB LED R/G/B | Not assigned | Choose and verify pins before wiring |

**Important:** ESP32-C3 SuperMini boards vary. Check the silkscreen and board schematic before connecting anything. GPIO availability and boot/USB functions can vary between board revisions. The proposed GPIO5 button assignment must be verified against the exact board revision before use.

### Power and wiring

- Disconnect USB and all power before changing wiring.
- Check the MAX30102 breakout's input-voltage requirements. Use 3.3 V only if the specific breakout supports it.
- Connect grounds together for bench testing.
- Do not connect the RGB LED without a separate current-limiting resistor on each R, G and B channel. The three 1 kΩ resistors are intended as one resistor per channel.
- The button uses an internal pull-up: one electrical side to GPIO5, the other to GND. On a 4-leg tactile switch, legs on the same side are commonly internally connected; use opposite sides.
- The AD8232 module's output must stay within the ESP32-C3 ADC input range. Confirm module output behaviour and board ADC limits before connecting.

## Software setup

1. Install the Arduino IDE.
2. Install the Espressif ESP32 board package and select the correct ESP32-C3 board.
3. Install **SparkFun MAX3010x Pulse and Proximity Sensor Library** using Arduino IDE Library Manager. The library commonly exposes the sensor as `MAX30105`, which also supports MAX30102 devices.
4. Open `PreEclampsia_Wearable.ino`.
5. Confirm the pin map and board selection before uploading.

The initial sketch checks whether the MAX30102 is detected and prints raw red/IR samples. The other modules are provided as development components and are not all wired into the main sketch yet.

## Development sequence

1. **Bench bring-up:** verify power, I²C detection and raw MAX30102 readings.
2. **PPG processing:** inspect raw waveforms; tune baseline removal, filtering and pulse-foot detection using recorded data.
3. **ECG acquisition:** verify ADC range, lead-off detection and clean ECG waveforms using an appropriate isolated test setup.
4. **R-peak detection:** validate against annotated recordings; do not assume the simple detector is accurate.
5. **Time synchronisation:** reconstruct MAX30102 FIFO sample times and ECG sample times against one common clock. Handle FIFO overflow, missed samples, read latency and clock drift.
6. **PAT event pairing:** only pair high-quality events after timing has been validated.
7. **Calibration and history:** keep original, active and candidate references separate. Never silently replace the original baseline.
8. **BLE and phone interface:** add only after reliable acquisition and timestamps are demonstrated.

## Calibration design principles

The intended data model has three separate layers:

- **Original personal reference:** the first accepted research reference; protected from automatic replacement.
- **Active reference:** the currently selected comparison reference.
- **Candidate reference:** a later proposed update that must be reviewed explicitly.

A reset/recalibration action must not erase the original reference or research history. Population-level pregnancy blood-pressure data must remain separate from an individual's PAT data. Do not insert population BP values as PAT thresholds or treat population averages as a personal baseline.

Any thresholds in this repository are **engineering placeholders**, not clinical thresholds. They must not be used to identify or exclude pre-eclampsia.

## Current limitations

- No clinical validation has been performed.
- No pregnancy-specific PAT reference range is established by this project.
- The simple PPG foot and ECG R-peak algorithms are experimental.
- The initial signal-quality module is only a placeholder.
- The PAT engine expects timestamps in the same clock domain; this is not yet solved by the raw sensor sketches.
- MAX30102 FIFO timing and overflow handling need implementation before PAT values can be trusted.
- Persistent calibration storage, data export, BLE and mobile UI are not implemented in the main sketch.
- The button logic is a UI event scaffold, not actual power switching or deep sleep.

## Contributing / next work

Keep changes small and testable. Record board revision, wiring, library version, sampling configuration and test conditions. Preserve raw test data and annotate known artefacts. Never describe bench results as clinical performance.

## License

See [LICENSE](LICENSE). This repository is provided for educational and research prototyping.
