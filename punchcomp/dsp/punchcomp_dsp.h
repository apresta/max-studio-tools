// Ported from the JSFX "NC76 S2" by Tukan Studios.
// See https://github.com/TukanStudios/TUKAN_STUDIOS_PLUGINS.

#pragma once

#include "Coils.h"

namespace punchcomp_dsp {

// Compression ratio button bank.
enum class RatioMode { kAllButtonsIn, k20to1, k12to1, k8to1, k4to1 };

struct Params {
  // Selects the ratio (and, for All Buttons In, switches on the ratio-boost
  // latch and the 10x-faster release).
  RatioMode ratio_mode = RatioMode::k4to1;

  // Input gain in dB, applied before the gain computer.
  double input_db = 0.0;

  // Output makeup gain in dB, applied after compression (and before the
  // fixed safety limiter).
  double output_db = 0.0;

  // Raw attack control value. The *internal* attack time constant is 2x this
  // value.
  double attack_us = 100.0;

  // Release time in milliseconds. In All Buttons In mode the *effective*
  // release time is 10x faster than this value.
  double release_ms = 250.0;

  double saturation = 0.0;

  bool soft_clip = false;
};

class Processor {
 public:
  // Reset all envelope/limiter state and (re)compute coefficients for
  // sample_rate. Must be called before the first ProcessBlock and on any
  // sample-rate change.
  void Prepare(double sample_rate) noexcept;

  // Snapshot parameters for use in the next ProcessBlock call. The caller
  // is responsible for ensuring this is not called concurrently with
  // ProcessBlock.
  void SetParams(const Params& p) noexcept;

  // Process num_frames stereo samples in-place.
  void ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                    const double* sc_r, int num_frames) noexcept;

  // Returns the gain reduction in dB at the end of the last processed
  // block (always >= 0; 0 = no compression). Makeup/output gain is excluded.
  double GainReductionDb() const noexcept { return gr_meter_db_; }

 private:
  // Resolve params_.ratio_mode into ratio_ / allin_.
  void ApplyRatioMode() noexcept;

  // Recompute the attack/release one-pole coefficients from params_ and
  // sample_rate_, including the fixed ratio-latch coefficients (which only
  // depend on sample_rate_, but are recomputed here too every time.
  void UpdateTimeConstants() noexcept;

  // exp(-1 / (tau_s * sr)). Returns 0 if tau_s or sr is non-positive.
  static double OnePoleCoefFromTau(double tau_s, double sr) noexcept;

  Params params_;
  double sample_rate_ = 44100.0;

  // Cached/derived values (recomputed by SetParams() / Prepare()).
  double ratio_ = 4.0;
  bool allin_ = false;
  double input_linear_ = 1.0;
  double output_linear_ = 1.0;
  double atcoef_ = 0.0;
  double relcoef_ = 0.0;
  double ratatcoef_ = 0.0;
  double ratrelcoef_ = 0.0;

  // Linked-stereo envelope state.
  double rundb_ = 0.0;
  double runratio_ = 0.0;

  // Fixed-coefficient safety limiter state.
  double ovrlgain_ = 1.0;

  // Smoothed gain reduction in dB (>= 0) from the most recently processed
  // sample, cached for GainReductionDb() reporting.
  double gr_meter_db_ = 0.0;

  saturation::Coils coils_;
};

}  // namespace punchcomp_dsp
