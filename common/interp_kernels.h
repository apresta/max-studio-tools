#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp {

inline constexpr double kClampLimit = 0.99999988;

constexpr double ClampSymmetric(double x) noexcept {
  return std::max(std::min(x, kClampLimit), -kClampLimit);
}

// Linear interpolation over a table covering the normalized range [0, 1].
inline double InterpolateLin(double x,
                             const std::vector<double>& table) noexcept {
  x = ClampSymmetric(x);
  x = std::max(x, 0.0);

  const int table_size = static_cast<int>(table.size());
  x *= (table_size - 1);

  const int index_int = static_cast<int>(std::floor(x));
  if (index_int >= table_size - 1) return table[table_size - 1];

  const double index_frac = x - index_int;
  return (table[index_int] * (1.0 - index_frac)) +
         (table[index_int + 1] * index_frac);
}

// Logarithmic-domain interpolation.
inline double InterpolateExp(double x, const std::vector<double>& table,
                             bool is_neg) noexcept {
  const int offset = is_neg ? 23 : 0;

  x = ClampSymmetric(x);

  if (x == 0.0) return table[offset + 23];

  int exp_val;
  double mant = std::frexp(x, &exp_val);

  if (std::abs(mant) == 1.0) {
    mant /= 2.0;
    exp_val += 1;
  }

  int index = 1 - exp_val;
  if (index < 0) exp_val = 0;

  double frac;
  if (index > 22) {
    frac = std::ldexp(mant, 22 + exp_val);
    index = 23;
  } else {
    frac = (mant <= 0.0) ? (mant + 0.5) * 2.0 : (mant - 0.5) * 2.0;
  }

  if (x < 0.0 && is_neg) {
    index = -index;
    frac += 1.0;
  }

  return (frac * (table[offset + index] - table[offset + index + 1])) +
         table[offset + index + 1];
}

}  // namespace dsp
