#pragma once
#include <Arduino.h>
#include "device_config.h"

struct EcgSample {
  uint32_t timestampUs = 0;
  uint16_t raw = 0;
  bool leadOff = true;
};

class EcgSensor {
public:
  void begin() {
    pinMode(DeviceConfig::ECG_LO_PLUS_PIN, INPUT);
    pinMode(DeviceConfig::ECG_LO_MINUS_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(DeviceConfig::ECG_OUTPUT_PIN, ADC_11db);
  }

  EcgSample read() {
    EcgSample sample;
    sample.timestampUs = micros();
    sample.leadOff =
      (digitalRead(DeviceConfig::ECG_LO_PLUS_PIN) == HIGH) ||
      (digitalRead(DeviceConfig::ECG_LO_MINUS_PIN) == HIGH);

    if (!sample.leadOff) {
      sample.raw = analogRead(DeviceConfig::ECG_OUTPUT_PIN);
    }
    return sample;
  }
};
