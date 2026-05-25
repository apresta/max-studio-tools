#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(__SSE__)
#include <pmmintrin.h>
#endif

namespace dsp {

// Shared mathematical constants.
static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSqrt2 = 1.41421356237309504880;
static constexpr double kSqrt2_2 = 0.70710678118654752440;  // sqrt(2)/2

// Soft-clip ceiling and the corresponding make-up gain (1 / ceiling).
static constexpr double kSoftClipCeiling = 0.99999;

// Denormal-guard constants.
static constexpr double kDenormalThreshold = 1.0e-15;
static constexpr double kDenormalFloor = 1.0e-14;  // > kDenormalThreshold

// Utility functions.

// Clamp v to [0, 1]. Works for float and double.
template <typename T>
inline constexpr T Clamp01(T v) noexcept {
  return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
}

// Square-law (bipolar) taper: maps x in [0,1] to (2x)^2 in [0,4].
// Used for gain controls where perceptual linearity follows power rather
// than amplitude. Unity gain at x = 0.5.
inline constexpr double BipolarSquaredGain(double x) noexcept {
  const double t = x * 2.0;
  return t * t;
}

// Maps a [0,1] parameter to a linear amplitude gain over +/-15 dB.
// At 0.5 the result is exactly 1.0 (0 dB, unity gain).
inline constexpr double DbLinearGain(double param) noexcept {
  return std::pow(10.0, ((param * 30.0) - 15.0) / 20.0);
}

// Replace denormals with kDenormalFloor while preserving sign.
inline double ZapDenormal(double x) noexcept {
  return (std::fabs(x) < kDenormalThreshold) ? kDenormalFloor : x;
}

// Sign of x: returns +1.0 for x >= 0, -1.0 for x < 0.
inline double sign(double x) noexcept { return x < 0.0 ? -1.0 : 1.0; }

// Enable hardware flush-to-zero (FTZ) and denormals-are-zero (DAZ) on the
// calling thread. Call once at audio-thread startup (e.g. in dspsetup or
// the top of the audio callback) before any DSP runs.
inline void SetFlushToZero() noexcept {
#if defined(__aarch64__)
  uint64_t fpcr;
  __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
  fpcr |= (UINT64_C(1) << 24);  // FZ bit
  __asm__ volatile("msr fpcr, %0" ::"r"(fpcr));
#elif defined(__SSE__)
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#if defined(__SSE3__)
  _MM_SET_DENORMALS_ZERO_MODE(
      _MM_DENORMALS_ZERO_ON);  // DAZ: treat denormal inputs as zero
#endif
#endif
}

}  // namespace dsp
