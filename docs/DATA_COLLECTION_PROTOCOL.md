# Data collection protocol

The field's main weakness is small datasets, so collection is designed in from the start.

1. **Participants.** Healthy adult volunteers with written consent. No pregnant participants without clinician involvement and ethics approval.
2. **Paired data.** Log device PAT next to a real upper-arm cuff reading, with time, posture and activity notes. The app's Calibrate screen does the pairing: enter the cuff reading straight after taking it.
3. **Conditions.** Sit still five minutes before each reading; same posture; arm at heart level. Spread readings over several days and times, including after light activity. At least 10 pairs per person.
4. **Per-person calibration.** Each wearer has their own line. One person's calibration is never reused for another. Refit weekly at first.
5. **Storage.** On the phone, as SQLite with CSV export. Names removed, random participant ID. Raw data and metadata (board revision, wiring, firmware version, sampling settings) preserved.
6. **Evaluation set in advance.** Agreement with the cuff (including leave-one-out error), false-alarm rate, negative predictive value, and results split by skin tone.
7. **Calibration tools.** `firmware/pe_wearable_core/tools/fit_calibration.py` fits SBP and DBP lines from logged pairs and prints a leave-one-out error.
8. **Later phase.** Clinician-supervised pilot with pregnant participants, asking whether alerts arrive earlier than routine visits would catch the change.

Population blood-pressure data stays separate from an individual's PAT data; population averages are never used as a personal baseline.
