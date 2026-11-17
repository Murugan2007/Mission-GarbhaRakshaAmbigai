# Mission GarbhaRaksha: project overview

Pre-eclampsia is a pregnancy complication that usually begins after week 20. Blood pressure climbs and organs such as the kidneys and liver come under strain, yet many women feel well until it is advanced. Left unnoticed it can become eclampsia, which means seizures and is an emergency for mother and baby. A 2024 review estimates more than 50,000 maternal and 500,000 fetal deaths a year, and much of that harm comes from delay: late to notice, late to reach care, late to be treated.

Blood pressure is checked at antenatal visits, usually weeks apart, while pre-eclampsia can develop in days. Nobody is watching in between.

**Mission GarbhaRaksha watches in between.** A low-cost wearable built around an ESP32-C3 tracks a blood-pressure-linked timing signal from every heartbeat. It learns each person's own baseline, then watches for a rising trend. Readable rules turn that trend into plain messages on the phone: keep wearing, re-check with a cuff today, call your clinic, or go to hospital now. It does not diagnose. It tells the person when to take a cuff reading, so they are checked earlier. Parts cost about Rs 2,000, against thousands for clinical-grade monitors.

All processing happens on the ESP32-C3, so raw body signals never leave the wearable. Compact risk summaries travel over encrypted Bluetooth Low Energy to a Flutter app that keeps a private, on-phone history a clinician can read.

## What is in this repository

| Path | What it is |
| --- | --- |
| `src/`, `PreEclampsia_Wearable.ino` | Firmware development modules and the sensor bring-up sketch |
| `firmware/pe_wearable_core/` | Reference timing core and sketch with a synthetic-signal test harness |
| `app/` | Flutter companion app (BLE client, risk rules, calibration, private history) |
| `research/` | Clinical background, landscape, mechanism, risk rules, evidence gaps, references |
| `docs/` | Architecture, BLE protocol, data collection protocol, roadmap with alternative components and systems |

## Status

The signal chain and app are implemented as a working MVP. Current work is tuning: filter and detector settings against real recordings, calibration quality across people, and the alternative components in the roadmap. This is a research prototype and a screening aid, not a medical device, and not a substitute for professional care. See the safety note in the top-level README.
