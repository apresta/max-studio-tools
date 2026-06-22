#pragma once

// Canonical one-pole (1st-order, 6 dB/oct) filter coefficient formulas.
//
// Normalized DF-I coefficients, using the same b/a letter convention as
// biquad_coeffs.h/biquad_state.h (b = feedforward, a = feedback,
// subtracted):
//   y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]

#include <cmath>

#include "biquad_coeffs.h"
#include "dsp_math.h"

namespace dsp {

struct OnePoleCoeffs {
  double b0 = 1.0, b1 = 0.0, a1 = 0.0;
};

// 1st-order highpass via the bilinear transform with tan() prewarping.
inline OnePoleCoeffs ComputeOnePoleHPCoeffs(double freq,
                                            double sample_rate) noexcept {
  freq = ClampFreq(freq, sample_rate);
  const double w0 = freq * 2.0 * kPi / sample_rate;
  const double k = w0 / std::tan(w0 * 0.5);
  const double denom_inv = 1.0 / (w0 + k);

  OnePoleCoeffs c;
  c.b0 = k * denom_inv;
  c.b1 = -k * denom_inv;
  c.a1 = (w0 - k) * denom_inv;
  return c;
}

// 1st-order lowpass via the bilinear transform with tan() prewarping.
inline OnePoleCoeffs ComputeOnePoleLPCoeffs(double freq,
                                            double sample_rate) noexcept {
  freq = ClampFreq(freq, sample_rate);
  const double w0 = freq * 2.0 * kPi / sample_rate;
  const double k = w0 / std::tan(w0 * 0.5);
  const double denom_inv = 1.0 / (w0 + k);

  OnePoleCoeffs c;
  c.b0 = w0 * denom_inv;
  c.b1 = w0 * denom_inv;
  c.a1 = (w0 - k) * denom_inv;
  return c;
}

}  // namespace dsp
