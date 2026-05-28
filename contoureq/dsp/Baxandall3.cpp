// This file is derived from the original Baxandall3 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Baxandall3.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "biquad_coeffs.h"
#include "biquad_state.h"
#include "dsp_math.h"
#include "vec.h"

using dsp::Vec2;

Baxandall3::Baxandall3() { Reset(); }

void Baxandall3::Prepare(double sample_rate) noexcept {
  sample_rate_ = (sample_rate > 0.0) ? sample_rate : 44100.0;
  coeffs_dirty_ = true;
  Reset();
}

void Baxandall3::Reset() noexcept {
  h_coeffs_ = l_coeffs_ = {};
  h_state_a_.Clear();
  h_state_b_.Clear();
  l_state_a_.Clear();
  l_state_b_.Clear();
  flip_ = false;
}

void Baxandall3::SetInputGain(double value) noexcept {
  input_gain_ = dsp::Clamp01(value);
  coeffs_dirty_ = true;
}

void Baxandall3::SetTrebleLevel(double value) noexcept {
  treble_level_ = dsp::Clamp01(value);
  coeffs_dirty_ = true;
}

void Baxandall3::SetBassLevel(double value) noexcept {
  bass_level_ = dsp::Clamp01(value);
  coeffs_dirty_ = true;
}

void Baxandall3::UpdateCoefficients() noexcept {
  // Treble (high-shelf implemented as input minus LP output).
  const double treble_gain = dsp::BipolarSquaredGain(treble_level_);

  // Crossover sweeps 200 Hz - 2200 Hz as treble_gain goes 0 -> 4.
  const double treble_freq =
      std::min(((2000.0 * treble_gain) + 200.0) / sample_rate_, 0.45);
  h_coeffs_ = dsp::ComputeLPCoeffs(treble_freq, kBesselQ, sample_rate_);

  // Bass (low-shelf: LP output passed through directly).
  // Mirror of treble: (1-C) factor so bass cuts as treble boosts.
  const double bass_freq_factor = dsp::BipolarSquaredGain(1.0 - bass_level_);
  const double bass_freq =
      std::min(((2000.0 * bass_freq_factor) + 200.0) / sample_rate_, 0.45);
  l_coeffs_ = dsp::ComputeLPCoeffs(bass_freq, kBesselQ, sample_rate_);

  coeffs_dirty_ = false;
}

void Baxandall3::ProcessSample(double& left, double& right,
                               const CoeffVec2& cv) noexcept {
  // Console5 encode: clamp to [-pi/2, pi/2] then sin() soft-clips; asin()
  // decodes. The clamp before sin() prevents wrap-around on hot signals.
  const double half_pi = dsp::kPi * 0.5;
  const Vec2 raw(left, right);
  const Vec2 clamped = dsp::max(dsp::min(raw * cv.input_g, half_pi), -half_pi);
  const Vec2 in{std::sin(clamped.l()), std::sin(clamped.r())};

  // Interleaved biquad.
  dsp::BiquadState& hs = flip_ ? h_state_a_ : h_state_b_;
  dsp::BiquadState& ls = flip_ ? l_state_a_ : l_state_b_;

  // High-shelf = input minus LP output.
  const Vec2 treble =
      in - hs.Tick(in, cv.h.b0, cv.h.b1, cv.h.b2, cv.h.a1, cv.h.a2);
  // Bass = LP output (passed through as-is for low-shelf).
  const Vec2 bass = ls.Tick(in, cv.l.b0, cv.l.b1, cv.l.b2, cv.l.a1, cv.l.a2);

  flip_ = !flip_;

  // Apply band gains and sum. Gain scalings are pre-computed per block.
  const Vec2 out = treble * cv.treble_g + bass * cv.bass_g;

  // Console5 decode: asin() inverts the encode sin(), restoring headroom.
  const double cl = std::max(std::min(out.l(), dsp::kSoftClipCeiling),
                             -dsp::kSoftClipCeiling);
  const double cr = std::max(std::min(out.r(), dsp::kSoftClipCeiling),
                             -dsp::kSoftClipCeiling);
  left = std::asin(cl);
  right = std::asin(cr);
}

void Baxandall3::ProcessBlock(double* left, double* right,
                              int num_frames) noexcept {
  if (!left || !right || num_frames <= 0) return;

  if (coeffs_dirty_) UpdateCoefficients();

  const CoeffVec2 cv{
      .h = h_coeffs_,
      .l = l_coeffs_,
      .input_g = Vec2(dsp::BipolarSquaredGain(input_gain_)),
      .treble_g = Vec2(dsp::BipolarSquaredGain(treble_level_)),
      .bass_g = Vec2(dsp::BipolarSquaredGain(bass_level_)),
  };

  for (int i = 0; i < num_frames; ++i) ProcessSample(left[i], right[i], cv);
}
