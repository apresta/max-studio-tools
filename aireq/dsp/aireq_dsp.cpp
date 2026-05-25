// This file is derived from the original Luftikus by lkjb.
// Copyright (c) lkjb (MIT license)

#include "aireq_dsp.h"

#include <cassert>
#include <cmath>
#include <cstring>

#include "vec.h"

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

AirEqDsp::AirEqDsp() : sample_rate_(CoeffCreator::k44100) {
  params_.gains.fill(0.0);
  Prepare(44100.0);
}

void AirEqDsp::SetParameters(const Params& p) noexcept {
  // Update high-shelf filter if the frequency selector changed.
  if (p.high_shelf != params_.high_shelf) {
    params_.high_shelf = p.high_shelf;
    SetupFilter(kShelfHi);
  }
  params_ = p;
}

void AirEqDsp::EnsureCapacity(int block_size) {
  if (block_size <= capacity_) return;
  capacity_ = block_size;
  const std::size_t n = static_cast<std::size_t>(capacity_) * 2;
  for (auto& v : band_data_) v.resize(n);
  interleaved_.resize(n);
}

void AirEqDsp::Prepare(double sample_rate) noexcept {
  sample_rate_ = CoeffCreator::SampleRateToEnum(sample_rate);
  for (auto& bq : biquads_) bq.Clear();
  for (int i = 0; i < kNumTypes; ++i) SetupFilter(static_cast<Type>(i));
}

void AirEqDsp::ProcessBlock(double* out_l, double* out_r, int num_frames) {
  assert(num_frames <= capacity_);

  // Interleave planar input into the scratch buffer.
  double* buf = interleaved_.data();
  for (int i = 0; i < num_frames; ++i) {
    buf[2 * i] = out_l[i];
    buf[2 * i + 1] = out_r[i];
  }

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

  if (params_.phase_inv) {
    for (int i = 0; i < num_frames; ++i) {
      const dsp::Vec2 v = -dsp::Vec2(buf[2 * i], buf[2 * i + 1]);
      buf[2 * i] = v.l();
      buf[2 * i + 1] = v.r();
    }
  }

  const std::size_t byte_count =
      sizeof(double) * static_cast<std::size_t>(num_frames) * 2;

  // Each band needs its own copy of the interleaved input because the biquads
  // run independently and accumulate into separate buffers before the final
  // weighted mix.
  double dc_gain = 0.0;
  for (int i = 0; i < kNumTypes; ++i) {
    double* data = band_data_[i].data();
    std::memcpy(data, buf, byte_count);
    biquads_[i].ProcessBlock(data, num_frames);
    if (i != kShelfHi || params_.high_shelf != kHighOff) dc_gain += g[i];
  }

  const double global_gain =
      params_.keep_gain ? kGainNormKept / dc_gain : kGainNorm;
  const double shelf_weight = (params_.high_shelf != kHighOff) ? 1.0 : 0.0;

  dsp::Vec2 vg[kNumTypes], vpg[kNumTypes];
  for (int n = 0; n < kNumTypes; ++n) {
    vg[n] = dsp::Vec2(g[n]);
    vpg[n] = dsp::Vec2(pg[n]);
  }
  const dsp::Vec2 vgg(global_gain);
  const dsp::Vec2 vsw(shelf_weight);
  const dsp::Vec2 vghi = vg[kShelfHi] * vsw;

  for (int i = 0; i < num_frames; ++i) {
    const dsp::Vec2 dry(buf[2 * i], buf[2 * i + 1]);

    dsp::Vec2 mix = (dsp::Vec2(band_data_[kShelfHi][2 * i],
                               band_data_[kShelfHi][2 * i + 1]) *
                         vpg[kShelfHi] +
                     dry) *
                    vghi;

    for (int n = 0; n < kShelfHi; ++n) {
      mix = mix + (dsp::Vec2(band_data_[n][2 * i], band_data_[n][2 * i + 1]) *
                       vpg[n] +
                   dry) *
                      vg[n];
    }

    const dsp::Vec2 out = mix * vgg;
    buf[2 * i] = out.l();
    buf[2 * i + 1] = out.r();
  }

  // Deinterleave scratch buffer back to planar output.
  for (int i = 0; i < num_frames; ++i) {
    out_l[i] = buf[2 * i];
    out_r[i] = buf[2 * i + 1];
  }
}

void AirEqDsp::SetupFilter(Type type) noexcept {
  double b[3] = {0, 0, 0};
  double a[3] = {1, 0, 0};

  static constexpr CoeffCreator::Type kFixedMap[kShelfHi] = {
      CoeffCreator::kBand10,  CoeffCreator::kBand40,   CoeffCreator::kBand160,
      CoeffCreator::kBand640, CoeffCreator::kShelf2k5,
  };

  if (type < kShelfHi) {
    CoeffCreator::SetCoeffs(kFixedMap[type], sample_rate_, b, a);
  } else {
    static constexpr CoeffCreator::Type kShelfMap[kNumHighShelves] = {
        CoeffCreator::kBand10,  // placeholder; zero-output biquad used instead
        CoeffCreator::kA2k5,   CoeffCreator::kA5k,  CoeffCreator::kA10k,
        CoeffCreator::kA20k,   CoeffCreator::kA40k,
    };

    if (params_.high_shelf == kHighOff) {
      b[0] = b[1] = b[2] = 0.0;
      a[0] = 1.0;
      a[1] = a[2] = 0.0;
    } else {
      CoeffCreator::SetCoeffs(kShelfMap[params_.high_shelf], sample_rate_, b,
                              a);
    }
  }

  assert(a[0] == 1.0);
  biquads_[type].SetBiquad(b[0], b[1], b[2], a[1], a[2]);
}
