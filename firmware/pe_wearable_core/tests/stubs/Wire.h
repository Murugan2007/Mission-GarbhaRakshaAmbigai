// A fake MAX30102 on a fake I2C bus: register map, FIFO pointers and SpO2-mode data layout
// (3 bytes RED then 3 bytes IR per sample), fed from an array of IR samples.
#pragma once
#include "Arduino.h"
#include <vector>

struct Max30102Sim {
  std::vector<uint32_t> ir;     // every IR sample the sensor will ever produce
  double tOffUs = 0, periodUs = 5000;
  int64_t readCount = 0;        // samples the host has taken out of the FIFO
  uint8_t regs[256] = {0};
  int64_t available() const {   // samples complete (plus 400 us of processing) by now
    double k = (double)(g_simNowUs - 400) - tOffUs;
    if (k < 0) return 0;
    int64_t a = (int64_t)(k / periodUs) + 1;
    return a > (int64_t)ir.size() ? (int64_t)ir.size() : a;
  }
};
extern Max30102Sim g_max;

class TwoWire {
 public:
  void begin(int, int) {}
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t) { n_ = 0; }
  size_t write(uint8_t b) { if (n_ < 4) tx_[n_++] = b; return 1; }
  uint8_t endTransmission(bool stop = true) {
    if (n_ >= 2 && stop) {      // register write
      uint8_t r = tx_[0], v = tx_[1];
      g_max.regs[r] = v;
      if (r == 0x04) g_max.readCount = g_max.available();        // pointer reset: FIFO emptied
      if (r == 0x09 && (v & 0x40)) g_max.regs[r] = 0;            // soft reset completes at once
    } else if (n_ == 1) {
      ptr_ = tx_[0];
    }
    return 0;
  }
  uint8_t requestFrom(uint8_t, uint8_t n) {
    rx_.clear();
    for (uint8_t i = 0; i < n; i++) {
      rx_.push_back(readByte());
      if (ptr_ != 0x07) ptr_++;      // registers auto-increment, the FIFO data register does not
    }
    pos_ = 0;
    return n;
  }
  int read() { return pos_ < rx_.size() ? rx_[pos_++] : -1; }

 private:
  uint8_t readByte() {
    int64_t avail = g_max.available(), unread = avail - g_max.readCount;
    switch (ptr_) {
      case 0xFF: return 0x15;
      case 0x04: return (uint8_t)(avail & 31);
      case 0x05: return unread >= 32 ? (uint8_t)(unread - 31 > 31 ? 31 : unread - 31) : 0;
      case 0x06: return (uint8_t)(g_max.readCount & 31);
      case 0x07: {
        // each byte of FIFO data advances through the sample being read
        uint32_t v = 0;
        int64_t i = g_max.readCount;
        if (i < (int64_t)g_max.ir.size()) v = g_max.ir[i];
        uint8_t b = 0;
        switch (sub_) {
          case 0: case 1: case 2: b = 0; break;                  // RED (LED off): zeros
          case 3: b = (v >> 16) & 3; break;
          case 4: b = (v >> 8) & 255; break;
          default: b = v & 255; break;
        }
        if (++sub_ == 6) { sub_ = 0; g_max.readCount++; }
        return b;
      }
      default: return g_max.regs[ptr_];
    }
  }
  uint8_t tx_[4], n_ = 0, ptr_ = 0, sub_ = 0;
  std::vector<uint8_t> rx_;
  size_t pos_ = 0;
};
extern TwoWire Wire;
