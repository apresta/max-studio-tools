#pragma once

#include "Baxandall.h"
#include "Baxandall3.h"

namespace contoureq_dsp {

struct Params {
  // 0 = Baxandall  (output trim + dB-linear taper, v1 crossover curves)
  // 1 = Baxandall3 (input drive + squared taper,   Bessel-Q shelves)
  int model = 0;  // [0, 1]
  // Gain knob: routes to Baxandall::output_level (model 0)
  //                   or Baxandall3::input_gain  (model 1).
  double gain = 0.5;    // [0, 1]
  double treble = 0.5;  // [0, 1]
  double bass = 0.5;    // [0, 1]
  bool phase_inv = false;
};

class Processor {
 public:
  void Prepare(double sample_rate) noexcept;
  void SetParams(const Params& p) noexcept;
  void ProcessBlock(double* out_l, double* out_r, int num_frames) noexcept;

 private:
  // EQ model variants; only the active one processes audio.
  Baxandall eq_v1_;   // model == 0
  Baxandall3 eq_v3_;  // model == 1

  Params params_;
  double sample_rate_ = 44100.0;
};

}  // namespace contoureq_dsp
