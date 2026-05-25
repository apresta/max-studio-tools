#pragma once
#include <cstddef>

#if !defined(DSP_SCALAR)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse2.h"
#define DSP_USE_SIMDE 1
#endif

namespace dsp {

struct Vec2 {

#if defined(DSP_USE_SIMDE)

  __m128d v;

  Vec2() noexcept : v(_mm_setzero_pd()) {}

  explicit Vec2(__m128d x) noexcept : v(x) {}

  // Construct from two scalars: a -> lane 0 (L), b -> lane 1 (R).
  Vec2(double a, double b) noexcept : v(_mm_set_pd(b, a)) {}

  // Broadcast a single scalar to both lanes.
  explicit Vec2(double s) noexcept : v(_mm_set1_pd(s)) {}

  double l() const noexcept { return _mm_cvtsd_f64(v); }

  double r() const noexcept {
    double t;
    _mm_storeh_pd(&t, v);
    return t;
  }

  Vec2 operator+(Vec2 o) const noexcept { return Vec2(_mm_add_pd(v, o.v)); }

  Vec2 operator-(Vec2 o) const noexcept { return Vec2(_mm_sub_pd(v, o.v)); }

  Vec2 operator*(Vec2 o) const noexcept { return Vec2(_mm_mul_pd(v, o.v)); }

  Vec2 operator/(Vec2 o) const noexcept { return Vec2(_mm_div_pd(v, o.v)); }

  Vec2 operator-() const noexcept {
    return Vec2(_mm_sub_pd(_mm_setzero_pd(), v));
  }

#else  // DSP_SCALAR: plain C++, compiler should auto-vectorize

  double v[2] = {0.0, 0.0};

  Vec2() noexcept = default;

  Vec2(double a, double b) noexcept : v{a, b} {}

  explicit Vec2(double s) noexcept : v{s, s} {}

  double L() const noexcept { return v[0]; }

  double R() const noexcept { return v[1]; }

  Vec2 operator+(Vec2 o) const noexcept {
    return {v[0] + o.v[0], v[1] + o.v[1]};
  }

  Vec2 operator-(Vec2 o) const noexcept {
    return {v[0] - o.v[0], v[1] - o.v[1]};
  }

  Vec2 operator*(Vec2 o) const noexcept {
    return {v[0] * o.v[0], v[1] * o.v[1]};
  }

  Vec2 operator/(Vec2 o) const noexcept {
    return {v[0] / o.v[0], v[1] / o.v[1]};
  }

  Vec2 operator-() const noexcept { return {-v[0], -v[1]}; }

#endif

  // Scalar-broadcast operators (identical for all platforms).
  Vec2 operator+(double s) const noexcept { return *this + Vec2(s); }

  Vec2 operator-(double s) const noexcept { return *this - Vec2(s); }

  Vec2 operator*(double s) const noexcept { return *this * Vec2(s); }

  Vec2 operator/(double s) const noexcept { return *this / Vec2(s); }

  Vec2& operator+=(Vec2 o) noexcept {
    *this = *this + o;
    return *this;
  }

  Vec2& operator-=(Vec2 o) noexcept {
    *this = *this - o;
    return *this;
  }

  Vec2& operator*=(Vec2 o) noexcept {
    *this = *this * o;
    return *this;
  }

  Vec2& operator*=(double s) noexcept {
    *this = *this * s;
    return *this;
  }
};

// Free functions (scalar on left, Vec2 on right).
inline Vec2 operator+(double s, Vec2 v) noexcept { return Vec2(s) + v; }

inline Vec2 operator-(double s, Vec2 v) noexcept { return Vec2(s) - v; }

inline Vec2 operator*(double s, Vec2 v) noexcept { return Vec2(s) * v; }

inline Vec2 operator/(double s, Vec2 v) noexcept { return Vec2(s) / v; }

// Element-wise math free functions.

#if defined(DSP_USE_SIMDE)

// Element-wise absolute value.
inline Vec2 abs(Vec2 x) noexcept {
  const __m128d kSignMask = _mm_set1_pd(-0.0);
  return Vec2(_mm_andnot_pd(kSignMask, x.v));
}

// Element-wise minimum.
inline Vec2 min(Vec2 a, Vec2 b) noexcept { return Vec2(_mm_min_pd(a.v, b.v)); }

// Element-wise maximum.
inline Vec2 max(Vec2 a, Vec2 b) noexcept { return Vec2(_mm_max_pd(a.v, b.v)); }

#else  // DSP_SCALAR

inline Vec2 abs(Vec2 x) noexcept {
  return Vec2(std::fabs(x.v[0]), std::fabs(x.v[1]));
}

inline Vec2 min(Vec2 a, Vec2 b) noexcept {
  return Vec2(a.v[0] < b.v[0] ? a.v[0] : b.v[0],
              a.v[1] < b.v[1] ? a.v[1] : b.v[1]);
}

inline Vec2 max(Vec2 a, Vec2 b) noexcept {
  return Vec2(a.v[0] > b.v[0] ? a.v[0] : b.v[0],
              a.v[1] > b.v[1] ? a.v[1] : b.v[1]);
}

#endif

// Clamp both lanes against a scalar bound.
inline Vec2 min(Vec2 a, double s) noexcept { return min(a, Vec2(s)); }

inline Vec2 max(Vec2 a, double s) noexcept { return max(a, Vec2(s)); }

// Element-wise square root.
#if defined(DSP_USE_SIMDE)
inline Vec2 sqrt(Vec2 x) noexcept { return Vec2(_mm_sqrt_pd(x.v)); }
#else
inline Vec2 sqrt(Vec2 x) noexcept {
  return Vec2(std::sqrt(x.v[0]), std::sqrt(x.v[1]));
}
#endif

// Exponentiation by squaring: returns x^n, n >= 1.
inline Vec2 pow_int(Vec2 x, int n) noexcept {
  Vec2 result(1.0);
  Vec2 base = x;
  while (n > 0) {
    if (n & 1) result = result * base;
    base = base * base;
    n >>= 1;
  }
  return result;
}

}  // namespace dsp
