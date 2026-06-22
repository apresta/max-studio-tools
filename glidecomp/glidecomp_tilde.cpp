#include "dsp/glidecomp_dsp.h"
#include "stereo_os_base.h"

class glidecomp_tilde : public base::StereoOsBase<glidecomp_tilde> {
 public:
  MIN_DESCRIPTION{"Glide Compressor: Tube-optical compressor emulation."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, dynamics, compressor"};
  MIN_RELATED{"omx.comp~, peakamp~, gain~"};

  c74::min::outlet<> out_gr{this, "gain reduction (dB)"};

 private:
  glidecomp_dsp::Processor proc_;
  glidecomp_dsp::Params params_;

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

  DECLARE_ATTR_DOUBLE(
      ratio, "Ratio",
      "Compression ratio (2.0-10.0). Determines the amount of gain "
      "reduction applied once the signal exceeds the threshold.",
      6.0, 2.0, 10.0, [this](double v) { params_.ratio = v; });

  DECLARE_ATTR_DOUBLE(threshold, "Threshold",
                      "Threshold in dB. Level at which compression begins.",
                      0.0, -40.0, 10.0,
                      [this](double v) { params_.threshold_db = v; });

  DECLARE_ATTR_DOUBLE(
      attack, "Attack",
      "Attack time control (0-10). Controls how quickly the compressor "
      "responds to signals exceeding the threshold.",
      5.0, 0.0, 10.0, [this](double v) { params_.attack = v; });

  DECLARE_ATTR_DOUBLE(
      release, "Release",
      "Release time control (0-10). Controls how quickly the compressor "
      "stops compressing after the signal falls below the threshold.",
      5.0, 0.0, 10.0, [this](double v) { params_.release = v; });

  DECLARE_ATTR_DOUBLE(output, "Output Gain",
                      "Output makeup gain in dB, applied after compression.",
                      0.0, -24.0, 24.0,
                      [this](double v) { params_.output_db = v; });

  DECLARE_ATTR_DOUBLE(saturation, "Saturation amount",
                      "Saturation amount applied using PurestDrive.", 0.0, 0.0,
                      1.0, [this](double v) { params_.saturation = v; });

  DECLARE_ATTR_BOOL(soft_clip, "Soft clip", "Toggles final soft clip stage.",
                    false, [this](bool v) { params_.soft_clip = v; });
};

MIN_EXTERNAL(glidecomp_tilde);
