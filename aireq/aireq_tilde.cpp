#include "dsp/aireq_dsp.h"
#include "oversample.h"
#include "stereo_os_base.h"

using namespace c74::min;

class aireq_tilde : public object<aireq_tilde>,
                    public StereoOsBase<aireq_tilde> {
 public:
  MIN_DESCRIPTION{"Air EQ: Analog emulation EQ with air band"};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

  inlet<> in_left{this, "(signal) left input", "signal"};
  inlet<> in_right{this, "(signal) right input", "signal"};
  outlet<> out_left{this, "(signal) left output", "signal"};
  outlet<> out_right{this, "(signal) right output", "signal"};
  outlet<> out_status{this, "latency <n>"};

 private:
  AirEqDsp eq_;
  AirEqDsp::Params params_;

 public:
  void PrepareEngine(double os_sr) {
    eq_.Prepare(os_sr);
  }

  void PreProcess() { eq_.SetParameters(params_); }

  void ProcessBlock(double* l, double* r, int n) { eq_.ProcessBlock(l, r, n); }

  attribute<number> gain10{
      this, "gain10", 0.0, title{"10 Hz Gain (dB)"},
      description{"Bell gain for the 10 Hz band."}, range{-10.0, 10.0},
      setter{MIN_FUNCTION{params_.gains[AirEqDsp::kBand10] =
                              clamp(static_cast<double>(args[0]), -10.0, 10.0);
  return args;
}
}
}
;

attribute<number> gain40{
    this, "gain40", 0.0, title{"40 Hz Gain (dB)"},
    description{"Bell gain for the 40 Hz band."}, range{-10.0, 10.0},
    setter{MIN_FUNCTION{params_.gains[AirEqDsp::kBand40] =
                            clamp(static_cast<double>(args[0]), -10.0, 10.0);
return args;
}
}
}
;

attribute<number> gain160{
    this, "gain160", 0.0, title{"160 Hz Gain (dB)"},
    description{"Bell gain for the 160 Hz band."}, range{-10.0, 10.0},
    setter{MIN_FUNCTION{params_.gains[AirEqDsp::kBand160] =
                            clamp(static_cast<double>(args[0]), -10.0, 10.0);
return args;
}
}
}
;

attribute<number> gain640{
    this, "gain640", 0.0, title{"640 Hz Gain (dB)"},
    description{"Bell gain for the 640 Hz band."}, range{-10.0, 10.0},
    setter{MIN_FUNCTION{params_.gains[AirEqDsp::kBand640] =
                            clamp(static_cast<double>(args[0]), -10.0, 10.0);
return args;
}
}
}
;

attribute<number> gain2k5{
    this, "gain2k5", 0.0, title{"2.5 kHz Shelf Gain (dB)"},
    description{"High-shelf gain at 2.5 kHz."}, range{-10.0, 10.0},
    setter{MIN_FUNCTION{params_.gains[AirEqDsp::kShelf2k5] =
                            clamp(static_cast<double>(args[0]), -10.0, 10.0);
return args;
}
}
}
;

attribute<number> gainhi{
    this, "gainhi", 0.0, title{"Air Shelf Gain (dB)"},
    description{"Gain for the variable-frequency air shelf."}, range{0.0, 10.0},
    setter{MIN_FUNCTION{params_.gains[AirEqDsp::kShelfHi] =
                            clamp(static_cast<double>(args[0]), 0.0, 10.0);
return args;
}
}
}
;

attribute<int> hitype{
    this, "hitype", 0, title{"Air Shelf Frequency"},
    description{"Corner frequency of the air shelf. "
                "0=off  1=2.5kHz  2=5kHz  3=10kHz  4=20kHz  5=40kHz"},
    range{0, static_cast<int>(AirEqDsp::kNumHighShelves) - 1},
    setter{MIN_FUNCTION{
        params_.high_shelf = static_cast<AirEqDsp::HighShelf>(
            clamp(static_cast<int>(args[0]), 0,
                  static_cast<int>(AirEqDsp::kNumHighShelves) - 1));
return args;
}
}
}
;

attribute<int> keepgain{
    this, "keepgain", 0, title{"Keep Gain"},
    description{"When 1, output level is compensated to preserve loudness."},
    range{0, 1},
    setter{MIN_FUNCTION{params_.keep_gain = (static_cast<int>(args[0]) != 0);
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

MIN_EXTERNAL(aireq_tilde);
