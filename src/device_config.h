#pragma once
#include <Arduino.h>

// Verify every pin against the exact ESP32-C3 SuperMini board revision.
namespace DeviceConfig {
  constexpr uint8_t ECG_OUTPUT_PIN = 3;
  constexpr uint8_t ECG_LO_PLUS_PIN = 4;
  constexpr uint8_t ECG_LO_MINUS_PIN = 10;

  constexpr uint8_t I2C_SDA_PIN = 6;
  constexpr uint8_t I2C_SCL_PIN = 7;

  constexpr uint8_t BUTTON_PIN = 5;

  constexpr uint32_t ECG_SAMPLE_RATE_HZ = 250;
  constexpr uint32_t ECG_SAMPLE_INTERVAL_US =
      1000000UL / ECG_SAMPLE_RATE_HZ;

  constexpr uint32_t PPG_SAMPLE_RATE_HZ = 200;
}
