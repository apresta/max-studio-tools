#pragma once

#include "Baxandall.h"
#include "Baxandall3.h"

namespace contoureq_dsp {

struct Parameters {
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
  // Reset all filter state and recompute coefficients for sample_rate.
  // Must be called at least once before ProcessBlock().
  void Prepare(double sample_rate);

  // Store new parameter values. Thread-safe only when the caller guarantees
  // no concurrent ProcessBlock().
  void SetParameters(const Parameters& p);

  // Process frames stereo frames in-place.
  void ProcessBlock(double* out_l, double* out_r, int frames);

 private:
  // Both engines are held as members; only the active one processes audio.
  Baxandall eq_v1_;   // model == 0
  Baxandall3 eq_v3_;  // model == 1

  Parameters params_;
  double sample_rate_{44100.0};
};

}  // namespace contoureq_dsp
