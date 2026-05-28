// This file is derived from the original Tube2 by Airwindows.
// Copyright (c) Airwindows (MIT license)

#include "Tube2.h"

#include <cassert>

#include "dsp_math.h"

using dsp::Vec2;

Tube2::Tube2(double input_pad, double tube_character)
    : input_pad_(dsp::Clamp01(input_pad)),
      tube_character_(dsp::Clamp01(tube_character)) {
  Reset();
}

void Tube2::SetInputPad(double value) noexcept {
  input_pad_ = dsp::Clamp01(value);
}

void Tube2::SetTubeCharacter(double value) noexcept {
  tube_character_ = dsp::Clamp01(value);
}

double Tube2::GetInputPad() const noexcept { return input_pad_; }

double Tube2::GetTubeCharacter() const noexcept { return tube_character_; }

void Tube2::Reset() noexcept {
  pre_waveshaper_avg_ = post_tube_avg_ = hysteresis_prev_ = Vec2(0.0);
}

void Tube2::ProcessBlock(double* out_l, double* out_r, int num_samples,
                         double sample_rate) noexcept {
  assert(num_samples >= 0);

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

  const Vec2 asym_pad(static_cast<double>(power_factor));
  const Vec2 gain_scaling(1.0 / static_cast<double>(power_factor + 1));
  const Vec2 output_scaling(1.0 + 1.0 / static_cast<double>(power_factor));

  const bool needs_sign_restore = ((power_factor + 1) % 2 == 0);

  for (int i = 0; i < num_samples; ++i) {

    double s_l = out_l[i];
    double s_r = out_r[i];

    Vec2 s(s_l, s_r);

    if (input_pad_ < 1.0) s = s * input_pad_;

    // Hi-rate pre-averaging.
    if (hi_rate) {
      Vec2 stored = s;
      s = (s + pre_waveshaper_avg_) * 0.5;
      pre_waveshaper_avg_ = stored;
    }

    // Hard clip to +/-1 before the waveshaper.
    s = dsp::max(dsp::min(s, 1.0), -1.0);

    // Asymmetric waveshaper.
    {
      s = s / asym_pad;
      Vec2 sh = -s;
      Vec2 sh_pos = dsp::max(sh, 0.0);
      Vec2 sh_neg = dsp::max(-sh, 0.0);
      sh = Vec2(1.0) + dsp::sqrt(sh_pos) - dsp::sqrt(sh_neg);
      s = s - s * dsp::abs(s) * sh * 0.25;
      s = s * asym_pad;
    }

    // Tube polynomial saturation.
    {
      Vec2 factor = dsp::pow_int(s, power_factor + 1);

      if (needs_sign_restore) {
        factor = dsp::abs(factor) * Vec2(dsp::sign(s.l()), dsp::sign(s.r()));
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
      Vec2 slew_pos = dsp::max(slew, 0.0);
      Vec2 slew_neg = dsp::max(-slew, 0.0);
      slew = Vec2(1.0) + dsp::sqrt(slew_pos) * 0.5 - dsp::sqrt(slew_neg) * 0.5;
      s = s - s * dsp::abs(s) * slew * gain_scaling;

      // Hard-clip to +/-kTubeClipCeiling and restore unity gain.
      s = dsp::max(dsp::min(s, kTubeClipCeiling), -kTubeClipCeiling) *
          kTubeClipMakeup;
    }

    out_l[i] = s.l();
    out_r[i] = s.r();
  }
}
