// Runs the real sketch (pe_wearable.ino) on a PC, against fake Arduino/MAX30102 stubs fed with
// synthetic data from gen_synth.py.   usage: sketch_sim events.csv [ppm]
// BEAT lines go to stdout.
#include <vector>
#include <string>
#include "stubs/Arduino.h"
#include "stubs/esp_timer.h"
#include "stubs/Wire.h"

int64_t g_simNowUs = 0;
float g_simAdcMv = 0;
int g_simLoP = 0, g_simLoN = 0;
SerialStub Serial;
Max30102Sim g_max;
TwoWire Wire;

#include "../pe_wearable/pe_wearable.ino"

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s events.csv [clock_ppm]\n", argv[0]); return 1; }
  double ppm = argc > 2 ? atof(argv[2]) : 0.0;
  struct E { int64_t t; float mv; };
  std::vector<E> ecg;
  FILE* f = fopen(argv[1], "r");
  if (!f) return 1;
  static char line[16384];
  while (fgets(line, sizeof line, f)) {
    if (line[0] == 'E') {
      char* s = line + 2;
      long long t = strtoll(s, &s, 10);
      long v = strtol(s + 1, &s, 10);
      ecg.push_back({t, (float)v});
    } else if (line[0] == 'P') {
      char* s = line + 2;
      strtoll(s, &s, 10);
      long n = strtol(s + 1, &s, 10);
      for (long i = 0; i < n; i++) g_max.ir.push_back((uint32_t)strtol(s + 1, &s, 10));
    }
  }
  fclose(f);
  g_max.tOffUs = 913000.0;                       // gen_synth.py: first sample completes here
  g_max.periodUs = 5000.0 * (1.0 + ppm * 1e-6);

  setup();
  size_t next = 0;
  int64_t end = ecg.empty() ? 0 : ecg.back().t + 1000000;
  for (int64_t T = 0; T < end; T += 1000) {
    while (next < ecg.size() && ecg[next].t <= T) {     // the ECG task fires at its own times
      g_simNowUs = ecg[next].t;
      g_simAdcMv = ecg[next].mv;
      ecgSampleOnce();
      next++;
    }
    g_simNowUs = T;
    loop();
  }
  return 0;
}
