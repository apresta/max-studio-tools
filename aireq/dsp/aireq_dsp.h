// This file is derived from the original Luftikus by lkjb.
// Copyright (c) lkjb (MIT license)

#pragma once

#include <array>
#include <vector>

#include "aireq_coeffs.h"
#include "biquad_state.h"
#include "vec.h"

// Fixed-coefficient stereo biquad that processes interleaved double frames.
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

  // Process a block of stereo frames stored as interleaved doubles.
  void ProcessBlock(double* buf, int num_frames) noexcept {
    for (int i = 0; i < num_frames; ++i) {
      const dsp::Vec2 x0{buf[2 * i], buf[2 * i + 1]};
      // Flush denormals before they accumulate in the state registers.
      const dsp::Vec2 y0 =
          dsp::FlushDenormal(st_.Compute(x0, b0_, b1_, b2_, a1_, a2_));
      buf[2 * i] = y0.l();
      buf[2 * i + 1] = y0.r();
      st_.Advance(x0, y0);
    }
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

  // Apply all parameters atomically (called once per block, under the lock).
  void SetParameters(const Params& p) noexcept;

  // Reset biquad state and select coefficient set for sample_rate.
  void Prepare(double sample_rate) noexcept;

  // Ensure the internal work buffers can hold at least block_size frames.
  // Must be called from PrepareEngine(), not from the audio thread.
  void EnsureCapacity(int block_size);

  // Process num_frames stereo frames in-place.
  // EnsureCapacity(num_frames) must have been called beforehand.
  void ProcessBlock(double* out_l, double* out_r, int num_frames);

 private:
  void SetupFilter(Type type) noexcept;

  Params params_;

  // Per-band interleaved work buffers (one per band, sized as 2 * capacity).
  std::array<std::vector<double>, kNumTypes> band_data_;

  // Interleaved scratch buffer for converting planar input before processing.
  std::vector<double> interleaved_;
  int capacity_{0};

  CoeffCreator::SampleRates sample_rate_;

  std::array<SimpleBiquad, kNumTypes> biquads_;
};
