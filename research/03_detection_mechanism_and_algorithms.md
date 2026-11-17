# Detection mechanism and algorithms

## What the device does and does not do

It does not diagnose pre-eclampsia. It flags a pattern that should trigger a proper check.

## The mechanism: pulse arrival time (PAT)

Each heartbeat sends a pressure wave along the arteries. When blood pressure is higher, the artery walls are tighter and the wave travels faster, so it arrives sooner. The ECG R-wave marks the start; the pulse foot at the finger or wrist marks the arrival. The gap is the pulse arrival time (PAT). Strictly, PTT is the transit between two arterial sites and PAT includes the pre-ejection period; many papers use PTT for both.

```
SBP ~ a + b / PAT        (a and b fitted per person against a cuff)
```

PAT is a trend signal. It drifts, so it needs regular recalibration against a validated cuff.

## What the readings tell you

| Reading | What it signals | Evidence and catch |
| --- | --- | --- |
| Cuff blood pressure | The diagnostic criterion itself | BUMP trials: home monitoring alone did not improve detection or control |
| Cuffless blood pressure | The same, continuously | A phone-camera estimator read about 17 mmHg lower systolic in women with pre-eclampsia (JMIR Hum Factors, 2026). NHS England: cuffless devices are not recommended for clinical use |
| Arterial stiffness (pulse wave velocity) | Higher before the disease | At 22-24 weeks, higher in the 29 of 140 at-risk women who later developed it (1.10 vs 0.99 times the median), measured with a clinical tonometer (PLoS ONE, 2011) |
| Heart rate variability | Less vagal, more sympathetic activity | A review of 26 studies shows a consistent pattern, but mostly cross-sectional (Frontiers in Neurology, 2019) |

## Algorithms in the literature and their maturity

| Approach | Example | Maturity |
| --- | --- | --- |
| Combined first-trimester screening | ASPRE: 76.7% of preterm cases detected at a 9.2% false-positive rate | Clinical practice |
| sFlt-1/PlGF blood ratio | Ratio of 38 or lower: 99.3% negative predictive value for one week, 36.7% positive predictive value | Clinical, observational |
| Threshold alerts on a wearable | 140/90 and 160/110 rules run on the device | Prototype |
| Fuzzy rules | Linguistic rules from a decision tree (Espinilla 2017) | Prototype |
| Classic machine learning | Naive Bayes in the 2024 watch paper: 77% accuracy, 86% recall on 352 records | Prototype |
| Pulse-wave and ECG machine learning | Sensitivity above 80%; AUC 0.98 from ECG up to 90 days before diagnosis (second-hand, from a 2026 review) | Early research |

## This project's pipeline

1. **Sample.** ECG at 250 Hz from a timer-driven task; pulse sensor at 200 samples/s read from its FIFO.
2. **Timestamp.** ECG samples are stamped at the ADC read. Pulse samples are stamped from their running sample number through an alpha-beta clock-tracking filter, not from the moment of the burst read.
3. **R-peak.** 50 Hz notch, band-pass, derivative, squaring, moving-window integral, adaptive threshold, then parabolic refinement of the peak time.
4. **Pulse foot.** Smooth, find the steepest upstroke, fit a line through the steep part, fit a sloping baseline line just before the rise, take where they cross. The sloping baseline keeps the foot stable against breathing drift.
5. **PAT** = foot time minus R time.
6. **Trend filter.** A beat joins the rolling median only if valid, with a believable heart rate, within 30 ms of the median of the last 15 kept beats, and with a pulse height within 0.4-2.5x of the median height. Six refusals in a row restart the history.
7. **Personal baseline and rules** (see `04_risk_rules.md`).

## Verification on synthetic signals

Generated ECG and pulse signals with a known true PAT, plus mains hum, breathing drift, noise, sensor clock error and bursts of motion. The core and the full sketch (against stub hardware) produce matching results:

| Scenario | Beats detected | Valid PAT | Per-beat error sd | Tracks the true drift (slope) |
| --- | --- | --- | --- | --- |
| 72 bpm baseline | 100% | 100% | 4.0 ms | 1.03 |
| 55 bpm | 100% | 100% | 4.0 ms | 1.10 |
| 100 bpm | 100% | 97% | 4.7 ms | 1.00 |
| Noise x2 | 100% | 96% | 8.0 ms | 0.89 |
| Noise x3 | 100% | 81% | 10.6 ms | 0.99 |
| Sensor clock +/-3000 ppm | 100% | 100% | 3.9-4.3 ms | 0.99-1.07 |
| 3 s motion burst | 98.5% | 100% | 3.8 ms | 1.03 |

The rolling median of 15 beats had an error sd of about 1.8 ms at baseline noise, against a simulated 11 ms fall in PAT over two minutes. A constant timing offset (filter delay) is absorbed by calibration. Reproduce with `python3 firmware/pe_wearable_core/tests/run_tests.py`.
