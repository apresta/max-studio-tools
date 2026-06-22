#include "dsp/contoureq_dsp.h"
#include "stereo_os_base.h"

class contoureq_tilde : public base::StereoOsBase<contoureq_tilde> {
 public:
  MIN_DESCRIPTION{"Contour EQ: Analog emulation EQ with shelving filters."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

 private:
  contoureq_dsp::Processor proc_;
  contoureq_dsp::Params params_;

 public:
  void PrepareEngine(double os_sr) { proc_.Prepare(os_sr); }

  void PreProcess() { proc_.SetParams(params_); }

  void ProcessBlock(double* l, double* r, const double* /*sc_l*/,
                    const double* /*sc_r*/, int num_frames) {
    proc_.ProcessBlock(l, r, num_frames);
  }

  DECLARE_ATTR_INT(
      model, "EQ Model",
      "EQ algorithm. "
      "0 = Baxandall  (dB-linear +/-15 dB taper, v1 crossover curves); "
      "1 = Baxandall3 (squared taper, Bessel-Q shelves, input drive). "
      "Switching resets filter state.",
      0, 0, 1, [this](int v) { params_.model = v; });

  DECLARE_ATTR_DOUBLE(gain, "Input / Output Gain",
                      "Model 0 (Baxandall):  output trim, +/-15 dB. "
                      "Model 1 (Baxandall3): input drive and soft-clip level.",
                      0.5, 0.0, 1.0, [this](double v) { params_.gain = v; });

  DECLARE_ATTR_DOUBLE(treble, "Treble",
                      "Treble shelf level and crossover frequency.", 0.5, 0.0,
                      1.0, [this](double v) { params_.treble = v; });

  DECLARE_ATTR_DOUBLE(bass, "Bass", "Bass shelf level and crossover frequency.",
                      0.5, 0.0, 1.0, [this](double v) { params_.bass = v; });

  DECLARE_ATTR_BOOL(phase_inv, "Phase Invert",
                    "Phase invert: 1 = inverted, 0 = normal.", false,
                    [this](bool v) { params_.phase_inv = v; });
};

MIN_EXTERNAL(contoureq_tilde);
