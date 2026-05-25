// This file is derived from the original Spiral2 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#pragma once

#include "dsp_math.h"

class Spiral2 {
 public:
  Spiral2() = default;

  void SetGain(double v) noexcept { gain_ = dsp::Clamp01(v); }

  double GetGain() const noexcept { return gain_; }

  void ProcessBlock(double* left, double* right, int num_frames) noexcept;

 private:
  double gain_ = 0.5;
};
