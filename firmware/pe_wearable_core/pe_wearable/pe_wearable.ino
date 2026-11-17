// pe_wearable.ino - pulse-timing recorder for the pre-eclampsia early-warning wearable.
//
// Board : ESP32-C3 SuperMini          (Arduino IDE: "ESP32C3 Dev Module", USB CDC On Boot = Enabled)
// Sensors: AD8232 ECG on GPIO3 (ADC1), MAX30102 pulse sensor on I2C (SDA GPIO6, SCL GPIO7)
// Not used in this build: thermistor, urine-strip colour sensor, BLE.
//
// What it does
//   * samples the ECG at 250 Hz from a FreeRTOS task, stamping each sample with the µs clock;
//   * reads the MAX30102 FIFO every ~15 ms (200 samples/s) and stamps each sample from its
//     sample number, not from the moment of the read (see pat_core.h, class PpgClock);
//   * finds each heartbeat's R peak and pulse foot and prints one line per beat:
//       BEAT,t_ms,rr_ms,pat_ms,ppg_amp,ecg_amp,valid,in_trend,median_pat_ms
//   * keeps a rolling median of good beats, for calibrating against a cuff.
//
// Serial commands (115200 baud, send with newline)
//   h              help
//   r              toggle raw "ecg_mV,ppg_counts" lines for the Serial Plotter
//   b              toggle BEAT lines
//   s              print status now
//   cuff 118 76    log a cuff reading next to the current median PAT:  CAL,ms,sys,dia,pat_ms,n
//
// PAT = pulse arrival time = R peak to pulse foot. It is a trend signal and becomes mmHg
// only after calibration against a real cuff. This is a learning prototype, not a medical device.

#include <Arduino.h>
#include <Wire.h>
#include "esp_timer.h"
#include "pat_core.h"

// ------------------------------- pins -------------------------------
static const int PIN_ECG  = 3;    // AD8232 OUTPUT (ADC1)
static const int PIN_LO_P = 4;    // AD8232 LO+
static const int PIN_LO_N = 10;   // AD8232 LO-
static const int PIN_SDA  = 6;    // MAX30102
static const int PIN_SCL  = 7;

// --------------------------- settings to tune ---------------------------
static const int   ECG_PERIOD_MS = 4;        // 250 Hz
static const float ECG_FS        = 250.0f;
static const float MAINS_HZ      = 50.0f;    // India 50 Hz; use 60 in the US
static const float PPG_FS        = 200.0f;   // 400 samples/s averaged by 2
static const float MIN_ECG_MV    = 40.0f;    // smallest believable R wave at the AD8232 output
static const float MIN_PPG_COUNTS = 60.0f;   // smallest believable pulse height
static const bool  PPG_INVERT    = true;     // reflected IR falls when blood volume rises
static const uint32_t PPG_POLL_MS = 15;

// ------------------------------ MAX30102 ------------------------------
static const uint8_t MAX_ADDR        = 0x57;
static const uint8_t REG_INT_STATUS1 = 0x00;
static const uint8_t REG_FIFO_WR_PTR = 0x04;   // 0x04 write ptr, 0x05 overflow counter, 0x06 read ptr
static const uint8_t REG_FIFO_DATA   = 0x07;
static const uint8_t REG_FIFO_CONFIG = 0x08;
static const uint8_t REG_MODE_CONFIG = 0x09;
static const uint8_t REG_SPO2_CONFIG = 0x0A;
static const uint8_t REG_LED1_PA     = 0x0C;   // red
static const uint8_t REG_LED2_PA     = 0x0D;   // infrared
static const uint8_t REG_PART_ID     = 0xFF;

static bool i2cWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool i2cRead(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t got = Wire.requestFrom((uint8_t)MAX_ADDR, n);
  if (got != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static void fifoReset() {
  i2cWrite(REG_FIFO_WR_PTR, 0);
  i2cWrite(REG_FIFO_WR_PTR + 1, 0);
  i2cWrite(REG_FIFO_WR_PTR + 2, 0);
}

static bool maxInit() {
  uint8_t id = 0;
  if (!i2cRead(REG_PART_ID, &id, 1) || id != 0x15) return false;
  i2cWrite(REG_MODE_CONFIG, 0x40);               // soft reset
  for (int i = 0; i < 20; i++) {
    delay(5);
    uint8_t m = 0x40;
    if (i2cRead(REG_MODE_CONFIG, &m, 1) && !(m & 0x40)) break;
  }
  i2cWrite(REG_FIFO_CONFIG, 0x2F);               // average 2 samples, no rollover
  i2cWrite(REG_SPO2_CONFIG, 0x2E);               // ADC range 4096 nA, 400 samples/s, 215 us pulses
  i2cWrite(REG_LED1_PA, 0x00);                   // red LED off (not needed here)
  i2cWrite(REG_LED2_PA, 0x24);                   // IR LED about 7 mA
  i2cWrite(REG_MODE_CONFIG, 0x03);               // SpO2 mode: FIFO holds RED then IR, 3 bytes each
  uint8_t dummy[2];
  i2cRead(REG_INT_STATUS1, dummy, 2);            // clear any pending interrupt flags
  fifoReset();
  return true;
}

// ------------------------------ ECG task ------------------------------
struct EcgSample { int64_t t; float mv; };
static const uint32_t ECG_RING = 256;            // power of two
static EcgSample g_ecg[ECG_RING];
static volatile uint32_t g_ecgHead = 0, g_ecgTail = 0, g_ecgDropped = 0;

// One ECG sample: read the ADC, then take the time. Called by the task every 4 ms.
void ecgSampleOnce() {
  float mv = (float)analogReadMilliVolts(PIN_ECG);
  int64_t t = esp_timer_get_time();
  uint32_t h = g_ecgHead;
  if (h - g_ecgTail >= ECG_RING) { g_ecgDropped = g_ecgDropped + 1; return; }
  g_ecg[h & (ECG_RING - 1)].t = t;
  g_ecg[h & (ECG_RING - 1)].mv = mv;
  __sync_synchronize();
  g_ecgHead = h + 1;
}

static void ecgTask(void*) {
  TickType_t period = pdMS_TO_TICKS(ECG_PERIOD_MS);
  if (period < 1) period = 1;                    // only if the RTOS tick is slower than 1 kHz
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&last, period);
    ecgSampleOnce();
  }
}

// ------------------------------- state -------------------------------
static pat::PatEngine    g_eng;
static pat::PpgClock     g_clk;
static pat::TrendTracker g_trend;
static int64_t  g_ppgCount = 0;                  // running number of the next PPG sample
static uint32_t g_ppgGaps = 0, g_i2cErrors = 0;
static float    g_irMean = 0, g_lastEcgMv = 0;
static bool     g_raw = false, g_beats = true;
static bool     g_leadsOff = true, g_leadsRaw = true;
static uint32_t g_leadsChangeMs = 0, g_lastPollMs = 0, g_lastStatMs = 0;
static char     g_line[48];
static uint8_t  g_lineLen = 0;

static void startEngine() {
  g_eng.begin(ECG_FS, MAINS_HZ, PPG_INVERT, MIN_PPG_COUNTS, MIN_ECG_MV);
  g_clk.begin(PPG_FS);
  g_trend.begin();
}

// -------------------------------- PPG --------------------------------
static void pollPpg() {
  uint8_t ptr[3];
  if (!i2cRead(REG_FIFO_WR_PTR, ptr, 3)) { g_i2cErrors++; return; }
  int64_t tRead = esp_timer_get_time();          // the moment the write pointer was read
  int wr = ptr[0] & 0x1F, ovf = ptr[1] & 0x1F, rd = ptr[2] & 0x1F;
  if (ovf) {                                     // FIFO filled up and lost samples
    fifoReset();
    g_eng.markPpgGap(tRead);
    g_ppgGaps++;
    return;
  }
  int n = (wr - rd) & 0x1F;
  if (n == 0) return;

  uint32_t ir[32];
  int got = 0;
  while (got < n) {
    int chunk = n - got;
    if (chunk > 16) chunk = 16;                  // keep each I2C read under the 128-byte buffer
    uint8_t buf[96];
    if (!i2cRead(REG_FIFO_DATA, buf, (uint8_t)(chunk * 6))) {
      g_i2cErrors++;
      fifoReset();
      g_eng.markPpgGap(tRead);
      return;
    }
    for (int i = 0; i < chunk; i++) {
      const uint8_t* b = buf + i * 6 + 3;        // 3 bytes RED, then 3 bytes IR
      ir[got + i] = (((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2]) & 0x3FFFF;
    }
    got += chunk;
  }

  // Same order as the PC tests: tell the clock about the read, then stamp every sample.
  if (g_clk.onRead(tRead, g_ppgCount + n - 1)) { g_eng.markPpgGap(tRead); g_ppgGaps++; }
  for (int i = 0; i < n; i++) {
    g_eng.pushPpg(g_clk.stamp(g_ppgCount), (float)ir[i]);
    g_ppgCount++;
    g_irMean += 0.01f * ((float)ir[i] - g_irMean);
    if (g_raw) Serial.printf("%.0f,%lu\n", g_lastEcgMv, (unsigned long)ir[i]);
  }
}

// ------------------------------ leads-off ------------------------------
static void updateLeads(uint32_t nowMs) {
  bool off = digitalRead(PIN_LO_P) || digitalRead(PIN_LO_N);
  if (off != g_leadsRaw) { g_leadsRaw = off; g_leadsChangeMs = nowMs; }
  if (nowMs - g_leadsChangeMs > 250 && g_leadsRaw != g_leadsOff) {
    g_leadsOff = g_leadsRaw;
    g_ecgTail = g_ecgHead;                       // drop samples taken while the electrodes moved
    if (g_leadsOff) {
      Serial.println("EVENT,leads_off");
    } else {
      Serial.println("EVENT,leads_on");
      startEngine();                             // fresh filters, as the electrodes just settled
    }
  }
}

// ------------------------------ output ------------------------------
static void drainResults() {
  pat::PatEngine::Result r;
  while (g_eng.pop(r)) {
    bool kept = g_trend.update(r);
    if (g_beats)
      Serial.printf("BEAT,%.3f,%.1f,%.1f,%.0f,%.0f,%d,%d,%.1f\n", (double)r.t_r_us / 1000.0,
                    r.rr_ms, r.pat_ms, r.ppg_amp, r.ecg_amp, r.valid ? 1 : 0, kept ? 1 : 0,
                    g_trend.medianPat());
  }
}

static void printStatus() {
  Serial.printf("STAT,leads=%s,ir_mean=%.0f,finger=%s,kept_beats=%d,median_pat_ms=%.1f,ppg_gaps=%lu,i2c_err=%lu,ecg_dropped=%lu,sensor_period_us=%.2f\n",
                g_leadsOff ? "OFF" : "ok", g_irMean, g_irMean > 20000.0f ? "yes" : "no",
                g_trend.recentCount(), g_trend.medianPat(), (unsigned long)g_ppgGaps,
                (unsigned long)g_i2cErrors, (unsigned long)g_ecgDropped, g_clk.periodUs());
}

static void handleLine(char* line) {
  if (line[0] == 'h' || line[0] == '?') {
    Serial.println("commands: r raw plot | b beat lines | s status | cuff <sys> <dia> | h help");
  } else if (line[0] == 'r') {
    g_raw = !g_raw;
    Serial.println(g_raw ? "raw on" : "raw off");
  } else if (line[0] == 'b') {
    g_beats = !g_beats;
    Serial.println(g_beats ? "beats on" : "beats off");
  } else if (line[0] == 's') {
    printStatus();
  } else if (strncmp(line, "cuff", 4) == 0) {
    int sys = 0, dia = 0;
    if (sscanf(line + 4, "%d %d", &sys, &dia) == 2 && sys > 0 && dia > 0) {
      if (g_trend.recentCount() < 8) Serial.println("WARN,not enough good beats yet: sit still for 20 s first");
      Serial.printf("CAL,%lu,%d,%d,%.2f,%d\n", (unsigned long)millis(), sys, dia, g_trend.medianPat(),
                    g_trend.recentCount());
    } else {
      Serial.println("usage: cuff 118 76");
    }
  }
}

static void readSerial() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (g_lineLen > 0) { g_line[g_lineLen] = 0; handleLine(g_line); g_lineLen = 0; }
    } else if (g_lineLen < sizeof(g_line) - 1) {
      g_line[g_lineLen++] = (char)c;
    }
  }
}

// ------------------------------ Arduino ------------------------------
void setup() {
  Serial.begin(115200);
  delay(800);
  pinMode(PIN_LO_P, INPUT);
  pinMode(PIN_LO_N, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ECG, ADC_11db);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  if (!maxInit()) Serial.println("ERROR,MAX30102 not found at 0x57: check SDA GPIO6, SCL GPIO7, 3V3, GND");
  startEngine();
  xTaskCreate(ecgTask, "ecg", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr);
  Serial.println("READY,send h for help");
}

void loop() {
  uint32_t nowMs = millis();
  readSerial();
  updateLeads(nowMs);

  while (g_ecgTail != g_ecgHead) {
    EcgSample s = g_ecg[g_ecgTail & (ECG_RING - 1)];
    g_ecgTail = g_ecgTail + 1;
    g_lastEcgMv = s.mv;
    if (!g_leadsOff) g_eng.pushEcg(s.t, s.mv);
  }

  if (nowMs - g_lastPollMs >= PPG_POLL_MS) {
    g_lastPollMs = nowMs;
    pollPpg();
  }
  drainResults();

  if (nowMs - g_lastStatMs >= 10000) {
    g_lastStatMs = nowMs;
    if (g_beats) printStatus();
  }
  delay(1);
}
