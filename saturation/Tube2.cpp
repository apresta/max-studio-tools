// This file is derived from the original Tube2 by Airwindows.
// Copyright (c) Airwindows (MIT license).

#include "Tube2.h"

#include "dsp_math.h"
#include "vec.h"

namespace saturation {

namespace {

constexpr double kTubeClipCeiling = 0.52;
constexpr double kTubeClipMakeup = 1.0 / kTubeClipCeiling;

}  // namespace

using dsp::Vec2;

void Tube2::SetInputPad(double value) noexcept {
  input_pad_ = dsp::Clamp01(value);
}

void Tube2::SetTubeCharacter(double value) noexcept {
  tube_character_ = dsp::Clamp01(value);
}

void Tube2::Reset() noexcept {
  pre_waveshaper_avg_ = post_tube_avg_ = hysteresis_prev_ = Vec2{0.0};
}

void Tube2::ProcessBlock(double* left, double* right, int num_samples,
                         double sample_rate) noexcept {
  const double overall_scale = sample_rate / 44100.0;
  const bool hi_rate = overall_scale > 1.9;
  const bool bypass = (tube_character_ == 0.0);

  if (bypass) {
    if (!was_bypassed_) Reset();
    was_bypassed_ = true;
    return;
  }
  was_bypassed_ = false;

  const double iterations = 1.0 - tube_character_;
  const int power_factor = static_cast<int>(9.0 * iterations) + 1;

  const Vec2 asym_pad{static_cast<double>(power_factor)};
  const Vec2 gain_scaling{1.0 / static_cast<double>(power_factor + 1)};
  const Vec2 output_scaling{1.0 + (1.0 / static_cast<double>(power_factor))};

  const bool needs_sign_restore = ((power_factor + 1) % 2 == 0);

  for (int i = 0; i < num_samples; ++i) {
    Vec2 s{left[i], right[i]};

    if (input_pad_ < 1.0) s = s * input_pad_;

    // Hi-rate pre-averaging.
    if (hi_rate) {
      Vec2 stored = s;
      s = (s + pre_waveshaper_avg_) * 0.5;
      pre_waveshaper_avg_ = stored;
    }

    // Hard clip to +/-1 before the waveshaper.
    s = dsp::Max(dsp::Min(s, 1.0), -1.0);

    // Asymmetric waveshaper.
    {
      s = s / asym_pad;
      Vec2 sh = -s;
      Vec2 sh_pos = dsp::Max(sh, 0.0);
      Vec2 sh_neg = dsp::Max(-sh, 0.0);
      sh = Vec2{1.0} + dsp::Sqrt(sh_pos) - dsp::Sqrt(sh_neg);
      s = s - s * dsp::Abs(s) * sh * 0.25;
      s = s * asym_pad;
    }

    // Tube polynomial saturation.
    {
      Vec2 factor = dsp::Pow(s, power_factor + 1);

      if (needs_sign_restore) {
        factor = dsp::Abs(factor) * Vec2{dsp::Sign(s.L()), dsp::Sign(s.R())};
      }

      factor = factor * gain_scaling;
      s = (s - factor) * output_scaling;
    }

    // Hi-rate post-averaging.
    if (hi_rate) {
      Vec2 stored = s;
      s = (s + post_tube_avg_) * 0.5;
      post_tube_avg_ = stored;
    }

    // Hysteresis.
    {
      Vec2 slew = hysteresis_prev_ - s;
      if (hi_rate) {
        Vec2 stored = s;
        s = (s + hysteresis_prev_) * 0.5;
        hysteresis_prev_ = stored;
      } else {
        hysteresis_prev_ = s;
      }
      Vec2 slew_pos = dsp::Max(slew, 0.0);
      Vec2 slew_neg = dsp::Max(-slew, 0.0);
      slew = Vec2{1.0} + dsp::Sqrt(slew_pos) * 0.5 - dsp::Sqrt(slew_neg) * 0.5;
      s = s - s * dsp::Abs(s) * slew * gain_scaling;

      // Hard-clip to +/-kTubeClipCeiling and restore unity gain.
      s = dsp::Max(dsp::Min(s, kTubeClipCeiling), -kTubeClipCeiling) *
          kTubeClipMakeup;
    }

    left[i] = s.L();
    right[i] = s.R();
  }
}

}  // namespace saturation
