// This file is derived from the original Tube2 by Airwindows.
// Copyright (c) Airwindows (MIT license).

#pragma once

#include "vec.h"

namespace saturation {

class Tube2 {
 public:
  void SetInputPad(double value) noexcept;
  void SetTubeCharacter(double value) noexcept;

  double GetInputPad() const noexcept { return input_pad_; }
  double GetTubeCharacter() const noexcept { return tube_character_; }

  // Process one stereo block in-place.
  void ProcessBlock(double* left, double* right, int num_samples,
                    double sample_rate = 44100.0) noexcept;

  // Reset all internal state (call between unrelated streams).
  void Reset() noexcept;

 private:
  double input_pad_{0.5};       // pre-waveshaper gain reduction
  double tube_character_{0.5};  // polynomial order selector

  dsp::Vec2 pre_waveshaper_avg_{0.0};  // hi-rate averaging before the waveshaper
  dsp::Vec2 post_tube_avg_{0.0};       // hi-rate averaging after tube saturation
  dsp::Vec2 hysteresis_prev_{0.0};     // previous sample for the hysteresis stage

  // Cached bypass state used to detect bypass->active transitions so we can
  // reset stale filter state before re-engaging.
  bool was_bypassed_{false};
};

}  // namespace saturation
