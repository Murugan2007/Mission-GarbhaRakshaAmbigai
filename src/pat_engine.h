#pragma once
#include <Arduino.h>

// PAT event-pairing scaffold. Both timestamps MUST already be mapped into
// the same validated clock domain. Do not pass MAX30102 FIFO read-time
// timestamps as if they were optical sample times.

namespace PatEngine {
  constexpr uint32_t MIN_PAT_US = 80000;   // engineering screen only
  constexpr uint32_t MAX_PAT_US = 500000;  // engineering screen only
  constexpr uint8_t EVENT_CAPACITY = 8;
  constexpr uint8_t HISTORY_SIZE = 9;

  struct RPeakEvent {
    uint32_t timestampUs = 0;
    bool valid = false;
    bool used = false;
  };

  struct PatResult {
    bool valid = false;
    uint32_t patUs = 0;
    float rollingMedianMs = 0.0f;
  };

  class Engine {
  public:
    void addRPeak(uint32_t timestampUs, bool signalGood) {
      peaks_[nextPeak_] = {timestampUs, signalGood, false};
      nextPeak_ = (nextPeak_ + 1) % EVENT_CAPACITY;
    }

    PatResult addPpgFoot(uint32_t footTimestampUs,
                         bool signalGood) {
      PatResult result;
      if (!signalGood) return result;

      int best = -1;
      uint32_t smallestAge = UINT32_MAX;

      for (uint8_t i = 0; i < EVENT_CAPACITY; ++i) {
        auto &peak = peaks_[i];
        if (!peak.valid || peak.used) continue;

        // Unsigned subtraction is wrap-safe for intervals below 2^31 us.
        const int32_t signedAge =
          static_cast<int32_t>(footTimestampUs - peak.timestampUs);

        if (signedAge < 0) continue;

        const uint32_t age = static_cast<uint32_t>(signedAge);
        if (age >= MIN_PAT_US && age <= MAX_PAT_US &&
            age < smallestAge) {
          smallestAge = age;
          best = i;
        }
      }

      if (best < 0) return result;

      peaks_[best].used = true;
      history_[historyIndex_] = smallestAge;
      historyIndex_ = (historyIndex_ + 1) % HISTORY_SIZE;
      if (historyCount_ < HISTORY_SIZE) ++historyCount_;

      result.valid = true;
      result.patUs = smallestAge;
      result.rollingMedianMs = medianMs();
      return result;
    }

    void clearRollingHistory() {
      historyCount_ = 0;
      historyIndex_ = 0;
    }

  private:
    float medianMs() const {
      if (historyCount_ == 0) return 0.0f;

      uint32_t sorted[HISTORY_SIZE];
      for (uint8_t i = 0; i < historyCount_; ++i) {
        sorted[i] = history_[i];
      }

      for (uint8_t i = 1; i < historyCount_; ++i) {
        const uint32_t value = sorted[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && sorted[j] > value) {
          sorted[j + 1] = sorted[j];
          --j;
        }
        sorted[j + 1] = value;
      }

      if (historyCount_ & 1U) {
        return sorted[historyCount_ / 2] / 1000.0f;
      }

      const uint8_t mid = historyCount_ / 2;
      return (static_cast<float>(sorted[mid - 1]) +
              static_cast<float>(sorted[mid])) / 2000.0f;
    }

    RPeakEvent peaks_[EVENT_CAPACITY]{};
    uint32_t history_[HISTORY_SIZE]{};
    uint8_t nextPeak_ = 0;
    uint8_t historyCount_ = 0;
    uint8_t historyIndex_ = 0;
  };
}
