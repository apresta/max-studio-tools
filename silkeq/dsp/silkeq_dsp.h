// The filter structure in this device is based on the original eq1979 (JSFX)
// code by D4p0up, with a few changes to the shelf filter computations.
// See https://github.com/D4p0up/eq1979.

#pragma once

#include <cmath>

#include "Coils.h"
#include "biquad_coeffs.h"
#include "biquad_state.h"
#include "but_filter.h"
#include "dsp_math.h"
#include "vec.h"

using dsp::Vec2;

namespace silkeq_dsp {

struct Parameters {
  double c_hpf_gain = 0.0;
  double c_mpf_gain = 0.0;
  int mpf_cut = 0;  // mid-frequency band selector
  double c_lpf_gain = 0.0;
  int lpf_cut = 0;  // low-frequency band selector
  int hpf_cut = 0;  // input HPF selector
  bool eq_enable = true;
  bool phase_inv = false;
  double saturation = 0.0;
};

// One-pole low-pass smoother.
struct LopFilter {
  double y0_ = 0.0;
  double coef_ = 0.0;

  void Init(double freq_hz, double sr) noexcept {
    y0_ = 0.0;
    coef_ = std::exp(-2.0 * dsp::kPi * freq_hz / sr);
  }

  void Seed(double value) noexcept { y0_ = value; }

  double Process(double x) noexcept {
    y0_ = x + (y0_ - x) * coef_;
    return y0_;
  }

  // Advance the smoother by n samples toward target x in one O(1) step.
  double Advance(double x, int n) noexcept {
    y0_ = x + (y0_ - x) * std::pow(coef_, static_cast<double>(n));
    return y0_;
  }
};

// One-pole high-pass filter.
struct HipFilter {
  Vec2 x0_;
  Vec2 y0_;
  double a0_ = 0.0;
  double a1_ = 0.0;
  double b1_ = 0.0;
  double sr_ = 44100.0;

  void ComputeCoeffs(double freq_hz) noexcept {
    b1_ = std::exp(-2.0 * dsp::kPi * freq_hz / sr_);
    a0_ = (1.0 + b1_) * 0.5;
    a1_ = -a0_;
  }

  void Init(double freq_hz, double sr) noexcept {
    sr_ = sr;
    x0_ = y0_ = Vec2{};
    ComputeCoeffs(freq_hz);
  }

  Vec2 Process(Vec2 sig) noexcept {
    Vec2 out = sig * a0_ + x0_ * a1_ + y0_ * b1_;
    x0_ = sig;
    y0_ = out;
    return out;
  }
};

// 2nd-order biquad EQ.
// Supports peaking (0), low-shelf (1), high-shelf (2).
struct EqFilter {
  double sr_ = 44100.0;
  double freq_ = 1000.0;
  double q_ = 0.5;
  double boost_ = 0.0;
  int type_ = 0;

  dsp::BiquadState st_;

  // b_[0..2] == b0, b1, b2  (feedforward)
  // a_[0..2] == 1.0, a1, a2 (feedback; a_[0] always written as 1.0 but is never
  //                          read in Process())
  double b_[3] = {1.0, 0.0, 0.0};
  double a_[3] = {1.0, 0.0, 0.0};

  void ComputeCoeffs() noexcept {
    // Frequency clamping and Q floor are handled inside each dsp function.
    switch (type_) {
      case 0:
        dsp::BiquadPeak(freq_, boost_, q_, sr_, b_, a_);
        break;
      case 1:
        dsp::BiquadLowShelf(freq_, boost_, q_, sr_, b_, a_);
        break;
      default:
        dsp::BiquadHighShelf(freq_, boost_, q_, sr_, b_, a_);
        break;
    }
  }

  void Init(double freq, double q, double boost, int type, double sr) noexcept {
    sr_ = sr;
    type_ = (type < 0) ? 0 : (type > 2 ? 2 : type);
    freq_ = freq;
    q_ = q;
    boost_ = boost;
    st_.Clear();
    ComputeCoeffs();
  }

  void SetFreq(double f) noexcept {
    if (f != freq_) {
      freq_ = f;
      ComputeCoeffs();
    }
  }

  void SetQ(double q) noexcept {
    if (q != q_) {
      q_ = q;
      ComputeCoeffs();
    }
  }

  void SetBoost(double b) noexcept {
    if (b != boost_) {
      boost_ = b;
      ComputeCoeffs();
    }
  }

  Vec2 Process(Vec2 sig) noexcept {
    // a_[0] is always 1.0; skip it and pass a1, a2 directly.
    return st_.Tick(sig, b_[0], b_[1], b_[2], a_[1], a_[2]);
  }
};

// Per-channel filter bank.
struct ChannelState {
  HipFilter f_in_a_;  // Stage 1: 11 Hz (before EQ)
  HipFilter f_in_b_;  // Stage 2: 5 Hz (before EQ, after stage 1)

  dsp::ButFilter f_slf_a_{dsp::ButType::HighPass};  // HPF stage 1
  dsp::ButFilter f_slf_b_{dsp::ButType::HighPass};  // HPF stage 2
  EqFilter f_slf_c_;                                // HPF bump correction 1
  EqFilter f_slf_d_;                                // HPF notch correction 2

  dsp::ButFilter f_hlf_a_{dsp::ButType::LowPass};  // anti-alias LPF stage 1
  dsp::ButFilter f_hlf_b_{dsp::ButType::LowPass};  // anti-alias LPF stage 2

  EqFilter f_bump_;  // static mid bump @ 850 Hz, +0.38 dB
  EqFilter f_drop_;  // static HF drop @ 18.3 kHz, -0.8 dB

  EqFilter f_hf_;       // high shelf (~12 kHz)
  EqFilter f_hf_bump_;  // interaction bump @ 820 Hz

  EqFilter f_mf_bump_;  // mid bump shaping
  EqFilter f_mf_;       // main mid peak

  EqFilter f_lf_bump_;  // low bump
  EqFilter f_lf_drop_;  // low drop
  EqFilter f_lf_;       // main low shelf

  void Init(double sr) noexcept {
    f_in_a_.Init(11.0, sr);
    f_in_b_.Init(5.0, sr);

    f_slf_a_.Init(50.0, sr);
    f_slf_b_.Init(50.0, sr);
    f_slf_c_.Init(50.0, 0.5, 0.0, 0, sr);
    f_slf_d_.Init(50.0, 0.5, 0.0, 0, sr);

    f_hlf_a_.Init(20843.0, sr);
    f_hlf_b_.Init(20843.0, sr);

    f_bump_.Init(850.0, 0.50, 0.38, 0, sr);
    f_drop_.Init(18300.0, 0.30, -0.80, 0, sr);

    f_hf_.Init(2000.0, 0.35, 0.0, 2, sr);
    f_hf_bump_.Init(820.0, 0.45, -2.0, 0, sr);

    f_mf_bump_.Init(380.0, 0.25, 1.0, 0, sr);
    f_mf_.Init(380.0, 0.5, 0.0, 0, sr);

    f_lf_bump_.Init(80.0, 0.25, 0.5, 0, sr);
    f_lf_drop_.Init(240.0, 0.50, -0.5, 0, sr);
    f_lf_.Init(80.0, 0.5, 0.0, 0, sr);
  }
};

class Processor {
 public:
  void Prepare(double sample_rate);
  void SetParameters(const Parameters& p);
  void ProcessBlock(double* out_l, double* out_r, int num_frames) noexcept;

 private:
  void UpdateHPF() noexcept;
  void UpdateHF() noexcept;
  void UpdateMF() noexcept;
  void UpdateLF() noexcept;

  double sr_ = 44100.0;
  ChannelState ch_;

  LopFilter p_hf_, p_mf_, p_lf_;

  Parameters params_;

  double hpf_gain_ = 0.0;
  double mpf_gain_ = 0.0;
  double lpf_gain_ = 0.0;

  Coils coils_;
};

}  // namespace silkeq_dsp
