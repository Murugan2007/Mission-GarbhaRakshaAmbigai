# Risk rules

Readable fuzzy rules turn the trend into one of four messages. The personal baseline is the average of the first week of calibrated readings; the original reference is kept on file and never silently replaced.

A rise of 5 mmHg or less over baseline counts as fully "stable"; a rise of 15 is fully "rising"; values in between are partly both. This overlap stops a single jumpy reading from raising an alarm. The bands are engineering placeholders to be tuned with data.

| # | IF blood-pressure trend is | AND protein strip is | THEN risk | App message |
| --- | --- | --- | --- | --- |
| 1 | Stable | Negative | Low | Nothing to do. Keep wearing |
| 2 | Rising | Negative or Trace | Medium | Re-check with your cuff today |
| 3 | Stable | 1+ or higher | Medium | Repeat the strip, then tell your clinic |
| 4 | Rising | 1+ or higher | High | Call your clinic today |
| 5 | Rising fast | Any | High | Call your clinic today |
| 6 | Any | Any, and cuff 140/90 or higher, twice, 4+ hours apart | High | Call your clinic today |
| 7 | Any | Any, and cuff 160/110 or higher, or symptoms reported | Urgent | Go to hospital now |

Rules 6 and 7 use the alert levels in the 2024 pre-eclampsia watch paper (from ACOG). The strip is treated as confirmation rather than a daily input. A "Low" result never means a pregnancy is safe. The same rules are implemented in `app/lib/services/risk_engine.dart` with unit tests in `app/test/`.
