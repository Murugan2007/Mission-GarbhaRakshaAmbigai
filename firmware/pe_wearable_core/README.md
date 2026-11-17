# Reference firmware core for the ESP32-C3

A learning prototype that times the gap between each heartbeat (AD8232 ECG) and the pulse reaching
the finger (MAX30102). That gap, the pulse arrival time (PAT), moves with blood pressure. **It is not
a medical device and its output must never be used to decide anyone's care.** Thermistor, urine-strip
reader and Bluetooth are not in this version.

## Status

| Item | State |
| --- | --- |
| Timing core `pat_core.h` | Implemented and verified on synthetic ECG and pulse signals with a known answer (run `python3 tests/run_tests.py`) |
| Sketch `pe_wearable.ino` | Implemented; runs end to end in a PC simulation against stub Arduino and a simulated MAX30102 (`tests/sketch_sim.cpp`) |
| Bench tuning | In progress: thresholds and filter settings are being tuned against real recordings |
| Toolchain checks | In progress: board-package-specific API details (for example `Wire` signatures) are being confirmed per ESP32 core version |
| Blood-pressure accuracy | Research question: needs per-person cuff calibration and a consented study |

All thresholds are engineering placeholders for tuning, not clinical limits.

## Wiring (ESP32-C3 SuperMini)

| Signal | Pin |
| --- | --- |
| AD8232 OUTPUT | GPIO3 |
| AD8232 LO+ / LO- | GPIO4 / GPIO10 |
| MAX30102 SDA / SCL | GPIO6 / GPIO7 |
| AD8232 and MAX30102 power | 3V3 and GND |

Keep off GPIO2, 8, 9 (boot pins). Power from a USB power bank or a protected LiPo through a TP4056
module into the 5V pin; never feed a cell into 3V3, and do not wear the electrodes while charging or
while connected to mains-powered equipment.

## Build

1. Arduino IDE with the **esp32 by Espressif** board package (2.x or 3.x).
2. Board: **ESP32C3 Dev Module**. Set **USB CDC On Boot: Enabled**.
3. Open the `pe_wearable` folder (it holds `pe_wearable.ino` and `pat_core.h` together), upload.
4. Serial monitor at 115200 baud, line ending "Newline".

## Use

| Command | What it does |
| --- | --- |
| `h` | Help |
| `r` | Toggle raw lines `ecg_mV,ppg_counts` for the Serial Plotter (phase 2: see each signal alone) |
| `b` | Toggle `BEAT` lines |
| `s` | Status: leads, finger present, good beats, median PAT, I2C errors |
| `cuff 118 76` | Log a cuff reading beside the current median PAT |

`BEAT,t_ms,rr_ms,pat_ms,ppg_amp,ecg_amp,valid,in_trend,median_pat_ms`: one line per heartbeat.
`valid` = a pulse foot was found, `in_trend` = the beat passed the outlier check and joined the
rolling median of the last 15 good beats.

Sit still, same posture, finger on the sensor, electrodes on (two on the chest, one reference).

## Calibrate

1. Sit still for five minutes. Take a cuff reading on an arm at heart level.
2. Type `cuff <sys> <dia>` in the monitor straight away. It prints a `CAL,...` line.
3. Repeat across several days and times, including after light activity: at least 10 readings.
4. Save the monitor output to `log.txt`, then `python3 tools/fit_calibration.py log.txt`.
   It prints `SBP = a + b / PAT` with a leave-one-out error. A weak correlation means the timing is
   not tracking pressure yet; do not trust any number from it.

## Run the tests (PC, no hardware)

```
python3 tests/run_tests.py          # needs g++ and numpy
```

It generates synthetic ECG and pulse signals with a known PAT, runs the timing core and then the
sketch (with stub hardware) over eight scenarios, and prints two tables. Details of what is
simulated are at the top of `tests/gen_synth.py`.

## Files

| Path | What |
| --- | --- |
| `pe_wearable/pe_wearable.ino` | The sketch: ECG task, MAX30102 FIFO driver, serial commands |
| `pe_wearable/pat_core.h` | Portable C++ core: filters, R-peak detector, pulse-foot finder, sensor-clock tracker, trend filter |
| `tests/` | Signal generator, scoring, core test, sketch simulator and stubs |
| `tools/fit_calibration.py` | Fits the calibration line from `CAL` lines |

## How the timing works, in short

* ECG at 250 Hz from a timer-driven task; each sample is stamped right after the ADC read.
* The MAX30102 hands over samples in bursts, so a sample's time is worked out from its running
  number (`PpgClock`, an alpha-beta filter that also follows the sensor's clock drift), not from when it was read.
* R peak: 50 Hz notch, band-pass, derivative, squaring, moving-window integral, adaptive threshold, then
  the peak time refined with a parabola on a lightly filtered copy.
* Pulse foot: smooth, find the steepest upstroke, fit a line through it, fit a sloping baseline line just
  before the rise, and take where they cross. The sloping baseline is what survives breathing drift.
* PAT = foot time minus R time. Beats outside plausible limits, or more than 30 ms from the rolling median, stay out of the trend.
* A constant offset in PAT (filter delay) is harmless: calibration absorbs it.
