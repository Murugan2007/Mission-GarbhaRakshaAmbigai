#!/usr/bin/env python3
"""Compare the core's output with the known truth. Prints one summary dict as JSON."""
import json
import sys

import numpy as np


def load(path, cols):
    rows = []
    with open(path) as f:
        for line in f:
            p = line.strip().split(",")
            try:
                rows.append([float(x) for x in p[:cols]])
            except ValueError:
                continue                      # header
    return np.array(rows)


def main():
    truth = load(sys.argv[1], 4)              # idx, R_us, PAT_ms, in_artifact
    res = load(sys.argv[2], 7)                # t_r_us, rr_ms, pat_ms, ppg_amp, ecg_amp, valid, in_trend
    warm = 6e6                                # ignore the first 6 s (filters and clock settling)

    tr_t, tr_pat, tr_art = truth[:, 1], truth[:, 2], truth[:, 3] > 0
    used = np.zeros(len(res), bool)
    matched, errs, pats_true, pats_est = 0, [], [], []
    art_beats = art_flagged = art_kept = 0
    eligible = 0
    for i in range(len(truth)):
        if tr_t[i] < warm:
            continue
        eligible += 1
        d = np.abs(res[:, 0] - tr_t[i])
        j = int(np.argmin(d)) if len(res) else -1
        if j < 0 or d[j] > 60000 or used[j]:
            continue
        used[j] = True
        matched += 1
        if tr_art[i]:
            art_beats += 1
            art_flagged += int(res[j, 5] == 0)
            art_kept += int(res[j, 6] > 0)
            continue
        if res[j, 5] > 0:
            errs.append(res[j, 2] - tr_pat[i])
            pats_true.append(tr_pat[i])
            pats_est.append(res[j, 2])
    unmatched = (~used) & (res[:, 0] > warm)
    false_pos = int(unmatched.sum())
    false_kept = int((unmatched & (res[:, 6] > 0)).sum())
    errs, pt, pe = np.array(errs), np.array(pats_true), np.array(pats_est)
    out = {
        "beats": eligible,
        "detected_pct": round(100.0 * matched / max(eligible, 1), 1),
        "false_beats": false_pos,
        "false_beats_in_trend": false_kept,
        "pat_valid_pct": round(100.0 * len(errs) / max(matched - art_beats, 1), 1),
    }
    if len(errs) > 5:
        out["bias_ms"] = round(float(errs.mean()), 2)
        out["err_sd_ms"] = round(float(errs.std()), 2)
        out["err_p95_ms"] = round(float(np.percentile(np.abs(errs - errs.mean()), 95)), 2)
        A = np.vstack([pt, np.ones_like(pt)]).T
        out["track_slope"] = round(float(np.linalg.lstsq(A, pe, rcond=None)[0][0]), 2)
        out["track_r"] = round(float(np.corrcoef(pt, pe)[0, 1]), 3)
    if art_beats:
        out["artifact_beats_flagged"] = "%d/%d" % (art_flagged, art_beats)
        out["artifact_beats_in_trend"] = "%d/%d" % (art_kept, art_beats)
    print(json.dumps(out))


if __name__ == "__main__":
    main()
