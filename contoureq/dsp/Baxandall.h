// This file is derived from the original Baxandall by Airwindows.
// Copyright (c) Airwindows (MIT license)

#pragma once

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

  struct BiquadCoeffs {
    double a0{}, a1{}, a2{};  // feed-forward; a1 = 2*a0, a2 = a0 (LP form)
    double b1{}, b2{};        // feedback (Direct Form II Transposed)
  };

  // v1 uses distinct fixed Q values for treble and bass shelves:
  //   treble Q = 0.4   (mildly underdamped, gentle peak at crossover)
  //   bass   Q = 0.2   (heavily overdamped, very smooth shelf transition)
  static constexpr double kTrebleQ = 0.4;
  static constexpr double kBassQ = 0.2;

  BiquadCoeffs h_coeffs_;  // high-shelf (treble) LP prototype
  BiquadCoeffs l_coeffs_;  // low-shelf  (bass) LP prototype

  // Biquad state. Lane 0 = L, lane 1 = R; lanes are independent.
  // Two banks (A/B) alternate each sample for a subtly decorrelated character.
  dsp::Vec2 h_state_a1_, h_state_a2_;  // high-shelf bank A
  dsp::Vec2 h_state_b1_, h_state_b2_;  // high-shelf bank B
  dsp::Vec2 l_state_a1_, l_state_a2_;  // low-shelf  bank A
  dsp::Vec2 l_state_b1_, l_state_b2_;  // low-shelf  bank B

  bool flip_{false};  // selects bank A (true) or bank B (false) each sample

  void UpdateCoefficients() noexcept;

  // Per-block cache: coefficients and pre-computed gains broadcast to Vec2.
  struct CoeffVec2 {
    dsp::Vec2 ha0, ha1, ha2, hb1, hb2;
    dsp::Vec2 la0, la1, la2, lb1, lb2;

    // Per-block gain scalings.
    dsp::Vec2 output_g;  // pre-encode output trim
    dsp::Vec2 treble_g;  // band gain applied after filter
    dsp::Vec2 bass_g;    // band gain applied after filter
  };

  void ProcessSample(double& left, double& right, const CoeffVec2& cv) noexcept;
};
