// Minimal stand-ins for the Arduino-ESP32 API, so the sketch can be built and run on a PC
// against synthetic signals (tests/sketch_sim.cpp). This checks the sketch's own logic;
// it does NOT check that the real ESP32 headers accept these calls.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define INPUT 0
#define ADC_11db 3
#define configMAX_PRIORITIES 25
#define pdMS_TO_TICKS(ms) ((uint32_t)(ms))
typedef uint32_t TickType_t;

extern int64_t g_simNowUs;
extern float g_simAdcMv;
extern int g_simLoP, g_simLoN;

inline uint32_t millis() { return (uint32_t)(g_simNowUs / 1000); }
inline void delay(uint32_t) {}
inline void pinMode(int, int) {}
inline int digitalRead(int pin) { return pin == 4 ? g_simLoP : g_simLoN; }
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline uint32_t analogReadMilliVolts(int) { return (uint32_t)(g_simAdcMv + 0.5f); }
inline TickType_t xTaskGetTickCount() { return 0; }
inline void vTaskDelayUntil(TickType_t*, TickType_t) {}
inline int xTaskCreate(void (*)(void*), const char*, uint32_t, void*, int, void*) { return 1; }

struct SerialStub {
  void begin(long) {}
  int available() { return 0; }
  int read() { return -1; }
  void println(const char* s) { printf("%s\n", s); }
  void print(const char* s) { printf("%s", s); }
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list ap; va_start(ap, fmt); int n = vprintf(fmt, ap); va_end(ap); return n;
  }
};
extern SerialStub Serial;
