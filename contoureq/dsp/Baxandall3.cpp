// This file is derived from the original Baxandall3 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Baxandall3.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "dsp_math.h"

using dsp::Vec2;

Baxandall3::Baxandall3() { Reset(); }

void Baxandall3::Prepare(double sample_rate) noexcept {
  sample_rate_ = (sample_rate > 0.0) ? sample_rate : 44100.0;
  coeffs_dirty_ = true;
  Reset();
}

void Baxandall3::Reset() noexcept {
  h_coeffs_ = l_coeffs_ = {};
  h_state_a1_ = h_state_a2_ = Vec2(0.0);
  h_state_b1_ = h_state_b2_ = Vec2(0.0);
  l_state_a1_ = l_state_a2_ = Vec2(0.0);
  l_state_b1_ = l_state_b2_ = Vec2(0.0);
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
  // BipolarSquaredGain maps [0,1] to [0,4] with unity at 0.5.
  const double treble_gain = dsp::BipolarSquaredGain(treble_level_);

  // Crossover sweeps 200 Hz - 2200 Hz as treble_gain goes 0 -> 4.
  const double treble_freq =
      std::min(((2000.0 * treble_gain) + 200.0) / sample_rate_, 0.45);

  {
    // Direct Form II Transposed low-pass biquad (Bessel Q = 1/sqrt(3)).
    // kBesselQ gives maximally-flat group delay, preserving transients.
    const double K = std::tan(dsp::kPi * treble_freq);
    const double norm = 1.0 / (1.0 + K / kBesselQ + K * K);
    h_coeffs_.a0 = K * K * norm;
    h_coeffs_.a1 = 2.0 * h_coeffs_.a0;  // a1 = 2*a0 for LP form
    h_coeffs_.a2 = h_coeffs_.a0;        // a2 = a0  for LP form
    h_coeffs_.b1 = 2.0 * (K * K - 1.0) * norm;
    h_coeffs_.b2 = (1.0 - K / kBesselQ + K * K) * norm;
  }

  // Bass (low-shelf: LP output passed through directly).
  // Mirror of treble: (1-C) factor so bass cuts as treble boosts.
  const double bass_freq_factor = dsp::BipolarSquaredGain(1.0 - bass_level_);
  const double bass_freq =
      std::min(((2000.0 * bass_freq_factor) + 200.0) / sample_rate_, 0.45);

  {
    const double K = std::tan(dsp::kPi * bass_freq);
    const double norm = 1.0 / (1.0 + K / kBesselQ + K * K);
    l_coeffs_.a0 = K * K * norm;
    l_coeffs_.a1 = 2.0 * l_coeffs_.a0;
    l_coeffs_.a2 = l_coeffs_.a0;
    l_coeffs_.b1 = 2.0 * (K * K - 1.0) * norm;
    l_coeffs_.b2 = (1.0 - K / kBesselQ + K * K) * norm;
  }

  coeffs_dirty_ = false;
}

void Baxandall3::ProcessSample(double& left, double& right,
                               const CoeffVec2& cv) noexcept {
  // Denormal guard.
  double s_l = dsp::ZapDenormal(left);
  double s_r = dsp::ZapDenormal(right);

  // Console5 encode: soft-clip input via sin() to prevent inter-stage
  // overs while preserving waveform shape at low levels.
  const double half_pi = dsp::kPi * 0.5;
  s_l = std::sin(std::fmax(std::fmin(s_l * cv.input_g.l(), half_pi), -half_pi));
  s_r = std::sin(std::fmax(std::fmin(s_r * cv.input_g.r(), half_pi), -half_pi));

  const Vec2 in(s_l, s_r);

  // Interleaved biquad (L and R processed in parallel via Vec2).
  // Alternating banks (flip_) gives a subtly decorrelated character.
  Vec2 treble, bass;

  if (flip_) {
    // High-shelf = input minus LP output.
    treble = in * cv.ha0 + h_state_a1_;
    h_state_a1_ = in * cv.ha1 - treble * cv.hb1 + h_state_a2_;
    h_state_a2_ = in * cv.ha2 - treble * cv.hb2;
    treble = in - treble;

    // Bass = LP output (passed through as-is).
    bass = in * cv.la0 + l_state_a1_;
    l_state_a1_ = in * cv.la1 - bass * cv.lb1 + l_state_a2_;
    l_state_a2_ = in * cv.la2 - bass * cv.lb2;
  } else {
    treble = in * cv.ha0 + h_state_b1_;
    h_state_b1_ = in * cv.ha1 - treble * cv.hb1 + h_state_b2_;
    h_state_b2_ = in * cv.ha2 - treble * cv.hb2;
    treble = in - treble;

    bass = in * cv.la0 + l_state_b1_;
    l_state_b1_ = in * cv.la1 - bass * cv.lb1 + l_state_b2_;
    l_state_b2_ = in * cv.la2 - bass * cv.lb2;
  }

  flip_ = !flip_;

  // Apply band gains and sum. Gain scalings are pre-computed per block.
  const Vec2 out = treble * cv.treble_g + bass * cv.bass_g;

  // Console5 decode: asin() inverts the encode sin(), restoring headroom.
  // kSoftClipCeiling guards against asin(x) domain error at |x| == 1.
  left = std::asin(std::fmax(std::fmin(out.l(), dsp::kSoftClipCeiling),
                             -dsp::kSoftClipCeiling));
  right = std::asin(std::fmax(std::fmin(out.r(), dsp::kSoftClipCeiling),
                              -dsp::kSoftClipCeiling));
}

void Baxandall3::ProcessBlock(double* left, double* right,
                              int num_frames) noexcept {
  if (!left || !right || num_frames <= 0) return;

  if (coeffs_dirty_) UpdateCoefficients();

  // Hoist all per-block scalings here so ProcessSample stays branch-free.
  const CoeffVec2 cv{
      Vec2(h_coeffs_.a0),
      Vec2(h_coeffs_.a1),
      Vec2(h_coeffs_.a2),
      Vec2(h_coeffs_.b1),
      Vec2(h_coeffs_.b2),
      Vec2(l_coeffs_.a0),
      Vec2(l_coeffs_.a1),
      Vec2(l_coeffs_.a2),
      Vec2(l_coeffs_.b1),
      Vec2(l_coeffs_.b2),
      Vec2(dsp::BipolarSquaredGain(input_gain_)),
      Vec2(dsp::BipolarSquaredGain(treble_level_)),
      Vec2(dsp::BipolarSquaredGain(bass_level_)),
  };

  for (int i = 0; i < num_frames; ++i) ProcessSample(left[i], right[i], cv);
}
