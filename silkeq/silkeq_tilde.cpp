#include "dsp/silkeq_dsp.h"
#include "stereo_os_base.h"
#include "oversample.h"

using namespace c74::min;

class silkeq_tilde : public object<silkeq_tilde>,
                     
                     public StereoOsBase<silkeq_tilde> {
 public:
  MIN_DESCRIPTION{"Silk EQ: Analog emulation EQ with transformer saturation"};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

  inlet<> in_left{this, "(signal) left input", "signal"};
  inlet<> in_right{this, "(signal) right input", "signal"};
  outlet<> out_left{this, "(signal) left output", "signal"};
  outlet<> out_right{this, "(signal) right output", "signal"};
  outlet<> out_status{this, "latency <n>"};

 private:
  silkeq_dsp::Processor dsp_;
  silkeq_dsp::Parameters params_;

 public:
  void PrepareEngine(double os_sr) { dsp_.Prepare(os_sr); }

  void PreProcess() { dsp_.SetParameters(params_); }

  void ProcessBlock(double* l, double* r, int n) {
    dsp_.ProcessBlock(l, r, n);
  }

  attribute<double> hf_gain{
      this, "hf_gain", 0.0, description{"High-frequency shelf gain in dB."},
      range{-16.0, 16.0},
      setter{MIN_FUNCTION{params_.c_hpf_gain = static_cast<double>(args[0]);
  return args;
}
}
}
;

attribute<double> mf_gain{
    this, "mf_gain", 0.0, description{"Mid-frequency peak gain in dB."},
    range{-18.0, 18.0},
    setter{MIN_FUNCTION{params_.c_mpf_gain = static_cast<double>(args[0]);
return args;
}
}
}
;

attribute<int> mf_freq{
    this, "mf_freq", 0,
    description{"Mid frequency selector: 0=Off, 1=360 Hz, 2=700 Hz, "
                "3=1.6 kHz, 4=3.2 kHz, 5=4.8 kHz, 6=7.2 kHz."},
    range{0, 6},
    setter{MIN_FUNCTION{params_.mpf_cut = static_cast<int>(args[0]);
return args;
}
}
}
;

attribute<double> lf_gain{
    this, "lf_gain", 0.0, description{"Low-frequency shelf gain in dB."},
    range{-16.0, 16.0},
    setter{MIN_FUNCTION{params_.c_lpf_gain = static_cast<double>(args[0]);
return args;
}
}
}
;

attribute<int> lf_freq{
    this, "lf_freq", 0,
    description{"Low frequency selector: 0=Off, 1=35 Hz, 2=60 Hz, "
                "3=110 Hz, 4=220 Hz."},
    range{0, 4},
    setter{MIN_FUNCTION{params_.lpf_cut = static_cast<int>(args[0]);
return args;
}
}
}
;

attribute<int> hpf{
    this, "hpf", 0,
    description{"Input high-pass filter: 0=Off, 1=50 Hz, 2=80 Hz, "
                "3=160 Hz, 4=300 Hz."},
    range{0, 4},
    setter{MIN_FUNCTION{params_.hpf_cut = static_cast<int>(args[0]);
return args;
}
}
}
;

attribute<bool> eq_enable{
    this, "eq_enable", true,
    description{"EQ enable: 1 = active, 0 = bypassed."},
    setter{MIN_FUNCTION{params_.eq_enable = static_cast<bool>(args[0]);
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

attribute<double> saturation{
    this, "saturation", 0.0,
    description{"Saturation amount applied after the EQ using Coils."},
    range{0.0, 1.0},
    setter{MIN_FUNCTION{params_.saturation = static_cast<double>(args[0]);
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
};

MIN_EXTERNAL(silkeq_tilde);
