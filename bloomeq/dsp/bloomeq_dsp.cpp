// This file is derived from the original EQP-WDF-1A by ABSounds.
// Copyright (c) ABSounds (GPL-3.0 license)

#include "bloomeq_dsp.h"

#include <cassert>

#include "denormal_guard.h"

namespace bloomeq_dsp {

void Processor::Apply(const Parameters& p) {
  eqp1a_l_.SetParams(p.lo_boost, p.lo_cut, p.hi_boost, p.hi_cut, p.hi_bandwidth,
                     p.lo_freq, p.hi_boost_freq, p.hi_cut_freq);
  eqp1a_r_.SetParams(p.lo_boost, p.lo_cut, p.hi_boost, p.hi_cut, p.hi_bandwidth,
                     p.lo_freq, p.hi_boost_freq, p.hi_cut_freq);
  tube2_.SetInputPad(p.gain);
  tube2_.SetTubeCharacter(p.saturation);
  current_params_ = p;
}

void Processor::Prepare(double sample_rate) {
  assert(sample_rate > 0.0);
  sample_rate_ = sample_rate;
  eqp1a_l_.Prepare(sample_rate);
  eqp1a_r_.Prepare(sample_rate);
  tube2_.Reset();
  Apply(current_params_);
  prepared_ = true;
}

void Processor::SetParameters(const Parameters& p) {
  const Parameters& c = current_params_;
  const bool needs_apply =
      p.lo_boost != c.lo_boost || p.lo_cut != c.lo_cut ||
      p.lo_freq != c.lo_freq || p.hi_boost != c.hi_boost ||
      p.hi_boost_freq != c.hi_boost_freq || p.hi_bandwidth != c.hi_bandwidth ||
      p.hi_cut != c.hi_cut || p.hi_cut_freq != c.hi_cut_freq ||
      p.gain != c.gain || p.saturation != c.saturation;

  if (needs_apply) {
    Apply(p);
  } else {
    current_params_ = p;
  }
}

void Processor::ProcessBlock(double* out_l, double* out_r,
                             int num_frames) noexcept {
  ScopedDenormalGuard denormal_guard;
  assert(prepared_);
  if (num_frames == 0) return;

  const bool phase = current_params_.phase_inv;
  const bool eq_on = current_params_.eq_enable;

  if (phase) {
    for (int i = 0; i < num_frames; ++i) {
      out_l[i] = -out_l[i];
      out_r[i] = -out_r[i];
    }
  }

  // EQ (optional): independent WDF circuits per channel.
  if (eq_on) {
    for (int i = 0; i < num_frames; ++i)
      out_l[i] = eqp1a_l_.ProcessSample(out_l[i]);
    for (int i = 0; i < num_frames; ++i)
      out_r[i] = eqp1a_r_.ProcessSample(out_r[i]);
  }

  // Saturation: single stereo Tube2 stage (in-place).
  tube2_.ProcessBlock(out_l, out_r, num_frames, sample_rate_);
}

}  // namespace bloomeq_dsp
