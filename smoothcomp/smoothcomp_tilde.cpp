#include "dsp/smoothcomp_dsp.h"
#include "stereo_os_base.h"

class smoothcomp_tilde : public base::StereoOsBase<smoothcomp_tilde> {
 public:
  MIN_DESCRIPTION{"Smooth Compressor: Vari-mu compressor emulation."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, dynamics, compressor"};
  MIN_RELATED{"omx.comp~, peakamp~, gain~"};

  c74::min::outlet<> out_gr{this, "gain reduction (dB)"};

 private:
  smoothcomp_dsp::Processor proc_;
  smoothcomp_dsp::Params params_;

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

  DECLARE_ATTR_INT(model, "Model",
                   "Historical unit model. 0 = 60050A (smooth, musical), "
                   "1 = 60070B (faster attack/release), "
                   "2 = 61010B (fastest). "
                   "Selects the sidechain filter corner, attack time, and gain-"
                   "reduction curve measured from that unit.",
                   0, 0, 2, [this](int v) {
                     params_.model = static_cast<smoothcomp_dsp::Model>(v);
                   });

  DECLARE_ATTR_DOUBLE(input, "Input Gain", "Input level in dB.", 0.0, -30.0,
                      30.0, [this](double v) { params_.input_db = v; });

  DECLARE_ATTR_DOUBLE(release, "Release", "Release time control (1-6).", 4.0,
                      1.0, 6.0, [this](double v) { params_.release_pos = v; });

  DECLARE_ATTR_DOUBLE(output, "Output Gain",
                      "Output makeup gain in dB, applied after compression.",
                      0.0, -30.0, 30.0,
                      [this](double v) { params_.output_db = v; });

  DECLARE_ATTR_DOUBLE(saturation, "Saturation Character",
                      "Tube saturation character.", 0.5, 0.0, 1.0,
                      [this](double v) { params_.saturation = v; });

  DECLARE_ATTR_BOOL(soft_clip, "Soft clip", "Toggles final soft clip stage.",
                    false, [this](bool v) { params_.soft_clip = v; });
};

MIN_EXTERNAL(smoothcomp_tilde);
