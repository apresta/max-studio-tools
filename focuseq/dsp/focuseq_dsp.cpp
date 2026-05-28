#include "focuseq_dsp.h"

#include <cassert>
#include <cmath>

#include "biquad_coeffs.h"
#include "denormal_guard.h"
#include "dsp_math.h"

FocusEqDsp::FocusEqDsp() {
  // Initialize both Butterworth filters at defaults.
  hpf_.Init(params_.hpf_freq, sample_rate_);
  lpf_.Init(params_.lpf_freq, sample_rate_);

  // Set all four EQ biquads to unity gain.
  UpdateLowCoeffs();
  UpdateLowMidCoeffs();
  UpdateHighMidCoeffs();
  UpdateHighCoeffs();
}

void FocusEqDsp::SetParameters(const Parameters& p) noexcept {
  const Parameters& c = params_;

  // HPF / LPF: always write the new freq/enabled into params_ so that
  // re-enabling later picks up any freq change made while disabled.
  if (p.hpf_freq != c.hpf_freq || p.hpf_enabled != c.hpf_enabled) {
    params_.hpf_freq = p.hpf_freq;
    params_.hpf_enabled = p.hpf_enabled;
    if (p.hpf_enabled) UpdateHpfCoeffs();
  }

  if (p.lpf_freq != c.lpf_freq || p.lpf_enabled != c.lpf_enabled) {
    params_.lpf_freq = p.lpf_freq;
    params_.lpf_enabled = p.lpf_enabled;
    if (p.lpf_enabled) UpdateLpfCoeffs();
  }

  // EQ bands.
  if (p.low_freq != c.low_freq || p.low_gain != c.low_gain ||
      p.low_q != c.low_q || p.low_shape != c.low_shape) {
    params_.low_freq = p.low_freq;
    params_.low_gain = p.low_gain;
    params_.low_q = p.low_q;
    params_.low_shape = p.low_shape;
    UpdateLowCoeffs();
  }

  if (p.lomid_freq != c.lomid_freq || p.lomid_gain != c.lomid_gain ||
      p.lomid_q != c.lomid_q) {
    params_.lomid_freq = p.lomid_freq;
    params_.lomid_gain = p.lomid_gain;
    params_.lomid_q = p.lomid_q;
    UpdateLowMidCoeffs();
  }

  if (p.himid_freq != c.himid_freq || p.himid_gain != c.himid_gain ||
      p.himid_q != c.himid_q) {
    params_.himid_freq = p.himid_freq;
    params_.himid_gain = p.himid_gain;
    params_.himid_q = p.himid_q;
    UpdateHighMidCoeffs();
  }

  if (p.high_freq != c.high_freq || p.high_gain != c.high_gain ||
      p.high_q != c.high_q || p.high_shape != c.high_shape) {
    params_.high_freq = p.high_freq;
    params_.high_gain = p.high_gain;
    params_.high_q = p.high_q;
    params_.high_shape = p.high_shape;
    UpdateHighCoeffs();
  }

  // Saturation gain.
  if (p.sat_gain != c.sat_gain) {
    params_.sat_gain = p.sat_gain;
    sat_.SetGain(static_cast<float>(p.sat_gain));
  }

  // Plain flags (no coefficients to recompute).
  params_.eq_enable = p.eq_enable;
  params_.phase_inv = p.phase_inv;
  params_.sat_enable = p.sat_enable;
}

void FocusEqDsp::UpdateHpfCoeffs() noexcept { hpf_.SetFreq(params_.hpf_freq); }

void FocusEqDsp::UpdateLpfCoeffs() noexcept { lpf_.SetFreq(params_.lpf_freq); }

void FocusEqDsp::UpdateLowCoeffs() noexcept {
  Computed& lc = last_computed_;
  if (params_.low_freq == lc.low_freq && params_.low_gain == lc.low_gain &&
      params_.low_q == lc.low_q && params_.low_shape == lc.low_shape)
    return;

  double b[3];
  double a[3];

  if (params_.low_gain == 0.0) {
    dsp::BiquadPassthrough(b, a);
  } else if (params_.low_shape == BandShape::Shelf) {
    dsp::BiquadLowShelf(params_.low_freq, params_.low_gain, params_.low_q,
                        sample_rate_, b, a);
  } else {
    const double eff_q =
        params_.low_q * std::pow(10.0, std::fabs(params_.low_gain) / 40.0);
    dsp::BiquadPeak(params_.low_freq, params_.low_gain, eff_q, sample_rate_, b,
                    a);
  }

  low_bq_.SetCoeffs(b[0], b[1], b[2], a[1], a[2]);
  lc.low_freq = params_.low_freq;
  lc.low_gain = params_.low_gain;
  lc.low_q = params_.low_q;
  lc.low_shape = params_.low_shape;
}

void FocusEqDsp::UpdateLowMidCoeffs() noexcept {
  Computed& lc = last_computed_;
  if (params_.lomid_freq == lc.lomid_freq &&
      params_.lomid_gain == lc.lomid_gain && params_.lomid_q == lc.lomid_q)
    return;

  double b[3];
  double a[3];

  if (params_.lomid_gain == 0.0) {
    dsp::BiquadPassthrough(b, a);
  } else {
    const double eff_q =
        params_.lomid_q * std::pow(10.0, std::fabs(params_.lomid_gain) / 40.0);
    dsp::BiquadPeak(params_.lomid_freq, params_.lomid_gain, eff_q, sample_rate_,
                    b, a);
  }

  lomid_bq_.SetCoeffs(b[0], b[1], b[2], a[1], a[2]);
  lc.lomid_freq = params_.lomid_freq;
  lc.lomid_gain = params_.lomid_gain;
  lc.lomid_q = params_.lomid_q;
}

void FocusEqDsp::UpdateHighMidCoeffs() noexcept {
  Computed& lc = last_computed_;
  if (params_.himid_freq == lc.himid_freq &&
      params_.himid_gain == lc.himid_gain && params_.himid_q == lc.himid_q)
    return;

  double b[3];
  double a[3];

  if (params_.himid_gain == 0.0) {
    dsp::BiquadPassthrough(b, a);
  } else {
    const double eff_q =
        params_.himid_q * std::pow(10.0, std::fabs(params_.himid_gain) / 40.0);
    dsp::BiquadPeak(params_.himid_freq, params_.himid_gain, eff_q, sample_rate_,
                    b, a);
  }

  himid_bq_.SetCoeffs(b[0], b[1], b[2], a[1], a[2]);
  lc.himid_freq = params_.himid_freq;
  lc.himid_gain = params_.himid_gain;
  lc.himid_q = params_.himid_q;
}

void FocusEqDsp::UpdateHighCoeffs() noexcept {
  Computed& lc = last_computed_;
  if (params_.high_freq == lc.high_freq && params_.high_gain == lc.high_gain &&
      params_.high_q == lc.high_q && params_.high_shape == lc.high_shape)
    return;

  double b[3];
  double a[3];

  if (params_.high_gain == 0.0) {
    dsp::BiquadPassthrough(b, a);
  } else if (params_.high_shape == BandShape::Shelf) {
    dsp::BiquadHighShelf(params_.high_freq, params_.high_gain, params_.high_q,
                         sample_rate_, b, a);
  } else {
    const double eff_q =
        params_.high_q * std::pow(10.0, std::fabs(params_.high_gain) / 40.0);
    dsp::BiquadPeak(params_.high_freq, params_.high_gain, eff_q, sample_rate_,
                    b, a);
  }

  high_bq_.SetCoeffs(b[0], b[1], b[2], a[1], a[2]);
  lc.high_freq = params_.high_freq;
  lc.high_gain = params_.high_gain;
  lc.high_q = params_.high_q;
  lc.high_shape = params_.high_shape;
}

void FocusEqDsp::Prepare(double sample_rate) noexcept {
  sample_rate_ = sample_rate;

  hpf_.Init(params_.hpf_freq, sample_rate_);
  lpf_.Init(params_.lpf_freq, sample_rate_);

  low_bq_.Clear();
  lomid_bq_.Clear();
  himid_bq_.Clear();
  high_bq_.Clear();

  // Invalidate the coefficient cache so every Update* call below recomputes
  // unconditionally. The sentinel value -1.0 can never be a valid frequency
  // or gain, so all equality checks will fail.
  last_computed_ = Computed{};

  UpdateLowCoeffs();
  UpdateLowMidCoeffs();
  UpdateHighMidCoeffs();
  UpdateHighCoeffs();
}

void FocusEqDsp::ProcessBlock(double* out_l, double* out_r,
                              int num_frames) noexcept {
  ScopedDenormalGuard denormal_guard;
  assert(out_l != nullptr && out_r != nullptr);
  assert(num_frames >= 0);

  if (params_.phase_inv) dsp::InvertPhase(out_l, out_r, num_frames);

  // HPF and LPF.
  if (params_.hpf_enabled || params_.lpf_enabled) {
    for (int i = 0; i < num_frames; ++i) {
      dsp::Vec2 x{out_l[i], out_r[i]};
      if (params_.hpf_enabled) x = hpf_.Process(x);
      if (params_.lpf_enabled) x = lpf_.Process(x);
      out_l[i] = x.l();
      out_r[i] = x.r();
    }
  }

  // EQ bands.
  if (params_.eq_enable) {
    low_bq_.ProcessBlock(out_l, out_r, num_frames);
    lomid_bq_.ProcessBlock(out_l, out_r, num_frames);
    himid_bq_.ProcessBlock(out_l, out_r, num_frames);
    high_bq_.ProcessBlock(out_l, out_r, num_frames);
  }

  // Saturation.
  if (params_.sat_enable) sat_.ProcessBlock(out_l, out_r, num_frames);
}
