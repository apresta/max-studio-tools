#include "dsp/bloomeq_dsp.h"
#include "stereo_os_base.h"

class bloomeq_tilde : public base::StereoOsBase<bloomeq_tilde> {
 public:
  MIN_DESCRIPTION{"Bloom EQ: Analog emulation EQ with boost/cut filters."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

 private:
  bloomeq_dsp::Processor proc_;
  bloomeq_dsp::Params params_;

 public:
  void PrepareEngine(double os_sr) { proc_.Prepare(os_sr); }

  void PreProcess() { proc_.SetParams(params_); }

  void ProcessBlock(double* l, double* r, const double* /*sc_l*/,
                    const double* /*sc_r*/, int num_frames) {
    proc_.ProcessBlock(l, r, num_frames);
  }

  DECLARE_ATTR_DOUBLE(lo_boost, "Low Boost Amount", "Low-frequency boost.",
                      bloomeq_dsp::kMinKnob, 0.0, 1.0,
                      [this](double v) { params_.lo_boost = v; });

  DECLARE_ATTR_DOUBLE(lo_cut, "Low Attenuation Amount",
                      "Low-frequency attenuation.", bloomeq_dsp::kMinKnob, 0.0,
                      1.0, [this](double v) { params_.lo_cut = v; });

  DECLARE_ATTR_INT(lo_freq, "Low Frequency",
                   "Low-band frequency selector. "
                   "0 = 20 Hz, 1 = 30 Hz, 2 = 60 Hz, 3 = 100 Hz",
                   2, 0, 3, [this](int v) { params_.lo_freq = v; });

  DECLARE_ATTR_DOUBLE(hi_boost, "High Boost Amount", "High-frequency boost.",
                      bloomeq_dsp::kMinKnob, 0.0, 1.0,
                      [this](double v) { params_.hi_boost = v; });

  DECLARE_ATTR_INT(
      hi_boost_freq, "High Boost Frequency",
      "High-boost frequency selector. "
      "0 = 3kHz, 1 = 4kHz, 2 = 5kHz, 3 = 8kHz, 4 = 10kHz, 5 = 12kHz, 6 = 16kHz",
      3, 0, 6, [this](int v) { params_.hi_boost_freq = v; });

  DECLARE_ATTR_DOUBLE(hi_bq, "High Boost Q",
                      "Q factor of the high-frequency boost shelf. "
                      "0 = widest, 1 = narrowest.",
                      0.5, 0.0, 1.0,
                      [this](double v) { params_.hi_bandwidth = 1.0 - v; });

  DECLARE_ATTR_DOUBLE(hi_cut, "High Attenuation Amount",
                      "High-frequency attenuation.", bloomeq_dsp::kMinKnob, 0.0,
                      1.0, [this](double v) { params_.hi_cut = v; });

  DECLARE_ATTR_INT(hi_cut_freq, "High Attenuation Frequency",
                   "High-cut frequency selector. 0=5kHz, 1=10kHz, 2=20kHz", 2,
                   0, 2, [this](int v) { params_.hi_cut_freq = v; });

  DECLARE_ATTR_BOOL(eq_enable, "EQ Enable",
                    "EQ enable: 1 = active, 0 = bypassed.", true,
                    [this](bool v) { params_.eq_enable = v; });

  DECLARE_ATTR_BOOL(phase_inv, "Phase Invert",
                    "Phase invert: 1 = inverted, 0 = normal.", false,
                    [this](bool v) { params_.phase_inv = v; });

  DECLARE_ATTR_DOUBLE(gain, "Saturation Input Gain",
                      "Input level fed into the Tube2 saturator.", 0.5, 0.0,
                      1.0, [this](double v) { params_.gain = v; });

  DECLARE_ATTR_DOUBLE(saturation, "Saturation Character",
                      "Tube saturation character.", 0.5, 0.0, 1.0,
                      [this](double v) { params_.saturation = v; });
};

MIN_EXTERNAL(bloomeq_tilde);
