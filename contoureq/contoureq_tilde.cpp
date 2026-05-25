#include "dsp/contoureq_dsp.h"
#include "oversample.h"
#include "stereo_os_base.h"

using namespace c74::min;

class contoureq_tilde : public object<contoureq_tilde>,
                        public StereoOsBase<contoureq_tilde> {
 public:
  MIN_DESCRIPTION{"Contour EQ: Analog emulation EQ with shelving filters"};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

  inlet<> in_left{this, "(signal) left input", "signal"};
  inlet<> in_right{this, "(signal) right input", "signal"};
  outlet<> out_left{this, "(signal) left output", "signal"};
  outlet<> out_right{this, "(signal) right output", "signal"};
  outlet<> out_status{this, "latency <n>"};

 private:
  contoureq_dsp::Processor dsp_;
  contoureq_dsp::Parameters params_;

 public:
  void PrepareEngine(double os_sr) { dsp_.Prepare(os_sr); }

  void PreProcess() { dsp_.SetParameters(params_); }

  void ProcessBlock(double* l, double* r, int n) { dsp_.ProcessBlock(l, r, n); }

  attribute<int> model{
      this, "model", 0, title{"EQ Model"},
      description{
          "EQ algorithm. "
          "0 = Baxandall  (dB-linear +/-15 dB taper, v1 crossover curves); "
          "1 = Baxandall3 (squared taper, Bessel-Q shelves, input drive). "
          "Switching resets filter state."},
      range{0, 1},
      setter{
          MIN_FUNCTION{params_.model = clamp(static_cast<int>(args[0]), 0, 1);
  return args;
}
}
}
;

attribute<number> gain{
    this, "gain", 0.5, title{"Input / Output Gain"},
    description{"Model 0 (Baxandall):  output trim, +/-15 dB. "
                "Model 1 (Baxandall3): input drive and soft-clip level."},
    range{0.0, 1.0},
    setter{MIN_FUNCTION{params_.gain =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
return args;
}
}
}
;

attribute<number> treble{
    this, "treble", 0.5, title{"Treble"},
    description{"Treble shelf level and crossover frequency."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{params_.treble =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
return args;
}
}
}
;

attribute<number> bass{
    this, "bass", 0.5, title{"Bass"},
    description{"Bass shelf level and crossover frequency."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{params_.bass =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
return args;
}
}
}
;

attribute<bool> phase_inv{
    this, "phase_inv", false,
    description{"Phase invert: 1 = inverted, 0 = normal."},
    setter{MIN_FUNCTION{params_.phase_inv = static_cast<bool>(args[0]);
return args;
}
}
}
;

attribute<int> oversample{
    this, "oversample", 0, title{"Oversampling"},
    description{"Oversampling ratio. 0=off  1=2x  2=4x  3=8x."},
    range{0, oversample::kMaxOversampleIndex},
    setter{MIN_FUNCTION{const int v = clamp(static_cast<int>(args[0]), 0,
                                            oversample::kMaxOversampleIndex);
const int latency = ReconfigureOversample(v);
if (latency >= 0) out_status.send("latency", latency);
return {v};
}
}
}
;

message<> dspsetup{
    this, "dspsetup",
    MIN_FUNCTION{const int latency = DspSetup(static_cast<double>(args[0]),
                                              static_cast<int>(args[1]),
                                              static_cast<int>(oversample));
out_status.send("latency", latency);
return {latency};
}
}
;
}
;

MIN_EXTERNAL(contoureq_tilde);
