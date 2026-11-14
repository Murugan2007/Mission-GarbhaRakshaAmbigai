/*
  Pre-eclampsia Early-Warning Wearable — research prototype
  ESP32-C3 + MAX30102 bring-up sketch.

  This sketch currently tests MAX30102 detection and raw red/IR acquisition.
  ECG, PAT, calibration, BLE and the user interface are separate development
  modules and are not yet integrated into a validated application.

  Health/safety: research prototype only. Not a BP monitor or diagnostic tool.
*/

#include <Arduino.h>
#include "src/device_config.h"
#include "src/max30102_sensor.h"

Max30102Sensor ppg;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Pre-eclampsia wearable research prototype");
  Serial.println("MAX30102 raw acquisition bring-up");

  if (!ppg.begin()) {
    Serial.println("ERROR: MAX30102 was not detected.");
    Serial.println("Check power, ground, SDA and SCL.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("timestamp_us,red,ir");
}

void loop() {
  PpgSample sample;

  while (ppg.readSample(sample)) {
    Serial.print(sample.readTimeUs);
    Serial.print(',');
    Serial.print(sample.red);
    Serial.print(',');
    Serial.println(sample.ir);
  }
}
