# Landscape and prior work

No device on the market detects pre-eclampsia on its own. What exists is a spread of clinical cuffs, pregnancy wearables and research prototypes.

| Device or project | Kind | What it does | Where it falls short |
| --- | --- | --- | --- |
| Withings BPM Pro 2 | Professional | Cuff-based monitor, in a clinical validation trial for pregnancy and pre-eclampsia | Still a cuff: a reading when you take one, not continuous |
| Bloomlife | Professional | Pregnancy wearable known for contraction and preterm-labour tracking | Pre-eclampsia-specific claims are limited |
| Ava bracelet | Consumer | FDA-cleared for fertility tracking, marketed loosely for pregnancy | Independent studies found heart-rate and sleep estimates not equivalent to reference devices |
| i-bracelet | Research (EU-funded) | Bracelet for blood pressure and pre-eclampsia detection with a published sensor-to-app architecture | A research platform; useful as a system design reference |
| SeismoWatch (Georgia Tech) | Research | Cuffless blood pressure from ECG, several PPG signals and motion sensors | The most rigorous open academic design, but a research rig |
| CC3200 system (Santhi et al.) | Research / IoT | Wearable measuring blood pressure, temperature and heart rate, raising a Wi-Fi alarm to a web app | Threshold alarm only |
| Fuzzy-logic classifiers (Espinilla et al., 2017) | Research / software | Five inputs (systolic and diastolic pressure, proteinuria, age, weight) through readable fuzzy rules | Says nothing about obtaining blood pressure continuously |

Device descriptions come from search results, not from the makers' own pages.

## Open problems across the field

1. Cuffless blood pressure drifts and needs recalibration. This is the biggest bottleneck.
2. Proteinuria is unsolved as a continuous wearable measurement.
3. Few devices have real clinical validation, and many citation trails return to two or three academic groups.
4. Maternal data privacy and interoperability are barely addressed.

## Where this project sits

The low-cost, edge-computing corner of that map: a small board, two body-signal channels, readable rules, and a private on-phone history.
