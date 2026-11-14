#pragma once
#include <Arduino.h>
#include <math.h>

// Experimental PPG conditioner and local-extremum event candidate.
// It is not a validated pulse-foot detector. Motion, pressure, perfusion,
// ambient light and waveform morphology can cause false or missed events.

struct PpgProcessed {
  float ac = 0.0f;
  bool fingerPresent = false;
  bool footCandidate = false;
  uint32_t candidateTimestampUs = 0;
};

class PpgProcessing {
public:
  void reset() {
    baselineReady_ = false;
    dc_ = 0.0f;
    acFiltered_ = 0.0f;
    previous_ = 0.0f;
    previousSlope_ = 0.0f;
    lastCandidateUs_ = 0;
  }

  PpgProcessed update(uint32_t ir, uint32_t sampleTimestampUs) {
    PpgProcessed out;

    // This threshold is a starting point only; tune for the actual module.
    out.fingerPresent = ir > fingerThreshold_;

    if (!out.fingerPresent) {
      reset();
      return out;
    }

    if (!baselineReady_) {
      dc_ = static_cast<float>(ir);
      baselineReady_ = true;
    }

    dc_ += 0.01f * (static_cast<float>(ir) - dc_);
    const float ac = static_cast<float>(ir) - dc_;
    acFiltered_ += 0.25f * (ac - acFiltered_);

    const float slope = acFiltered_ - previous_;

    // A local minimum is only a candidate foot. Confirming a pulse
    // requires subsequent waveform context and quality checks.
    if (previousSlope_ <= 0.0f && slope > 0.0f &&
        (uint32_t)(sampleTimestampUs - lastCandidateUs_) >=
          minimumCandidateSpacingUs_) {
      out.footCandidate = true;
      out.candidateTimestampUs = sampleTimestampUs;
      lastCandidateUs_ = sampleTimestampUs;
    }

    previousSlope_ = slope;
    previous_ = acFiltered_;
    out.ac = acFiltered_;
    return out;
  }

private:
  static constexpr uint32_t fingerThreshold_ = 50000;
  static constexpr uint32_t minimumCandidateSpacingUs_ = 300000;

  bool baselineReady_ = false;
  float dc_ = 0.0f;
  float acFiltered_ = 0.0f;
  float previous_ = 0.0f;
  float previousSlope_ = 0.0f;
  uint32_t lastCandidateUs_ = 0;
};
