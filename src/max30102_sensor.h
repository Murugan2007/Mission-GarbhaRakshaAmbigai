#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "device_config.h"

struct PpgSample {
  uint32_t readTimeUs = 0;
  uint32_t red = 0;
  uint32_t ir = 0;
};

class Max30102Sensor {
public:
  bool begin() {
    Wire.begin(DeviceConfig::I2C_SDA_PIN,
               DeviceConfig::I2C_SCL_PIN);

    if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
      return false;
    }

    // Starting bench configuration only. Tune LED current and sample
    // settings after checking the exact breakout and observed waveforms.
    sensor.setup(
      0x1F,  // LED brightness
      1,     // sample averaging
      2,     // Red + IR
      DeviceConfig::PPG_SAMPLE_RATE_HZ,
      411,   // pulse width
      4096   // ADC range
    );

    sensor.setPulseAmplitudeRed(0x1F);
    sensor.setPulseAmplitudeIR(0x1F);
    sensor.setPulseAmplitudeGreen(0);

    return true;
  }

  // Returns one sample currently available in the library FIFO.
  // readTimeUs is the time the FIFO sample was read, NOT its precise
  // optical acquisition timestamp. Do not use it directly for PAT.
  bool readSample(PpgSample &out) {
    sensor.check();

    if (!sensor.available()) {
      return false;
    }

    out.red = sensor.getRed();
    out.ir = sensor.getIR();
    out.readTimeUs = micros();

    sensor.nextSample();
    return true;
  }

private:
  MAX30105 sensor;
};
