// This file is derived from the original PurestDrive by Airwindows.
// Copyright (c) Airwindows (MIT license).

#pragma once

#include "vec.h"

namespace saturation {

class PurestDrive {
 public:
  // drive: saturation intensity. 0.0 = bypass, 1.0 = full effect.
  void SetDrive(double value) noexcept;

  double GetDrive() const noexcept { return drive_; }

  // Process one stereo block in-place.
  void ProcessBlock(double* left, double* right, int num_samples) noexcept;

  // Reset inter-sample state (call between unrelated streams).
  void Reset() noexcept;

 private:
  double drive_{0.0};  // saturation intensity [0, 1]

  // sin(dry_sample) from the previous frame, stored for both channels.
  // Adding it to the current saturated sample and taking the absolute value
  // gives the per-sample blend factor: large when consecutive saturated
  // samples share sign (smooth content), small when they oppose (highs /
  // transients).
  dsp::Vec2 prev_sin_{0.0};
};

}  // namespace saturation
