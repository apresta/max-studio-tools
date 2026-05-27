// This file is derived from the original Baxandall3 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#pragma once

#include "vec.h"

// Bessel-maximally-flat Q = 1/sqrt(3).
static constexpr double kBesselQ = 0.57735026919;

class Baxandall3 {
 public:
  Baxandall3();

  void Prepare(double sample_rate) noexcept;

  void Reset() noexcept;

  void SetInputGain(double value) noexcept;    // input gain/drive
  void SetTrebleLevel(double value) noexcept;  // treble level + crossover
  void SetBassLevel(double value) noexcept;    // bass level + crossover

  double GetInputGain() const noexcept { return input_gain_; }

  double GetTrebleLevel() const noexcept { return treble_level_; }

  double GetBassLevel() const noexcept { return bass_level_; }

  // Process a non-interleaved stereo block (double precision).
  void ProcessBlock(double* left, double* right, int num_frames) noexcept;

 private:
  // Parameters, all in [0, 1].
  double input_gain_{0.5};
  double treble_level_{0.5};
  double bass_level_{0.5};

  double sample_rate_{44100.0};

  // Coefficients are recalculated only when parameters or sample rate change.
  bool coeffs_dirty_{true};

  struct BiquadCoeffs {
    double a0{}, a1{}, a2{};  // feed-forward; a1 = 2*a0, a2 = a0 (LP form)
    double b1{}, b2{};        // feedback (Direct Form II Transposed)
  };

  BiquadCoeffs h_coeffs_;  // high-shelf (treble) LP prototype
  BiquadCoeffs l_coeffs_;  // low-shelf  (bass) LP prototype

  // Biquad state. Lane 0 = L, lane 1 = R; lanes are independent.
  // Two banks (A/B) are maintained and alternated each sample (see flip_).
  // This dual-bank technique decorrelates the two filter paths slightly,
  // giving a more organic, less "perfect" filter character.
  dsp::Vec2 h_state_a1_, h_state_a2_;  // high-shelf bank A
  dsp::Vec2 h_state_b1_, h_state_b2_;  // high-shelf bank B
  dsp::Vec2 l_state_a1_, l_state_a2_;  // low-shelf  bank A
  dsp::Vec2 l_state_b1_, l_state_b2_;  // low-shelf  bank B

  bool flip_{false};  // selects bank A (true) or bank B (false) each sample

  void UpdateCoefficients() noexcept;

  struct CoeffVec2 {
    dsp::Vec2 ha0, ha1, ha2, hb1, hb2;
    dsp::Vec2 la0, la1, la2, lb1, lb2;

    // Per-block gain scalings.
    dsp::Vec2 input_g, treble_g, bass_g;
  };

  void ProcessSample(double& left, double& right, const CoeffVec2& cv) noexcept;
};
