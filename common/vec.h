#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

namespace dsp {

struct Vec2 {
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

  Vec2& operator/=(Vec2 o) noexcept {
    *this = *this / o;
    return *this;
  }

  Vec2& operator+=(double s) noexcept {
    *this = *this + s;
    return *this;
  }

  Vec2& operator-=(double s) noexcept {
    *this = *this - s;
    return *this;
  }

  Vec2& operator*=(double s) noexcept {
    *this = *this * s;
    return *this;
  }

  Vec2& operator/=(double s) noexcept {
    *this = *this / s;
    return *this;
  }
};

inline Vec2 operator+(double s, Vec2 v) noexcept { return Vec2(s) + v; }

inline Vec2 operator-(double s, Vec2 v) noexcept { return Vec2(s) - v; }

inline Vec2 operator*(double s, Vec2 v) noexcept { return Vec2(s) * v; }

inline Vec2 operator/(double s, Vec2 v) noexcept { return Vec2(s) / v; }

inline Vec2 Abs(Vec2 x) noexcept {
  return Vec2(std::abs(x.v[0]), std::abs(x.v[1]));
}

inline Vec2 Min(Vec2 a, Vec2 b) noexcept {
  return Vec2(a.v[0] < b.v[0] ? a.v[0] : b.v[0],
              a.v[1] < b.v[1] ? a.v[1] : b.v[1]);
}

inline Vec2 Max(Vec2 a, Vec2 b) noexcept {
  return Vec2(a.v[0] > b.v[0] ? a.v[0] : b.v[0],
              a.v[1] > b.v[1] ? a.v[1] : b.v[1]);
}

inline Vec2 Sqrt(Vec2 x) noexcept {
  return Vec2(std::sqrt(x.v[0]), std::sqrt(x.v[1]));
}

inline Vec2 Min(Vec2 a, double s) noexcept { return Min(a, Vec2(s)); }

inline Vec2 Max(Vec2 a, double s) noexcept { return Max(a, Vec2(s)); }

inline Vec2 Pow(Vec2 x, int n) noexcept {
  assert(n >= 0);
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
