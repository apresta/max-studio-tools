#pragma once

#include "biquad_state.h"
#include "vec.h"

namespace dsp {

class DynamicBiquad {
 public:
  void Clear() noexcept { st_.Clear(); }

  void SetCoeffs(double b0, double b1, double b2, double a1,
                 double a2) noexcept {
    b0_ = b0;
    b1_ = b1;
    b2_ = b2;
    a1_ = a1;
    a2_ = a2;
  }

  void SetPassthrough() noexcept { SetCoeffs(1.0, 0.0, 0.0, 0.0, 0.0); }

  // Process num_frames stereo frames.
  void ProcessBlock(double* l, double* r, int num_frames) noexcept {
    for (int i = 0; i < num_frames; ++i) {
      const Vec2 x0{l[i], r[i]};
      const Vec2 y0 = st_.Compute(x0, b0_, b1_, b2_, a1_, a2_);
      l[i] = y0.L();
      r[i] = y0.R();
      st_.Advance(x0, y0);
    }
  }

 private:
  double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0;
  double a1_ = 0.0, a2_ = 0.0;

  BiquadState st_;
};

}  // namespace dsp
