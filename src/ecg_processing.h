#pragma once
#include <Arduino.h>
#include <math.h>

// Experimental local-maximum detector.
// Validate against annotated ECG recordings before using its events.
// A simple amplitude detector is not robust to all morphologies or noise.

struct EcgProcessed {
  float filtered = 0.0f;
  bool rPeakCandidate = false;
  uint32_t rPeakTimestampUs = 0;
};

class EcgProcessing {
public:
  void reset() {
    baselineReady_ = false;
    baseline_ = 0.0f;
    filtered_ = 0.0f;
    olderMagnitude_ = 0.0f;
    previousMagnitude_ = 0.0f;
    previousTimestampUs_ = 0;
    lastPeakUs_ = 0;
    envelope_ = 0.0f;
  }

  EcgProcessed update(uint16_t raw, uint32_t timestampUs) {
    EcgProcessed out;

    if (!baselineReady_) {
      baseline_ = raw;
      baselineReady_ = true;
    }

    baseline_ += 0.002f * (static_cast<float>(raw) - baseline_);
    const float highPass = static_cast<float>(raw) - baseline_;
    filtered_ += 0.35f * (highPass - filtered_);

    const float magnitude = fabsf(filtered_);
    envelope_ *= 0.995f;
    if (magnitude > envelope_) envelope_ = magnitude;

    const float threshold = fmaxf(minAmplitude_, envelope_ * 0.35f);

    if (previousMagnitude_ > olderMagnitude_ &&
        previousMagnitude_ >= magnitude &&
        previousMagnitude_ > threshold &&
        (lastPeakUs_ == 0 ||
         (uint32_t)(previousTimestampUs_ - lastPeakUs_) >= refractoryUs_)) {
      out.rPeakCandidate = true;
      out.rPeakTimestampUs = previousTimestampUs_;
      lastPeakUs_ = previousTimestampUs_;
    }

    olderMagnitude_ = previousMagnitude_;
    previousMagnitude_ = magnitude;
    previousTimestampUs_ = timestampUs;

    out.filtered = filtered_;
    return out;
  }

private:
  static constexpr float minAmplitude_ = 50.0f; // ADC counts; tune on bench
  static constexpr uint32_t refractoryUs_ = 250000;

  bool baselineReady_ = false;
  float baseline_ = 0.0f;
  float filtered_ = 0.0f;
  float olderMagnitude_ = 0.0f;
  float previousMagnitude_ = 0.0f;
  float envelope_ = 0.0f;
  uint32_t previousTimestampUs_ = 0;
  uint32_t lastPeakUs_ = 0;
};
