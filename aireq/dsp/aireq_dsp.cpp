// This file is derived from the original Luftikus by lkjb.
// Copyright (c) lkjb (MIT license)

#include "aireq_dsp.h"

#include <cassert>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"

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

AirEqDsp::AirEqDsp() : sample_rate_(coeffs::k44100) {
  params_.gains.fill(0.0);
  Prepare(44100.0);
}

void AirEqDsp::SetParameters(const Params& p) noexcept {
  if (p.high_shelf != params_.high_shelf) {
    params_.high_shelf = p.high_shelf;
    SetupFilter(kShelfHi);
  }
  params_ = p;
}

void AirEqDsp::Prepare(double sample_rate) noexcept {
  sample_rate_ = coeffs::SampleRateToEnum(sample_rate);
  for (auto& bq : biquads_) bq.Clear();
  for (int i = 0; i < kNumTypes; ++i) SetupFilter(static_cast<Type>(i));
}

void AirEqDsp::ProcessBlock(double* out_l, double* out_r, int num_frames) {
  ScopedDenormalGuard denormal_guard;
  double g[kNumTypes];
  double pg[kNumTypes];

  for (int i = 0; i < kShelfHi; ++i) {
    const double x = params_.gains[i] / 20.0 + 0.5;

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
    const double x = params_.gains[kShelfHi] / 10.0;
    g[kShelfHi] =
        0.5 * kAmplitude /
        (kDenomOffset +
         (x <= 0.5 ? kGainFloor - kLinSlopeHi * x
                   : kExpScaleHi * std::exp(-kExpRateHi * x) - kExpOffsetHi));
    pg[kShelfHi] = 1.0;
  }

  double dc_gain = 0.0;
  for (int n = 0; n < kNumTypes; ++n) {
    if (n != kShelfHi || params_.high_shelf != kHighOff) dc_gain += g[n];
  }

  const double global_gain =
      params_.keep_gain ? kGainNormKept / dc_gain : kGainNorm;
  const double shelf_weight = (params_.high_shelf != kHighOff) ? 1.0 : 0.0;

  if (params_.phase_inv) dsp::InvertPhase(out_l, out_r, num_frames);

  for (int i = 0; i < num_frames; ++i) {
    const dsp::Vec2 dry(out_l[i], out_r[i]);
    dsp::Vec2 mix(0.0, 0.0);

    // High shelf band (conditionally weighted).
    {
      const dsp::Vec2 band = biquads_[kShelfHi].Tick(dry);
      mix = (band * pg[kShelfHi] + dry) * (g[kShelfHi] * shelf_weight);
    }

    for (int n = 0; n < kShelfHi; ++n) {
      const dsp::Vec2 band = biquads_[n].Tick(dry);
      mix = mix + (band * pg[n] + dry) * g[n];
    }

    const dsp::Vec2 out = mix * global_gain;
    out_l[i] = out.l();
    out_r[i] = out.r();
  }
}

void AirEqDsp::SetupFilter(Type type) noexcept {
  double b[3] = {0, 0, 0};
  double a[3] = {1, 0, 0};

  static constexpr coeffs::Type kFixedMap[kShelfHi] = {
      coeffs::kBand10,  coeffs::kBand40,   coeffs::kBand160,
      coeffs::kBand640, coeffs::kShelf2k5,
  };

  if (type < kShelfHi) {
    coeffs::SetCoeffs(kFixedMap[type], sample_rate_, b, a);
  } else {
    static constexpr coeffs::Type kShelfMap[kNumHighShelves] = {
        coeffs::kBand10,  // placeholder; zero-output biquad used instead
        coeffs::kA2k5,   coeffs::kA5k,  coeffs::kA10k,
        coeffs::kA20k,   coeffs::kA40k,
    };

    if (params_.high_shelf == kHighOff) {
      b[0] = b[1] = b[2] = 0.0;
      a[0] = 1.0;
      a[1] = a[2] = 0.0;
    } else {
      coeffs::SetCoeffs(kShelfMap[params_.high_shelf], sample_rate_, b,
                              a);
    }
  }

  assert(a[0] == 1.0);
  biquads_[type].SetBiquad(b[0], b[1], b[2], a[1], a[2]);
}
