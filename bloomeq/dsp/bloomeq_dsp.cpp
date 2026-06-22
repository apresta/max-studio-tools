// This file is derived from the original EQP-WDF-1A by ABSounds.
// Copyright (c) ABSounds (GPL-3.0 license).

#include "bloomeq_dsp.h"

#include <cassert>

#include "denormal_guard.h"
#include "dsp_math.h"

namespace bloomeq_dsp {

void Processor::Apply(const Params& p) noexcept {
  // Clamp continuous knob values away from zero: several WDF element
  // impedances become degenerate (zero resistance / infinite susceptance)
  // at exactly 0.0.
  const double lo_boost = std::max(p.lo_boost, kMinKnob);
  const double lo_cut = std::max(p.lo_cut, kMinKnob);
  const double hi_boost = std::max(p.hi_boost, kMinKnob);
  const double hi_cut = std::max(p.hi_cut, kMinKnob);
  const double hi_bandwidth = std::max(p.hi_bandwidth, kMinKnob);
  eqp1a_l_.SetParams(lo_boost, lo_cut, hi_boost, hi_cut, hi_bandwidth,
                     p.lo_freq, p.hi_boost_freq, p.hi_cut_freq);
  eqp1a_r_.SetParams(lo_boost, lo_cut, hi_boost, hi_cut, hi_bandwidth,
                     p.lo_freq, p.hi_boost_freq, p.hi_cut_freq);
  tube2_.SetInputPad(p.gain);
  tube2_.SetTubeCharacter(p.saturation);
  params_ = p;
}

void Processor::Prepare(double sample_rate) noexcept {
  assert(sample_rate > 0.0);
  sample_rate_ = sample_rate;
  eqp1a_l_.Prepare(sample_rate);
  eqp1a_r_.Prepare(sample_rate);
  tube2_.Reset();
  Apply(params_);
}

void Processor::SetParams(const Params& p) noexcept {
  const Params& c = params_;
  const bool needs_apply =
      p.lo_boost != c.lo_boost || p.lo_cut != c.lo_cut ||
      p.lo_freq != c.lo_freq || p.hi_boost != c.hi_boost ||
      p.hi_boost_freq != c.hi_boost_freq || p.hi_bandwidth != c.hi_bandwidth ||
      p.hi_cut != c.hi_cut || p.hi_cut_freq != c.hi_cut_freq ||
      p.gain != c.gain || p.saturation != c.saturation;

  if (needs_apply) {
    Apply(p);
  } else {
    params_ = p;
  }
}

void Processor::ProcessBlock(double* out_l, double* out_r,
                             int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  if (params_.phase_inv) dsp::InvertPhase(out_l, out_r, num_frames);

  // EQ (optional): independent WDF circuits per channel.
  if (params_.eq_enable) {
    for (int i = 0; i < num_frames; ++i) {
      out_l[i] = eqp1a_l_.ProcessSample(out_l[i]);
      out_r[i] = eqp1a_r_.ProcessSample(out_r[i]);
    }
  }

  // Saturation: single stereo Tube2 stage (in-place).
  tube2_.ProcessBlock(out_l, out_r, num_frames, sample_rate_);
}

}  // namespace bloomeq_dsp
