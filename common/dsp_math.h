#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dsp {

// Math constants.
static constexpr double kPi = M_PI;
static constexpr double kHalfPi = kPi / 2.0;
static constexpr double kSqrt2 = M_SQRT2;
static constexpr double kHalfSqrt2 = kSqrt2 / 2.0;
static constexpr double kSoftClipOutputScale = 0.944060876;
static constexpr double kSoftClipInputDrive =
    1.0592537255;  // 1/kSoftClipOutputScale

template <typename Fn>
void Apply(double* block, int num_frames, Fn fn) noexcept {
  for (int i = 0; i < num_frames; ++i) block[i] = fn(block[i]);
}

template <typename T>
constexpr T Clamp(T v, T lo, T hi) noexcept {
  return v < lo ? lo : (v > hi ? hi : v);
}

template <typename T>
constexpr T Clamp01(T v) noexcept {
  return Clamp(v, T(0), T(1));
}

constexpr double BipolarSquaredGain(double x) noexcept {
  const double t = x * 2.0;
  return t * t;
}

constexpr double DbLinearGain(double param) noexcept {
  return std::pow(10.0, ((param * 30.0) - 15.0) / 20.0);
}

constexpr double Sign(double x) noexcept { return x < 0.0 ? -1.0 : 1.0; }

constexpr void InvertPhase(double* l, double* r, int num_frames) noexcept {
  for (int i = 0; i < num_frames; ++i) {
    l[i] = -l[i];
    r[i] = -r[i];
  }
}

// Convert decibels to a linear amplitude ratio.
constexpr double DbToLinear(double db) noexcept {
  return std::pow(10.0, db / 20.0);
}

// Convert a linear amplitude ratio to decibels.
constexpr double LinearToDb(double linear) noexcept {
  return 20.0 * std::log10(linear);
}

inline double SoftClip(double x) noexcept {
  double a = std::abs(x) * kSoftClipInputDrive;
  a = std::min(a, kHalfPi);
  double y = std::sin(a);
  return std::copysign(y, x) * kSoftClipOutputScale;
}

}  // namespace dsp
