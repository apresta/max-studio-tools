#include "dsp/silkeq_dsp.h"
#include "stereo_os_base.h"

class silkeq_tilde : public base::StereoOsBase<silkeq_tilde> {
 public:
  MIN_DESCRIPTION{"Silk EQ: Analog emulation EQ with transformer saturation."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

 private:
  silkeq_dsp::Processor proc_;
  silkeq_dsp::Params params_;

 public:
  void PrepareEngine(double os_sr) { proc_.Prepare(os_sr); }

  void PreProcess() { proc_.SetParams(params_); }

  void ProcessBlock(double* l, double* r, const double* /*sc_l*/,
                    const double* /*sc_r*/, int num_frames) {
    proc_.ProcessBlock(l, r, num_frames);
  }

  DECLARE_ATTR_DOUBLE(hf_gain, "High-frequency shelf gain",
                      "High-frequency shelf gain in dB.", 0.0, -16.0, 16.0,
                      [this](double v) { params_.hpf_gain = v; });

  DECLARE_ATTR_DOUBLE(mf_gain, "Mid-frequency peak gain",
                      "Mid-frequency peak gain in dB.", 0.0, -18.0, 18.0,
                      [this](double v) { params_.mpf_gain = v; });

  DECLARE_ATTR_INT(mf_freq, "Mid frequency selector",
                   "0 = Off, 1 = 360 Hz, 2 = 700 Hz, 3 = 1.6 kHz, 4 = 3.2 kHz, "
                   "5 = 4.8 kHz, 6 = 7.2 kHz.",
                   0, 0, 6, [this](int v) { params_.mpf_cut = v; });

  DECLARE_ATTR_DOUBLE(lf_gain, "Low-frequency shelf gain",
                      "Low-frequency shelf gain in dB.", 0.0, -16.0, 16.0,
                      [this](double v) { params_.lpf_gain = v; });

  DECLARE_ATTR_INT(lf_freq, "Low frequency selector",
                   "0 = Off, 1 = 35 Hz, 2 = 60 Hz, 3 = 110 Hz, 4 = 220 Hz.", 0,
                   0, 4, [this](int v) { params_.lpf_cut = v; });

  DECLARE_ATTR_INT(hpf, "Input high-pass filter",
                   "0 = Off, 1 = 50 Hz, 2 = 80 Hz, 3 = 160 Hz, 4 = 300 Hz.", 0,
                   0, 4, [this](int v) { params_.hpf_cut = v; });

  DECLARE_ATTR_BOOL(eq_enable, "EQ Enable",
                    "EQ enable: 1 = active, 0 = bypassed.", true,
                    [this](bool v) { params_.eq_enable = v; });

  DECLARE_ATTR_BOOL(phase_inv, "Phase Invert",
                    "Phase invert: 1 = inverted, 0 = normal.", false,
                    [this](bool v) { params_.phase_inv = v; });

  DECLARE_ATTR_DOUBLE(saturation, "Saturation amount",
                      "Saturation amount applied after the EQ using Coils.",
                      0.0, 0.0, 1.0,
                      [this](double v) { params_.saturation = v; });
};

MIN_EXTERNAL(silkeq_tilde);
