#include "contoureq_dsp.h"

#include "denormal_guard.h"
#include "dsp_math.h"

namespace contoureq_dsp {

void Processor::Prepare(double sample_rate) {
  sample_rate_ = sample_rate;
  eq_v1_.Prepare(sample_rate);
  eq_v3_.Prepare(sample_rate);
}

void Processor::SetParameters(const Parameters& p) {
  // If the model changed, re-prepare the newly selected engine so stale
  // filter state from a previous run never bleeds into the new algorithm.
  if (p.model != params_.model) {
    if (p.model == 0) {
      eq_v1_.Prepare(sample_rate_);
    } else {
      eq_v3_.Prepare(sample_rate_);
    }
  }

  if (p.model == 0) {
    // Baxandall: input_gain knob drives the output trim.
    eq_v1_.SetOutputLevel(p.gain);
    eq_v1_.SetTrebleLevel(p.treble);
    eq_v1_.SetBassLevel(p.bass);
  } else {
    // Baxandall3: input_gain knob drives the input drive / soft-clip level.
    eq_v3_.SetInputGain(p.gain);
    eq_v3_.SetTrebleLevel(p.treble);
    eq_v3_.SetBassLevel(p.bass);
  }

  params_ = p;
}

void Processor::ProcessBlock(double* out_l, double* out_r, int frames) {
  ScopedDenormalGuard denormal_guard;
  if (frames == 0) return;

  if (params_.phase_inv) dsp::InvertPhase(out_l, out_r, frames);

  if (params_.model == 0) {
    eq_v1_.ProcessBlock(out_l, out_r, frames);
  } else {
    eq_v3_.ProcessBlock(out_l, out_r, frames);
  }
}

}  // namespace contoureq_dsp
