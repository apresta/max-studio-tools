// This file is derived from the original 4A-2A by Queen Mary University.
// Copyright (c) Chin-Yun Yu and Gyorgy Fazekas (MPL-2.0 license).
//
// DSP parameters have been rescaled to better match the reference LA-2A units.

#pragma once

#include <vector>

#include "SingleEndedTriode.h"

namespace levelcomp_dsp {

// High-level UI params exposed by the Max wrapper. The Processor translates
// these into the five internal DSP coefficients via ApplyPeakMapping().
struct Params {
  // LA-2A "Peak Reduction" control (0–100).
  double peak_reduction = 0.0;

  // 0 = Compress (~3:1 effective), 1 = Limit (~inf:1 effective).
  int limit_mode = 0;

  // Output makeup gain in dB, applied after gain reduction.
  double output_db = 0.0;

  double saturation = 0.0;

  bool soft_clip = false;
};

class Processor {
 public:
  void Prepare(double sample_rate) noexcept;
  void SetParams(const Params& p) noexcept;

  // Processes samples in-place on both channels.
  void ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                    const double* sc_r, int num_frames) noexcept;

  // Returns the gain reduction in dB at the end of the last processed
  // block (always >= 0; 0 = no compression). Makeup/output gain is excluded.
  double GainReductionDb() const noexcept { return gr_meter_db_; }

 private:
  // Converts a time constant in milliseconds to a one-pole IIR coefficient.
  double MsToCoef(double ms) const noexcept;

  // Map Params (peak_reduction + limit_mode) onto the five internal DSP
  // coefficients, storing results in threshold_db_ .. makeup_db_.
  // Called by SetParams() before the coefficient cache is refreshed.
  void ApplyPeakMapping() noexcept;

  Params params_;
  double sample_rate_ = 44100.0;

  // Derived DSP coefficients, cached by SetParams().
  // (Consistent with the caching convention in the other Processor classes.)
  double th_linear_ = 1.0;
  double makeup_linear_ = 1.0;
  double comp_slope_ = 0.0;
  double at_ = 0.0;
  double rt_ = 0.0;

  // Gain smoothing state: last computed gain value carried across blocks.
  double g_prev_ = 1.0;

  // Per-sample gain buffer. Index 0 holds the inter-block carry-over;
  // indices 1..num_frames hold the smoothed gain for the current block.
  // Grown lazily on the first ProcessBlock call if needed.
  std::vector<double> g_buf_;

  // Cached GR-meter reading in dB, refreshed at the end of each
  // ProcessBlock call.
  double gr_meter_db_ = 0.0;

  double output_linear_ = 1.0;

  saturation::SingleEndedTriode triode_;
};

}  // namespace levelcomp_dsp
