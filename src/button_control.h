#pragma once
#include <Arduino.h>
#include "device_config.h"

enum class ButtonEvent : uint8_t {
  None,
  ShortPress,
  PowerOn,
  PowerOff,
  ResetCalibration
};

class ButtonControl {
public:
  void begin() {
    pinMode(DeviceConfig::BUTTON_PIN, INPUT_PULLUP);
  }

  ButtonEvent update(bool deviceOn) {
    const uint32_t now = millis();
    const bool raw = digitalRead(DeviceConfig::BUTTON_PIN);

    if (raw != lastRaw_) {
      lastRaw_ = raw;
      lastChangeMs_ = now;
    }

    if ((uint32_t)(now - lastChangeMs_) >= debounceMs_ &&
        raw != stableState_) {
      stableState_ = raw;

      if (stableState_ == LOW) {
        pressedAtMs_ = now;
        longActionSent_ = false;
      } else {
        const uint32_t duration = now - pressedAtMs_;

        if (longActionSent_) {
          return ButtonEvent::None;
        }

        if (!deviceOn && duration >= powerHoldMs_) {
          return ButtonEvent::PowerOn;
        }

        if (deviceOn && duration >= powerHoldMs_) {
          return ButtonEvent::PowerOff;
        }

        if (deviceOn && duration <= shortPressMaxMs_) {
          return ButtonEvent::ShortPress;
        }
      }
    }

    if (stableState_ == LOW && deviceOn && !longActionSent_ &&
        (uint32_t)(now - pressedAtMs_) >= resetHoldMs_) {
      longActionSent_ = true;
      return ButtonEvent::ResetCalibration;
    }

    return ButtonEvent::None;
  }

private:
  static constexpr uint32_t debounceMs_ = 30;
  static constexpr uint32_t shortPressMaxMs_ = 1000;
  static constexpr uint32_t powerHoldMs_ = 5000;
  static constexpr uint32_t resetHoldMs_ = 10000;

  bool lastRaw_ = HIGH;
  bool stableState_ = HIGH;
  bool longActionSent_ = false;

  uint32_t lastChangeMs_ = 0;
  uint32_t pressedAtMs_ = 0;
};
