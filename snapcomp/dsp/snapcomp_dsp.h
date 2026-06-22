// Ported from the JSFX "VCA Compressor S2" by Tukan Studios.
// See https://github.com/TukanStudios/TUKAN_STUDIOS_PLUGINS.

#pragma once

#include "Spiral2.h"

namespace snapcomp_dsp {

struct Params {
  // Threshold in dBFS. Gain reduction begins above this level.
  double threshold_db = -20.0;

  // Ratio control, 0..6, mapping to {1, 2, 4, 6, 8, 12, 20} : 1.
  int ratio_index = 1;

  // Attack control, 0..1 (continuous). Log-mapped to a 0.1-100 ms time
  // constant.
  double attack = 0.1;

  // Release control, 0..1 (continuous). Log-mapped to a 15-500 ms time
  // constant.
  double release = 0.1;

  // Output makeup gain in dB, applied after compression.
  double output_db = 0.0;

  // Soft knee toggle. On = 15 dB quadratic knee; off = hard knee.
  bool soft_knee = false;

  // Toggles the Spiral2 saturation stage.
  bool sat_enable = false;

  bool soft_clip = false;
};

class Processor {
 public:
  void Prepare(double sample_rate) noexcept;
  void SetParams(const Params& p) noexcept;

  // Processes samples in-place on both channels.
  void ProcessBlock(double* out_l, double* out_r, const double* sc_l,
                    const double* sc_r, int num_frames) noexcept;

  // Gain reduction in dB at the end of the last processed block (always
  // >= 0; 0 = no compression). Makeup/output gain is excluded.
  double GainReductionDb() const noexcept { return gr_meter_db_; }

 private:
  // Static gain curve (the "gain computer"), with optional quadratic knee.
  double CalcGainDb(double level_db) const noexcept;

  // Topology 1 ("Punch"): detect on the (smoothed) input level directly.
  double ComputeGain(double mx) noexcept;

  Params params_;
  double sample_rate_ = 44100.0;

  // Shared envelope-follower state (stereo-linked; reused verbatim by
  // whichever topology is active).
  double level_state_ = 0.0;         // "yL": smoothed detector level.
  double release_hold_state_ = 0.0;  // "ya": release-rate-limited follower.
  double gain_correction_db_ = 0.0;  // "c": last gain correction (<= 0).

  // Parameter-derived coefficients, refreshed in SetParams().
  double output_gain_linear_ = 1.0;
  double iratio_ = 1.0;
  double knee_width_db_ = 0.0;
  double quad_factor_ = 0.0;
  double attack_coef_ = 0.0;
  double release_coef_ = 0.0;

  double gr_meter_db_ = 0.0;

  saturation::Spiral2 spiral2_;
};

}  // namespace snapcomp_dsp
