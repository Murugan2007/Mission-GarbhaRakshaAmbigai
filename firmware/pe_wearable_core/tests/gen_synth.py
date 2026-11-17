#!/usr/bin/env python3
"""Synthetic ECG + MAX30102-style pulse data with a KNOWN pulse arrival time (PAT).

Writes two files:
  <out>_events.csv   what the ESP32-C3 would see, in time order:
                       E,<t_us>,<mV>                    one ECG ADC sample (timestamped when read)
                       P,<t_read_us>,<n>,<raw1>,...     one MAX30102 FIFO read of n new samples
  <out>_truth.csv    beat_index,R_time_us,PAT_ms,in_artifact

What is simulated (all of it is a stress test for the timing code):
  * ECG at 250 Hz: P-QRS-T beats, 0.3 Hz baseline wander, 50 Hz mains hum, white noise,
    breathing-linked RR changes, task-scheduling jitter on the sample times.
  * Pulse at 200 Hz from the MAX30102 (raw IR ~110000 counts, pulse makes it DROP), breathing
    drift, slow drift, noise, 18-bit quantisation.
  * The sensor's own clock is off from the ESP32 clock by --clock-ppm.
  * The ESP32 reads the FIFO every ~20 ms with jitter (and a rare 60 ms stall), exactly like loop().
  * The true PAT drifts down 15 ms over the run (a blood-pressure rise), with breathing
    modulation and 2 ms beat-to-beat physiological scatter.
"""
import argparse
import numpy as np


def g(x, mu, s):
    return np.exp(-0.5 * ((x - mu) / s) ** 2)


def ppg_pulse(tau):
    """Unit pulse: asymmetric systolic peak + a smaller reflected (diastolic) bump."""
    s = np.where(tau < 0.16, 0.05, 0.11)
    return np.exp(-0.5 * ((tau - 0.16) / s) ** 2) + 0.35 * g(tau, 0.42, 0.07)


def true_foot_offset():
    """Where the classic intersecting-tangent foot of ppg_pulse sits on its own time axis."""
    tau = np.arange(-0.3, 0.9, 0.0001)
    p = ppg_pulse(tau)
    sl = np.gradient(p, tau)
    m = (tau > -0.1) & (tau < 0.3)
    i = np.argmax(np.where(m, sl, -1e9))
    base = p[(tau > -0.25) & (tau < -0.05)].min()
    return tau[i] - (p[i] - base) / sl[i]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--dur", type=float, default=120.0, help="seconds")
    ap.add_argument("--hr", type=float, default=72.0, help="mean heart rate, bpm")
    ap.add_argument("--noise", type=float, default=1.0, help="noise multiplier for ECG and pulse")
    ap.add_argument("--clock-ppm", type=float, default=0.0, help="sensor clock error vs ESP32, ppm")
    ap.add_argument("--burst", action="store_true", help="add a 3 s motion artifact at t=60 s")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    rng = np.random.default_rng(a.seed)

    dur_us = a.dur * 1e6
    # ---------------- beats ----------------
    rr_mean = 60.0 / a.hr
    R = []
    t = 1.0
    while t < a.dur - 1.5:
        R.append(t)
        t += rr_mean + 0.04 * np.sin(2 * np.pi * 0.25 * t) + rng.normal(0, 0.012)
    R = np.array(R)
    pat = (0.230 - 0.015 * R / a.dur + 0.004 * np.sin(2 * np.pi * 0.25 * R + 1.0)
           + rng.normal(0, 0.002, len(R)))
    foot_tau = true_foot_offset()
    onset = R + pat - foot_tau          # pulse reference time so its tangent-foot lands at R + PAT

    # ---------------- ECG (250 Hz) ----------------
    fs_ecg = 250.0
    k = np.arange(int((a.dur - 0.2) * fs_ecg))
    te = k / fs_ecg + 0.1234 + rng.uniform(0.0, 0.0008, len(k))      # read-time jitter, 0-0.8 ms
    ecg = np.zeros_like(te)
    ia = np.searchsorted(R, te)
    for off in (-2, -1, 0, 1):
        j = np.clip(ia + off, 0, len(R) - 1)
        dt = te - R[j]
        ecg += 600.0 * (0.10 * g(dt, -0.16, 0.025) - 0.12 * g(dt, -0.028, 0.010) + 1.00 * g(dt, 0.0, 0.011)
                        - 0.20 * g(dt, 0.030, 0.012) + 0.30 * g(dt, 0.26, 0.05))
    ecg += 1500.0                                                     # AD8232 output idles near 1.5 V
    ecg += 150.0 * np.sin(2 * np.pi * 0.30 * te + 0.7)                # baseline wander
    ecg += 60.0 * np.sin(2 * np.pi * 50.0 * te)                       # mains hum (India: 50 Hz)
    ecg += rng.normal(0, 12.0 * a.noise, len(te))
    if a.burst:
        m = (te > 60) & (te < 63)
        slow = np.convolve(rng.normal(0, 1, len(te)), np.ones(15) / 15, mode="same")
        ecg[m] += 500.0 * slow[m] * 3.0 + rng.normal(0, 80.0, m.sum())
    ecg = np.round(np.clip(ecg, 0, 3300) / 0.806) * 0.806             # 12-bit ADC over 3.3 V
    te_us = np.round(te * 1e6).astype(np.int64)

    # ---------------- PPG (MAX30102, 200 Hz nominal) ----------------
    p_nom = 5000.0                                                    # us
    p_true = p_nom * (1.0 + a.clock_ppm * 1e-6)
    t_off = 913_000.0
    n_ppg = int((dur_us - t_off - 0.3e6) / p_true)
    kk = np.arange(n_ppg)
    Tk = t_off + kk * p_true                                          # true time each sample completes (us)
    ts = Tk * 1e-6
    sig = np.zeros_like(ts)
    ib = np.searchsorted(onset, ts)
    for off in (-2, -1, 0):
        j = np.clip(ib + off, 0, len(onset) - 1)
        sig += ppg_pulse(ts - onset[j])
    raw = 110000.0 - 600.0 * sig                                      # reflected light FALLS when blood volume rises
    raw += 400.0 * np.sin(2 * np.pi * 0.25 * ts + 0.3)                # breathing
    raw += 2000.0 * np.sin(2 * np.pi * 0.02 * ts)                     # slow drift
    raw += rng.normal(0, 20.0 * a.noise, len(ts))
    if a.burst:
        m = (ts > 60) & (ts < 63)
        raw[m] += np.cumsum(rng.normal(0, 150.0, m.sum()))
    raw = np.round(raw).astype(np.int64)

    # ---------------- the ESP32 polling the FIFO ----------------
    events = []
    for i in range(len(te_us)):
        events.append((int(te_us[i]), "E,%d,%d" % (te_us[i], int(round(ecg[i])))))
    polls = np.arange(1, int(dur_us / 20000.0)) * 20000.0 + rng.uniform(0, 5000.0, int(dur_us / 20000.0) - 1)
    stall = rng.random(len(polls)) < 0.01
    polls = np.sort(polls + np.where(stall, 60000.0, 0.0))
    read_upto = 0
    for tp in polls:
        avail = int(np.searchsorted(Tk + 400.0, tp, side="right"))    # samples complete by now
        if avail <= read_upto:
            continue
        lo = max(read_upto, avail - 32)                               # FIFO holds 32 samples
        vals = raw[lo:avail]
        t_read = int(round(tp + 400.0))                               # timestamp taken after the I2C read
        events.append((t_read, "P,%d,%d,%s" % (t_read, len(vals), ",".join(str(int(v)) for v in vals))))
        read_upto = avail
    events.sort(key=lambda e: e[0])
    with open(a.out + "_events.csv", "w") as f:
        for _, line in events:
            f.write(line + "\n")

    with open(a.out + "_truth.csv", "w") as f:
        for i in range(len(R)):
            art = 1 if (a.burst and 59.5 < R[i] < 63.5) else 0
            f.write("%d,%d,%.3f,%d\n" % (i, int(round(R[i] * 1e6)), pat[i] * 1e3, art))
    print("beats=%d ecg=%d ppg=%d foot_offset=%.1f ms" % (len(R), len(te_us), n_ppg, foot_tau * 1e3))


if __name__ == "__main__":
    main()
