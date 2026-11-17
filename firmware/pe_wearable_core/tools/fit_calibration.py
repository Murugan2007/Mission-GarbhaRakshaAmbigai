#!/usr/bin/env python3
"""Fit the calibration line  SBP = a + b / PAT  from CAL lines logged by the sketch.

  1. In the serial monitor, take a cuff reading and type  cuff 118 76  (systolic diastolic).
     The sketch prints  CAL,ms,sys,dia,median_pat_ms,beats_in_median.
  2. Save the monitor output to a text file (any other lines are ignored).
  3. python3 tools/fit_calibration.py log.txt

Needs numpy. Use at least 10 readings spread over several days and conditions.
"""
import sys

import numpy as np


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    rows = []
    for line in open(sys.argv[1], errors="ignore"):
        line = line.strip()
        if not line.startswith("CAL,"):
            continue
        p = line.split(",")
        try:
            sys_, dia, pat, n = float(p[2]), float(p[3]), float(p[4]), int(p[5])
        except (ValueError, IndexError):
            continue
        if n >= 8 and pat > 0:
            rows.append((sys_, dia, pat))
    if len(rows) < 6:
        print("Need at least 6 usable CAL lines (with 8+ good beats); found %d." % len(rows))
        return 1
    d = np.array(rows)
    x = 1.0 / d[:, 2]
    for name, y in (("SBP", d[:, 0]), ("DBP", d[:, 1])):
        A = np.vstack([np.ones_like(x), x]).T
        (a, b), *_ = np.linalg.lstsq(A, y, rcond=None)
        res = y - A @ np.array([a, b])
        loo = []
        for i in range(len(y)):
            m = np.arange(len(y)) != i
            c = np.linalg.lstsq(A[m], y[m], rcond=None)[0]
            loo.append(y[i] - A[i] @ c)
        r = np.corrcoef(x, y)[0, 1]
        print("%s = %.2f + %.1f / PAT(ms)   n=%d  r=%.2f  fit sd=%.1f mmHg  leave-one-out sd=%.1f mmHg"
              % (name, a, b, len(y), r, res.std(), np.std(loo)))
    print("Check the plot or the r value before trusting these. A weak r means the timing is not tracking pressure yet.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
