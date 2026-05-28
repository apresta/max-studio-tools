// This file is derived from the original Coils by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Coils.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "dsp_math.h"
#include "vec.h"

using dsp::Vec2;

Coils::Coils(double saturation, double coreDC)
    : saturation_(dsp::Clamp01(saturation)), core_dc_(dsp::Clamp01(coreDC)) {
  Prepare(44100.0);
}

void Coils::Prepare(double sample_rate) noexcept {
  assert(sample_rate > 0.0);
  sample_rate_ = (sample_rate > 0.0) ? sample_rate : 44100.0;

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

double Coils::GetSaturation() const noexcept { return saturation_; }

double Coils::GetCoreDC() const noexcept { return core_dc_; }

void Coils::Reset() noexcept { z1_ = z2_ = Vec2(0.0); }

void Coils::ProcessBlock(double* out_l, double* out_r,
                         int num_samples) noexcept {
  assert(num_samples >= 0);

  const double* in_l = out_l;
  const double* in_r = out_r;

  // output_compensation decreases as saturation increases (1 - A^2),
  // attenuating the output to compensate for the gain added by sin().
  // kMinBoost prevents division-by-zero at full saturation.
  double output_compensation = 1.0 - saturation_ * saturation_;
  output_compensation = std::max(output_compensation, kCoilsMinBoost);

  // drive_scale = 1 / output_compensation: feeds more signal into sin() as
  // saturation increases, making the distortion progressively harder.
  const double drive_scale = 1.0 / output_compensation;

  const double offset = core_dc_ * 2.0 - 1.0;
  const double sin_offset = std::sin(offset);

  // Store biquad coefficients into Vec2.
  const Vec2 v_b0(b0_);
  const Vec2 v_a1(a1_);
  const Vec2 v_a2(a2_);
  const Vec2 v_offset(offset);

  for (int i = 0; i < num_samples; ++i) {
    const double s_l = in_l[i];
    const double s_r = in_r[i];

    const Vec2 dry(s_l, s_r);

    // Bandpass biquad (Direct Form I).
    // Isolates the transformer resonance band before distortion.
    Vec2 temp = dry * v_b0 + z1_;
    z1_ = z2_ - (temp * v_a1);
    z2_ = dry * (-v_b0) - temp * v_a2;  // b2 = -b0
    const Vec2 band = temp;             // band = bandlimited signal

    // sin() distortion applied to the out-of-band (high-energy) content.
    // The band content bypasses distortion to preserve low-frequency detail.
    const Vec2 arg = (dry - band) * drive_scale + v_offset;
    const Vec2 sat{std::sin(arg.l()), std::sin(arg.r())};
    out_l[i] = band.l() + (sat.l() - sin_offset) * output_compensation;
    out_r[i] = band.r() + (sat.r() - sin_offset) * output_compensation;
  }
}
