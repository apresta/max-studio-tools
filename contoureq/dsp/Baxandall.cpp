// This file is derived from the original Baxandall by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Baxandall.h"

#include <algorithm>
#include <cmath>

#include "biquad_coeffs.h"
#include "biquad_state.h"
#include "dsp_math.h"
#include "vec.h"

using dsp::Vec2;

Baxandall::Baxandall() { Reset(); }

void Baxandall::Prepare(double sample_rate) noexcept {
  sample_rate_ = (sample_rate > 0.0) ? sample_rate : 44100.0;
  coeffs_dirty_ = true;
  Reset();
}

void Baxandall::Reset() noexcept {
  h_coeffs_ = l_coeffs_ = {};
  h_state_a_.Clear();
  h_state_b_.Clear();
  l_state_a_.Clear();
  l_state_b_.Clear();
  flip_ = false;
}

void Baxandall::SetTrebleLevel(double value) noexcept {
  treble_level_ = dsp::Clamp01(value);
  coeffs_dirty_ = true;
}

void Baxandall::SetBassLevel(double value) noexcept {
  bass_level_ = dsp::Clamp01(value);
  coeffs_dirty_ = true;
}

void Baxandall::SetOutputLevel(double value) noexcept {
  output_level_ = dsp::Clamp01(value);
  coeffs_dirty_ = true;
}

void Baxandall::UpdateCoefficients() noexcept {
  // Treble crossover scales linearly with treble gain.
  const double treble_gain = dsp::DbLinearGain(treble_level_);
  const double treble_freq =
      std::min((4410.0 * treble_gain) / sample_rate_, 0.45);
  h_coeffs_ = dsp::ComputeLPCoeffs(treble_freq, kTrebleQ, sample_rate_);

  // Bass crossover scales inversely with bass gain.
  const double bass_gain = dsp::DbLinearGain(bass_level_);
  const double bass_freq = std::min((8820.0 / bass_gain) / sample_rate_, 0.45);
  l_coeffs_ = dsp::ComputeLPCoeffs(bass_freq, kBassQ, sample_rate_);

  coeffs_dirty_ = false;
}

void Baxandall::ProcessSample(double& left, double& right,
                              const CoeffVec2& cv) noexcept {
  const double s_l = left * cv.output_g.l();
  const double s_r = right * cv.output_g.r();

  // Console5 encode: sin() soft-clips the input, decode asin() inverts it.
  const Vec2 in{std::sin(s_l), std::sin(s_r)};

  // Interleaved biquad.
  dsp::BiquadState& hs = flip_ ? h_state_a_ : h_state_b_;
  dsp::BiquadState& ls = flip_ ? l_state_a_ : l_state_b_;

  // High-shelf = input minus LP output.
  const Vec2 treble =
      in - hs.Tick(in, cv.h.b0, cv.h.b1, cv.h.b2, cv.h.a1, cv.h.a2);
  // Bass = LP output (passed through as-is for low-shelf).
  const Vec2 bass = ls.Tick(in, cv.l.b0, cv.l.b1, cv.l.b2, cv.l.a1, cv.l.a2);

  flip_ = !flip_;

  // Apply independent band gains and sum.
  const Vec2 out = treble * cv.treble_g + bass * cv.bass_g;

  // Console5 decode: asin() inverts the encode sin(), restoring headroom.
  {
    const double cl = std::max(std::min(out.l(), dsp::kSoftClipCeiling),
                               -dsp::kSoftClipCeiling);
    const double cr = std::max(std::min(out.r(), dsp::kSoftClipCeiling),
                               -dsp::kSoftClipCeiling);
    left = std::asin(cl);
    right = std::asin(cr);
  }
}

void Baxandall::ProcessBlock(double* left, double* right,
                             int num_frames) noexcept {
  if (!left || !right || num_frames <= 0) return;

  if (coeffs_dirty_) UpdateCoefficients();

  // treble_g and bass_g are the independent dB-linear band gains.
  const CoeffVec2 cv{
      .h = h_coeffs_,
      .l = l_coeffs_,
      .output_g = Vec2(dsp::DbLinearGain(output_level_)),
      .treble_g = Vec2(dsp::DbLinearGain(treble_level_)),
      .bass_g = Vec2(dsp::DbLinearGain(bass_level_)),
  };

  for (int i = 0; i < num_frames; ++i) ProcessSample(left[i], right[i], cv);
}
