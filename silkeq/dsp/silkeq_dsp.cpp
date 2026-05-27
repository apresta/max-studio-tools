#include "silkeq_dsp.h"

#include "denormal_guard.h"

#include <cassert>
#include <cmath>

namespace silkeq_dsp {

void Processor::UpdateHPF() noexcept {
  if (params_.hpf_cut == 0) return;

  struct HpfPreset {
    double cut, b1, g1, q1, b2, g2, q2;
  };

  static constexpr HpfPreset kTable[] = {
      {34, 46, 1.1, 1.6, 78, -0.3, 0.7},     // cut = 1
      {64, 76, 2.4, 1.2, 110, -1.6, 0.7},    // cut = 2
      {132, 160, 2.2, 1.9, 302, -0.5, 1.0},  // cut = 3
      {270, 330, 2.4, 1.6, 665, -0.4, 0.7},  // cut = 4
  };

  if (params_.hpf_cut < 1 || params_.hpf_cut > 4) return;
  const HpfPreset& p = kTable[params_.hpf_cut - 1];

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

  struct MfPreset {
    double bump_freq;
    double main_freq_base;
    double main_freq_gain_coeff;
    double main_q_gain_divisor;
  };

  static constexpr MfPreset kTable[] = {
      {380, 358, 0.0, 60.0},                // cut = 1
      {700, 750, 0.0, 55.0},                // cut = 2
      {1600, 1590, 0.0, 60.0},              // cut = 3
      {3200, 3200, 0.0, 72.0},              // cut = 4
      {6800, 5800, -600.0 / 18.0, 35.0},    // cut = 5
      {12200, 9400, -2130.0 / 18.0, 32.0},  // cut = 6
  };

  if (params_.mpf_cut < 1 || params_.mpf_cut > 6) return;
  const MfPreset& p = kTable[params_.mpf_cut - 1];

  const double ag = std::abs(mpf_gain_);
  ch_.f_mf_bump_.SetFreq(p.bump_freq);
  ch_.f_mf_.SetFreq(p.main_freq_base + p.main_freq_gain_coeff * ag);
  ch_.f_mf_.SetQ(0.22 + ag / p.main_q_gain_divisor);
  ch_.f_mf_.SetBoost(mpf_gain_);
}

void Processor::UpdateLF() noexcept {
  if (params_.lpf_cut == 0) return;

  static constexpr double kBumpFreq[] = {0, 35, 80, 130, 240};
  static constexpr double kDropFreq[] = {0, 240, 420, 540, 1000};
  static constexpr double kMainFreq[] = {0, 30, 32, 50, 80};
  static constexpr double kMainQ[] = {0, 0.22, 0.12, 0.12, 0.12};

  const int idx = params_.lpf_cut;
  const double drop_freq = kDropFreq[idx] + std::abs(lpf_gain_ * 5.0);
  const double bump_boost = 0.5 + static_cast<double>(idx) / 8.0;
  const double drop_boost = -0.5 - static_cast<double>(idx) / 11.0 -
                            lpf_gain_ / (2.6 + 2.0 / static_cast<double>(idx));

  ch_.f_lf_bump_.SetFreq(kBumpFreq[idx]);
  ch_.f_lf_bump_.SetBoost(bump_boost);
  ch_.f_lf_drop_.SetFreq(drop_freq);
  ch_.f_lf_drop_.SetBoost(drop_boost);
  ch_.f_lf_.SetFreq(kMainFreq[idx]);
  ch_.f_lf_.SetQ(kMainQ[idx]);
  ch_.f_lf_.SetBoost(lpf_gain_);
}

void Processor::Prepare(double sample_rate) {
  sr_ = sample_rate;

  ch_.Init(sample_rate);

  p_hf_.Init(10.0, sample_rate);
  p_mf_.Init(10.0, sample_rate);
  p_lf_.Init(10.0, sample_rate);

  coils_.Prepare(sample_rate);

  // Pre-load smoothers so there is no initial ramp.
  hpf_gain_ = params_.c_hpf_gain;
  mpf_gain_ = params_.c_mpf_gain;
  lpf_gain_ = params_.c_lpf_gain;
  p_hf_.Seed(hpf_gain_);
  p_mf_.Seed(mpf_gain_);
  p_lf_.Seed(lpf_gain_);

  UpdateHPF();
  UpdateHF();
  UpdateMF();
  UpdateLF();
}

void Processor::SetParameters(const Parameters& p) {
  const int old_hpf_cut = params_.hpf_cut;
  const int old_mpf_cut = params_.mpf_cut;
  const int old_lpf_cut = params_.lpf_cut;
  params_ = p;
  if (p.hpf_cut != old_hpf_cut) UpdateHPF();
  if (p.mpf_cut != old_mpf_cut) UpdateMF();
  if (p.lpf_cut != old_lpf_cut) UpdateLF();
}

void Processor::ProcessBlock(double* out_l, double* out_r,
                             int num_frames) noexcept {
  ScopedDenormalGuard denormal_guard;
  if (num_frames == 0) return;

  // Block-rate parameter smoothing.
  {
    const double prev_hpf = hpf_gain_;
    const double prev_mpf = mpf_gain_;
    const double prev_lpf = lpf_gain_;

    hpf_gain_ = p_hf_.Advance(params_.c_hpf_gain, num_frames);
    mpf_gain_ = p_mf_.Advance(params_.c_mpf_gain, num_frames);
    lpf_gain_ = p_lf_.Advance(params_.c_lpf_gain, num_frames);

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

    out_l[i] = x.l();
    out_r[i] = x.r();
  }

  if (params_.saturation > 0.0) {
    coils_.SetSaturation(params_.saturation);
    coils_.ProcessBlock(out_l, out_r, num_frames);
  }
}

}  // namespace silkeq_dsp
