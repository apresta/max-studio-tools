// This file is derived from the original PurestDrive by Airwindows.
// Copyright (c) Airwindows (MIT license).

#include "PurestDrive.h"

#include <cmath>

#include "dsp_math.h"
#include "vec.h"

namespace saturation {

using dsp::Vec2;

void PurestDrive::SetDrive(double value) noexcept {
  drive_ = dsp::Clamp01(value);
}

void PurestDrive::Reset() noexcept { prev_sin_ = Vec2{0.0}; }

void PurestDrive::ProcessBlock(double* left, double* right,
                               int num_samples) noexcept {
  for (int i = 0; i < num_samples; ++i) {
    const Vec2 dry{left[i], right[i]};

    // Sin() waveshaper: gently clips peaks while leaving near-zero signal
    // almost unchanged (sin(x) ≈ x for small x).
    const Vec2 saturated{std::sin(dry.L()), std::sin(dry.R())};

    // Adaptive blend factor, independently computed per channel.
    //
    // Both prev_sin_ and saturated lie in [-1, 1], so their sum's absolute
    // value is at most 2; multiplying by 0.5 keeps blend in [0, 1].
    //
    //   blend ≈ drive  when consecutive saturated samples share sign
    //                  (smooth, low-frequency content -> full saturation)
    //   blend ≈ 0      when they oppose
    //                  (zero crossings, transients, high-freq -> mostly dry)
    //
    // The result is a frequency-selective drive without any explicit filter.
    const Vec2 blend = dsp::Abs(prev_sin_ + saturated) * (0.5 * drive_);

    // Crossfade between dry and saturated according to the adaptive blend.
    const Vec2 wet = dry * (Vec2{1.0} - blend) + saturated * blend;
    left[i] = wet.L();
    right[i] = wet.R();

    // Carry the saturated signal forward as the previous-frame reference.
    // sin(dry) is used (not the blended output) so the modulation stays
    // independent of the drive amount.
    prev_sin_ = saturated;
  }
}

}  // namespace saturation
