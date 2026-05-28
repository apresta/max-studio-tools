// This file is derived from the original Baxandall by Airwindows.
// Copyright (c) Airwindows (MIT license)

#pragma once

#include "biquad_coeffs.h"
#include "biquad_state.h"
#include "vec.h"

class Baxandall {
 public:
  Baxandall();

  void Prepare(double sample_rate) noexcept;

  void Reset() noexcept;

  void SetTrebleLevel(double value) noexcept;  // treble gain + crossover freq
  void SetBassLevel(double value) noexcept;    // bass gain + crossover freq
  void SetOutputLevel(double value) noexcept;  // output trim (pre-encode)

  double GetTrebleLevel() const noexcept { return treble_level_; }

  double GetBassLevel() const noexcept { return bass_level_; }

  double GetOutputLevel() const noexcept { return output_level_; }

  // Process a non-interleaved stereo block (double precision).
  void ProcessBlock(double* left, double* right, int num_frames) noexcept;

 private:
  // Parameters, all in [0, 1]. Map to +/-15 dB via DbLinearGain().
  double treble_level_{0.5};
  double bass_level_{0.5};
  double output_level_{0.5};

  double sample_rate_{44100.0};

  // Coefficients are recalculated only when parameters or sample rate change.
  bool coeffs_dirty_{true};

  // Fixed Q values for treble and bass shelves.
  static constexpr double kTrebleQ = 0.4;
  static constexpr double kBassQ = 0.2;

  dsp::BiquadCoeffs h_coeffs_;  // high-shelf (treble) LP prototype
  dsp::BiquadCoeffs l_coeffs_;  // low-shelf  (bass) LP prototype

  // Biquad state. Lane 0 = L, lane 1 = R; lanes are independent.
  // Two banks (A/B) alternate each sample for a subtly decorrelated character.
  dsp::BiquadState h_state_a_, h_state_b_;  // high-shelf banks
  dsp::BiquadState l_state_a_, l_state_b_;  // low-shelf  banks

  bool flip_{false};  // selects bank A (true) or bank B (false) each sample

  void UpdateCoefficients() noexcept;

  // Per-block cache: coefficients and pre-computed gains broadcast to Vec2.
  struct CoeffVec2 {
    dsp::BiquadCoeffs h;  // high-shelf LP prototype
    dsp::BiquadCoeffs l;  // low-shelf  LP prototype

    // Per-block gain scalings.
    dsp::Vec2 output_g;  // pre-encode output trim
    dsp::Vec2 treble_g;  // band gain applied after filter
    dsp::Vec2 bass_g;    // band gain applied after filter
  };

  void ProcessSample(double& left, double& right, const CoeffVec2& cv) noexcept;
};
