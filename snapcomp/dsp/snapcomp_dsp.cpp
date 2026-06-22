#include "snapcomp_dsp.h"

#include <algorithm>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"

namespace snapcomp_dsp {

namespace {

// Ratio control: index 0..6 -> compression ratio.
constexpr double kRatioValues[7] = {1.0, 2.0, 4.0, 6.0, 8.0, 12.0, 20.0};

// Attack/release mapping (explog(x, factor) == x^factor; factor < 1 skews the
// curve toward the low end faster).
constexpr double kMinAttackMs = 0.1;
constexpr double kMaxAttackMs = 100.0;
constexpr double kAttackExp = 1.5;

constexpr double kMinReleaseMs = 15.0;
constexpr double kMaxReleaseMs = 500.0;
constexpr double kReleaseExp = 2.5;

// Floor used before log()/log10() calls. Also guards the Punch topology's
// cold-start log10(0).
constexpr double kMinLevel = 0.000001;

}  // namespace

double Processor::CalcGainDb(double level_db) const noexcept {
  const double diff = level_db - params_.threshold_db;

  if (knee_width_db_ > 0.0) {
    if (2.0 * diff < -knee_width_db_) { return level_db; }
    if (2.0 * std::abs(diff) <= knee_width_db_) {
      const double tmp = diff + (0.5 * knee_width_db_);
      return level_db + (tmp * tmp * quad_factor_);
    }
    return params_.threshold_db + (diff * iratio_);
  }

  return (level_db >= params_.threshold_db)
             ? (params_.threshold_db + (diff * iratio_))
             : level_db;
}

double Processor::ComputeGain(double mx) noexcept {
  // Level detection in the linear domain: the smoothed input level itself
  // is what gets fed through the static curve, so a transient is heard
  // before the detector has caught up to it.
  const double level_db = dsp::LinearToDb(std::max(level_state_, kMinLevel));
  const double gain_db = CalcGainDb(level_db);

  const double xL = std::max(std::abs(mx), kMinLevel);
  if (xL > level_state_) {
    level_state_ = (attack_coef_ * level_state_) + ((1.0 - attack_coef_) * xL);
  } else {
    release_hold_state_ = std::max(xL, (release_coef_ * release_hold_state_) +
                                           ((1.0 - release_coef_) * xL));
    level_state_ = (attack_coef_ * level_state_) +
                   ((1.0 - attack_coef_) * release_hold_state_);
  }

  gain_correction_db_ = gain_db - level_db;
  return dsp::DbToLinear(gain_correction_db_);
}

void Processor::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;

  level_state_ = 0.0;
  release_hold_state_ = 0.0;
  gain_correction_db_ = 0.0;
  gr_meter_db_ = 0.0;

  // Attack/release coefficients are sample-rate-dependent, so refresh them.
  SetParams(params_);
}

void Processor::SetParams(const Params& p) noexcept {
  params_ = p;

  output_gain_linear_ = dsp::DbToLinear(params_.output_db);

  const double ratio = kRatioValues[params_.ratio_index];
  iratio_ = 1.0 / ratio;

  knee_width_db_ = params_.soft_knee ? 15.0 : 0.0;
  quad_factor_ =
      (knee_width_db_ > 0.0) ? ((iratio_ - 1.0) / (2.0 * knee_width_db_)) : 0.0;

  const double attack_ms =
      ((kMaxAttackMs - kMinAttackMs) * std::pow(params_.attack, kAttackExp)) +
      kMinAttackMs;
  const double release_ms = ((kMaxReleaseMs - kMinReleaseMs) *
                             std::pow(params_.release, kReleaseExp)) +
                            kMinReleaseMs;

  attack_coef_ = std::exp(-1.0 / (0.0005 * attack_ms * sample_rate_));
  release_coef_ = std::exp(-1.0 / (0.0005 * release_ms * sample_rate_));
}

void Processor::ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                             const double* sc_r, int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  for (int i = 0; i < num_frames; ++i) {
    const double in_l = out_l[i];
    const double in_r = out_r[i];

    // Detector source: when an external sidechain is present, use the
    // louder of its two channels; otherwise use the louder pre-gained main
    // channel.
    const double mx = sc_l ? std::max(std::abs(sc_l[i]), std::abs(sc_r[i]))
                           : std::max(std::abs(in_l), std::abs(in_r));

    const double gain_lin = ComputeGain(mx);

    double y_l = in_l * gain_lin * output_gain_linear_;
    double y_r = in_r * gain_lin * output_gain_linear_;

    out_l[i] = y_l;
    out_r[i] = y_r;
  }

  // Saturation.
  if (params_.sat_enable) spiral2_.ProcessBlock(out_l, out_r, num_frames);

  if (params_.soft_clip) {
    dsp::Apply(out_l, num_frames, dsp::SoftClip);
    dsp::Apply(out_r, num_frames, dsp::SoftClip);
  }

  gr_meter_db_ = -gain_correction_db_;
}

}  // namespace snapcomp_dsp
