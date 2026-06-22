// The table-based implementation in this device is ported from the original
// CL1B by JClones, with some important changes.
// See https://github.com/JClones/JSFXClones (MIT License).
//
// Some parameter-wiring mistakes have been fixed. This port only implements
// manual mode (full control of attack and release parameters).

#pragma once

#include "PurestDrive.h"

namespace glidecomp_dsp {

struct Params {
  // Compression ratio (2.0 to 10.0).
  double ratio = 6.0;

  // Threshold in dBFS. Gain reduction begins above this level.
  double threshold_db = 0.0;

  // Attack control, 0..10 (continuous).
  double attack = 5.0;

  // Release control, 0..10 (continuous).
  double release = 5.0;

  // Output makeup gain in dB, applied after gain reduction.
  double output_db = 0.0;

  double saturation = 0.0;

  bool soft_clip = false;
};

class Processor {
 public:
  void Prepare(double sample_rate) noexcept;
  void SetParams(const Params& p) noexcept;

  void ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                    const double* sc_r, int num_frames) noexcept;

  double GainReductionDb() const noexcept { return gr_meter_db_; }

 private:
  Params params_;
  double sample_rate_ = 44100.0;

  // Dual-rate smoothing filter state (shared/mono, stereo-linked).
  double lpf1_state_ = 0.0;
  double lpf2_state_ = 0.0;

  // Attack/release level-follower state (shared/mono, stereo-linked).
  double level_state_ = 0.0;

  // Per-channel post-EQ (gentle 20 kHz roll-off) smoothing state.
  double post_eq_s1_ = 0.0;
  double post_eq_s2_ = 0.0;

  // Fixed time-constant coefficients, set once in Prepare()
  // (sample-rate-dependent, parameter-independent).
  double lpf1_attack_ = 0.0;
  double lpf1_release_ = 0.0;
  double lpf2_attack_ = 0.0;
  double lpf2_release_ = 0.0;
  double release_k_ = 0.0;
  double post_eq_k_ = 0.0;

  // Parameter-dependent table lookups, refreshed in SetParams().
  double t3_ = 0.0;   // ratio -> sidechain-shaping curve position
  double t4_ = 0.0;   // T3 -> blend factor between shaping curves
  double t7_ = 1.0;   // threshold -> detector input scale
  double t8_ = 0.0;   // attack -> level-follower attack rate
  double t9_ = 0.0;   // release -> level-follower release rate
  double t10_ = 1.0;  // ratio -> output gain compensation
  double t11_ = 1.0;  // output_db -> output makeup gain

  double gr_meter_db_ = 0.0;

  saturation::PurestDrive purest_drive_;
};

}  // namespace glidecomp_dsp
