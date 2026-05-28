// This file is derived from the original Baxandall by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Baxandall.h"

#include <algorithm>
#include <cmath>

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
  h_state_a1_ = h_state_a2_ = Vec2(0.0);
  h_state_b1_ = h_state_b2_ = Vec2(0.0);
  l_state_a1_ = l_state_a2_ = Vec2(0.0);
  l_state_b1_ = l_state_b2_ = Vec2(0.0);
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
  const double treble_gain = dsp::DbLinearGain(treble_level_);

  // Treble crossover scales linearly with treble gain: more boost pushes
  // the shelf corner up (wider presence region), more cut pulls it down.
  // Origin: 4410 Hz * gain / sample_rate (= Nyquist/10 at unity, 44.1 kHz).
  const double treble_freq =
      std::min((4410.0 * treble_gain) / sample_rate_, 0.45);

  {
    // Direct Form II Transposed low-pass biquad.
    // Q = kTrebleQ = 0.4: gently underdamped, mild peak at the shelf knee.
    const double k = std::tan(dsp::kPi * treble_freq);
    const double norm = 1.0 / (1.0 + k / kTrebleQ + k * k);
    h_coeffs_.a0 = k * k * norm;
    h_coeffs_.a1 = 2.0 * h_coeffs_.a0;
    h_coeffs_.a2 = h_coeffs_.a0;
    h_coeffs_.b1 = 2.0 * (k * k - 1.0) * norm;
    h_coeffs_.b2 = (1.0 - k / kTrebleQ + k * k) * norm;
  }

  // Bass crossover scales inversely with bass gain: more boost lowers the
  // corner (deeper bass), more cut raises it (thinner bass texture).
  // Origin: 8820 Hz / gain / sample_rate (= 2x treble base, inverse taper).
  const double bass_gain = dsp::DbLinearGain(bass_level_);
  const double bass_freq = std::min((8820.0 / bass_gain) / sample_rate_, 0.45);

  {
    // Q = kBassQ = 0.2: heavily overdamped for an ultra-smooth shelf rolloff.
    const double k = std::tan(dsp::kPi * bass_freq);
    const double norm = 1.0 / (1.0 + k / kBassQ + k * k);
    l_coeffs_.a0 = k * k * norm;
    l_coeffs_.a1 = 2.0 * l_coeffs_.a0;
    l_coeffs_.a2 = l_coeffs_.a0;
    l_coeffs_.b1 = 2.0 * (k * k - 1.0) * norm;
    l_coeffs_.b2 = (1.0 - k / kBassQ + k * k) * norm;
  }

  coeffs_dirty_ = false;
}

void Baxandall::ProcessSample(double& left, double& right,
                              const CoeffVec2& cv) noexcept {
  const double s_l = left * cv.output_g.l();
  const double s_r = right * cv.output_g.r();

  // Console5 encode: sin() soft-clips the input, decode asin() inverts it.
  const Vec2 in{std::sin(s_l), std::sin(s_r)};

  // Interleaved biquad. Alternating banks (flip_) decorrelates the filter
  // paths slightly for a more organic, less clinical character.
  Vec2 treble;
  Vec2 bass;

  if (flip_) {
    // High-shelf = input minus LP output.
    treble = in * cv.ha0 + h_state_a1_;
    h_state_a1_ = in * cv.ha1 - treble * cv.hb1 + h_state_a2_;
    h_state_a2_ = in * cv.ha2 - treble * cv.hb2;
    treble = in - treble;

    // Bass = LP output (passed through as-is for low-shelf).
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
      Vec2(dsp::DbLinearGain(output_level_)),
      Vec2(dsp::DbLinearGain(treble_level_)),
      Vec2(dsp::DbLinearGain(bass_level_)),
  };

  for (int i = 0; i < num_frames; ++i) ProcessSample(left[i], right[i], cv);
}
