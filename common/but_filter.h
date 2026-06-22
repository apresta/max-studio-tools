#pragma once

#include "biquad_coeffs.h"
#include "biquad_state.h"
#include "dsp_math.h"

namespace dsp {

enum class ButType { kLowPass, kHighPass };

class ButFilter {
 public:
  explicit ButFilter(ButType t) noexcept : type_(t) {}

  void Init(double freq_hz, double sr) noexcept {
    sr_ = sr;
    st_.Clear();
    ComputeCoeffs(freq_hz);
  }

  // Recompute coefficients without touching state.
  void SetFreq(double freq_hz) noexcept { ComputeCoeffs(freq_hz); }

  Vec2 Process(Vec2 sig) noexcept {
    return st_.Tick(sig, b0_, b1_, b2_, a1_, a2_);
  }

 private:
  // Recompute Butterworth biquad coefficients for freq_hz.
  // Delegates to the RBJ-cookbook biquad at Q = kSqrt2_2 = 1/sqrt(2),
  // which gives the maximally-flat (Butterworth) response.
  // BiquadLowPass/BiquadHighPass handle the Nyquist-margin clamp.
  void ComputeCoeffs(double freq_hz) noexcept {
    double b[3];
    double a[3];
    if (type_ == ButType::kLowPass) {
      BiquadLowPass(freq_hz, kHalfSqrt2, sr_, b, a);
    } else {
      BiquadHighPass(freq_hz, kHalfSqrt2, sr_, b, a);
    }
    b0_ = b[0];
    b1_ = b[1];
    b2_ = b[2];
    a1_ = a[1];
    a2_ = a[2];
  }

  double sr_ = 44100.0;
  BiquadState st_;

  // Feedforward coefficients (b0/b1/b2 in DF-I notation).
  double b0_ = 0.0, b1_ = 0.0, b2_ = 0.0;

  // Feedback coefficients (a1/a2 in DF-I notation).
  double a1_ = 0.0, a2_ = 0.0;
  ButType type_;
};

}  // namespace dsp
