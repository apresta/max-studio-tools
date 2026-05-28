#pragma once

#include <cmath>

#include "biquad_state.h"
#include "dsp_math.h"

namespace dsp {

enum class ButType { LowPass, HighPass };

struct ButFilter {
  double sr_ = 44100.0;
  BiquadState st_;

  // Feedforward coefficients (b0/b1/b2 in DF-I notation).
  double b0_ = 0.0, b1_ = 0.0, b2_ = 0.0;

  // Feedback coefficients (a1/a2 in DF-I notation).
  double a1_ = 0.0, a2_ = 0.0;
  ButType type_;

  explicit ButFilter(ButType t) noexcept : type_(t) {}

  // Recompute Butterworth biquad coefficients for freq_hz.
  void ComputeCoeffs(double freq_hz) noexcept {
    const double ny = sr_ * 0.49;
    if (freq_hz < 1.0) freq_hz = 1.0;
    if (freq_hz > ny) freq_hz = ny;

    // LP uses cot(w_0/2); HP uses tan(w_0/2).
    const double c = (type_ == ButType::LowPass)
                         ? 1.0 / std::tan((kPi / sr_) * freq_hz)
                         : std::tan((kPi / sr_) * freq_hz);
    const double c2 = c * c;

    const double inv_a0 = 1.0 / (1.0 + kSqrt2 * c + c2);
    b0_ = b2_ = inv_a0;
    b1_ = (type_ == ButType::LowPass) ? 2.0 * inv_a0 : -2.0 * inv_a0;
    a1_ = (type_ == ButType::LowPass) ? b1_ * (1.0 - c2)
                                      : 2.0 * inv_a0 * (c2 - 1.0);
    a2_ = inv_a0 * (1.0 - kSqrt2 * c + c2);
  }

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
};

}  // namespace dsp
