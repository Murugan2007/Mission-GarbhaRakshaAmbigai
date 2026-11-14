#pragma once
#include <Arduino.h>

// Data-model scaffold only. Persistence, versioning, checksums, timestamps,
// review UI and atomic writes still need to be implemented and tested.
// Do not use this scaffold for clinical decisions.

struct CalibrationReference {
  bool valid = false;
  float patMedianMs = 0.0f;
  float patMadMs = 0.0f;
  uint32_t acceptedAt = 0; // Placeholder until real time is available.
};

struct CalibrationStore {
  CalibrationReference original;
  CalibrationReference active;
  CalibrationReference candidate;

  bool hasCandidate() const {
    return candidate.valid;
  }

  // Candidate acceptance must be an explicit, informed review action.
  // The original reference is deliberately never overwritten here.
  bool approveCandidateAfterReview() {
    if (!candidate.valid) return false;
    active = candidate;
    candidate = CalibrationReference{};
    return true;
  }

  void rejectCandidate() {
    candidate = CalibrationReference{};
  }

  void clearCandidateOnly() {
    candidate = CalibrationReference{};
  }
};
