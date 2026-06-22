// This file is derived from the original Luftikus by lkjb.
// Copyright (c) lkjb (MIT license).

#include "aireq_dsp.h"

#include <cassert>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"

namespace aireq_dsp {

namespace {

// Gain-curve constants.
constexpr double kDenomOffset = 5.56;
constexpr double kAmplitude = 56.2;
constexpr double kExpScale = 4011.4;
constexpr double kExpRate = 7.4746;
constexpr double kExpOffset = 0.54573;
constexpr double kLinSlope = 810.0;
constexpr double kLinSlopeHi = 823.6;
constexpr double kExpScaleHi = 3258.2;
constexpr double kExpRateHi = 7.4126;
constexpr double kExpOffsetHi = 1.8466;
constexpr double kGainNorm = 0.29;
constexpr double kGainNormKept = 0.398;
constexpr double kGainFloor = 500.0;

}  // namespace

void Processor::SetParams(const Params& p) noexcept {
  const bool shelf_changed = p.high_shelf != params_.high_shelf;
  params_ = p;
  if (shelf_changed) SetupFilter(BandType::kShelfHi);
}

void Processor::Prepare(double sample_rate) noexcept {
  sample_rate_ = coeffs::SampleRateToEnum(sample_rate);
  for (auto& bq : biquads_) bq.Clear();
  for (int i = 0; i < static_cast<int>(BandType::kNumTypes); ++i)
    SetupFilter(static_cast<BandType>(i));
}

void Processor::ProcessBlock(double* out_l, double* out_r,
                             int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  constexpr int kShelfHiIdx = BandType::kShelfHi;
  constexpr int kNumTypesIdx = BandType::kNumTypes;

  double g[kNumTypesIdx];
  double pg[kNumTypesIdx];

  for (int i = 0; i < kShelfHiIdx; ++i) {
    const double x = (params_.gains[i] / 20.0) + 0.5;

    if (x > 0.5) {
      g[i] =
          0.5 * kAmplitude /
          (kDenomOffset + (kExpScale * std::exp(-kExpRate * x) - kExpOffset));
      pg[i] = 1.0;
    } else if (x >= 0.25) {
      g[i] = 0.5 * kAmplitude /
             (kDenomOffset + (kGainFloor - kLinSlope * (x - 0.25) * 2.0));
      pg[i] = 1.0;
    } else {
      g[i] = 0.5 * kAmplitude / (kDenomOffset + kGainFloor);
      pg[i] = x * 4.0;
    }
  }

  {
    const double x = params_.gains[kShelfHiIdx] / 10.0;
    g[kShelfHiIdx] =
        0.5 * kAmplitude /
        (kDenomOffset +
         (x <= 0.5 ? kGainFloor - (kLinSlopeHi * x)
                   : (kExpScaleHi * std::exp(-kExpRateHi * x)) - kExpOffsetHi));
    pg[kShelfHiIdx] = 1.0;
  }

  double dc_gain = 0.0;
  for (int n = 0; n < kNumTypesIdx; ++n) {
    if (n != kShelfHiIdx || params_.high_shelf != HighShelf::kHighOff)
      dc_gain += g[n];
  }

  const double global_gain =
      params_.keep_gain ? kGainNormKept / dc_gain : kGainNorm;
  const double shelf_weight =
      (params_.high_shelf != HighShelf::kHighOff) ? 1.0 : 0.0;

  if (params_.phase_inv) dsp::InvertPhase(out_l, out_r, num_frames);

  for (int i = 0; i < num_frames; ++i) {
    const dsp::Vec2 dry(out_l[i], out_r[i]);
    dsp::Vec2 mix(0.0, 0.0);

    // High shelf band (conditionally weighted).
    {
      const dsp::Vec2 band = biquads_[kShelfHiIdx].Tick(dry);
      mix = (band * pg[kShelfHiIdx] + dry) * (g[kShelfHiIdx] * shelf_weight);
    }

    for (int n = 0; n < kShelfHiIdx; ++n) {
      const dsp::Vec2 band = biquads_[n].Tick(dry);
      mix = mix + (band * pg[n] + dry) * g[n];
    }

    const dsp::Vec2 out = mix * global_gain;
    out_l[i] = out.L();
    out_r[i] = out.R();
  }
}

void Processor::SetupFilter(BandType type) noexcept {
  double b[3] = {0.0, 0.0, 0.0};
  double a[3] = {1.0, 0.0, 0.0};

  constexpr int kShelfHiIdx = static_cast<int>(BandType::kShelfHi);

  static constexpr coeffs::FilterType kFixedMap[kShelfHiIdx] = {
      coeffs::FilterType::kBand10,   coeffs::FilterType::kBand40,
      coeffs::FilterType::kBand160,  coeffs::FilterType::kBand640,
      coeffs::FilterType::kShelf2k5,
  };

  if (type < BandType::kShelfHi) {
    SetCoeffs(kFixedMap[type], sample_rate_, b, a);
  } else {
    static constexpr coeffs::FilterType
        kShelfMap[static_cast<int>(HighShelf::kNumHighShelves)] = {
            coeffs::FilterType::kBand10,  // placeholder; zero-output biquad
                                          // used instead
            coeffs::FilterType::kA2k5,
            coeffs::FilterType::kA5k,
            coeffs::FilterType::kA10k,
            coeffs::FilterType::kA20k,
            coeffs::FilterType::kA40k,
        };

    if (params_.high_shelf == HighShelf::kHighOff) {
      b[0] = b[1] = b[2] = 0.0;
      a[0] = 1.0;
      a[1] = a[2] = 0.0;
    } else {
      SetCoeffs(kShelfMap[static_cast<int>(params_.high_shelf)], sample_rate_,
                b, a);
    }
  }

  assert(a[0] == 1.0);
  biquads_[static_cast<int>(type)].SetBiquad(b[0], b[1], b[2], a[1], a[2]);
}

}  // namespace aireq_dsp
