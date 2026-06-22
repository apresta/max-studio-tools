#include "glidecomp_dsp.h"

#include <algorithm>
#include <cmath>

#include "denormal_guard.h"
#include "dsp_math.h"
#include "interp_kernels.h"

namespace glidecomp_dsp {

namespace {

// Measured tables.
const std::vector<double> kT3Ratio = []() {
  std::vector<double> v(25, 0.01);
  const double head[] = {0.999999,  0.99,       0.5626293, 0.2993541,
                         0.1536661, 0.07558671, 0.036547,  0.01702715};
  std::copy(std::begin(head), std::end(head), v.begin());
  return v;
}();

const std::vector<double> kT4Blend = {
    0.0,       0.03416149, 0.07852706, 0.1228926, 0.1672582, 0.2116238,
    0.2559893, 0.3003549,  0.3447205,  0.3890861, 0.4334517, 0.4778172,
    0.5221828, 0.5665483,  0.6109139,  0.6552795, 0.6996451, 0.7440106,
    0.7883762, 0.8327418,  0.8771074,  0.921473,  0.9658385, 0.999999};

const std::vector<double> kT5Sidechain = []() {
  std::vector<double> v(25, 0.00001000000);
  const double head[] = {
      0.01,       1.0,      0.9947661,  0.9844928,  0.9651101,   0.9302186,
      0.8630559,  0.755419, 0.6082814,  0.4397123,  0.2796561,   0.162245,
      0.08780019, 0.04508,  0.02209106, 0.01019185, 0.004130001, 0.001069335};
  std::copy(std::begin(head), std::end(head), v.begin());
  return v;
}();

const std::vector<double> kT6Mix = {
    0.0,       0.0434687, 0.08694739, 0.1304261, 0.1739048, 0.2173835,
    0.2608622, 0.3043409, 0.3478196,  0.3912983, 0.434777,  0.4782557,
    0.5217344, 0.565213,  0.6086918,  0.6521704, 0.6956491, 0.7391278,
    0.7826065, 0.8260852, 0.8695639,  0.9130426, 0.9565213, 0.999999};

const std::vector<double> kT8AttackRate = []() {
  std::vector<double> v(46, 0.0);
  std::fill(v.begin(), v.begin() + 11, 0.002257127);
  const double tail[] = {
      0.000807641,    0.0002590034,   0.0001466583,   0.000105361,
      0.00008688696,  0.00007712693,  0.00007082194,  0.00006535164,
      0.00005942077,  0.00005248035,  0.00004474115,  0.00003699339,
      0.00002985739,  0.00002377450,  0.00001915936,  0.00001565825,
      0.00001302099,  0.00001102717,  0.000009554097, 0.000008418394,
      0.000007519858, 0.000006788958, 0.000006188009, 0.000005677829,
      0.000005232528, 0.000004838532, 0.000004491788, 0.000004193505,
      0.000003938723, 0.000003725471, 0.000003552736, 0.000003421820,
      0.000003326748, 0.000003267550, 0.000003245660};
  std::copy(std::begin(tail), std::end(tail), v.begin() + 11);
  return v;
}();

const std::vector<double> kT9ReleaseRate = {
    0.00004848326,  0.00004848326,  0.00004848326,  0.00004188835,
    0.00002785662,  0.00001560057,  0.00001201397,  0.000008427365,
    0.000005328864, 0.000004453937, 0.000003579009, 0.000002704082,
    0.000002101815, 0.000001772209, 0.000001442603, 0.000001112997,
    8.481028e-7,    6.281776e-7,    4.082524e-7,    2.060375e-7,
    1.126142e-7,    1.919095e-8,    3.280834e-10,   -4.332800e-9};

const std::vector<double> kT10OutputComp = []() {
  std::vector<double> v(25, 1.0);
  const double head[] = {0.8766871, 0.8766871, 0.9343757, 0.966794,
                         0.9838194, 0.9926132, 0.9970101, 0.9992085};
  std::copy(std::begin(head), std::end(head), v.begin());
  return v;
}();

const std::vector<double> kT12Detector = []() {
  std::vector<double> v(48, 0.0);
  const double pos[] = {
      0.000002987261, 0.000005974523, 0.00001194905, 0.00002389809,
      0.00004779618,  0.00009559237,  0.0001911847,  0.0003823695,
      0.0007647389,   0.001529478,    0.003058956,   0.006117912,
      0.01223582,     0.02447165,     0.04894329,    0.09788658,
      0.1957732,      0.3915463,      0.7830927};
  std::copy(std::begin(pos), std::end(pos), v.begin());
  std::fill(v.begin() + 19, v.begin() + 29, 1.0);
  const double neg[] = {0.7810927,  0.3895463,   0.1937732,
                        0.09588659, 0.04694329,  0.02247165,
                        0.01023582, 0.004117912, 0.001058956};
  std::copy(std::begin(neg), std::end(neg), v.begin() + 29);
  // Indices 38..47 remain 0.
  return v;
}();

const std::vector<double> kT13GainShape = []() {
  std::vector<double> v(252, 1.0);
  const double head[] = {
      0.0,        0.002895139, 0.01001967, 0.01859283, 0.0278201, 0.03739988,
      0.04719125, 0.05711957,  0.06714159, 0.07723056, 0.087369,  0.09754504,
      0.1077503,  0.1179788,   0.1282261,  0.1384886,  0.1487638, 0.1590496,
      0.1693444,  0.1796468,   0.1899559,  0.2002707,  0.2105905, 0.2209146,
      0.2312426,  0.2415741,   0.2519086,  0.2622458,  0.2725855, 0.2829274,
      0.2932713,  0.303617,    0.3139643,  0.3243132,  0.3346635, 0.345015,
      0.3553677,  0.3657215,   0.3760763,  0.3864319,  0.3967885, 0.4071458,
      0.4175039,  0.4278626,   0.4382221,  0.4485821,  0.4589426, 0.4693037,
      0.4796653,  0.4900274,   0.5003899,  0.5107529,  0.5211161, 0.5314798,
      0.5418439,  0.5522082,   0.562573,   0.5729379,  0.5833032, 0.5936688,
      0.6040345,  0.6144006,   0.6247668,  0.6351333,  0.6455,    0.6558669,
      0.666234,   0.6766013,   0.6869688,  0.6973364,  0.7077042, 0.7180721,
      0.7284402,  0.7388085,   0.7491769,  0.7595453,  0.769914,  0.7802827,
      0.7906516,  0.8010206,   0.8113897,  0.8217589,  0.8321282, 0.8424976,
      0.8528671,  0.8632367,   0.8736063,  0.883976,   0.8943459, 0.9047158,
      0.9150858,  0.9254559,   0.935826,   0.9461962,  0.9565665, 0.9669368,
      0.9773072,  0.9876777,   0.9980482};
  std::copy(std::begin(head), std::end(head), v.begin());
  // Indices 99..251 remain 1.
  return v;
}();

// Fixed gain-stage constants.
constexpr double kDetectorInputTrim = 0.08098298;
constexpr double kMixA1 = 0.01193628;
constexpr double kMixB1 = 0.9323384;
constexpr double kMixA2 = 0.4595526;
constexpr double kMixB2 = 1.0;
constexpr double kOutputScaler = 33.768673;

// Feedback-gain epsilon: sets the no-signal gain reduction floor and the
// curvature of gain_reduction = kFeedbackEps / clamp(inv_gr + kFeedbackEps).
constexpr double kFeedbackEps = 0.0029900903;

// lpf1/lpf2 blend weights used to form the feedback signal (inv_gr).
constexpr double kLpf1FeedbackWeight = 0.2998201;
constexpr double kLpf2FeedbackWeight = 0.079904087;

}  // namespace

void Processor::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;

  // Time constants measured from the original hardware. lpf1 is the fast
  // smoother, lpf2 the slow one; their weighted sum forms the feedback
  // signal that drives gain_reduction.
  constexpr double kLpf1AttackSec = 1.324200 * 0.001;
  constexpr double kLpf1ReleaseSec = 1.782562 * 0.001;
  constexpr double kLpf2AttackSec = 28.011420 * 0.001;
  constexpr double kLpf2ReleaseSec = 26.260180 * 0.001;
  constexpr double kReleaseSec = 5.898;

  lpf1_attack_ = std::exp(-1.0 / (sample_rate_ * kLpf1AttackSec));
  lpf1_release_ = std::exp(-1.0 / (sample_rate_ * kLpf1ReleaseSec));
  lpf2_attack_ = std::exp(-1.0 / (sample_rate_ * kLpf2AttackSec));
  lpf2_release_ = std::exp(-1.0 / (sample_rate_ * kLpf2ReleaseSec));

  release_k_ = std::exp(-1.0 / (sample_rate_ * kReleaseSec));
  post_eq_k_ = 1.0 - std::exp(-2.0 * dsp::kPi * (20000.0 / sample_rate_));

  lpf1_state_ = 0.0;
  lpf2_state_ = 0.0;
  level_state_ = 0.0;
  post_eq_s1_ = 0.0;
  post_eq_s2_ = 0.0;
  gr_meter_db_ = 0.0;

  // Parameter-dependent tables depend on sample_rate_ (T8/T9 are rescaled
  // relative to a 44.1 kHz reference), so refresh them here too.
  SetParams(params_);
}

void Processor::SetParams(const Params& p) noexcept {
  params_ = p;

  const double ratio_norm = (params_.ratio - 2.0) / 8.0;
  const double attack_norm = params_.attack * 0.1;
  const double release_norm = params_.release * 0.1;

  t3_ = dsp::InterpolateExp(ratio_norm, kT3Ratio, false);
  t10_ = dsp::InterpolateExp(ratio_norm, kT10OutputComp, false);

  // T8/T9 are measured at 44.1 kHz and rescaled for the running sample rate.
  t8_ = dsp::InterpolateLin(attack_norm, kT8AttackRate) *
        (44100.0 / sample_rate_);
  t9_ = dsp::InterpolateLin(release_norm, kT9ReleaseRate) *
        (44100.0 / sample_rate_);

  t4_ = dsp::InterpolateLin(t3_, kT4Blend);
  t7_ = dsp::DbToLinear(-40.0 - params_.threshold_db);
  t11_ = dsp::DbToLinear(-30.0 + params_.output_db);

  purest_drive_.SetDrive(params_.saturation);
}

void Processor::ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                             const double* sc_r, int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  // Tracks the gain reduction factor of the most recently processed sample.
  double last_gr = 1.0;

  for (int i = 0; i < num_frames; ++i) {
    const double x1 = out_l[i];
    const double x2 = out_r[i];

    // When an external sidechain is present, the detector channels use its
    // L/R samples directly instead of the main input; gain reduction is
    // still applied to x1/x2.
    const double d1 = sc_l ? sc_l[i] : x1;
    const double d2 = sc_r ? sc_r[i] : x2;

    // Feedback signal path: combine the two smoothing filters' states
    // into a single feedback level, then derive the instantaneous gain
    // reduction from it.
    const double inv_gr = (lpf1_state_ * kLpf1FeedbackWeight) +
                          (lpf2_state_ * kLpf2FeedbackWeight);
    const double gain_reduction =
        kFeedbackEps / dsp::ClampSymmetric(inv_gr + kFeedbackEps);

    // Feedback sidechain shaping: the feedback level also selects a
    // blend between two fixed gain-stage curves (A1/B1 vs A2/B2), with the
    // blend itself further blended by t4_ (which is itself a function of
    // the ratio control).
    const double t5 = dsp::InterpolateExp(inv_gr, kT5Sidechain, false);
    const double t6 = dsp::InterpolateLin(t5, kT6Mix);

    const double m1 = (kMixA1 * t6) + (kMixB1 * (1.0 - t6));
    const double m2 = (kMixA2 * t6) + (kMixB2 * (1.0 - t6));
    const double mult = (m1 * (1.0 - t4_)) + (m2 * t4_);

    // Detector path: scale the detector input by the sidechain shaping and the
    // threshold-derived trim, then pass through the detector's nonlinear
    // (log-domain) response curve.
    const double input1 = d1 * kDetectorInputTrim * mult * t7_;
    const double input2 = d2 * kDetectorInputTrim * mult * t7_;

    const double det1 = dsp::InterpolateExp(input1, kT12Detector, true);
    const double det2 = dsp::InterpolateExp(input2, kT12Detector, true);

    // Stereo-linked level: the louder channel drives gain reduction on
    // both, matching the hardware's single shared sidechain.
    const double level = std::max(std::abs(det1), std::abs(det2));

    // Attack/release level-follower.
    if (level >= level_state_) {
      // Attack: linear ramp toward the new (higher) level, capped so it
      // never overshoots the target.
      level_state_ = std::min(level_state_ + t8_, level);
    } else {
      // Release: exponential decay toward zero, offset by a linear
      // release-rate term, capped so it never undershoots the target.
      level_state_ = std::max((level_state_ * release_k_) - t9_, level);
    }

    const double t13 = dsp::InterpolateLin(level_state_, kT13GainShape);

    // Dual-rate smoothing: lpf1 (fast) and lpf2 (slow) each track t13 with
    // independent attack/release time constants; their weighted sum (above,
    // as inv_gr) is what shapes gain_reduction for the next sample.
    const double lpf1_k = (t13 > lpf1_state_) ? lpf1_attack_ : lpf1_release_;
    lpf1_state_ = t13 + ((lpf1_state_ - t13) * lpf1_k);

    const double lpf2_k = (t13 > lpf2_state_) ? lpf2_attack_ : lpf2_release_;
    lpf2_state_ = t13 + ((lpf2_state_ - t13) * lpf2_k);

    // Output: apply ratio-dependent output compensation (t10_), output
    // makeup gain (t11_), the instantaneous gain reduction, and the fixed
    // output-stage scaler.
    const double y1 = x1 * t10_ * t11_ * gain_reduction * kOutputScaler;
    const double y2 = x2 * t10_ * t11_ * gain_reduction * kOutputScaler;

    last_gr = gain_reduction;

    // Gentle one-pole roll-off above ~20 kHz, applied per channel after
    // gain reduction.
    post_eq_s1_ += (y1 - post_eq_s1_) * post_eq_k_;
    post_eq_s2_ += (y2 - post_eq_s2_) * post_eq_k_;

    out_l[i] = post_eq_s1_;
    out_r[i] = post_eq_s2_;
  }

  if (params_.saturation > 0.0) {
    purest_drive_.ProcessBlock(out_l, out_r, num_frames);
  }

  if (params_.soft_clip) {
    dsp::Apply(out_l, num_frames, dsp::SoftClip);
    dsp::Apply(out_r, num_frames, dsp::SoftClip);
  }

  gr_meter_db_ = -dsp::LinearToDb(last_gr);
}

}  // namespace glidecomp_dsp
