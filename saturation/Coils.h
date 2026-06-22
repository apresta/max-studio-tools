// This file is derived from the original Coils by Airwindows.
// Copyright (c) Airwindows (MIT license).

#pragma once

#include "vec.h"

namespace saturation {

class Coils {
 public:
  // Prepare must be called before processing.
  void Prepare(double sample_rate) noexcept;

  void SetSaturation(double value) noexcept;  // 0.0 - 1.0
  void SetCoreDC(double value) noexcept;      // 0.0 - 1.0  (0.5 = neutral)

  double GetSaturation() const noexcept { return saturation_; }
  double GetCoreDC() const noexcept { return core_dc_; }

  // Process one stereo block in-place.
  void ProcessBlock(double* left, double* right, int num_samples) noexcept;

  // Reset biquad state (call between unrelated streams).
  void Reset() noexcept;

 private:
  // Parameters, all in [0, 1].
  double saturation_{0.0};
  double core_dc_{0.5};

  double sample_rate_{44100.0};

  // Pre-computed 600 Hz bandpass biquad coefficients.
  // Recomputed in Prepare(); depends only on sample rate.
  double b0_{0.0};
  double a1_{0.0};
  double a2_{0.0};
  // b1 = 0, b2 = -b0  (Direct Form I bandpass)

  // Biquad delay registers. Lane 0 = L, lane 1 = R; lanes are independent.
  dsp::Vec2 z1_{0.0};
  dsp::Vec2 z2_{0.0};
};

}  // namespace saturation
