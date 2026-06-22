// Max/MSP wrapper around the NC76 S2-derived punchcomp_dsp::Processor.
// See punchcomp_dsp.h/.cpp for the actual DSP; this file is just attribute
// plumbing, following the same StereoOsBase convention as before.

#include "dsp/punchcomp_dsp.h"
#include "stereo_os_base.h"

class punchcomp_tilde : public base::StereoOsBase<punchcomp_tilde> {
 public:
  MIN_DESCRIPTION{"Punch Compressor: FET compressor emulation (NC76 S2 port)."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, dynamics, compressor"};
  MIN_RELATED{"omx.comp~, peakamp~, gain~"};

  c74::min::outlet<> out_gr{this, "gain reduction (dB)"};

 private:
  punchcomp_dsp::Processor proc_;
  punchcomp_dsp::Params params_;

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

  DECLARE_ATTR_INT(ratio, "Ratio",
                   "Compression ratio button bank. 0 = All Buttons In, "
                   "1 = 20:1, 2 = 12:1, 3 = 8:1, 4 = 4:1.",
                   4, 0, 4, [this](int v) {
                     params_.ratio_mode =
                         static_cast<punchcomp_dsp::RatioMode>(v);
                   });

  DECLARE_ATTR_DOUBLE(input, "Input Gain", "Input gain in dB.", 0.0, -20.0,
                      30.0, [this](double v) { params_.input_db = v; });

  DECLARE_ATTR_DOUBLE(output, "Output Gain",
                      "Output makeup gain in dB, applied after compression.",
                      0.0, -20.0, 30.0,
                      [this](double v) { params_.output_db = v; });

  DECLARE_ATTR_DOUBLE(attack, "Attack", "Attack time in microseconds.", 100.0,
                      20.0, 800.0, [this](double v) { params_.attack_us = v; });

  DECLARE_ATTR_DOUBLE(release, "Release",
                      "Release time in ms. 10x faster than this value when "
                      "Ratio is set to All Buttons In.",
                      250.0, 10.0, 1100.0,
                      [this](double v) { params_.release_ms = v; });

  DECLARE_ATTR_DOUBLE(saturation, "Saturation amount",
                      "Saturation amount applied using Coils.", 0.0, 0.0, 1.0,
                      [this](double v) { params_.saturation = v; });

  DECLARE_ATTR_BOOL(soft_clip, "Soft clip", "Toggles final soft clip stage.",
                    false, [this](bool v) { params_.soft_clip = v; });
};

MIN_EXTERNAL(punchcomp_tilde);
