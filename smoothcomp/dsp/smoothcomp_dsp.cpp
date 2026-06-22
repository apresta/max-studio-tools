#include "smoothcomp_dsp.h"

#include <algorithm>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"
#include "one_pole_coeffs.h"

namespace smoothcomp_dsp {

namespace {

// RMS warmup window used by the level detector on reset (avoids a hard attack
// transient on the very first samples after Prepare).
constexpr double kWarmupSec = 0.300;

// dB subtracted at input stage and added back at output stage.
// This achieves a familiar behavior with fairly neutral impact when both knobs
// are at 0 dB.
constexpr double kInputPad = 45.0;

constexpr double kAttackTimeSec[3] = {0.147, 0.168, 0.050};

constexpr double kMinLevel = 0.0005623413251903491;  // -65 dBFS

}  // namespace

void SplineTable::Build(std::vector<double> xs, std::vector<double> ys) {
  xs_ = std::move(xs);
  const int n = static_cast<int>(xs_.size());
  segments_.assign(n, Segment{0.0, 0.0, 0.0, 0.0});

  for (int i = 0; i < n - 1; ++i) {
    const double x1 = xs_[i];
    const double x2 = xs_[i + 1];
    const double y1 = ys[i];
    const double y2 = ys[i + 1];

    double dy1;
    double dy2;
    if (i - 1 >= 0) {
      dy1 = (y2 - ys[i - 1]) / (x2 - xs_[i - 1]);
    } else {
      dy1 = (y2 - y1) / (x2 - x1);
    }
    if (i + 2 < n) {
      dy2 = (ys[i + 2] - y1) / (xs_[i + 2] - x1);
    } else {
      dy2 = (y2 - y1) / (x2 - x1);
    }

    const double delta_x = x2 - x1;
    const double delta_y = y2 - y1;
    const double sum_dy = dy1 + dy2;
    const double delta_x2 = delta_x * delta_x;
    const double delta_x3 = delta_x2 * delta_x;

    Segment& s = segments_[i];
    s.c0 = (delta_x * sum_dy - 2.0 * delta_y) / delta_x3;
    s.c1 = -(((dy1 + sum_dy) * delta_x2) +
             ((sum_dy * x1 - delta_y) * 3.0 * delta_x) - (6.0 * x1 * delta_y)) /
           delta_x3;
    s.c2 = (dy1 * delta_x3 + (dy1 + sum_dy) * 2.0 * x1 * delta_x2 +
            (sum_dy * x1 - 2.0 * delta_y) * 3.0 * x1 * delta_x -
            6.0 * x1 * x1 * delta_y) /
           delta_x3;
    s.c3 =
        -(((x1 * dy1 - y1) * delta_x3) + ((dy1 + sum_dy) * x1 * x1 * delta_x2) +
          ((sum_dy * x1 - 3.0 * delta_y) * x1 * x1 * delta_x) -
          (2.0 * x1 * x1 * x1 * delta_y)) /
        delta_x3;
  }

  // Straight line for the first segment.
  {
    const double x0 = xs_[0];
    const double x1 = xs_[1];
    const double y0 = ys[0];
    const double y1 = ys[1];
    segments_[0] = Segment{0.0, 0.0, (y1 - y0) / (x1 - x0),
                           (x1 * y0 - y1 * x0) / (x1 - x0)};
  }

  // Straight line for the last segment.
  {
    const double x0 = xs_[n - 2];
    const double x1 = xs_[n - 1];
    const double y0 = ys[n - 2];
    const double y1 = ys[n - 1];
    segments_[n - 2] = Segment{0.0, 0.0, (y1 - y0) / (x1 - x0),
                               (x1 * y0 - y1 * x0) / (x1 - x0)};
  }
}

double SplineTable::Interpolate(double x) const noexcept {
  const int n = static_cast<int>(xs_.size());
  x = dsp::Clamp(x, xs_.front(), xs_.back());

  // Binary search for the containing segment.
  int left = 0;
  int right = n - 1;
  while (right - left > 1) {
    const int mid = (left + right) / 2;
    if (x < xs_[mid]) {
      right = mid;
    } else {
      left = mid;
    }
  }

  const Segment& s = segments_[left];
  return (x * x * x * s.c0) + (x * x * s.c1) + (x * s.c2) + s.c3;
}

double Processor::SecondsToCoef(double s) const noexcept {
  return std::pow(0.2, 1.0 / (s * sample_rate_));
}

void Processor::DesignSidechainHpf(Model model) noexcept {
  const double freq_hz = (model == Model::k60070B) ? 10.0 : 32.9;
  const dsp::OnePoleCoeffs c =
      dsp::ComputeOnePoleHPCoeffs(freq_hz, sample_rate_);
  sidechain_hpf_b0_ = c.b0;
  sidechain_hpf_b1_ = c.b1;
  sidechain_hpf_a1_ = c.a1;
}

void Processor::BuildTables() noexcept {
  // Release time table.
  static constexpr double kReleaseX[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  static constexpr double kReleaseY[3][6] = {
      {3.4, 6.5, 11.4, 22.0, 38.0, 64.0},
      {2.9, 5.0, 8.1, 15.6, 25.3, 44.0},
      {1.0, 1.9, 3.2, 5.8, 9.8, 16.9},
  };
  for (int m = 0; m < 3; ++m) {
    release_time_table_[m].Build(
        std::vector<double>(kReleaseX, kReleaseX + 6),
        std::vector<double>(kReleaseY[m], kReleaseY[m] + 6));
  }

  // Gain-reduction transfer table.
  static constexpr double kGrX[3][18] = {
      {0.0000000000000, 0.0002106149709, 0.0003686990847, 0.0006501130364,
       0.0011044513000, 0.0016804788350, 0.0023251490470, 0.0029973624020,
       0.0036842304960, 0.0043689564300, 0.0051289380440, 0.0061411803430,
       0.0074524766260, 0.0089647505090, 0.0108051531700, 0.0135162656500,
       0.0175188865400, 1.0000000000000},
      {0.0000000000000, 0.0001822577969, 0.0003190966185, 0.0005625128294,
       0.0009804053352, 0.0015538258180, 0.0022126659440, 0.0028988540950,
       0.0035634511280, 0.0042128857880, 0.0049078830270, 0.0057280561550,
       0.0068850359210, 0.0083364521810, 0.0104270474000, 0.0134988681400,
       0.0166703550600, 1.0000000000000},
      {0.0000000000000, 0.0002118388963, 0.0003683106940, 0.0006512028181,
       0.0011267457390, 0.0017141596740, 0.0023674152850, 0.0030527255360,
       0.0037581071970, 0.0045289696780, 0.0054706732940, 0.0066954517650,
       0.0081442878920, 0.0096584563910, 0.0111818552200, 0.0139228675800,
       0.0172579735900, 1.0000000000000},
  };
  static constexpr double kGrY[3][18] = {
      {1.19441334700, 1.19441334700, 1.17514954100, 1.16456995300,
       1.11193381000, 0.95086977030, 0.73942550540, 0.53572080760,
       0.37008517710, 0.24665404760, 0.16273974240, 0.10951501340,
       0.07469268520, 0.05049766445, 0.03420736713, 0.02404922493,
       0.01751888654, 0.00000000000},
      {1.03359767900, 1.03359767900, 1.01705228000, 1.00764867400,
       0.98704744970, 0.87920535940, 0.70365451860, 0.51811434480,
       0.35795275110, 0.23784291470, 0.15572572970, 0.10214781390,
       0.06900549260, 0.04695851429, 0.03301034542, 0.02401826988,
       0.01667035506, 0.00000000000},
      {1.20135432000, 1.20135432000, 1.17391162900, 1.16652211600,
       1.13437929100, 0.96992748780, 0.75286667980, 0.54561590170,
       0.37750617640, 0.25568776440, 0.17358290440, 0.11939927650,
       0.08162638572, 0.05440524970, 0.03539994489, 0.02477268373,
       0.01725797359, 0.00000000000},
  };
  for (int m = 0; m < 3; ++m) {
    gr_table_[m].Build(std::vector<double>(kGrX[m], kGrX[m] + 18),
                       std::vector<double>(kGrY[m], kGrY[m] + 18));
  }
}

void Processor::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;
  BuildTables();
  DesignSidechainHpf(params_.model);
  sidechain_hpf_state_.Clear();

  startup_counter_max_ = static_cast<int>(sample_rate_ * kWarmupSec);

  for (Channel& ch : channels_) {
    ch.level_state = 0.0;
    ch.feedback_gain = 1.0;
    ch.startup_counter = 0;
    ch.startup_level_sum = 0.0;
  }

  gr_meter_db_ = 0.0;

  // attack_coef_/release_coef_ depend on sample_rate_ (via SecondsToCoef()),
  // so refresh them here too.
  SetParams(params_);
}

void Processor::SetParams(const Params& p) noexcept {
  const bool model_changed = (p.model != params_.model);
  params_ = p;

  const int model_idx = static_cast<int>(params_.model);

  input_gain_linear_ = dsp::DbToLinear(params_.input_db - kInputPad);
  output_gain_linear_ = dsp::DbToLinear(params_.output_db + kInputPad);

  attack_coef_ = SecondsToCoef(kAttackTimeSec[model_idx]);
  release_coef_ = SecondsToCoef(
      release_time_table_[model_idx].Interpolate(params_.release_pos));

  if (model_changed) DesignSidechainHpf(params_.model);

  tube2_.SetTubeCharacter(params_.saturation);
}

double Processor::GetLevel(Channel& ch, double sidechain_sample) noexcept {
  double level = std::abs(sidechain_sample);
  level = dsp::Clamp(level, kMinLevel, 1.0);

  const double coef = (level > ch.level_state) ? attack_coef_ : release_coef_;

  if (ch.startup_counter >= startup_counter_max_) {
    ch.level_state = ((1.0 - coef) * level) + (ch.level_state * coef);
  } else {
    // RMS warmup: avoids an attack transient on the very first block(s)
    // after reset, before the attack/release filter has any history.
    ch.startup_level_sum += level * level;
    ch.startup_counter += 1;
    ch.level_state = std::sqrt(ch.startup_level_sum / ch.startup_counter);
  }

  return ch.level_state;
}

void Processor::ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                             const double* sc_r, int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  const int model_idx = static_cast<int>(params_.model);
  const bool has_external_sidechain = (sc_l != nullptr);

  // Tracks the gain actually applied to the last output sample (the
  // pre-update value of feedback_gain). Initialised to the current state
  // so a zero-length block leaves the meter unchanged.
  double last_gr_l = channels_[0].feedback_gain;
  double last_gr_r = channels_[1].feedback_gain;

  for (int i = 0; i < num_frames; ++i) {
    // Snapshot the gain being applied this iteration before it is
    // overwritten by the detector update below.
    last_gr_l = channels_[0].feedback_gain;
    last_gr_r = channels_[1].feedback_gain;

    // Feedback topology: each channel's sample is scaled by the gain
    // reduction computed from its previous sample.
    const double y_l = out_l[i] * input_gain_linear_ * last_gr_l;
    const double y_r = out_r[i] * input_gain_linear_ * last_gr_r;

    // Detector input: either the cell's own (already gain-reduced) output
    // or the sidechain sample (scaled by the Input control). feedback_gain is
    // intentionally not applied to an external sidechain.
    const double sc_sample_l =
        has_external_sidechain ? sc_l[i] * input_gain_linear_ : y_l;
    const double sc_sample_r =
        has_external_sidechain ? sc_r[i] * input_gain_linear_ : y_r;

    // Tick the sidechain HPF for both channels simultaneously using Vec2.
    const dsp::Vec2 sc_in(sc_sample_l, sc_sample_r);
    const dsp::Vec2 sc_out = sidechain_hpf_state_.Tick(
        sc_in, sidechain_hpf_b0_, sidechain_hpf_b1_, sidechain_hpf_a1_);

    const double level_l = GetLevel(channels_[0], sc_out.L());
    const double level_r = GetLevel(channels_[1], sc_out.R());

    // The detector level is scaled by 1.1 before the table lookup.
    channels_[0].feedback_gain =
        gr_table_[model_idx].Interpolate(level_l * 1.1);
    channels_[1].feedback_gain =
        gr_table_[model_idx].Interpolate(level_r * 1.1);

    out_l[i] = y_l * output_gain_linear_;
    out_r[i] = y_r * output_gain_linear_;
  }

  // Meter: gain that was actually applied to the last output sample.
  gr_meter_db_ =
      std::max(0.0, -dsp::LinearToDb(std::min(last_gr_l, last_gr_r)));

  tube2_.ProcessBlock(out_l, out_r, num_frames, sample_rate_);

  if (params_.soft_clip) {
    dsp::Apply(out_l, num_frames, dsp::SoftClip);
    dsp::Apply(out_r, num_frames, dsp::SoftClip);
  }
}

}  // namespace smoothcomp_dsp
