// The table-based implementation in this device is ported from the original
// RS124 by JClones, with some important changes.
// See https://github.com/JClones/JSFXClones (MIT License).
//
// We have removed the "SuperFuse" mode (not present on the original hardware)
// and the "Box Tone" coloration network. We also replaced the input/output gain
// tables with generic linear gain parameters for more intuitive control.

#pragma once

#include <array>
#include <vector>

#include "Tube2.h"
#include "one_pole_state.h"

namespace smoothcomp_dsp {

enum class Model {
  k60050A = 0,
  k60070B = 1,
  k61010B = 2,
};

struct Params {
  // Compressor model: selects sidechain HPF corner, attack time, and the
  // measured gain-reduction transfer curve used by each historical unit.
  Model model = Model::k60050A;

  // Input gain in dB. Drives the vari-mu cell harder for more compression.
  double input_db = 0.0;

  // Release time control, 1..6 (continuous). Maps to the measured release
  // table for the selected model.
  double release_pos = 4.0;

  // Output makeup gain in dB, applied after gain reduction.
  double output_db = 0.0;

  double saturation = 0.0;

  bool soft_clip = false;
};

// Natural cubic-spline lookup table over a fixed set of (x, y) control
// points. Used for the release-time curve and gain-reduction transfer curve.
class SplineTable {
 public:
  // xs and ys must be the same length (>= 2), with xs strictly increasing.
  void Build(std::vector<double> xs, std::vector<double> ys);

  double Interpolate(double x) const noexcept;

 private:
  struct Segment {
    double c0, c1, c2, c3;  // c0*x^3 + c1*x^2 + c2*x + c3
  };

  std::vector<double> xs_;
  std::vector<Segment> segments_;
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
  struct Channel {
    double level_state{0.0};
    double feedback_gain{1.0};
    int startup_counter{0};
    double startup_level_sum{0.0};
  };

  // One-pole attack/release time constant.
  double SecondsToCoef(double s) const noexcept;

  void DesignSidechainHpf(Model model) noexcept;

  double GetLevel(Channel& ch, double sidechain_sample) noexcept;

  void BuildTables() noexcept;

  Params params_;
  double sample_rate_{44100.0};

  std::array<Channel, 2> channels_;

  // Sidechain HPF ahead of the level detector.
  // Ticked once per sample on both channels together via Vec2.
  dsp::OnePoleState sidechain_hpf_state_;
  double sidechain_hpf_b0_{1.0};
  double sidechain_hpf_b1_{0.0};
  double sidechain_hpf_a1_{0.0};

  double input_gain_linear_{1.0};
  double output_gain_linear_{1.0};
  double attack_coef_{0.0};
  double release_coef_{0.0};

  int startup_counter_max_{0};  // for warmup

  double gr_meter_db_{0.0};

  // Measured tables, one per model.
  // Built once in BuildTables(); selecting a model just swaps which spline
  // is consulted, so no rebuild is needed when the model attribute changes.
  std::array<SplineTable, 3> release_time_table_;  // release pos -> sec
  std::array<SplineTable, 3> gr_table_;            // detector level -> gain

  saturation::Tube2 tube2_;
};

}  // namespace smoothcomp_dsp
