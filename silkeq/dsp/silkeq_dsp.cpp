// The filter structure in this device is based on the original eq1979 (JSFX)
// code by D4p0up, with a few changes to the shelf filter computations.
// See https://github.com/D4p0up/eq1979 (GPL-3.0 License).

#include "silkeq_dsp.h"

#include <cassert>
#include <cmath>

#include "denormal_guard.h"

namespace silkeq_dsp {

namespace {

struct HpfPreset {
  double cut, b1, g1, q1, b2, g2, q2;
};

constexpr HpfPreset kHpfTable[] = {
    {34.0, 46.0, 1.1, 1.6, 78.0, -0.3, 0.7},     // cut = 1
    {64.0, 76.0, 2.4, 1.2, 110.0, -1.6, 0.7},    // cut = 2
    {132.0, 160.0, 2.2, 1.9, 302.0, -0.5, 1.0},  // cut = 3
    {270.0, 330.0, 2.4, 1.6, 665.0, -0.4, 0.7},  // cut = 4
};

struct MfPreset {
  double bump_freq;
  double main_freq_base;
  double main_freq_gain_coeff;
  double main_q_gain_divisor;
};

constexpr MfPreset kMfTable[] = {
    {380.0, 358.0, 0.0, 60.0},                // cut = 1
    {700.0, 750.0, 0.0, 55.0},                // cut = 2
    {1600.0, 1590.0, 0.0, 60.0},              // cut = 3
    {3200.0, 3200.0, 0.0, 72.0},              // cut = 4
    {6800.0, 5800.0, -600.0 / 18.0, 35.0},    // cut = 5
    {12200.0, 9400.0, -2130.0 / 18.0, 32.0},  // cut = 6
};

constexpr double kLfBumpFreq[] = {0.0, 35.0, 80.0, 130.0, 240.0};
constexpr double kLfDropFreq[] = {0.0, 240.0, 420.0, 540.0, 1000.0};
constexpr double kLfMainFreq[] = {0.0, 30.0, 32.0, 50.0, 80.0};
constexpr double kLfMainQ[] = {0.0, 0.22, 0.12, 0.12, 0.12};

}  // namespace

void Processor::UpdateHPF() noexcept {
  if (params_.hpf_cut == 0) return;
  if (params_.hpf_cut < 1 || params_.hpf_cut > 4) return;
  const HpfPreset& p = kHpfTable[params_.hpf_cut - 1];

  ch_.f_slf_a_.SetFreq(p.cut);
  ch_.f_slf_b_.SetFreq(p.cut);
  ch_.f_slf_c_.SetFreq(p.b1);
  ch_.f_slf_c_.SetQ(p.q1);
  ch_.f_slf_c_.SetBoost(p.g1);
  ch_.f_slf_d_.SetFreq(p.b2);
  ch_.f_slf_d_.SetQ(p.q2);
  ch_.f_slf_d_.SetBoost(p.g2);
}

void Processor::UpdateHF() noexcept {
  ch_.f_hf_.SetBoost(hpf_gain_);
  ch_.f_hf_bump_.SetBoost(hpf_gain_ * (-3.5 / 18.0));
}

void Processor::UpdateMF() noexcept {
  if (params_.mpf_cut == 0) return;
  if (params_.mpf_cut < 1 || params_.mpf_cut > 6) return;
  const MfPreset& p = kMfTable[params_.mpf_cut - 1];

  const double ag = std::abs(mpf_gain_);
  ch_.f_mf_bump_.SetFreq(p.bump_freq);
  ch_.f_mf_.SetFreq(p.main_freq_base + (p.main_freq_gain_coeff * ag));
  ch_.f_mf_.SetQ(0.22 + (ag / p.main_q_gain_divisor));
  ch_.f_mf_.SetBoost(mpf_gain_);
}

void Processor::UpdateLF() noexcept {
  if (params_.lpf_cut == 0) return;

  const int idx = params_.lpf_cut;
  const double drop_freq = kLfDropFreq[idx] + std::abs(lpf_gain_ * 5.0);
  const double bump_boost = 0.5 + (static_cast<double>(idx) / 8.0);
  const double drop_boost =
      -0.5 - (static_cast<double>(idx) / 11.0) -
      (lpf_gain_ / (2.6 + 2.0 / static_cast<double>(idx)));

  ch_.f_lf_bump_.SetFreq(kLfBumpFreq[idx]);
  ch_.f_lf_bump_.SetBoost(bump_boost);
  ch_.f_lf_drop_.SetFreq(drop_freq);
  ch_.f_lf_drop_.SetBoost(drop_boost);
  ch_.f_lf_.SetFreq(kLfMainFreq[idx]);
  ch_.f_lf_.SetQ(kLfMainQ[idx]);
  ch_.f_lf_.SetBoost(lpf_gain_);
}

void Processor::Prepare(double sample_rate) noexcept {
  ch_.Init(sample_rate);

  p_hf_.Init(10.0, sample_rate);
  p_mf_.Init(10.0, sample_rate);
  p_lf_.Init(10.0, sample_rate);

  coils_.Prepare(sample_rate);

  // Pre-load smoothers so there is no initial ramp.
  hpf_gain_ = params_.hpf_gain;
  mpf_gain_ = params_.mpf_gain;
  lpf_gain_ = params_.lpf_gain;
  p_hf_.Seed(hpf_gain_);
  p_mf_.Seed(mpf_gain_);
  p_lf_.Seed(lpf_gain_);

  UpdateHPF();
  UpdateHF();
  UpdateMF();
  UpdateLF();
}

void Processor::SetParams(const Params& p) noexcept {
  const int old_hpf_cut = params_.hpf_cut;
  const int old_mpf_cut = params_.mpf_cut;
  const int old_lpf_cut = params_.lpf_cut;
  params_ = p;
  if (p.hpf_cut != old_hpf_cut) UpdateHPF();
  if (p.mpf_cut != old_mpf_cut) UpdateMF();
  if (p.lpf_cut != old_lpf_cut) UpdateLF();
  coils_.SetSaturation(params_.saturation);
}

void Processor::ProcessBlock(double* out_l, double* out_r,
                             int num_frames) noexcept {
  dsp::ScopedDenormalGuard denormal_guard;

  // Block-rate parameter smoothing.
  {
    const double prev_hpf = hpf_gain_;
    const double prev_mpf = mpf_gain_;
    const double prev_lpf = lpf_gain_;

    hpf_gain_ = p_hf_.Advance(params_.hpf_gain, num_frames);
    mpf_gain_ = p_mf_.Advance(params_.mpf_gain, num_frames);
    lpf_gain_ = p_lf_.Advance(params_.lpf_gain, num_frames);

    if (std::abs(hpf_gain_ - prev_hpf) > 1e-6) UpdateHF();
    if (std::abs(mpf_gain_ - prev_mpf) > 1e-6) UpdateMF();
    if (std::abs(lpf_gain_ - prev_lpf) > 1e-6) UpdateLF();
  }

  // Sample loop (no coefficient recomputation inside).
  for (int i = 0; i < num_frames; ++i) {
    Vec2 x{out_l[i], out_r[i]};
    x = ch_.f_in_a_.Process(x);

    if (params_.phase_inv) x = -x;

    x = ch_.f_in_b_.Process(x);

    if (params_.eq_enable) {
      if (params_.hpf_cut > 0) {
        x = ch_.f_slf_a_.Process(x);
        x = ch_.f_slf_b_.Process(x);
        x = ch_.f_slf_c_.Process(x);
        x = ch_.f_slf_d_.Process(x);
      }

      x = ch_.f_bump_.Process(x);
      x = ch_.f_drop_.Process(x);
      x = ch_.f_hf_.Process(x);
      x = ch_.f_hf_bump_.Process(x);

      if (params_.mpf_cut > 0) {
        x = ch_.f_mf_bump_.Process(x);
        x = ch_.f_mf_.Process(x);
      }

      if (params_.lpf_cut > 0) {
        x = ch_.f_lf_bump_.Process(x);
        x = ch_.f_lf_drop_.Process(x);
        x = ch_.f_lf_.Process(x);
      }
    }

    x = ch_.f_hlf_a_.Process(x);
    x = ch_.f_hlf_b_.Process(x);

    out_l[i] = x.L();
    out_r[i] = x.R();
  }

  if (params_.saturation > 0.0) coils_.ProcessBlock(out_l, out_r, num_frames);
}

}  // namespace silkeq_dsp
