#include "dsp/focuseq_dsp.h"
#include "stereo_os_base.h"
#include "oversample.h"

using namespace c74::min;

class focuseq_tilde : public object<focuseq_tilde>,
                      public StereoOsBase<focuseq_tilde> {
 public:
  MIN_DESCRIPTION{"Focus EQ: Analog emulation EQ based on solid state console"};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

  inlet<> in_left{this, "(signal) left input", "signal"};
  inlet<> in_right{this, "(signal) right input", "signal"};
  outlet<> out_left{this, "(signal) left output", "signal"};
  outlet<> out_right{this, "(signal) right output", "signal"};
  outlet<> out_status{this, "latency <n>"};

 private:
  FocusEqDsp eq_;
  FocusEqDsp::Parameters params_;

 public:
  void PrepareEngine(double os_sr) {
    const int max_os_frames = vs_ * os_.ratio();
    eq_.Prepare(os_sr);
    eq_.EnsureCapacity(max_os_frames);
  }

  void PreProcess() { eq_.SetParameters(params_); }

  void ProcessBlock(double* l, double* r, int n) {
    eq_.ProcessBlock(l, r, n);
  }

  attribute<number> hpf{
      this, "hpf", 16.0, title{"HPF Frequency (Hz)"},
      description{"Butterworth 12 dB/oct high-pass filter corner frequency."},
      range{16.0, 350.0},
      setter{MIN_FUNCTION{
          params_.hpf_freq = clamp(static_cast<double>(args[0]), 16.0, 350.0);
          return args;
      }}};

  attribute<int> hpfon{
      this, "hpfon", 0, title{"HPF Enable"},
      description{"Toggles the high-pass filter."}, range{0, 1},
      setter{MIN_FUNCTION{
          params_.hpf_enabled = (static_cast<int>(args[0]) != 0);
          return args;
      }}};

  attribute<number> lpf{
      this, "lpf", 20000.0, title{"LPF Frequency (Hz)"},
      description{"Butterworth 12 dB/oct low-pass filter corner frequency"},
      range{1000.0, 20000.0},
      setter{MIN_FUNCTION{
          params_.lpf_freq =
              clamp(static_cast<double>(args[0]), 1000.0, 20000.0);
          return args;
      }}};

  attribute<int> lpfon{
      this, "lpfon", 0, title{"LPF Enable"},
      description{"Toggles the low-pass filter."}, range{0, 1},
      setter{MIN_FUNCTION{
          params_.lpf_enabled = (static_cast<int>(args[0]) != 0);
          return args;
      }}};

  attribute<number> lowfreq{
      this, "lowfreq", 120.0, title{"Low Band Frequency (Hz)"},
      description{"Shelf knee or bell center frequency."}, range{30.0, 450.0},
      setter{MIN_FUNCTION{
          params_.low_freq = clamp(static_cast<double>(args[0]), 30.0, 450.0);
          return args;
      }}};

  attribute<number> lowgain{
      this, "lowgain", 0.0, title{"Low Band Gain (dB)"},
      description{"Boost or cut in dB."}, range{-16.0, 16.0},
      setter{MIN_FUNCTION{
          params_.low_gain = clamp(static_cast<double>(args[0]), -16.0, 16.0);
          return args;
      }}};

  attribute<int> lowtype{
      this, "lowtype", 0, title{"Low Band Shape"},
      description{"0 = low shelf (default), 1 = bell (peaking EQ)."},
      range{0, 1},
      setter{MIN_FUNCTION{
          params_.low_shape = (static_cast<int>(args[0]) != 0)
                                  ? FocusEqDsp::BandShape::Bell
                                  : FocusEqDsp::BandShape::Shelf;
          return args;
      }}};

  attribute<number> lmfreq{
      this, "lmfreq", 500.0, title{"Low-Mid Frequency (Hz)"},
      description{"Bell center frequency."}, range{200.0, 2500.0},
      setter{MIN_FUNCTION{
          params_.lomid_freq =
              clamp(static_cast<double>(args[0]), 200.0, 2500.0);
          return args;
      }}};

  attribute<number> lmgain{
      this, "lmgain", 0.0, title{"Low-Mid Gain (dB)"},
      description{"Boost or cut in dB."}, range{-16.0, 16.0},
      setter{MIN_FUNCTION{
          params_.lomid_gain =
              clamp(static_cast<double>(args[0]), -16.0, 16.0);
          return args;
      }}};

  attribute<number> lmq{
      this, "lmq", 0.5, title{"Low-Mid Q"},
      description{"Quality factor. Proportional-Q is applied on top."},
      range{0.0, 1.0},
      setter{MIN_FUNCTION{
          params_.lomid_q = clamp(static_cast<double>(args[0]), 0.0, 1.0);
          return args;
      }}};

  attribute<number> hmfreq{
      this, "hmfreq", 3000.0, title{"High-Mid Frequency (Hz)"},
      description{"Bell center frequency."}, range{600.0, 7000.0},
      setter{MIN_FUNCTION{
          params_.himid_freq =
              clamp(static_cast<double>(args[0]), 600.0, 7000.0);
          return args;
      }}};

  attribute<number> hmgain{
      this, "hmgain", 0.0, title{"High-Mid Gain (dB)"},
      description{"Boost or cut in dB."}, range{-16.0, 16.0},
      setter{MIN_FUNCTION{
          params_.himid_gain =
              clamp(static_cast<double>(args[0]), -16.0, 16.0);
          return args;
      }}};

  attribute<number> hmq{
      this, "hmq", 0.5, title{"High-Mid Q"},
      description{"Quality factor. Proportional-Q is applied on top."},
      range{0.0, 1.0},
      setter{MIN_FUNCTION{
          params_.himid_q = clamp(static_cast<double>(args[0]), 0.0, 1.0);
          return args;
      }}};

  attribute<number> highfreq{
      this, "highfreq", 5000.0, title{"High Band Frequency (Hz)"},
      description{"Shelf knee or bell center frequency."},
      range{1500.0, 16000.0},
      setter{MIN_FUNCTION{
          params_.high_freq =
              clamp(static_cast<double>(args[0]), 1500.0, 16000.0);
          return args;
      }}};

  attribute<number> highgain{
      this, "highgain", 0.0, title{"High Band Gain (dB)"},
      description{"Boost or cut in dB."}, range{-16.0, 16.0},
      setter{MIN_FUNCTION{
          params_.high_gain = clamp(static_cast<double>(args[0]), -16.0, 16.0);
          return args;
      }}};

  attribute<int> hightype{
      this, "hightype", 0, title{"High Band Shape"},
      description{"0 = high shelf (default), 1 = bell (peaking EQ)."},
      range{0, 1},
      setter{MIN_FUNCTION{
          params_.high_shape = (static_cast<int>(args[0]) != 0)
                                   ? FocusEqDsp::BandShape::Bell
                                   : FocusEqDsp::BandShape::Shelf;
          return args;
      }}};

  attribute<int> eq_enable{
      this, "eq_enable", 1, title{"EQ Enable"},
      description{"EQ enable: 1 = active (default), 0 = bypassed."},
      range{0, 1},
      setter{MIN_FUNCTION{
          params_.eq_enable = (static_cast<int>(args[0]) != 0);
          return args;
      }}};

  attribute<int> phase_inv{
      this, "phase_inv", 0, title{"Phase Invert"},
      description{"Phase invert: 1 = inverted, 0 = normal."}, range{0, 1},
      setter{MIN_FUNCTION{
          params_.phase_inv = (static_cast<int>(args[0]) != 0);
          return args;
      }}};

  attribute<int> sat_enable{
      this, "sat_enable", 0, title{"Saturation Enable"},
      description{"Toggles the Spiral2 soft-saturation stage."}, range{0, 1},
      setter{MIN_FUNCTION{
          params_.sat_enable = (static_cast<int>(args[0]) != 0);
          return args;
      }}};

  attribute<number> gain{
      this, "gain", 0.5, title{"Saturation Input Gain"},
      description{"Input gain for the Spiral2 saturation stage."},
      range{0.0, 1.0},
      setter{MIN_FUNCTION{
          params_.sat_gain = clamp(static_cast<double>(args[0]), 0.0, 1.0);
          return args;
      }}};

  attribute<int> oversample{
      this, "oversample", 0, title{"Oversampling"},
      description{"Oversampling ratio. 0=off  1=2x  2=4x  3=8x."},
      range{0, oversample::kMaxOversampleIndex},
      setter{MIN_FUNCTION{
          const int v = clamp(static_cast<int>(args[0]), 0,
                              oversample::kMaxOversampleIndex);
          const int latency = ReconfigureOversample(v);
          if (latency >= 0) out_status.send("latency", latency);
          return {v};
      }}};

  message<> dspsetup{
      this, "dspsetup", MIN_FUNCTION{
          const int latency = DspSetup(static_cast<double>(args[0]),
                                       static_cast<int>(args[1]),
                                       static_cast<int>(oversample));
          out_status.send("latency", latency);
          return {latency};
      }};
};

MIN_EXTERNAL(focuseq_tilde);
