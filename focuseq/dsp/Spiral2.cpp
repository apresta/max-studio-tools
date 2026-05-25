// This file is derived from the original Spiral2 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Spiral2.h"

#include <cmath>

#include "dsp_math.h"

namespace {

// Apply sin(x*|x|)/|x| saturation to two independent channels.
inline void SaturateScalar(double& L, double& R, double g) noexcept {
  L *= g;
  R *= g;

  const double aL = std::fabs(L);
  const double aR = std::fabs(R);

  if (aL > 0.0) L = std::sin(L * aL) / aL;
  if (aR > 0.0) R = std::sin(R * aR) / aR;
}

}  // namespace

void Spiral2::ProcessBlock(double* left, double* right,
                           int num_frames) noexcept {
  if (!left || !right || num_frames <= 0) return;

  // Square-law taper: g = (2*gain)^2
  const double t = gain_ * 2.0;
  const double g = t * t;

  for (int i = 0; i < num_frames; ++i) {
    double L = dsp::ZapDenormal(left[i]);
    double R = dsp::ZapDenormal(right[i]);

    SaturateScalar(L, R, g);

    left[i] = L;
    right[i] = R;
  }
}
