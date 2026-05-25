#pragma once

#include "biquad_state.h"
#include "vec.h"

namespace dsp {

struct DynamicBiquad {
  // Coefficient storage.
  double b0_{1.0}, b1_{0.0}, b2_{0.0};
  double a1_{0.0}, a2_{0.0};

  BiquadState st_;

  // Zero all state.
  void Clear() noexcept { st_.Clear(); }

  // Coefficient update.
  void SetCoeffs(double b0, double b1, double b2, double a1,
                 double a2) noexcept {
    b0_ = b0;
    b1_ = b1;
    b2_ = b2;
    a1_ = a1;
    a2_ = a2;
  }

  // Reset to unity-gain passthrough.
  void SetPassthrough() noexcept { SetCoeffs(1.0, 0.0, 0.0, 0.0, 0.0); }

  // Process num_frames stereo frames stored as interleaved doubles:
  //   buf[0] = L[0], buf[1] = R[0], buf[2] = L[1], ...
  void ProcessBlock(double* buf, int num_frames) noexcept {
    for (int i = 0; i < num_frames; ++i) {
      const Vec2 x0{buf[2 * i], buf[2 * i + 1]};
      const Vec2 y0 = FlushDenormal(st_.Compute(x0, b0_, b1_, b2_, a1_, a2_));
      buf[2 * i] = y0.l();
      buf[2 * i + 1] = y0.r();
      st_.Advance(x0, y0);
    }
  }
};

}  // namespace dsp
