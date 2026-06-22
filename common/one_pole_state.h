#pragma once

#include "vec.h"

namespace dsp {

struct OnePoleState {
  Vec2 x1, y1;

  void Clear() noexcept { x1 = y1 = Vec2{}; }

  Vec2 Compute(Vec2 x0, double b0, double b1, double a1) const noexcept {
    return x0 * b0 + x1 * b1 - y1 * a1;
  }

  void Advance(Vec2 x0, Vec2 y0) noexcept {
    x1 = x0;
    y1 = y0;
  }

  Vec2 Tick(Vec2 x0, double b0, double b1, double a1) noexcept {
    const Vec2 y0 = Compute(x0, b0, b1, a1);
    Advance(x0, y0);
    return y0;
  }
};

}  // namespace dsp
