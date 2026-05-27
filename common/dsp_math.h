#pragma once

#include <cmath>
#include <cstddef>

namespace dsp {

// Math constants.
static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSqrt2 = 1.41421356237309504880;
static constexpr double kSqrt2_2 = 0.70710678118654752440;  // sqrt(2)/2

// Hard ceiling used before asin() to stay within its [-1, 1] domain.
static constexpr double kSoftClipCeiling = 0.99999;

template <typename T>
inline constexpr T Clamp01(T v) noexcept {
  return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
}

inline constexpr double BipolarSquaredGain(double x) noexcept {
  const double t = x * 2.0;
  return t * t;
}

inline constexpr double DbLinearGain(double param) noexcept {
  return std::pow(10.0, ((param * 30.0) - 15.0) / 20.0);
}

inline double sign(double x) noexcept { return x < 0.0 ? -1.0 : 1.0; }

}  // namespace dsp
