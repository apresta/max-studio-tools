// This file is derived from the original SingleEndedTriode by Airwindows.
// Copyright (c) Airwindows (MIT license).

#include "SingleEndedTriode.h"

#include <cmath>

#include "dsp_math.h"
#include "vec.h"

namespace saturation {

using dsp::Vec2;

namespace {

// Fixed constant: sin(0.5).  Used to re-centre the signal after the triode
// waveshaper shifts its operating point down by 0.5 (post-intensity scaling).
// Computed once at startup; no per-instance state is needed.
const double kPostSine = std::sin(0.5);

// Restore the sign of `original` to the magnitude `mag`, lane-by-lane.
inline Vec2 CopySign(Vec2 mag, Vec2 original) noexcept {
  return Vec2{std::copysign(mag.L(), original.L()),
              std::copysign(mag.R(), original.R())};
}

}  // namespace

void SingleEndedTriode::SetTriodeDrive(double value) noexcept {
  triode_drive_ = dsp::Clamp01(value);
}

void SingleEndedTriode::SetClassAB(double value) noexcept {
  class_ab_ = dsp::Clamp01(value);
}

void SingleEndedTriode::SetClassB(double value) noexcept {
  class_b_ = dsp::Clamp01(value);
}

void SingleEndedTriode::ProcessBlock(double* left, double* right,
                                     int num_samples) const noexcept {
  // Triode: quadratic taper maps [0, 1] -> [0, 8].
  // The +0.001 ensures intensity is never zero, preventing division-by-zero
  // in the reciprocal scaling step at the end of the stage.
  const double intensity = (triode_drive_ * triode_drive_ * 8.0) + 0.001;
  const bool do_triode = triode_drive_ > 0.0;

  // Class AB: cubic taper maps [0, 1] -> [0, 0.125].
  const double softcrossover = class_ab_ * class_ab_ * class_ab_ / 8.0;
  const bool do_class_ab = class_ab_ > 0.0;

  // Class B: seventh-power taper maps [0, 1] -> [0, 0.125].
  // The steep taper keeps the dead band small until the knob is pushed hard.
  const double hardcrossover = std::pow(class_b_, 7.0) / 8.0;
  const bool do_class_b = class_b_ > 0.0;

  // Pre-broadcast loop-invariant scalars into Vec2 so they are ready for
  // stereo arithmetic without repeated construction inside the hot loop.
  const Vec2 v_intensity{intensity};
  const Vec2 v_inv_intensity{1.0 / intensity};
  const Vec2 v_post_sine{kPostSine};
  const Vec2 v_softcrossover{softcrossover};
  const Vec2 v_hardcrossover{hardcrossover};

  for (int i = 0; i < num_samples; ++i) {
    const Vec2 dry{left[i], right[i]};
    Vec2 s = dry;

    // Stage 1: Triode asymmetric saturation.
    // Shift the signal's operating point down by 0.5 (after scaling by
    // intensity) so that positive and negative half-cycles see different
    // fractions of the sin() waveshaper. This asymmetry produces even-order
    // harmonics (2nd, 4th, ...) characteristic of a single-ended class-A
    // triode.
    if (do_triode) {
      s = s * v_intensity - Vec2{0.5};

      // Soft-clip magnitude through sin(), clamping the input to [0, pi/2]
      // to keep sin() in [0, 1], then restore the original sign.
      Vec2 mag = dsp::Min(dsp::Abs(s), dsp::kPi * 0.5);
      mag = Vec2{std::sin(mag.L()), std::sin(mag.R())};
      s = CopySign(mag, s);

      // Re-centre the DC bias introduced by the -0.5 operating-point shift.
      // postsine = sin(0.5): a zero-valued input traverses -0.5 -> sin(0.5)
      // through the waveshaper, so adding sin(0.5) returns it to zero.
      // Then undo the intensity scaling.
      s = (s + v_post_sine) * v_inv_intensity;
    }

    // Stage 2: Class AB soft crossover notch.
    // Subtract a square-root-weighted amount from the signal magnitude near
    // zero, creating a smooth notch. Models the slight dead zone of a lightly
    // biased push-pull output stage.
    if (do_class_ab) {
      Vec2 mag = dsp::Abs(s);
      mag = mag - v_softcrossover * (mag + dsp::Sqrt(mag));
      mag = dsp::Max(mag, 0.0);
      s = CopySign(mag, s);
    }

    // Stage 3: Class B hard crossover dead band.
    // Subtract a fixed threshold from the magnitude and clamp to zero.
    // Models a severely under-biased push-pull stage running deep into Class B.
    if (do_class_b) {
      Vec2 mag = dsp::Abs(s);
      mag = dsp::Max(mag - v_hardcrossover, 0.0);
      s = CopySign(mag, s);
    }

    left[i] = s.L();
    right[i] = s.R();
  }
}

}  // namespace saturation
