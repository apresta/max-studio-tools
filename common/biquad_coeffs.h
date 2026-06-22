#pragma once

// Canonical biquad EQ coefficient formulas.
//
// Normalized DF-I coefficients:
//   b[3]  -- feedforward (b0, b1, b2)
//   a[3]  -- feedback    (a0=1, a1, a2)   [a0 is always written as 1.0]

#include <algorithm>
#include <cmath>

#include "dsp_math.h"

namespace dsp {

// Clamp f to the open interval (1 Hz, Nyquist * 0.499).
constexpr double ClampFreq(double f, double sr) noexcept {
  const double ny = sr * 0.499;
  f = std::max(f, 1.0);
  f = std::min(f, ny);
  return f;
}

// Set b / a to the unity-gain all-pass (identity) biquad.
constexpr void BiquadPassthrough(double* b, double* a) noexcept {
  b[0] = 1.0;
  b[1] = 0.0;
  b[2] = 0.0;
  a[0] = 1.0;
  a[1] = 0.0;
  a[2] = 0.0;
}

// Peaking (bell) EQ biquad.
// gain_db > 0 boosts, < 0 cuts. Q sets bandwidth.
inline void BiquadPeak(double freq, double gain_db, double Q, double sr,
                       double* b, double* a) noexcept {
  freq = ClampFreq(freq, sr);
  Q = std::max(Q, 0.01);

  const double amp = std::pow(10.0, gain_db / 40.0);  // sqrt(linear gain)
  const double w0 = 2.0 * kPi * freq / sr;
  const double cos_w0 = std::cos(w0);
  const double alpha = std::sin(w0) / (2.0 * Q);

  const double a0_inv = 1.0 / (1.0 + alpha / amp);
  b[0] = (1.0 + alpha * amp) * a0_inv;
  b[1] = (-2.0 * cos_w0) * a0_inv;
  b[2] = (1.0 - alpha * amp) * a0_inv;
  a[0] = 1.0;
  a[1] = b[1];  // a1 == b1 for peak EQ
  a[2] = (1.0 - alpha / amp) * a0_inv;
}

// Low-shelf biquad with Q-parameterized transition slope.
inline void BiquadLowShelf(double freq, double gain_db, double Q, double sr,
                           double* b, double* a) noexcept {
  freq = ClampFreq(freq, sr);
  Q = std::max(Q, 0.01);

  const double amp = std::pow(10.0, gain_db / 40.0);
  const double w0 = 2.0 * kPi * freq / sr;
  const double cos_w0 = std::cos(w0);
  const double sin_w0 = std::sin(w0);
  const double alpha = sin_w0 / (2.0 * Q);
  const double sqrt_amp = std::sqrt(amp);
  const double two_sq_alpha = 2.0 * sqrt_amp * alpha;

  const double ap1 = amp + 1.0;
  const double am1 = amp - 1.0;
  const double ap1c = ap1 * cos_w0;
  const double am1c = am1 * cos_w0;

  const double a0_inv = 1.0 / (ap1 + am1c + two_sq_alpha);
  b[0] = amp * (ap1 - am1c + two_sq_alpha) * a0_inv;
  b[1] = 2.0 * amp * (am1 - ap1c) * a0_inv;
  b[2] = amp * (ap1 - am1c - two_sq_alpha) * a0_inv;
  a[0] = 1.0;
  a[1] = -2.0 * (am1 + ap1c) * a0_inv;
  a[2] = (ap1 + am1c - two_sq_alpha) * a0_inv;
}

// 2nd-order Butterworth low-pass biquad (Audio EQ Cookbook).
// Q = 1/sqrt(2) ~ 0.707 gives a maximally-flat (Butterworth) response.
// Custom Q values raise or lower the resonance peak at the cutoff.
inline void BiquadLowPass(double freq, double Q, double sr, double* b,
                          double* a) noexcept {
  freq = ClampFreq(freq, sr);
  Q = std::max(Q, 0.01);

  const double w0 = 2.0 * kPi * freq / sr;
  const double cos_w0 = std::cos(w0);
  const double alpha = std::sin(w0) / (2.0 * Q);
  const double a0_inv = 1.0 / (1.0 + alpha);

  b[0] = (1.0 - cos_w0) * 0.5 * a0_inv;
  b[1] = (1.0 - cos_w0) * a0_inv;
  b[2] = (1.0 - cos_w0) * 0.5 * a0_inv;
  a[0] = 1.0;
  a[1] = -2.0 * cos_w0 * a0_inv;
  a[2] = (1.0 - alpha) * a0_inv;
}

// 2nd-order high-pass biquad (Audio EQ Cookbook).
// Q = 1/sqrt(2) ~ 0.707 gives a Butterworth response.
// The denominator is identical to BiquadLowPass; only the numerator differs.
inline void BiquadHighPass(double freq, double Q, double sr, double* b,
                           double* a) noexcept {
  freq = ClampFreq(freq, sr);
  Q = std::max(Q, 0.01);

  const double w0 = 2.0 * kPi * freq / sr;
  const double cos_w0 = std::cos(w0);
  const double alpha = std::sin(w0) / (2.0 * Q);
  const double a0_inv = 1.0 / (1.0 + alpha);

  b[0] = (1.0 + cos_w0) * 0.5 * a0_inv;
  b[1] = -(1.0 + cos_w0) * a0_inv;
  b[2] = (1.0 + cos_w0) * 0.5 * a0_inv;
  a[0] = 1.0;
  a[1] = -2.0 * cos_w0 * a0_inv;
  a[2] = (1.0 - alpha) * a0_inv;
}

// High-shelf biquad with Q-parameterized transition slope.
inline void BiquadHighShelf(double freq, double gain_db, double Q, double sr,
                            double* b, double* a) noexcept {
  freq = ClampFreq(freq, sr);
  Q = std::max(Q, 0.01);

  const double amp = std::pow(10.0, gain_db / 40.0);
  const double w0 = 2.0 * kPi * freq / sr;
  const double cos_w0 = std::cos(w0);
  const double sin_w0 = std::sin(w0);
  const double alpha = sin_w0 / (2.0 * Q);
  const double sqrt_amp = std::sqrt(amp);
  const double two_sq_alpha = 2.0 * sqrt_amp * alpha;

  const double ap1 = amp + 1.0;
  const double am1 = amp - 1.0;
  const double ap1c = ap1 * cos_w0;
  const double am1c = am1 * cos_w0;

  const double a0_inv = 1.0 / (ap1 - am1c + two_sq_alpha);
  b[0] = amp * (ap1 + am1c + two_sq_alpha) * a0_inv;
  b[1] = -2.0 * amp * (am1 + ap1c) * a0_inv;
  b[2] = amp * (ap1 + am1c - two_sq_alpha) * a0_inv;
  a[0] = 1.0;
  a[1] = 2.0 * (am1 - ap1c) * a0_inv;
  a[2] = (ap1 - am1c - two_sq_alpha) * a0_inv;
}

// Normalized DF-I biquad coefficients (b0, b1, b2 feedforward; a1, a2
// feedback). a0 is always 1 and is not stored.
// Default-constructed value is identity (passthrough).
struct BiquadCoeffs {
  double b0 = 1.0, b1 = 0.0, b2 = 0.0;
  double a1 = 0.0, a2 = 0.0;
};

// Returns LP biquad coefficients for a normalized frequency (0..0.5) and Q.
// Converts to Hz internally for BiquadLowPass.
inline BiquadCoeffs ComputeLPCoeffs(double norm_freq, double q,
                                    double sample_rate) noexcept {
  const double freq_hz = norm_freq * sample_rate;
  double b[3];
  double a[3];
  BiquadLowPass(freq_hz, q, sample_rate, b, a);
  return {b[0], b[1], b[2], a[1], a[2]};
}

// Returns HP biquad coefficients for a normalized frequency (0..0.5) and Q.
// Converts to Hz internally for BiquadHighPass.
inline BiquadCoeffs ComputeHPCoeffs(double norm_freq, double q,
                                    double sample_rate) noexcept {
  const double freq_hz = norm_freq * sample_rate;
  double b[3];
  double a[3];
  BiquadHighPass(freq_hz, q, sample_rate, b, a);
  return {b[0], b[1], b[2], a[1], a[2]};
}

}  // namespace dsp
