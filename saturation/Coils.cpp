// This file is derived from the original Coils by Airwindows.
// Copyright (c) Airwindows (MIT license).

#include "Coils.h"

#include <algorithm>
#include <cmath>

#include "dsp_math.h"
#include "vec.h"

namespace saturation {

namespace {

// Coils transformer resonance frequency and Q.
constexpr double kCoilsBandHz = 600.0;
constexpr double kCoilsBandQ = 0.023;

// Minimum boost floor (prevents divide-by-zero in drive_scale).
constexpr double kCoilsMinBoost = 0.001;

}  // namespace

using dsp::Vec2;

void Coils::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;

  // 600 Hz bandpass biquad; models transformer core resonance.
  const double freq = kCoilsBandHz / sample_rate_;
  const double k = std::tan(dsp::kPi * freq);
  const double norm = 1.0 / (1.0 + k / kCoilsBandQ + k * k);
  b0_ = k / kCoilsBandQ * norm;
  a1_ = 2.0 * (k * k - 1.0) * norm;
  a2_ = (1.0 - k / kCoilsBandQ + k * k) * norm;
  // b1 = 0, b2 = -b0  (standard bandpass Direct Form I)

  Reset();
}

void Coils::SetSaturation(double value) noexcept {
  saturation_ = dsp::Clamp01(value);
}

void Coils::SetCoreDC(double value) noexcept { core_dc_ = dsp::Clamp01(value); }

void Coils::Reset() noexcept { z1_ = z2_ = Vec2{0.0}; }

void Coils::ProcessBlock(double* left, double* right,
                         int num_samples) noexcept {
  // output_compensation decreases as saturation increases (1 - A^2),
  // attenuating the output to compensate for the gain added by sin().
  // kCoilsMinBoost prevents division-by-zero at full saturation.
  double output_compensation = 1.0 - (saturation_ * saturation_);
  output_compensation = std::max(output_compensation, kCoilsMinBoost);

  // drive_scale = 1 / output_compensation: feeds more signal into sin() as
  // saturation increases.
  const double drive_scale = 1.0 / output_compensation;

  const double offset = (core_dc_ * 2.0) - 1.0;
  const double sin_offset = std::sin(offset);

  // Store biquad coefficients into Vec2.
  const Vec2 v_b0{b0_};
  const Vec2 v_a1{a1_};
  const Vec2 v_a2{a2_};
  const Vec2 v_offset{offset};

  for (int i = 0; i < num_samples; ++i) {
    const Vec2 dry{left[i], right[i]};

    // Bandpass biquad (Direct Form I).
    // Isolates the transformer resonance band before distortion.
    Vec2 temp = dry * v_b0 + z1_;
    z1_ = z2_ - (temp * v_a1);
    z2_ = dry * (-v_b0) - temp * v_a2;  // b2 = -b0
    const Vec2 band = temp;             // band = bandlimited signal

    // sin() distortion applied to the out-of-band (high-energy) content.
    const Vec2 arg = (dry - band) * drive_scale + v_offset;
    const Vec2 sat{std::sin(arg.L()), std::sin(arg.R())};
    left[i] = band.L() + ((sat.L() - sin_offset) * output_compensation);
    right[i] = band.R() + ((sat.R() - sin_offset) * output_compensation);
  }
}

}  // namespace saturation
