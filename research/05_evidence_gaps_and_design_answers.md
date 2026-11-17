# Evidence gaps and how the design answers them

| Gap in the evidence | What this design does |
| --- | --- |
| Cuffless estimates are least accurate in the people who matter; pre-eclampsia changes arterial stiffness, so a calibration made in a healthy week may drift | Do not present the output as a diagnosis. Use the PAT trend as a prompt to take a cuff reading; the cuff is the source of truth |
| In the 2024 paper the machine-learning model scores registration risk factors while the watch alerts are plain thresholds, so sensors and model are not connected | Start from baseline risk (age, BMI, history) and adjust it with the sensor trend |
| Fixed thresholds ignore gestational age and the person | Personal rolling baseline, adjustable by weeks of pregnancy |
| Motion is the main obstacle for pulse sensors | Measure at rest or asleep; discard noisy beats |
| Small datasets with few positive cases | Consented data collection with paired cuff readings, evaluation set in advance, results by skin tone |
| A false alarm is costly | Judge as a rule-out or triage tool and report negative predictive value, as PROGNOSIS did |
| Urine strips can be insensitive | Keep the strip as confirmation, not a daily input |
