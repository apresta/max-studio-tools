#include "dsp/bloomeq_dsp.h"
#include "stereo_os_base.h"
#include "oversample.h"

using namespace c74::min;

class bloomeq_tilde : public object<bloomeq_tilde>,
                      
                      public StereoOsBase<bloomeq_tilde> {
 public:
  MIN_DESCRIPTION{"Bloom EQ: Analog emulation EQ with boost/cut filters"};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

  inlet<> in_left{this, "(signal) left input", "signal"};
  inlet<> in_right{this, "(signal) right input", "signal"};
  outlet<> out_left{this, "(signal) left output", "signal"};
  outlet<> out_right{this, "(signal) right output", "signal"};
  outlet<> out_status{this, "latency <n>"};

 private:
  bloomeq_dsp::Processor eq_;
  bloomeq_dsp::Parameters params_;

 public:
  void PrepareEngine(double os_sr) { eq_.Prepare(os_sr); }

  void PreProcess() { eq_.SetParameters(params_); }

  void ProcessBlock(double* l, double* r, int n) {
    eq_.ProcessBlock(l, r, n);
  }

  attribute<number> lo_boost{
      this, "lo_boost", bloomeq_dsp::kMinKnob, title{"Low Boost Amount"},
      description{"Low-frequency boost."}, range{0.0, 1.0},
      setter{MIN_FUNCTION{const double v =
                              clamp(static_cast<double>(args[0]), 0.0, 1.0);
  params_.lo_boost = std::max(v, bloomeq_dsp::kMinKnob);
  return args;
}
}
}
;

attribute<number> lo_cut{
    this, "lo_cut", bloomeq_dsp::kMinKnob, title{"Low Attenuation Amount"},
    description{"Low-frequency attenuation."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{const double v =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
params_.lo_cut = std::max(v, bloomeq_dsp::kMinKnob);
return args;
}
}
}
;

attribute<int> lo_freq{this, "lo_freq", 2, title{"Low Frequency"},
                       description{"Low-band frequency selector. "
                                   "0=20 Hz  1=30 Hz  2=60 Hz  3=100 Hz"},
                       range{0, 3},
                       setter{MIN_FUNCTION{params_.lo_freq = clamp(
                                               static_cast<int>(args[0]), 0, 3);
return args;
}
}
}
;

attribute<number> hi_boost{
    this, "hi_boost", bloomeq_dsp::kMinKnob, title{"High Boost Amount"},
    description{"High-frequency boost."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{const double v =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
params_.hi_boost = std::max(v, bloomeq_dsp::kMinKnob);
return args;
}
}
}
;

attribute<int> hi_boost_freq{
    this, "hi_boost_freq", 3, title{"High Boost Frequency"},
    description{"High-boost frequency selector. "
                "0=3kHz  1=4kHz  2=5kHz  3=8kHz  4=10kHz  5=12kHz  6=16kHz"},
    range{0, 6},
    setter{MIN_FUNCTION{params_.hi_boost_freq =
                            clamp(static_cast<int>(args[0]), 0, 6);
return args;
}
}
}
;

attribute<number> hi_bq{
    this, "hi_bq", 0.5, title{"High Boost Q"},
    description{"Q factor of the high-frequency boost shelf. "
                "0 = widest, 1 = narrowest."},
    range{0.0, 1.0},
    setter{MIN_FUNCTION{const double v =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
params_.hi_bandwidth = 1.0 - std::max(v, bloomeq_dsp::kMinKnob);
return args;
}
}
}
;

attribute<number> hi_cut{
    this, "hi_cut", bloomeq_dsp::kMinKnob, title{"High Attenuation Amount"},
    description{"High-frequency attenuation."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{const double v =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
params_.hi_cut = std::max(v, bloomeq_dsp::kMinKnob);
return args;
}
}
}
;

attribute<int> hi_cut_freq{
    this, "hi_cut_freq", 2, title{"High Attenuation Frequency"},
    description{"High-cut frequency selector. 0=5kHz  1=10kHz  2=20kHz"},
    range{0, 2},
    setter{MIN_FUNCTION{params_.hi_cut_freq =
                            clamp(static_cast<int>(args[0]), 0, 2);
return args;
}
}
}
;

attribute<int> eq_enable{
    this, "eq_enable", 1, title{"EQ Enable"},
    description{"EQ enable: 1 = active (default), 0 = bypassed."}, range{0, 1},
    setter{MIN_FUNCTION{params_.eq_enable = (static_cast<int>(args[0]) != 0);
return args;
}
}
}
;

attribute<int> phase_inv{
    this, "phase_inv", 0, title{"Phase Invert"},
    description{"Phase invert: 1 = inverted, 0 = normal."}, range{0, 1},
    setter{MIN_FUNCTION{params_.phase_inv = (static_cast<int>(args[0]) != 0);
return args;
}
}
}
;

attribute<number> gain{
    this, "gain", 0.5, title{"Saturation Input Gain"},
    description{"Input level fed into the Tube2 saturator."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{params_.gain =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
return args;
}
}
}
;

attribute<number> saturation{
    this, "saturation", 0.5, title{"Saturation Character"},
    description{"Tube saturation character."}, range{0.0, 1.0},
    setter{MIN_FUNCTION{params_.saturation =
                            clamp(static_cast<double>(args[0]), 0.0, 1.0);
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

message<> dspsetup{this, "dspsetup",
                   MIN_FUNCTION{
const int latency = DspSetup(static_cast<double>(args[0]),
                             static_cast<int>(args[1]),
                             static_cast<int>(oversample));
out_status.send("latency", latency);
return {latency};
}
}
;
}
;

MIN_EXTERNAL(bloomeq_tilde);
