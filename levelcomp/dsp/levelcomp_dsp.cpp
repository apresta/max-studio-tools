// This file is derived from the original 4A-2A by Queen Mary University.
// Copyright (c) Chin-Yun Yu and Gyorgy Fazekas (MPL-2.0 license).
//
// DSP parameters have been rescaled to better match the reference LA-2A units.

#include "levelcomp_dsp.h"

#include <algorithm>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"

namespace levelcomp_dsp {

namespace {

// Peak-to-parameter mapping tables (13 control points x 5 params).
// Index order: [threshold_db, ratio, attack_ms, release_ms, makeup_db].
constexpr double kCompPoints[13][5] = {
    {-13.075952529907227, 5.435610294342041, 30.516525268554688,
     470.176513671875, 0.697806715965271},
    {-14.217720985412598, 4.510651588439941, 17.315229415893555,
     371.4613342285156, 0.6503340005874634},
    {-15.320412635803223, 4.193428039550781, 11.579639434814453,
     324.1263427734375, 0.6253266930580139},
    {-16.131834030151367, 4.151314735412598, 8.878186225891113,
     300.0677795410156, 0.6053885817527771},
    {-16.88168716430664, 4.212928295135498, 7.19603967666626, 283.0182189941406,
     0.5758121609687805},
    {-17.586275100708008, 4.265491962432861, 5.948177814483643,
     266.1832580566406, 0.5347366333007812},
    {-19.835796356201172, 4.289010047912598, 3.8497214317321777,
     254.91220092773438, 0.4238012433052063},
    {-21.904220581054688, 4.237095355987549, 2.8647751808166504,
     242.99441528320312, 0.33676016330718994},
    {-24.04867935180664, 4.137104511260986, 2.2430710792541504,
     234.94378662109375, 0.29632773995399475},
    {-27.549327850341797, 3.9736087322235107, 1.7689090967178345,
     248.10983276367188, 0.047231659293174744},
    {-31.510976791381836, 3.874948501586914, 1.265961766242981,
     245.48947143554688, 0.03817363455891609},
    {-33.69828796386719, 3.791938066482544, 1.0862258672714233,
     244.5567626953125, 0.1468079835176468},
    {-35.34008026123047, 3.711195945739746, 0.9788835048675537,
     244.128662109375, 0.3002852201461792}};

constexpr double kLimitPoints[13][5] = {
    {-12.927042961120605, 6.511876583099365, 34.9532585144043,
     450.67974853515625, 0.7328264117240906},
    {-14.168609619140625, 4.7533416748046875, 17.140609741210938,
     350.7540283203125, 0.6642460227012634},
    {-15.303581237792969, 4.407552719116211, 11.43269157409668,
     302.6725769042969, 0.6261841058731079},
    {-16.005937576293945, 4.388558387756348, 8.920721054077148,
     284.55804443359375, 0.602465033531189},
    {-16.920991897583008, 4.4896087646484375, 6.798874378204346,
     269.3534240722656, 0.5653380155563354},
    {-17.654096603393555, 4.59498929977417, 5.717840671539307,
     258.5350341796875, 0.5199187397956848},
    {-19.92206573486328, 4.652866363525391, 3.6811776161193848,
     246.35675048828125, 0.4087653160095215},
    {-22.056045532226562, 4.640170097351074, 2.7373666763305664,
     238.0178985595703, 0.32971465587615967},
    {-24.036930084228516, 4.611164093017578, 2.213550567626953,
     233.04624938964844, 0.29381534457206726},
    {-27.528459548950195, 4.570751190185547, 1.6432621479034424,
     233.04624938964844, 0.23869140446186066},
    {-31.49081039428711, 4.547694206237793, 1.234323501586914,
     243.91519165039062, 0.20797717571258545},
    {-33.483543395996094, 4.520030498504639, 1.0648689270019531,
     251.61624145507812, 0.242126926779747},
    {-34.903114318847656, 4.471458435058594, 0.9657101035118103,
     256.3176574707031, 0.3162499666213989}};

}  // namespace

void Processor::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;
  g_prev_ = 1.0;
  g_buf_.clear();
  gr_meter_db_ = 0.0;
}

void Processor::ApplyPeakMapping() noexcept {
  // Resolve peak_reduction + limit_mode into the five intermediate DSP
  // parameters, then convert them immediately to the cached coefficients that
  // ProcessBlock() reads. Using locals keeps the named DSP quantities visible
  // for readability without requiring extra members.
  double threshold_db;
  double ratio;
  double attack_ms;
  double release_ms;
  double makeup_db;

  if (params_.peak_reduction == 0.0) {
    threshold_db = 0.0;
    ratio = 1.0;
    attack_ms = 1.0;
    release_ms = 100.0;
    makeup_db = 0.0;
  } else {
    const double (*table)[5] = params_.limit_mode ? kLimitPoints : kCompPoints;

    // Softplus mapping: slow slope 0–40, ~linear 40–100.
    // kCenter: display value where slope = 50% of linear (transition midpoint).
    // kK: steepness of transition (higher = sharper).
    static constexpr double kK = 0.2;
    static constexpr double kCenter = 15.0;
    auto sp = [](double x) { return std::log1p(std::exp(x)); };
    const double normalized =
        12.0 *
        (sp(kK * (params_.peak_reduction - kCenter)) -
         sp(kK * (0.0 - kCenter))) /
        (sp(kK * (100.0 - kCenter)) - sp(kK * (0.0 - kCenter)));

    const int lower = std::min(11, static_cast<int>(normalized));
    const int upper = lower + 1;
    const double p = normalized - lower;  // fractional part

    threshold_db = (p * table[upper][0]) + ((1.0 - p) * table[lower][0]);
    ratio = (p * table[upper][1]) + ((1.0 - p) * table[lower][1]);
    attack_ms = (p * table[upper][2]) + ((1.0 - p) * table[lower][2]);
    release_ms = (p * table[upper][3]) + ((1.0 - p) * table[lower][3]);
    makeup_db = (p * table[upper][4]) + ((1.0 - p) * table[lower][4]);
  }

  // Cache all derived coefficients so ProcessBlock() reads only members.
  th_linear_ = dsp::DbToLinear(threshold_db);
  makeup_linear_ = dsp::DbToLinear(makeup_db);
  comp_slope_ = 1.0 - (1.0 / ratio);
  at_ = MsToCoef(attack_ms);
  rt_ = MsToCoef(release_ms);
}

void Processor::SetParams(const Params& p) noexcept {
  params_ = p;
  ApplyPeakMapping();
  output_linear_ = dsp::DbToLinear(params_.output_db);
  triode_.SetTriodeDrive(params_.saturation);
}

double Processor::MsToCoef(double ms) const noexcept {
  // One-pole IIR coefficient: 1 - exp(-2200 / ms / sample_rate).
  // The factor 2200 preserves the time-constant convention from the original.
  return 1.0 - std::exp(-2200.0 / ms / sample_rate_);
}

void Processor::ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                             const double* sc_r, int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;
  if (num_frames == 0) return;

  // Grow the gain buffer lazily. In normal use this only happens once per
  // Prepare() call, since the host block size is fixed after dspsetup.
  if (static_cast<int>(g_buf_.size()) < num_frames + 1) {
    g_buf_.resize(num_frames + 1, 1.0);
  }

  // Seed g_buf_[0] with the gain carried over from the previous block so the
  // smoothing filter has no discontinuity at block boundaries.
  g_buf_[0] = g_prev_;

  // Step 1: instantaneous gain from the detector source.
  // With no sidechain: max(|l|, |r|) gives a linked-stereo detector so a loud
  // transient on either channel drives gain reduction on both.
  // With an external sidechain: max(|sc_l[i]|, |sc_r[i]|) drives detection
  // on both channels, the same linked-stereo behavior applied to the
  // sidechain pair.
  // g[n] = min(1, (peak / threshold) ^ -compSlope)
  for (int i = 0; i < num_frames; ++i) {
    const double peak = sc_l ? std::max(std::abs(sc_l[i]), std::abs(sc_r[i]))
                             : std::max(std::abs(out_l[i]), std::abs(out_r[i]));
    g_buf_[i + 1] = std::min(1.0, std::pow(peak / th_linear_, -comp_slope_));
  }

  // Step 2: smooth gain with asymmetric attack/release filter.
  // Causal one-pole IIR; coefficient switches between 'at_' and 'rt_' depending
  // on whether the gain is falling (attack) or rising (release).
  std::partial_sum(g_buf_.cbegin(), g_buf_.cbegin() + num_frames + 1,
                   g_buf_.begin(), [this](double g_prev, double g_inst) {
                     const double coef = (g_inst < g_prev) ? at_ : rt_;
                     return (g_inst * coef) + (g_prev * (1.0 - coef));
                   });

  // Save the last smoothed gain for the next block.
  g_prev_ = g_buf_[num_frames];
  gr_meter_db_ = -dsp::LinearToDb(g_prev_);

  // Step 3: apply identical smoothed gain + makeup to both channels.
  for (int i = 0; i < num_frames; ++i) {
    const double g = g_buf_[i + 1] * makeup_linear_ * output_linear_;
    out_l[i] *= g;
    out_r[i] *= g;
  }

  if (params_.saturation > 0.0) triode_.ProcessBlock(out_l, out_r, num_frames);

  if (params_.soft_clip) {
    dsp::Apply(out_l, num_frames, dsp::SoftClip);
    dsp::Apply(out_r, num_frames, dsp::SoftClip);
  }
}

}  // namespace levelcomp_dsp
