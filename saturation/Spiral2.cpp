// This file is derived from the original Spiral2 by Airwindows.
// Copyright (c) Airwindows (MIT license).

#include "Spiral2.h"

#include <cmath>

#include "vec.h"

namespace saturation {

namespace {

// Apply sin(x*|x|)/|x| saturation to a stereo pair.
inline dsp::Vec2 SaturateVec(dsp::Vec2 s, double g) noexcept {
  s = s * g;
  const dsp::Vec2 a = dsp::Abs(s);
  const dsp::Vec2 afloor =
      dsp::Max(a, 1e-18);  // prevent /0; sin(x)/x -> 1 near 0
  const dsp::Vec2 arg = s * afloor;
  return dsp::Vec2{std::sin(arg.L()), std::sin(arg.R())} / afloor;
}

}  // namespace

void Spiral2::ProcessBlock(double* left, double* right,
                           int num_samples) const noexcept {
  // Square-law taper: g = (2*gain)^2
  const double t = gain_ * 2.0;
  const double g = t * t;

  for (int i = 0; i < num_samples; ++i) {
    const dsp::Vec2 dry{left[i], right[i]};
    const dsp::Vec2 wet = SaturateVec(dry, g);
    left[i] = wet.L();
    right[i] = wet.R();
  }
}

}  // namespace saturation
