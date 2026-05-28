// This file is derived from the original Coils by Airwindows.
// Copyright (c) Airwindows (MIT license)

#pragma once

#include "vec.h"

// Coils transformer resonance frequency and Q.
static constexpr double kCoilsBandHz = 600.0;
static constexpr double kCoilsBandQ = 0.023;

// Minimum boost floor (prevents divide-by-zero in drive_scale).
static constexpr double kCoilsMinBoost = 0.001;

class Coils {
 public:
  explicit Coils(double saturation = 0.0, double coreDC = 0.5);

  // Prepare must be called (or the sample_rate constructor argument used)
  // before processing.
  void Prepare(double sample_rate) noexcept;

  void SetSaturation(double value) noexcept;  // 0.0 - 1.0
  void SetCoreDC(double value) noexcept;      // 0.0 - 1.0  (0.5 = neutral)

  double GetSaturation() const noexcept;
  double GetCoreDC() const noexcept;

  // Process one stereo block in-place.
  void ProcessBlock(double* out_l, double* out_r, int num_samples) noexcept;

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
  dsp::Vec2 z1_;
  dsp::Vec2 z2_;
};
