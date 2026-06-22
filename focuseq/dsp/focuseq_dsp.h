#pragma once

#include "Spiral2.h"
#include "but_filter.h"
#include "dynamic_biquad.h"

namespace focuseq_dsp {

enum class BandShape { kShelf, kBell };

struct Params {
  double hpf_freq = 16.0;
  bool hpf_enabled = false;

  double lpf_freq = 20000.0;
  bool lpf_enabled = false;

  double low_freq = 120.0;
  double low_gain = 0.0;
  double low_q = 0.5;
  BandShape low_shape = BandShape::kShelf;

  double lomid_freq = 500.0;
  double lomid_gain = 0.0;
  double lomid_q = 1.0;

  double himid_freq = 3000.0;
  double himid_gain = 0.0;
  double himid_q = 1.0;

  double high_freq = 5000.0;
  double high_gain = 0.0;
  double high_q = 0.5;
  BandShape high_shape = BandShape::kShelf;

  bool eq_enable = true;
  bool phase_inv = false;
  bool sat_enable = false;
  double sat_gain = 0.5;
};

class Processor {
 public:
  void SetParams(const Params& p) noexcept;

  const Params& GetParams() const noexcept { return params_; }

  // Reset all filter state and recompute coefficients for sample_rate.
  void Prepare(double sample_rate) noexcept;

  // Process num_frames stereo frames in-place.
  void ProcessBlock(double* out_l, double* out_r, int num_frames) noexcept;

 private:
  void UpdateHpfCoeffs() noexcept;
  void UpdateLpfCoeffs() noexcept;
  void UpdateLowCoeffs() noexcept;
  void UpdateLowMidCoeffs() noexcept;
  void UpdateHighMidCoeffs() noexcept;
  void UpdateHighCoeffs() noexcept;

  // Inputs that were last fed into each Update*Coeffs() call.
  // Each Update* checks these before calling into biquad_coeffs.h and skips
  // the transcendental computation when nothing has changed.
  struct Computed {
    // Low band.
    double low_freq = -1.0;  // sentinel: forces recompute on first call
    double low_gain = -1.0;
    double low_q = -1.0;
    BandShape low_shape = BandShape::kBell;

    // Low-mid band.
    double lomid_freq = -1.0;
    double lomid_gain = -1.0;
    double lomid_q = -1.0;

    // High-mid band.
    double himid_freq = -1.0;
    double himid_gain = -1.0;
    double himid_q = -1.0;

    // High band.
    double high_freq = -1.0;
    double high_gain = -1.0;
    double high_q = -1.0;
    BandShape high_shape = BandShape::kBell;
  };

  dsp::ButFilter hpf_{dsp::ButType::kHighPass};
  dsp::ButFilter lpf_{dsp::ButType::kLowPass};

  dsp::DynamicBiquad low_bq_;
  dsp::DynamicBiquad lomid_bq_;
  dsp::DynamicBiquad himid_bq_;
  dsp::DynamicBiquad high_bq_;

  saturation::Spiral2 spiral2_;

  double sample_rate_ = 44100.0;
  Params params_;
  Computed last_computed_;
};

}  // namespace focuseq_dsp
