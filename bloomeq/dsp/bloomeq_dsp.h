// This file is derived from the original EQP-WDF-1A by ABSounds.
// Copyright (c) ABSounds (GPL-3.0 license)

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <tuple>

#include "Tube2.h"
#include "chowdsp_wdf.h"
#include "dsp_math.h"

namespace wdft = chowdsp::wdft;

class EQP1A {
 public:
  EQP1A() = default;

  void SetLoBoost(double lo_boost) {
    if (lo_boost_ != lo_boost) {
      r_boost_lo_.setResistanceValue(std::pow(lo_boost, kLoKnobSkewExp) *
                                     kRBoostLoMax);
      lo_boost_ = lo_boost;
    }
  }

  void SetLoCut(double lo_cut) {
    if (lo_cut_ != lo_cut) {
      r_cut_lo_.setResistanceValue(std::pow(lo_cut, kLoKnobSkewExp) *
                                   kRCutLoMax);
      lo_cut_ = lo_cut;
    }
  }

  void SetHiBoost(double hi_boost) {
    if (hi_boost_ != hi_boost) {
      r_boost_hi1_.setResistanceValue((1.0 - hi_boost) * kRBoostHiMax);
      r_boost_hi2_.setResistanceValue(hi_boost * kRBoostHiMax);
      hi_boost_ = hi_boost;
    }
  }

  void SetHiCut(double hi_cut) {
    if (hi_cut_ != hi_cut) {
      r_cut_hi1_.setResistanceValue((1.0 - hi_cut) * kRCutHiMax);
      r_cut_hi2_.setResistanceValue(hi_cut * kRCutHiMax);
      hi_cut_ = hi_cut;
    }
  }

  // hi_bandwidth: 0 = narrowest shelf, 1 = widest shelf.
  // Maps linearly to r_q_ resistance: wider bandwidth -> higher resistance.
  void SetHiBandwidth(double hi_bandwidth) {
    if (hi_bandwidth_ != hi_bandwidth) {
      r_q_.setResistanceValue(hi_bandwidth * kRQMax);
      hi_bandwidth_ = hi_bandwidth;
    }
  }

  void SetLoFreq(int lo_freq) {
    if (lo_freq_ != lo_freq) {
      // Maps selector (0-3) to the physical corner frequency in Hz.
      // 0 -> 20 Hz (default), 1 -> 30 Hz, 2 -> 60 Hz, 3 -> 100 Hz.
      static constexpr int kLoFreqHz[] = {20, 30, 60, 100};
      const int freq = kLoFreqHz[std::clamp(lo_freq, 0, 3)];
      auto [c1, c2] = GetCapValuesLoFreq(freq);
      lo_freq_c1_.setCapacitanceValue(c1);
      lo_freq_c2_.setCapacitanceValue(c2);
      lo_freq_ = lo_freq;
    }
  }

  void SetHiBoostFreq(int hi_boost_freq) {
    if (hi_boost_freq_ != hi_boost_freq) {
      // Maps selector (0-6) to the physical boost frequency in Hz.
      static constexpr int kHiBoostFreqHz[] = {3000,  4000,  5000, 8000,
                                               10000, 12000, 16000};
      const int freq = kHiBoostFreqHz[std::clamp(hi_boost_freq, 0, 6)];
      auto [c1, l1] = GetCompValuesHiBoost(freq);
      c1_.setCapacitanceValue(c1);
      l1_.setInductanceValue(l1);
      hi_boost_freq_ = hi_boost_freq;
    }
  }

  void SetHiCutFreq(int hi_cut_freq) {
    if (hi_cut_freq_ != hi_cut_freq) {
      // Maps selector (0-2) to the physical cut frequency in kHz.
      // 0 -> 5 kHz, 1 -> 10 kHz, 2 -> 20 kHz.
      static constexpr int kHiCutFreqKhz[] = {5, 10, 20};
      const int freq = kHiCutFreqKhz[std::clamp(hi_cut_freq, 0, 2)];
      c_hi_cut_.setCapacitanceValue(GetCapValueHiCut(freq));
      hi_cut_freq_ = hi_cut_freq;
    }
  }

  void SetParams(double lo_boost, double lo_cut, double hi_boost, double hi_cut,
                 double hi_bandwidth, int lo_freq, int hi_boost_freq,
                 int hi_cut_freq) {
    SetLoBoost(lo_boost);
    SetLoCut(lo_cut);
    SetHiBoost(hi_boost);
    SetHiCut(hi_cut);
    SetHiBandwidth(hi_bandwidth);
    SetLoFreq(lo_freq);
    SetHiCutFreq(hi_cut_freq);
    SetHiBoostFreq(hi_boost_freq);
  }

  void Prepare(double sr) {
    c1_.prepare(sr);
    l1_.prepare(sr);
    lo_freq_c1_.prepare(sr);
    lo_freq_c2_.prepare(sr);
    c_hi_cut_.prepare(sr);
    sample_rate_ = sr;
  }

  double ProcessSample(double sample) {
    vin_.setVoltage(sample);
    vin_.incident(s3_.reflected());
    s3_.incident(vin_.reflected());
    return (-wdft::voltage<double>(j_) + wdft::voltage<double>(r3_)) * 15.4882;
  }

 private:
  static constexpr double kRBoostHiMax = 12e3;
  static constexpr double kRQMax = 3.9e3;
  static constexpr double kRCutHiMax = 1e3;
  static constexpr double kRBoostLoMax = 8.2e3;
  static constexpr double kRCutLoMax = 110e3;
  static constexpr double kLoKnobSkewExp = 1.0 / 0.3;
  static constexpr double kRatioL1C1 = 12e6;
  static constexpr double kR1Value = 91.0;
  static constexpr double kR2Value = 1e3;
  static constexpr double kR3Value = 10e3;
  static constexpr double kRQOffsetValue = 390.0;

  static constexpr double kLoBoostIni = 0.00001;
  static constexpr double kLoCutIni = 0.00001;
  static constexpr double kHiBoostIni = 0.00001;
  static constexpr double kHiCutIni = 0.00001;
  static constexpr double kHiBandwidthIni = 0.5;
  static constexpr int kLoFreqIni = 2;
  static constexpr int kHiBoostFreqIni = 2;
  static constexpr int kHiCutFreqIni = 2;

  double sample_rate_ = 44100.0;
  double lo_boost_ = kLoBoostIni;
  double lo_cut_ = kLoCutIni;
  double hi_boost_ = kHiBoostIni;
  double hi_cut_ = kHiCutIni;
  double hi_bandwidth_ = kHiBandwidthIni;
  int lo_freq_ = kLoFreqIni;
  int hi_boost_freq_ = kHiBoostFreqIni;
  int hi_cut_freq_ = kHiCutFreqIni;

  wdft::ResistorT<double> r_boost_hi1_{(1.0 - kHiBoostIni) * kRBoostHiMax};
  wdft::CapacitorT<double> c1_{std::get<0>(GetCompValuesHiBoost(5000))};
  wdft::InductorT<double> l1_{std::get<1>(GetCompValuesHiBoost(5000))};
  wdft::ResistorT<double> r_q_{kHiBandwidthIni * kRQMax};
  wdft::ResistorT<double> r_q_offset_{kRQOffsetValue};
  wdft::WDFSeriesT<double, decltype(r_q_), decltype(r_q_offset_)> s_rq_{
      r_q_, r_q_offset_};
  wdft::WDFSeriesT<double, decltype(c1_), decltype(l1_)> s_clc_{c1_, l1_};
  wdft::WDFSeriesT<double, decltype(s_clc_), decltype(s_rq_)> c_{s_clc_, s_rq_};
  wdft::ResistorT<double> r_boost_hi2_{kHiBoostIni * kRBoostHiMax};
  wdft::ResistorT<double> r_cut_hi1_{(1.0 - kHiCutIni) * kRCutHiMax};
  wdft::ResistorT<double> r_cut_hi2_{kHiCutIni * kRCutHiMax};
  wdft::CapacitorT<double> c_hi_cut_{GetCapValueHiCut(20)};
  wdft::ResistorT<double> r1_{kR1Value};
  wdft::CapacitorT<double> lo_freq_c1_{std::get<0>(GetCapValuesLoFreq(20))};
  wdft::ResistorT<double> r_cut_lo_{kLoCutIni * kRCutLoMax};
  wdft::WDFParallelT<double, decltype(lo_freq_c1_), decltype(r_cut_lo_)> h_{
      lo_freq_c1_, r_cut_lo_};
  wdft::ResistorT<double> r2_{kR2Value};
  wdft::ResistorT<double> r3_{kR3Value};
  wdft::WDFSeriesT<double, decltype(r2_), decltype(r3_)> i_{r2_, r3_};
  wdft::ResistorT<double> r_boost_lo_{kLoBoostIni * kRBoostLoMax};
  wdft::CapacitorT<double> lo_freq_c2_{std::get<1>(GetCapValuesLoFreq(20))};
  wdft::WDFParallelT<double, decltype(r_boost_lo_), decltype(lo_freq_c2_)> j_{
      r_boost_lo_, lo_freq_c2_};

  wdft::WDFParallelT<double, decltype(c_), decltype(r_boost_hi2_)> p_cd_{
      c_, r_boost_hi2_};
  wdft::WDFParallelT<double, decltype(r_cut_hi2_), decltype(c_hi_cut_)> p_fg_{
      r_cut_hi2_, c_hi_cut_};
  wdft::WDFSeriesT<double, decltype(r_cut_hi1_), decltype(p_fg_)> s_efg_{
      r_cut_hi1_, p_fg_};
  wdft::WDFSeriesT<double, decltype(h_), decltype(i_)> s_hi_{h_, i_};
  wdft::WDFParallelT<double, decltype(s_efg_), decltype(s_hi_)> p_efghi_{s_efg_,
                                                                         s_hi_};
  wdft::WDFSeriesT<double, decltype(r_boost_hi1_), decltype(p_cd_)> s1_{
      r_boost_hi1_, p_cd_};
  wdft::WDFSeriesT<double, decltype(s1_), decltype(p_efghi_)> s2_{s1_,
                                                                  p_efghi_};
  wdft::WDFSeriesT<double, decltype(s2_), decltype(j_)> s3_{s2_, j_};
  wdft::IdealVoltageSourceT<double, decltype(s3_)> vin_{s3_};

  static std::tuple<double, double> GetCompValuesHiBoost(int fc) {
    const double two_pi = 2.0 * dsp::kPi;
    const double c =
        std::sqrt(1.0 / (kRatioL1C1 * (two_pi * fc) * (two_pi * fc)));
    return {c, kRatioL1C1 * c};
  }

  // Valid lo_freq values: 20, 30, 60, 100 Hz.
  // Each maps to a capacitor pair that sets the low-frequency corner.
  static std::tuple<double, double> GetCapValuesLoFreq(int lo_freq) {
    double c2;
    switch (lo_freq) {
      case 20:
        c2 = 2.2e-6;
        break;
      case 30:
        c2 = 1.1e-6;
        break;
      case 60:
        c2 = 560e-9;
        break;
      case 100:
        c2 = 330e-9;
        break;
      default:
        assert(false);
        c2 = 2.2e-6;  // unreachable
        break;
    }
    return {c2 / 30.0, c2};
  }

  static double GetCapValueHiCut(int hi_cut_freq) {
    switch (hi_cut_freq) {
      case 5:
        return 270e-9;
      case 10:
        return 135e-9;
      default:
        return 68e-9;
    }
  }
};

namespace bloomeq_dsp {

// Minimum safe knob value for WDF resistance/capacitance parameters.
// At exactly 0.0 several WDF element impedances become degenerate (zero
// resistance or infinite susceptance).
static constexpr double kMinKnob = 1e-4;

struct Parameters {
  double lo_boost = kMinKnob;
  double lo_cut = kMinKnob;
  int lo_freq = 2;  // freq selector

  double hi_boost = kMinKnob;
  int hi_boost_freq = 3;
  // hi_bandwidth: 0 = narrowest shelf transition, 1 = widest.
  // The tilde layer converts the user-facing "narrowness" knob to this value
  // via (1 - knob), so hi_bandwidth = 1.0 at the default center position.
  double hi_bandwidth = 0.5;

  double hi_cut = kMinKnob;
  int hi_cut_freq = 2;  // freq selector

  bool eq_enable = true;
  bool phase_inv = false;

  double gain = 0.5;
  double saturation = 0.5;
};

// Stereo processor: owns independent EQP1A circuits for L and R (WDF circuit
// state is inherently per-channel and cannot be shared), plus a single stereo
// Tube2 saturation stage.
class Processor {
 public:
  void Prepare(double sample_rate);
  void SetParameters(const Parameters& p);

  // Process num_frames stereo frames in-place.
  void ProcessBlock(double* out_l, double* out_r, int num_frames) noexcept;

 private:
  void Apply(const Parameters& p);

  EQP1A eqp1a_l_;
  EQP1A eqp1a_r_;
  Tube2 tube2_;
  Parameters current_params_;
  bool prepared_ = false;
  double sample_rate_ = 44100.0;
};

}  // namespace bloomeq_dsp
