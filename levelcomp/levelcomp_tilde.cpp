#include "dsp/levelcomp_dsp.h"
#include "stereo_os_base.h"

class levelcomp_tilde : public base::StereoOsBase<levelcomp_tilde> {
 public:
  MIN_DESCRIPTION{"Level Compressor: Optical compressor emulation."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, dynamics, compressor"};
  MIN_RELATED{"omx.comp~, peakamp~, gain~"};

  c74::min::outlet<> out_gr{this, "gain reduction (dB)"};

 private:
  levelcomp_dsp::Processor proc_;
  levelcomp_dsp::Params params_;

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
      peak, "Peak Reduction",
      "LA-2A peak reduction (0–100). Drives all five DSP parameters.", 0.0, 0.0,
      100.0, [this](double v) { params_.peak_reduction = v; });

  DECLARE_ATTR_INT(
      mode, "Compress / Limit",
      "0 = Compress (~3:1 effective), 1 = Limit (~inf:1 effective). "
      "Selects the learned parameter table used by the peak mapping. "
      "Recalculates all five DSP parameters immediately.",
      0, 0, 1, [this](int v) { params_.limit_mode = v; });

  DECLARE_ATTR_DOUBLE(output, "Output Gain",
                      "Output makeup gain in dB, applied after compression.",
                      0.0, 0.0, 24.0,
                      [this](double v) { params_.output_db = v; });

  DECLARE_ATTR_DOUBLE(saturation, "Saturation amount",
                      "Saturation amount applied using SingleEndedTriode.", 0.0,
                      0.0, 1.0, [this](double v) { params_.saturation = v; });

  DECLARE_ATTR_BOOL(soft_clip, "Soft clip", "Toggles final soft clip stage.",
                    false, [this](bool v) { params_.soft_clip = v; });
};

MIN_EXTERNAL(levelcomp_tilde);
