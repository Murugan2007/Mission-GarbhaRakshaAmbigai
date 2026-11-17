// Replays a synthetic event file through the same calls the ESP32-C3 sketch makes.
//   g++ -O2 -std=c++17 test_core.cpp -o test_core && ./test_core events.csv results.csv
#include "../pe_wearable/pat_core.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s events.csv results.csv\n", argv[0]); return 1; }
  FILE* in = fopen(argv[1], "r");
  FILE* out = fopen(argv[2], "w");
  if (!in || !out) { fprintf(stderr, "cannot open files\n"); return 1; }

  pat::PatEngine eng;
  eng.begin(250.0f, 50.0f, /*invertPpg=*/true, /*minPpgAmp=*/40.0f, /*minEcgAmpMv=*/40.0f);
  pat::TrendTracker trend;
  trend.begin();
  pat::PpgClock clk;
  clk.begin(200.0f);
  int64_t ppgCount = 0;
  long gaps = 0;

  static char line[16384];
  fprintf(out, "t_r_us,rr_ms,pat_ms,ppg_amp,ecg_amp,valid,in_trend\n");
  while (fgets(line, sizeof line, in)) {
    if (line[0] == 'E') {
      char* s = line + 2;
      long long t = strtoll(s, &s, 10);
      long v = strtol(s + 1, &s, 10);
      eng.pushEcg(t, (float)v);
    } else if (line[0] == 'P') {
      char* s = line + 2;
      long long tRead = strtoll(s, &s, 10);
      long n = strtol(s + 1, &s, 10);
      // same sequence as the sketch: tell the clock about the read, then stamp every sample
      if (clk.onRead(tRead, ppgCount + n - 1)) { eng.markPpgGap(tRead); gaps++; }
      for (long i = 0; i < n; i++) {
        long raw = strtol(s + 1, &s, 10);
        eng.pushPpg(clk.stamp(ppgCount), (float)raw);
        ppgCount++;
      }
    }
    pat::PatEngine::Result r;
    while (eng.pop(r)) {
      bool kept = trend.update(r);
      fprintf(out, "%lld,%.3f,%.3f,%.1f,%.1f,%d,%d\n", (long long)r.t_r_us, r.rr_ms, r.pat_ms,
              r.ppg_amp, r.ecg_amp, r.valid ? 1 : 0, kept ? 1 : 0);
    }
  }
  fclose(out);
  fprintf(stderr, "clock re-anchors: %ld, estimated sensor period: %.3f us\n", gaps, clk.periodUs());
  return 0;
}
