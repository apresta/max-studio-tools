#include "punchcomp_dsp.h"

#include <algorithm>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"

namespace punchcomp_dsp {

namespace {

constexpr double kInternalThreshold = -20.0;

// Fixed-coefficient safety limiter. Linked on L+R, hard knee.
const double kLimiterThreshold =
    std::pow(10.0, (2.0 * 1.5) - 2.0);  // 10.0 linear (~+20 dBFS on 0.5*(L+R))
const double kLimiterAttack =
    std::pow(10.0, -0.01 - (2.0 * 0.5));  // ~0.0977 / sample
const double kLimiterRelease =
    std::pow(10.0, -2.0 - (3.0 * 0.5));  // ~0.000316 / sample

}  // namespace

double Processor::OnePoleCoefFromTau(double tau_s, double sr) noexcept {
  if (tau_s <= 0.0 || sr <= 0.0) return 0.0;
  return std::exp(-1.0 / (tau_s * sr));
}

void Processor::ApplyRatioMode() noexcept {
  switch (params_.ratio_mode) {
    case RatioMode::k4to1:
      ratio_ = 4.0;
      allin_ = false;
      break;
    case RatioMode::k8to1:
      ratio_ = 8.0;
      allin_ = false;
      break;
    case RatioMode::k12to1:
      ratio_ = 12.0;
      allin_ = false;
      break;
    case RatioMode::k20to1:
      ratio_ = 20.0;
      allin_ = false;
      break;
    case RatioMode::kAllButtonsIn:
      // Note: ratio_ is never read while allin_ is true.
      ratio_ = 20.0;
      allin_ = true;
      break;
  }
}

void Processor::UpdateTimeConstants() noexcept {
  double attack_s = 2.0 * params_.attack_us * 1.0e-6;

  const double release_s =
      allin_ ? (params_.release_ms / 10000.0) : (params_.release_ms / 1000.0);

  atcoef_ = OnePoleCoefFromTau(attack_s, sample_rate_);
  relcoef_ = OnePoleCoefFromTau(release_s, sample_rate_);

  // Fixed ratio-latch coefficients: a 10 us "attack" and a 500 ms
  // "release" on the All Buttons In ratio creep.
  ratatcoef_ = OnePoleCoefFromTau(0.00001, sample_rate_);
  ratrelcoef_ = OnePoleCoefFromTau(0.5, sample_rate_);
}

void Processor::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;

  rundb_ = 0.0;
  runratio_ = 0.0;
  ovrlgain_ = 1.0;
  gr_meter_db_ = 0.0;

  ApplyRatioMode();
  UpdateTimeConstants();
  coils_.Prepare(sample_rate_);
}

void Processor::SetParams(const Params& p) noexcept {
  params_ = p;

  // ApplyRatioMode() must run first: UpdateTimeConstants() reads allin_ to
  // pick the All Buttons In release-time special case.
  ApplyRatioMode();
  UpdateTimeConstants();

  input_linear_ = dsp::DbToLinear(params_.input_db);
  output_linear_ = dsp::DbToLinear(params_.output_db);

  coils_.SetSaturation(params_.saturation);
}

void Processor::ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                             const double* sc_r, int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  for (int i = 0; i < num_frames; ++i) {
    const double in_l = out_l[i];
    const double in_r = out_r[i];

    double driven_l = in_l * input_linear_;
    double driven_r = in_r * input_linear_;

    double det_l;
    double det_r;
    if (sc_l != nullptr) {
      det_l = sc_l[i] * input_linear_;
      det_r = sc_r[i] * input_linear_;
    } else {
      det_l = driven_l;
      det_r = driven_r;
    }

    // Linked-stereo peak detector.
    const double det = std::max(std::abs(det_l), std::abs(det_r));
    const double overdb =
        std::max(0.0, dsp::LinearToDb(det) - kInternalThreshold);

    // All Buttons In ratio-boost creep.
    const double creep_target = (overdb - rundb_ > 5.0) ? 4.0 : runratio_;

    // Attack/release envelope (rundb_) and the ratio-creep smoother
    // (runratio_) share the same attack/release branch.
    if (overdb > rundb_) {
      rundb_ = overdb + (atcoef_ * (rundb_ - overdb));
      runratio_ = creep_target + (ratatcoef_ * (runratio_ - creep_target));
    } else {
      rundb_ = overdb + (relcoef_ * (rundb_ - overdb));
      runratio_ = creep_target + (ratrelcoef_ * (runratio_ - creep_target));
    }

    // Gain computer.
    const double cratio = allin_ ? (4.0 + runratio_) : ratio_;
    const double allratio = allin_ ? 1.4 : 1.0;
    const double gr_db = -rundb_ * allratio * (cratio - 1.0) / cratio;
    const double grv = dsp::DbToLinear(gr_db);
    gr_meter_db_ = -gr_db;

    // Apply gain reduction + output makeup.
    double comp_l = driven_l * grv * output_linear_;
    double comp_r = driven_r * grv * output_linear_;

    // Fixed-coefficient safety limiter. It detects the
    // L+R *sum* (not max(|L|,|R|) -- a different detector than the
    // compressor above), scaled by the *previous* sample's ovrlgain_, then
    // updates ovrlgain_, then applies the just-updated value to this
    // sample's output.
    const double ovrl_level = 0.5 * ovrlgain_ * std::abs(comp_l + comp_r);
    if (ovrl_level > kLimiterThreshold) {
      ovrlgain_ -= kLimiterAttack * (ovrl_level - kLimiterThreshold);
    } else {
      ovrlgain_ += kLimiterRelease * (1.0 - ovrlgain_);
    }

    out_l[i] = comp_l * ovrlgain_;
    out_r[i] = comp_r * ovrlgain_;
  }

  if (params_.saturation > 0.0) coils_.ProcessBlock(out_l, out_r, num_frames);

  if (params_.soft_clip) {
    dsp::Apply(out_l, num_frames, dsp::SoftClip);
    dsp::Apply(out_r, num_frames, dsp::SoftClip);
  }
}

}  // namespace punchcomp_dsp
