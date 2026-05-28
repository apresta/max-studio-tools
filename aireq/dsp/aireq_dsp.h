// This file is derived from the original Luftikus by lkjb.
// Copyright (c) lkjb (MIT license)

#pragma once

#include <array>

#include "aireq_coeffs.h"
#include "biquad_state.h"
#include "vec.h"

// Fixed-coefficient stereo biquad.
class SimpleBiquad {
 public:
  SimpleBiquad() : b0_(1), b1_(0), b2_(0), a1_(0), a2_(0) { Clear(); }

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
  double b0_, b1_, b2_;
  double a1_, a2_;
  dsp::BiquadState st_;
};

class AirEqDsp {
 public:
  enum Type {
    kBand10,
    kBand40,
    kBand160,
    kBand640,
    kShelf2k5,
    kShelfHi,
    kNumTypes
  };

  enum HighShelf {
    kHighOff,
    kHigh2k5,
    kHigh5k,
    kHigh10k,
    kHigh20k,
    kHigh40k,
    kNumHighShelves
  };

  struct Params {
    std::array<double, kNumTypes> gains{};
    HighShelf high_shelf{kHighOff};
    bool keep_gain{false};
    bool phase_inv{false};
  };

  AirEqDsp();

  void SetParameters(const Params& p) noexcept;

  // Reset biquad state and select coefficient set for sample_rate.
  void Prepare(double sample_rate) noexcept;

  // Process num_frames stereo frames in-place.
  void ProcessBlock(double* out_l, double* out_r, int num_frames);

 private:
  void SetupFilter(Type type) noexcept;

  Params params_;
  coeffs::SampleRates sample_rate_;
  std::array<SimpleBiquad, kNumTypes> biquads_;
};
