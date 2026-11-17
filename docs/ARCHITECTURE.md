# Architecture

```
 body-signal sensing (ECG channel + optical pulse channel)
            |  ADC / I2C
            v
   ESP32-C3 (FreeRTOS, C++)
     - 250 Hz timer-driven ECG task, microsecond timestamps
     - 200 sample/s pulse FIFO reader with sample-number timestamps (alpha-beta clock tracker)
     - R-peak detector, pulse-foot finder, PAT = foot - R
     - trend filter: rolling median of the last 15 good beats
            |  BLE 5, encrypted, ~30 s summary packets + immediate alerts
            v
   Flutter app
     - private on-phone SQLite history, CSV export
     - per-person calibration: SBP = a + b / PAT (cuff pairs)
     - personal baseline (original kept, active resettable)
     - fuzzy rules -> Low / Medium / High / Urgent message
```

## Design decisions

| Decision | Why |
| --- | --- |
| ECG plus optical pulse timing, not a cuff | Continuous and cheap; the principle behind research cuffless designs |
| ESP32-C3 SuperMini | Small, low cost, Wi-Fi and BLE 5; one 160 MHz core is enough at these sample rates |
| Pulse samples stamped from their sample number | The sensor delivers samples in bursts; read time is wrong by up to a sample period and the timing gap is the whole signal |
| Trend and cuff prompt, not a diagnosis | Cuffless estimates drift and pregnancy changes arterial stiffness |
| Readable fuzzy rules, not a black-box model | Runs on a small chip, can be read and challenged by a clinician, little training data exists |
| Processing on the device, summaries only over BLE | Privacy and battery |
| Local-first app storage | Health data stays on the phone unless the person consents to more |

## Power

Protected LiPo cell through a TP4056 charger module into the board's 5V pin. Never feed the cell into 3V3. Lower the charge current with the module's programming resistor (about 1200 / R in ohms; 4.7 kOhm gives about 250 mA). Do not wear the electrodes while charging or while connected to mains-powered equipment.

## Pin map (ESP32-C3 SuperMini)

| Signal | Pin |
| --- | --- |
| ECG output | GPIO3 (ADC1) |
| ECG leads-off LO+ / LO- | GPIO4 / GPIO10 |
| Pulse sensor SDA / SCL | GPIO6 / GPIO7 |

Avoid GPIO2, 8 and 9 (boot pins). Check every pin against the exact board revision.
