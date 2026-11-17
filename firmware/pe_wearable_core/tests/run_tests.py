#!/usr/bin/env python3
"""Build and run everything on synthetic signals, then print two result tables.

  python3 tests/run_tests.py [work_dir]      (from the project folder; needs g++, numpy)

Table 1: the timing core (pat_core.h) alone.
Table 2: the real sketch (pe_wearable.ino) running on a PC against stub Arduino/MAX30102 code.
Both are scored against the known true pulse arrival time in the synthetic data.
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCENARIOS = [
    ("Baseline, 72 bpm", []),
    ("Slow heart, 55 bpm", ["--hr", "55"]),
    ("Fast heart, 100 bpm", ["--hr", "100"]),
    ("Noise x2", ["--noise", "2"]),
    ("Noise x3", ["--noise", "3"]),
    ("Pulse clock +3000 ppm", ["--clock-ppm", "3000"]),
    ("Pulse clock -3000 ppm", ["--clock-ppm", "-3000"]),
    ("3 s motion burst", ["--burst"]),
]
COLS = ["detected_pct", "false_beats", "pat_valid_pct", "bias_ms", "err_sd_ms", "err_p95_ms",
        "track_slope", "track_r", "false_beats_in_trend", "artifact_beats_in_trend"]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def ppm_of(extra):
    return extra[extra.index("--clock-ppm") + 1] if "--clock-ppm" in extra else "0"


def sim_to_csv(sim_stdout, path):
    with open(path, "w") as f:
        f.write("t_r_us,rr_ms,pat_ms,ppg_amp,ecg_amp,valid,in_trend\n")
        for line in sim_stdout.splitlines():
            if line.startswith("BEAT,"):
                p = line.split(",")
                f.write("%d,%s,%s,%s,%s,%s,%s\n" % (round(float(p[1]) * 1000), p[2], p[3], p[4], p[5], p[6], p[7]))


def table(rows):
    out = ["| Scenario | " + " | ".join(COLS) + " |", "|" + "---|" * (len(COLS) + 1)]
    for name, r in rows:
        out.append("| " + name + " | " + " | ".join(str(r.get(c, "-")) for c in COLS) + " |")
    return "\n".join(out)


def main():
    work = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp(prefix="pe_tests_")
    os.makedirs(work, exist_ok=True)
    core = os.path.join(work, "test_core")
    sim = os.path.join(work, "sketch_sim")
    run(["g++", "-O2", "-std=c++17", "-Wall", "-Wextra", os.path.join(HERE, "test_core.cpp"), "-o", core])
    run(["g++", "-O2", "-std=c++17", "-Wall", "-Wextra", "-x", "c++", "-I", os.path.join(HERE, "stubs"),
         os.path.join(HERE, "sketch_sim.cpp"), "-o", sim])
    core_rows, sketch_rows = [], []
    for n, (name, extra) in enumerate(SCENARIOS):
        base = os.path.join(work, "s%d" % n)
        run([sys.executable, os.path.join(HERE, "gen_synth.py"), "--out", base] + extra)
        run([core, base + "_events.csv", base + "_core.csv"])
        ev = run([sys.executable, os.path.join(HERE, "evaluate.py"), base + "_truth.csv", base + "_core.csv"])
        core_rows.append((name, json.loads(ev.stdout)))
        out = run([sim, base + "_events.csv", ppm_of(extra)])
        sim_to_csv(out.stdout, base + "_sketch.csv")
        ev = run([sys.executable, os.path.join(HERE, "evaluate.py"), base + "_truth.csv", base + "_sketch.csv"])
        sketch_rows.append((name, json.loads(ev.stdout)))
    print("Table 1: timing core alone\n")
    print(table(core_rows))
    print("\nTable 2: the sketch on a PC with stub hardware\n")
    print(table(sketch_rows))


if __name__ == "__main__":
    main()
