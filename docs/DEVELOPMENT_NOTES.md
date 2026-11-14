# Development notes

## Current state

This repository is a **scaffold**, not a finished integrated device. The main Arduino sketch only brings up the MAX30102 and prints raw FIFO values. The other headers contain separable development logic for the button, PPG processing, ECG acquisition/processing, signal-quality gating, PAT pairing and calibration data structure.

## Timing is the critical unresolved issue

- ECG ADC samples are timestamped when acquired using `micros()`.
- MAX30102 samples are buffered in a sensor FIFO. The time when firmware reads a FIFO entry is not the exact time the optical sample was acquired.
- To calculate PAT, implement a FIFO sample-time reconstruction using the configured sensor sample rate, detect FIFO overflow/missed samples, and map sensor sample times to the ESP32 clock.
- Measure and document timing uncertainty. Do not treat the PAT output as meaningful until this is validated.

## Algorithm limitations

- `PpgProcessing` detects a local-minimum candidate, not a fully confirmed physiological pulse foot.
- `EcgProcessing` uses a simple local-maximum and amplitude threshold. It is not a robust clinical-grade QRS detector.
- `SignalQuality` requires motion acceptability and quality scores from future algorithms; no IMU is currently installed and the scores are not yet computed.
- `PatEngine` pairs events but cannot determine whether the pairing is physiologically correct by itself.
- The PAT range constants are engineering sanity checks only, not clinical reference limits.

## Recommended bench tests

1. Verify the exact ESP32-C3 board pinout and power rails.
2. Run the MAX30102-only sketch and capture raw red/IR samples.
3. Test the PPG processor offline against recorded waveforms and manually inspect detections.
4. Verify AD8232 ADC levels and lead-off pins without connecting a person to a non-isolated computer.
5. Test ECG detection against known annotated ECG data.
6. Build and validate a shared-clock timestamp reconstruction layer.
7. Test PAT pairing with synthetic timestamp sequences, including rollover, missing beats and duplicate events.
8. Only then integrate the modules.

## Data and ethics

Preserve raw data and metadata. Do not automatically replace an original reference with a newer value. Do not convert population blood-pressure research into PAT thresholds. Any human-subject research requires appropriate oversight and consent.
