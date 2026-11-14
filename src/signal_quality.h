#pragma once
#include <Arduino.h>

// Placeholder quality gate. Do not interpret this as a clinically validated
// signal-quality index. Replace with documented, data-tested metrics.

struct SignalQuality {
  bool ecgLeadConnected = false;
  bool ppgContactPresent = false;
  bool motionAcceptable = false; // No IMU is currently included.
  float ecgScore = 0.0f;
  float ppgScore = 0.0f;
};

inline bool signalsPassEngineeringGate(const SignalQuality &q) {
  return q.ecgLeadConnected &&
         q.ppgContactPresent &&
         q.motionAcceptable &&
         q.ecgScore >= 0.70f &&
         q.ppgScore >= 0.70f;
}
