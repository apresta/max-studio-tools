// This file is derived from the original Luftikus by lkjb.
// Copyright (c) lkjb (MIT license).

#pragma once

#include <array>

#include "aireq_coeffs.h"
#include "biquad_state.h"
#include "vec.h"

namespace aireq_dsp {

// Fixed-coefficient stereo biquad.
class SimpleBiquad {
 public:
  SimpleBiquad() { Clear(); }

  void Clear() noexcept { st_.Clear(); }

  void SetBiquad(double b0, double b1, double b2, double a1,
                 double a2) noexcept {
    b0_ = b0;
    b1_ = b1;
    b2_ = b2;
    a1_ = a1;
    a2_ = a2;
  }

  dsp::Vec2 Tick(dsp::Vec2 x) noexcept {
    return st_.Tick(x, b0_, b1_, b2_, a1_, a2_);
  }

 private:
  double b0_ = 0.0, b1_ = 0.0, b2_ = 0.0;
  double a1_ = 0.0, a2_ = 0.0;
  dsp::BiquadState st_;
};

enum BandType {
  kBand10 = 0,
  kBand40 = 1,
  kBand160 = 2,
  kBand640 = 3,
  kShelf2k5 = 4,
  kShelfHi = 5,
  kNumTypes = 6
};

enum class HighShelf {
  kHighOff,
  kHigh2k5,
  kHigh5k,
  kHigh10k,
  kHigh20k,
  kHigh40k,
  kNumHighShelves
};

struct Params {
  std::array<double, static_cast<int>(BandType::kNumTypes)> gains = {};
  HighShelf high_shelf = HighShelf::kHighOff;
  bool keep_gain = false;
  bool phase_inv = false;
};

class Processor {
 public:
  void SetParams(const Params& p) noexcept;

  // Reset biquad state and select coefficient set for sample_rate.
  void Prepare(double sample_rate) noexcept;

  // Process num_frames stereo frames in-place.
  void ProcessBlock(double* out_l, double* out_r, int num_frames) noexcept;

 private:
  void SetupFilter(BandType type) noexcept;

  Params params_;
  coeffs::SampleRates sample_rate_ = coeffs::SampleRates::k44100;
  std::array<SimpleBiquad, static_cast<int>(coeffs::FilterType::kNumTypes)>
      biquads_;
};

}  // namespace aireq_dsp
