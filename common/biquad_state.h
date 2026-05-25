#pragma once

#include "dsp_math.h"
#include "vec.h"

namespace dsp {

struct BiquadState {
  Vec2 x1{}, x2{}, y1{}, y2{};

  // Zero all state.
  void Clear() noexcept { x1 = x2 = y1 = y2 = Vec2{}; }

  // Evaluate the DF-I equation without updating state.
  Vec2 Compute(Vec2 x0, double b0, double b1, double b2, double a1,
               double a2) const noexcept {
    return x0 * b0 + x1 * b1 + x2 * b2 - y1 * a1 - y2 * a2;
  }

  // Commit one sample pair into state.
  void Advance(Vec2 x0, Vec2 y0) noexcept {
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
  }

  // Combined Compute + Advance (common case: no post-processing needed).
  Vec2 Tick(Vec2 x0, double b0, double b1, double b2, double a1,
            double a2) noexcept {
    const Vec2 y0 = Compute(x0, b0, b1, b2, a1, a2);
    Advance(x0, y0);
    return y0;
  }
};

// Flush each lane of a Vec2 to zero independently if it falls below the
// denormal threshold. Call after Compute() and before Advance() to prevent
// subnormal accumulation in the state registers.
inline Vec2 FlushDenormal(Vec2 y) noexcept {
  const double l =
      (y.l() > -kDenormalThreshold && y.l() < kDenormalThreshold) ? 0.0 : y.l();
  const double r =
      (y.r() > -kDenormalThreshold && y.r() < kDenormalThreshold) ? 0.0 : y.r();
  return Vec2{l, r};
}

}  // namespace dsp
