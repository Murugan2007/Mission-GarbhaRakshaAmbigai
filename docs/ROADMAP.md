# Roadmap: tuning, and alternative components and systems

## Current focus: tuning

| Area | Work |
| --- | --- |
| Signal chain | Tune filter and detector settings on real recordings; confirm per-board API details across ESP32 core versions |
| Calibration | Refit schedule, drift handling, quality gates on the fit's correlation |
| Rules | Replace placeholder trend bands with data-driven ones; add gestational-age adjustment |
| Power | Battery life tests, charge-current setting, low-power sampling windows |

## Alternative components to evaluate

| Function | Current | Alternatives to evaluate | Why |
| --- | --- | --- | --- |
| ECG front end | Single-lead analog module | Integrated ECG front ends (for example ADS1292R, MAX30003) | Higher resolution, built-in leads-off, lower noise |
| Pulse sensing | Reflective optical module | Integrated ECG-plus-PPG sensor modules (for example MAX86150); other optical front ends | One shared clock for both signals removes the timestamp reconstruction problem |
| Motion rejection | Beat-quality gating | Add an IMU (for example LSM6DS3, ICM-42688) | Reject movement-corrupted beats and measure only at rest |
| MCU | ESP32-C3 | ESP32-C6, nRF52840 | Lower BLE power, Thread/Matter options, native secure boot |
| Charging | TP4056 module | Power-path charger with fuel gauge | Charge while powered, battery percentage in the app |
| Temperature | none | Digital skin-temperature sensor | Supporting signal |
| Edema | none | Bio-impedance front end | Fluid retention, an emerging signal |
| Urine protein | Manual strip | Colour sensor reading a strip in a fixed holder | Objective confirmation, not a daily input |
| Form factor | Chest strap plus finger or wrist sensor | Patch ECG, ring or wrist pulse sensor | Comfort over months of pregnancy |

## Alternative and future systems

| Area | Option |
| --- | --- |
| On-device model | Light TinyML model beside the fuzzy rules, trained only on consented data |
| Baseline risk | Combine registration risk factors (age, BMI, history) with the sensor trend |
| Phone | Optional consent-based sync to a clinician dashboard (FastAPI + PostgreSQL), anonymised summaries only |
| Updates | Over-the-air firmware updates over BLE |
| Access | Local-language messages and voice prompts; a health-worker mode for community care |
| Validation | Test against the IEEE cuffless blood-pressure standard under ethics approval; clinician-supervised pilot |

Each alternative is evaluated against the same synthetic-signal harness first, then real recordings.
