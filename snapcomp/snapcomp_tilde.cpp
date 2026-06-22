#include "dsp/snapcomp_dsp.h"
#include "stereo_os_base.h"

class snapcomp_tilde : public base::StereoOsBase<snapcomp_tilde> {
 public:
  MIN_DESCRIPTION{"Snap Compressor: VCA compressor emulation."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, dynamics, compressor"};
  MIN_RELATED{"omx.comp~, peakamp~, gain~"};

  c74::min::outlet<> out_gr{this, "gain reduction (dB)"};

 private:
  snapcomp_dsp::Processor proc_;
  snapcomp_dsp::Params params_;

 public:
  void PrepareEngine(double os_sr) { proc_.Prepare(os_sr); }

  void PreProcess() {
    proc_.SetParams(params_);
    out_gr.send(proc_.GainReductionDb());
  }

  void ProcessBlock(double* l, double* r, const double* sc_l,
                    const double* sc_r, int num_frames) {
    proc_.ProcessBlock(l, r, sc_l, sc_r, num_frames);
  }

  DECLARE_ATTR_DOUBLE(threshold, "Threshold",
                      "Threshold in dB. Level at which compression begins.",
                      -20.0, -40.0, 0.0,
                      [this](double v) { params_.threshold_db = v; });

  DECLARE_ATTR_INT(
      ratio, "Ratio",
      "Compression ratio. 0 = 1:1, 1 = 2:1, 2 = 4:1, 3 = 6:1, 4 = 8:1, "
      "5 = 12:1, 6 = 20:1.",
      1, 0, 6, [this](int v) { params_.ratio_index = v; });

  DECLARE_ATTR_BOOL(knee, "Knee",
                    "0 = hard knee. 1 = 15 dB quadratic soft knee.", false,
                    [this](bool v) { params_.soft_knee = v; });

  DECLARE_ATTR_DOUBLE(
      attack, "Attack",
      "Attack control (0-1). Log-mapped to a 0.1-100 ms time constant.", 0.1,
      0.0, 1.0, [this](double v) { params_.attack = v; });

  DECLARE_ATTR_DOUBLE(
      release, "Release",
      "Release control (0-1). Log-mapped to a 15-500 ms time constant.", 0.1,
      0.0, 1.0, [this](double v) { params_.release = v; });

  DECLARE_ATTR_DOUBLE(output, "Output Gain",
                      "Output makeup gain in dB, applied after compression.",
                      0.0, -24.0, 24.0,
                      [this](double v) { params_.output_db = v; });

  DECLARE_ATTR_BOOL(sat_enable, "Saturation Enable",
                    "Toggles the Spiral2 soft-saturation stage.", false,
                    [this](bool v) { params_.sat_enable = v; });

  DECLARE_ATTR_BOOL(soft_clip, "Soft clip", "Toggles final soft clip stage.",
                    false, [this](bool v) { params_.soft_clip = v; });
};

MIN_EXTERNAL(snapcomp_tilde);
