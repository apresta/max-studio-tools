// This file is derived from the original Tube2 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#pragma once

#include "vec.h"

static constexpr double kTubeClipCeiling = 0.52;
static constexpr double kTubeClipMakeup =
    1.0 / kTubeClipCeiling;  // ~= 1.923076923

class Tube2 {
 public:
  Tube2(double input_pad = 0.5, double tube_character = 0.5);

  void SetInputPad(double value) noexcept;
  void SetTubeCharacter(double value) noexcept;

  double GetInputPad() const noexcept;
  double GetTubeCharacter() const noexcept;

  // Process one stereo block in-place.
  void ProcessBlock(double* out_l, double* out_r, int num_samples,
                    double sample_rate = 44100.0) noexcept;

  // Reset all internal state (call between unrelated streams).
  void Reset() noexcept;

 private:
  double input_pad_;       // pre-waveshaper gain reduction
  double tube_character_;  // polynomial order selector

  dsp::Vec2 pre_waveshaper_avg_;  // hi-rate averaging before the waveshaper
  dsp::Vec2 post_tube_avg_;       // hi-rate averaging after tube saturation
  dsp::Vec2 hysteresis_prev_;     // previous sample for the hysteresis stage

  // Cached bypass state used to detect bypass->active transitions so we can
  // reset stale filter state before re-engaging.
  bool was_bypassed_{false};
};
