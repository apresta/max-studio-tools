#pragma once

#include "vec.h"

namespace dsp {

struct BiquadState {
  Vec2 x1{}, x2{}, y1{}, y2{};

  void Clear() noexcept { x1 = x2 = y1 = y2 = Vec2{}; }

  Vec2 Compute(Vec2 x0, double b0, double b1, double b2, double a1,
               double a2) const noexcept {
    return x0 * b0 + x1 * b1 + x2 * b2 - y1 * a1 - y2 * a2;
  }

  void Advance(Vec2 x0, Vec2 y0) noexcept {
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
  }

  Vec2 Tick(Vec2 x0, double b0, double b1, double b2, double a1,
            double a2) noexcept {
    const Vec2 y0 = Compute(x0, b0, b1, b2, a1, a2);
    Advance(x0, y0);
    return y0;
  }
};

}  // namespace dsp
