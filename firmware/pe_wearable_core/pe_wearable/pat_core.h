// pat_core.h - beat-timing core for the pre-eclampsia early-warning wearable.
//
// Portable C++ with no Arduino calls, so this exact file runs on the ESP32-C3
// and in the PC tests (tests/test_core.cpp).
//
//   ECG -> R-peak time    --+
//                           +--> PAT = pulse foot time - R-peak time
//   PPG -> pulse foot time --+
//
// PAT (pulse arrival time) is a trend signal. It only becomes mmHg after a
// calibration against a real cuff, and a constant timing bias (filter delays,
// the clock model below) is absorbed by that calibration.
#pragma once
#include <stdint.h>
#include <math.h>

namespace pat {

static const float kPi = 3.14159265358979f;

// ---------------------------------------------------------------------------
// Biquad filter (RBJ cookbook), transposed direct form II.
// ---------------------------------------------------------------------------
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

  void set(float B0, float B1, float B2, float A0, float A1, float A2) {
    b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0;
    a1 = A1 / A0; a2 = A2 / A0;
    z1 = z2 = 0;
  }
  void lowpass(float fs, float fc, float q = 0.70710678f) {
    float w = 2 * kPi * fc / fs, c = cosf(w), al = sinf(w) / (2 * q);
    set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
  }
  void highpass(float fs, float fc, float q = 0.70710678f) {
    float w = 2 * kPi * fc / fs, c = cosf(w), al = sinf(w) / (2 * q);
    set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
  }
  void notch(float fs, float f0, float q = 30.0f) {
    float w = 2 * kPi * f0 / fs, c = cosf(w), al = sinf(w) / (2 * q);
    set(1, -2 * c, 1, 1 + al, -2 * c, 1 - al);
  }
  float process(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
  // Start as if x0 had been the input forever, so a large DC offset (the
  // AD8232 idles near 1.5 V) causes no start-up transient. Returns the
  // steady-state output.
  float prime(float x0) {
    float y0 = x0 * (b0 + b1 + b2) / (1 + a1 + a2);
    z1 = y0 - b0 * x0;
    z2 = b2 * x0 - a2 * y0;
    return y0;
  }
};

// ---------------------------------------------------------------------------
// ECG R-peak detector: Pan-Tompkins style detection, then the R time is
// refined on a lightly filtered copy with parabolic interpolation.
// Search-back and multi-lead logic are left out on purpose (single lead).
// ---------------------------------------------------------------------------
class EcgDetector {
 public:
  struct Beat {
    int64_t t_us;   // refined R-peak time (device clock, microseconds)
    float amp_mv;   // |R| amplitude on the lightly filtered signal
  };

  void begin(float fs, float mains_hz = 50.0f, float min_amp_mv = 40.0f) {
    fs_ = fs;
    minAmp_ = min_amp_mv;
    useNotch_ = (mains_hz > 0.0f && mains_hz < 0.45f * fs);
    if (useNotch_) notch_.notch(fs, mains_hz);
    hp_.highpass(fs, 0.5f);
    lp_.lowpass(fs, 40.0f);
    bpHp_.highpass(fs, 5.0f);
    bpLp_.lowpass(fs, 15.0f);
    mwiN_ = (int)(0.150f * fs + 0.5f);
    if (mwiN_ < 1) mwiN_ = 1;
    if (mwiN_ > MWI_MAX) mwiN_ = MWI_MAX;
    warmN_ = (int)(2.0f * fs);
    reset();
  }

  void reset() {
    primed_ = false;
    n_ = 0;
    rp_ = 0; ringN_ = 0;
    for (int i = 0; i < MWI_MAX; i++) mwiBuf_[i] = 0;
    mwiPos_ = 0; mwiSum_ = 0;
    b1_ = b2_ = 0;
    m1_ = m2_ = 0; tPrev_ = 0;
    warmMax_ = 0;
    spki_ = npki_ = thr_ = 0;
    lastPeakT_ = -REFRACT_US * 4;
    lastAcceptT_ = 0;
    beats_ = 0; avgAmp_ = 0;
  }

  // Feed one sample (millivolts) with its timestamp. Returns true when a beat
  // was confirmed; the beat is written to `out`. A beat is confirmed roughly
  // 100-150 ms after the R wave, because the detector waits for the end of the QRS.
  bool push(int64_t t, float x, Beat& out) {
    if (!primed_) {
      float v = x;
      if (useNotch_) v = notch_.prime(v);
      v = hp_.prime(v);
      v = lp_.prime(v);
      v = bpHp_.prime(v);
      bpLp_.prime(v);
      primed_ = true;
    }
    float v = useNotch_ ? notch_.process(x) : x;
    float xl = lp_.process(hp_.process(v));            // 0.5-40 Hz, used to time the R peak
    float b = bpLp_.process(bpHp_.process(xl));        // 5-15 Hz, used to detect the QRS
    float d = b - b2_;                                 // derivative over two samples
    b2_ = b1_;
    b1_ = b;
    float s = d * d;
    mwiSum_ += (double)s - (double)mwiBuf_[mwiPos_];   // moving-window integral
    mwiBuf_[mwiPos_] = s;
    if (++mwiPos_ >= mwiN_) mwiPos_ = 0;
    float m0 = (float)(mwiSum_ / mwiN_);
    if (m0 < 0) m0 = 0;

    ringT_[rp_] = t;
    ringV_[rp_] = xl;
    rp_ = (rp_ + 1) & (RING - 1);
    if (ringN_ < RING) ringN_++;
    n_++;

    bool found = false;
    if (n_ <= warmN_) {
      if (m0 > warmMax_) warmMax_ = m0;
      if (n_ == warmN_) {
        spki_ = 0.5f * warmMax_;
        npki_ = 0.05f * warmMax_;
        thr_ = npki_ + 0.25f * (spki_ - npki_);
        lastAcceptT_ = t;
      }
    } else if (m1_ > m2_ && m1_ >= m0) {
      // m1_ was a local maximum of the integrated signal
      float pk = m1_;
      int64_t tPk = tPrev_;
      if (pk > thr_) {
        if (tPk - lastPeakT_ >= REFRACT_US) {
          Beat bt;
          if (refine(tPk, bt)) {
            spki_ = 0.125f * pk + 0.875f * spki_;
            lastPeakT_ = tPk;
            lastAcceptT_ = t;
            out = bt;
            found = true;
          } else {
            npki_ = 0.125f * pk + 0.875f * npki_;   // looked like a beat, was not (T wave, artifact)
          }
        }
      } else {
        npki_ = 0.125f * pk + 0.875f * npki_;
        if (t - lastAcceptT_ > STALL_US) spki_ *= 0.7f;  // threshold too high: let it fall
      }
      thr_ = npki_ + 0.25f * (spki_ - npki_);
    }
    m2_ = m1_;
    m1_ = m0;
    tPrev_ = t;
    return found;
  }

 private:
  enum { RING = 128, MWI_MAX = 64 };
  static const int64_t REFRACT_US = 250000;   // no second beat within 250 ms
  static const int64_t STALL_US = 2500000;
  static const int64_t WIN_US = 250000;       // R search window before the integrator peak

  int ringPos(int k) const { return (rp_ - 1 - k) & (RING - 1); }   // k = 0 is the newest sample

  bool refine(int64_t tPk, Beat& bt) {
    int kBest = -1;
    float vBest = 0;
    for (int k = 0; k < ringN_; k++) {
      int i = ringPos(k);
      int64_t t = ringT_[i];
      if (t > tPk) continue;
      if (t < tPk - WIN_US) break;
      float a = fabsf(ringV_[i]);
      if (a > vBest) { vBest = a; kBest = k; }
    }
    if (kBest < 0) return false;
    float amp = vBest;
    if (amp < minAmp_) return false;
    if (beats_ >= 3 && amp < 0.4f * avgAmp_) return false;

    int64_t tR = ringT_[ringPos(kBest)];
    if (kBest >= 1 && kBest + 1 < ringN_) {
      int iN = ringPos(kBest - 1), iC = ringPos(kBest), iO = ringPos(kBest + 1);   // newer, centre, older
      float ya = fabsf(ringV_[iO]), yb = fabsf(ringV_[iC]), yc = fabsf(ringV_[iN]);
      float den = ya - 2 * yb + yc;
      if (den < -1e-6f) {
        float delta = 0.5f * (ya - yc) / den;      // vertex offset, -0.5..0.5 samples
        if (delta > 0.5f) delta = 0.5f;
        if (delta < -0.5f) delta = -0.5f;
        float halfSpan = 0.5f * (float)(ringT_[iN] - ringT_[iO]);
        tR += (int64_t)(delta * halfSpan);
      }
    }
    avgAmp_ = (beats_ == 0) ? amp : 0.8f * avgAmp_ + 0.2f * amp;
    beats_++;
    bt.t_us = tR;
    bt.amp_mv = amp;
    return true;
  }

  float fs_ = 250, minAmp_ = 40;
  bool useNotch_ = true, primed_ = false;
  Biquad notch_, hp_, lp_, bpHp_, bpLp_;
  int mwiN_ = 37, mwiPos_ = 0, warmN_ = 500, n_ = 0;
  float mwiBuf_[MWI_MAX];
  double mwiSum_ = 0;
  float b1_ = 0, b2_ = 0, m1_ = 0, m2_ = 0, warmMax_ = 0;
  float spki_ = 0, npki_ = 0, thr_ = 0, avgAmp_ = 0;
  int beats_ = 0;
  int64_t tPrev_ = 0, lastPeakT_ = 0, lastAcceptT_ = 0;
  int64_t ringT_[RING];
  float ringV_[RING];
  int rp_ = 0, ringN_ = 0;
};

// ---------------------------------------------------------------------------
// Pulse (PPG) foot finder.
// For one heartbeat it smooths the pulse, finds the steepest upstroke, fits a
// straight line to the 20-80 % part of the rise, and returns the time where
// that line meets the baseline (the "foot", via the intersecting-tangent idea).
// The MAX30102 raw IR value DROPS when blood volume rises, so the default is
// to invert the signal first.
// ---------------------------------------------------------------------------
class PpgFootFinder {
 public:
  struct Foot {
    int64_t t_us;   // foot time
    float amp;      // pulse height in sensor counts
    float slope;    // upstroke slope in counts per second
  };

  void begin(bool invert, float minAmp) {
    invert_ = invert;
    minAmp_ = minAmp;
    h_ = 0;
    n_ = 0;
  }

  void push(int64_t t, float raw) {
    t_[h_] = t;
    v_[h_] = invert_ ? -raw : raw;
    h_ = (h_ + 1) & (RING - 1);
    if (n_ < RING) n_++;
  }

  // Find the foot for the beat whose R peak is at tR, using samples from 150 ms
  // before tR up to tEnd.
  //   1. smooth, then find the steepest part of the upstroke;
  //   2. fit a straight line (the "tangent") through the whole steep part
  //      (every sample whose slope is at least half the maximum);
  //   3. walk back to where the rise begins, and fit a straight, possibly
  //      sloping, baseline line through the ~120 ms just before it;
  //   4. the foot is where the two lines cross.
  // A sloping baseline matters: breathing moves the baseline by hundreds of
  // counts per second, as much as a small pulse.
  bool find(int64_t tR, int64_t tEnd, Foot& out) const {
    float ts[MAXW], v[MAXW], p[MAXW], sl[MAXW];
    int n = 0;
    const int64_t tStart = tR - 150000;
    for (int k = 0; k < n_; k++) {
      int i = (h_ - n_ + k) & (RING - 1);          // oldest first
      int64_t t = t_[i];
      if (t < tStart) continue;
      if (t > tEnd) break;
      if (n >= MAXW) return fail(1);
      ts[n] = (float)(t - tR) * 1e-6f;
      v[n] = v_[i];
      n++;
    }
    if (n < 60) return fail(2);
    if (ts[0] > -0.11f) return fail(3);              // start of the window is missing (data hole)

    // 5-point binomial smoothing; linear phase, so no timing shift.
    p[0] = v[0]; p[1] = v[1]; p[n - 2] = v[n - 2]; p[n - 1] = v[n - 1];
    for (int i = 2; i < n - 2; i++)
      p[i] = (v[i - 2] + 4 * v[i - 1] + 6 * v[i] + 4 * v[i + 1] + v[i + 2]) * (1.0f / 16.0f);
    for (int i = 0; i < n; i++) sl[i] = 0;
    for (int i = 4; i < n - 4; i++) sl[i] = (p[i + 2] - p[i - 2]) / (ts[i + 2] - ts[i - 2]);

    // Steepest upstroke between 60 ms and the end of the window.
    float tHi = ts[n - 1] - 0.06f;
    if (tHi > 0.50f) tHi = 0.50f;
    int iS = -1;
    float slMax = 0;
    for (int i = 4; i < n - 4; i++) {
      if (ts[i] < 0.06f || ts[i] > tHi) continue;
      if (sl[i] > slMax) { slMax = sl[i]; iS = i; }
    }
    if (iS < 0 || slMax <= 0) return fail(4);
    if (ts[iS] > tHi - 0.01f) return fail(5);                         // upstroke cut by the window edge

    // The steep part: contiguous samples with slope >= half the maximum.
    int i1 = iS, i2 = iS;
    while (i1 > 4 && sl[i1 - 1] >= 0.5f * slMax) i1--;
    while (i2 < n - 5 && sl[i2 + 1] >= 0.5f * slMax) i2++;
    if (i2 - i1 < 4) return fail(6);
    // Noise makes the single steepest sample jump around by a few samples, and a
    // fit window that is lopsided about the steepest point tilts the tangent.
    // So centre the window on the slope-weighted middle of the steep part.
    float wsum = 0, isum = 0;
    for (int i = i1; i <= i2; i++) {
      float w = sl[i] - 0.5f * slMax;
      if (w > 0) { wsum += w; isum += w * (float)i; }
    }
    int ic = (wsum > 0) ? (int)(isum / wsum + 0.5f) : iS;
    int h = ic - i1;
    if (i2 - ic < h) h = i2 - ic;
    if (h < 2) return fail(6);
    i1 = ic - h;
    i2 = ic + h;
    float m1, tm1, pm1, rms1;
    if (!lsLine(ts, p, i1, i2, m1, tm1, pm1, rms1)) return fail(7);
    if (m1 <= 0) return fail(8);

    // Where the rise begins.
    int io = i1;
    while (io > 6 && sl[io - 1] > 0.12f * slMax) io--;

    // Baseline: straight line through the ~120 ms (24 samples) before the rise.
    int ib2 = io - 3, ib1 = ib2 - 23;
    if (ib1 < 2) ib1 = 2;
    if (ib2 - ib1 < 11) return fail(9);
    float m0, tm0, pm0, rms0;
    if (!lsLine(ts, p, ib1, ib2, m0, tm0, pm0, rms0)) return fail(10);
    if (m1 - m0 < 0.5f * slMax) return fail(11);

    // Pulse peak just after the steepest point.
    int jEnd = iS;
    while (jEnd < n - 3 && ts[jEnd] < ts[iS] + 0.30f) jEnd++;
    int iPk = iS;
    for (int j = iS; j <= jEnd; j++)
      if (p[j] > p[iPk]) iPk = j;
    float amp = p[iPk] - (pm0 + m0 * (ts[iPk] - tm0));
    if (amp < minAmp_) return fail(12);

    // Quality: straight steep part, quiet baseline, sensible slopes.
    if (rms1 > 0.06f * amp) return fail(13);
    if (rms0 > 0.08f * amp) return fail(14);
    if (fabsf(m0) > 0.35f * m1) return fail(15);

    float tf = ((pm0 - m0 * tm0) - (pm1 - m1 * tm1)) / (m1 - m0);
    if (tf < ts[io] - 0.04f || tf > ts[iS]) return fail(16);

    out.t_us = tR + (int64_t)(tf * 1e6f);
    out.amp = amp;
    out.slope = m1;
    return true;
  }

  // Why the last find() said no (0 = it did not); handy in tests.
  int lastFail() const { return fail_; }

  int64_t latest() const { return n_ ? t_[(h_ - 1) & (RING - 1)] : 0; }

 private:
  enum { RING = 512, MAXW = 200 };   // RING must be a power of two: indices wrap with & (RING - 1)

  // Least-squares line y = ym + m (t - tm) through points i0..i1; also the rms residual.
  static bool lsLine(const float* t, const float* y, int i0, int i1, float& m, float& tm, float& ym, float& rms) {
    int cnt = i1 - i0 + 1;
    if (cnt < 3) return false;
    tm = 0; ym = 0;
    for (int j = i0; j <= i1; j++) { tm += t[j]; ym += y[j]; }
    tm /= cnt; ym /= cnt;
    float sxx = 0, sxy = 0;
    for (int j = i0; j <= i1; j++) {
      sxx += (t[j] - tm) * (t[j] - tm);
      sxy += (t[j] - tm) * (y[j] - ym);
    }
    if (sxx <= 0) return false;
    m = sxy / sxx;
    float ss = 0;
    for (int j = i0; j <= i1; j++) {
      float e = y[j] - (ym + m * (t[j] - tm));
      ss += e * e;
    }
    rms = sqrtf(ss / cnt);
    return true;
  }

  bool fail(int code) const { fail_ = code; return false; }
  mutable int fail_ = 0;
  bool invert_ = true;
  float minAmp_ = 40;
  int64_t t_[RING];
  float v_[RING];
  int h_ = 0, n_ = 0;
};

// ---------------------------------------------------------------------------
// Maps MAX30102 FIFO sample numbers to device time.
// The sensor has its own clock and the chip is read in bursts, so "time of
// reading" is wrong by up to one sample period, and the sensor clock drifts a
// little against the ESP32 clock. After every FIFO read, the newest sample is
// less than one period old, which gives a noisy but unbiased time for that
// sample number; an alpha-beta filter smooths those and tracks the drift.
// ---------------------------------------------------------------------------
class PpgClock {
 public:
  void begin(float nominalHz, double alpha = 0.02, double beta = 0.0001, double gapUs = 30000.0) {
    periodUs_ = 1e6 / nominalHz;
    alpha_ = alpha; beta_ = beta; gapUs_ = gapUs;
    init_ = false;
    o_ = 0; r_ = 0; idxRef_ = 0;
  }

  // Call right after reading the FIFO. idxNewest = running number of the newest
  // sample just read (first sample is 0). Returns true if the timing jumped
  // (lost samples, FIFO overflow): the clock was re-anchored.
  bool onRead(int64_t tReadUs, int64_t idxNewest) {
    double y = (double)tReadUs - (double)idxNewest * periodUs_ - 0.5 * periodUs_;
    if (!init_) {
      o_ = y; r_ = 0; idxRef_ = idxNewest; init_ = true;
      return false;
    }
    double dIdx = (double)(idxNewest - idxRef_);
    double pred = o_ + r_ * dIdx;
    double e = y - pred;
    if (fabs(e) > gapUs_) {
      o_ = y; r_ = 0; idxRef_ = idxNewest;
      return true;
    }
    o_ = pred + alpha_ * e;
    if (dIdx > 0) r_ += (beta_ / dIdx) * e;
    idxRef_ = idxNewest;
    return false;
  }

  int64_t stamp(int64_t idx) const {
    double t = o_ + r_ * (double)(idx - idxRef_) + (double)idx * periodUs_;
    return (int64_t)(t + (t >= 0 ? 0.5 : -0.5));
  }

  // Estimated sensor sample period in microseconds (nominal + drift).
  double periodUs() const { return periodUs_ + r_; }

 private:
  double periodUs_ = 5000, alpha_ = 0.02, beta_ = 0.0001, gapUs_ = 30000;
  double o_ = 0, r_ = 0;
  int64_t idxRef_ = 0;
  bool init_ = false;
};

// ---------------------------------------------------------------------------
// PatEngine: ties the two together. Feed ECG samples and PPG samples (in any
// interleaving); read back one Result per heartbeat about 0.6 s after the R wave.
// ---------------------------------------------------------------------------
class PatEngine {
 public:
  struct Result {
    int64_t t_r_us;   // R-peak time
    float rr_ms;      // time since the previous R peak (0 for the first beat)
    float pat_ms;     // R peak to pulse foot (0 when not valid)
    float ppg_amp;    // pulse height in sensor counts
    float ecg_amp;    // R amplitude in mV
    bool valid;
  };

  void begin(float ecgFs, float mainsHz, bool invertPpg, float minPpgAmp, float minEcgAmpMv) {
    ecg_.begin(ecgFs, mainsHz, minEcgAmpMv);
    ppg_.begin(invertPpg, minPpgAmp);
    pendN_ = 0;
    resHead_ = resTail_ = 0;
    lastR_ = 0;
    gapT_ = -(((int64_t)1) << 60);
    tLatest_ = 0;
  }

  void pushEcg(int64_t t, float mv) {
    EcgDetector::Beat b;
    if (ecg_.push(t, mv, b)) {
      float rr = (lastR_ > 0) ? (float)(b.t_us - lastR_) * 1e-3f : 0.0f;
      lastR_ = b.t_us;
      if (pendN_ == PEND) {                        // PPG side is not keeping up
        emit(pend_[0], 0, 0, false);
        shiftPending();
      }
      pend_[pendN_].t = b.t_us;
      pend_[pendN_].rr = rr;
      pend_[pendN_].amp = b.amp_mv;
      pendN_++;
      process();
    }
  }

  void pushPpg(int64_t t, float raw) {
    ppg_.push(t, raw);
    tLatest_ = t;
    process();
  }

  // Tell the engine the PPG timeline jumped around time t (lost samples).
  void markPpgGap(int64_t t) { gapT_ = t; }

  bool pop(Result& r) {
    if (resHead_ == resTail_) return false;
    r = res_[resTail_ & (RES - 1)];
    resTail_++;
    return true;
  }

 private:
  enum { PEND = 8, RES = 16 };
  static const int64_t WAIT_US = 550000;   // wait this long after R before judging the pulse

  struct Pend { int64_t t; float rr; float amp; };

  void shiftPending() {
    for (int i = 1; i < pendN_; i++) pend_[i - 1] = pend_[i];
    pendN_--;
  }

  void emit(const Pend& p, float patMs, float ppgAmp, bool valid) {
    Result r;
    r.t_r_us = p.t;
    r.rr_ms = p.rr;
    r.pat_ms = patMs;
    r.ppg_amp = ppgAmp;
    r.ecg_amp = p.amp;
    r.valid = valid;
    res_[resHead_ & (RES - 1)] = r;
    resHead_++;
    if (resHead_ - resTail_ > RES) resTail_ = resHead_ - RES;   // drop the oldest
  }

  void process() {
    while (pendN_ > 0 && tLatest_ >= pend_[0].t + WAIT_US) {
      Pend p = pend_[0];
      int64_t tEnd = p.t + WAIT_US;
      if (pendN_ > 1) {
        int64_t lim = pend_[1].t + 80000;          // the next beat's foot cannot come earlier than this
        if (lim < tEnd) tEnd = lim;
      }
      bool gap = (gapT_ > p.t - 100000 && gapT_ < p.t + WAIT_US + 50000);
      PpgFootFinder::Foot f;
      bool ok = false;
      float patMs = 0;
      if (!gap && ppg_.find(p.t, tEnd, f)) {
        patMs = (float)(f.t_us - p.t) * 1e-3f;
        ok = (patMs >= 60.0f && patMs <= 500.0f);
      }
      emit(p, ok ? patMs : 0.0f, ok ? f.amp : 0.0f, ok);
      shiftPending();
    }
  }

  EcgDetector ecg_;
  PpgFootFinder ppg_;
  Pend pend_[PEND];
  int pendN_ = 0;
  Result res_[RES];
  uint32_t resHead_ = 0, resTail_ = 0;
  int64_t lastR_ = 0, gapT_ = 0, tLatest_ = 0;
};

// ---------------------------------------------------------------------------
// TrendTracker: decides which beats go into the trend, and keeps the rolling
// median PAT. A beat is kept when it is valid, its heart rate is believable,
// its PAT is within 30 ms of the median of the last 15 kept beats, and its
// pulse height is within 0.4-2.5x the median height. If six beats in a row are
// refused, the person's state has really changed, so the history is restarted.
// ---------------------------------------------------------------------------
class TrendTracker {
 public:
  void begin() { n_ = 0; head_ = 0; refusedRun_ = 0; kept_ = 0; }

  bool update(const PatEngine::Result& r) {
    if (!r.valid) return false;
    if (r.rr_ms < 350.0f || r.rr_ms > 1700.0f) return false;     // also drops the very first beat (rr = 0)
    if (n_ >= 8) {
      float medPat = median(pat_, n_), medAmp = median(amp_, n_);
      bool ok = fabsf(r.pat_ms - medPat) <= 30.0f && r.ppg_amp >= 0.4f * medAmp && r.ppg_amp <= 2.5f * medAmp;
      if (!ok) {
        if (++refusedRun_ < 6) return false;
        n_ = 0; head_ = 0;                                         // restart the history
      }
    }
    refusedRun_ = 0;
    pat_[head_] = r.pat_ms;
    amp_[head_] = r.ppg_amp;
    head_ = (head_ + 1) % N;
    if (n_ < N) n_++;
    kept_++;
    return true;
  }

  float medianPat() const { return n_ ? median(pat_, n_) : 0.0f; }
  int keptTotal() const { return kept_; }
  int recentCount() const { return n_; }

 private:
  enum { N = 15 };
  static float median(const float* a, int n) {
    float t[N];
    for (int i = 0; i < n; i++) t[i] = a[i];
    for (int i = 1; i < n; i++) {                                  // insertion sort, n <= 15
      float x = t[i];
      int j = i - 1;
      while (j >= 0 && t[j] > x) { t[j + 1] = t[j]; j--; }
      t[j + 1] = x;
    }
    return (n & 1) ? t[n / 2] : 0.5f * (t[n / 2 - 1] + t[n / 2]);
  }
  float pat_[N], amp_[N];
  int n_ = 0, head_ = 0, refusedRun_ = 0, kept_ = 0;
};

}  // namespace pat
