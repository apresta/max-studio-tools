#include "dsp/focuseq_dsp.h"
#include "stereo_os_base.h"

class focuseq_tilde : public base::StereoOsBase<focuseq_tilde> {
 public:
  MIN_DESCRIPTION{
      "Focus EQ: Analog emulation EQ based on solid state console."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

 private:
  focuseq_dsp::Processor proc_;
  focuseq_dsp::Params params_;

 public:
  void PrepareEngine(double os_sr) { proc_.Prepare(os_sr); }

  void PreProcess() { proc_.SetParams(params_); }

  void ProcessBlock(double* l, double* r, const double* /*sc_l*/,
                    const double* /*sc_r*/, int num_frames) {
    proc_.ProcessBlock(l, r, num_frames);
  }

  DECLARE_ATTR_DOUBLE(
      hpf, "HPF Frequency (Hz)",
      "Butterworth 12 dB/oct high-pass filter corner frequency.", 16.0, 16.0,
      350.0, [this](double v) { params_.hpf_freq = v; });

  DECLARE_ATTR_BOOL(hpfon, "HPF Enable", "Toggles the high-pass filter.", false,
                    [this](bool v) { params_.hpf_enabled = v; });

  DECLARE_ATTR_DOUBLE(lpf, "LPF Frequency (Hz)",
                      "Butterworth 12 dB/oct low-pass filter corner frequency",
                      20000.0, 1000.0, 20000.0,
                      [this](double v) { params_.lpf_freq = v; });

  DECLARE_ATTR_BOOL(lpfon, "LPF Enable", "Toggles the low-pass filter.", false,
                    [this](bool v) { params_.lpf_enabled = v; });

  DECLARE_ATTR_DOUBLE(lowfreq, "Low Band Frequency (Hz)",
                      "Shelf knee or bell center frequency.", 120.0, 30.0,
                      450.0, [this](double v) { params_.low_freq = v; });

  DECLARE_ATTR_DOUBLE(lowgain, "Low Band Gain (dB)", "Boost or cut in dB.", 0.0,
                      -16.0, 16.0, [this](double v) { params_.low_gain = v; });

  DECLARE_ATTR_INT(lowtype, "Low Band Shape",
                   "0 = low shelf, 1 = bell (peaking EQ).", 0, 0, 1,
                   [this](int v) {
                     params_.low_shape = (v != 0)
                                             ? focuseq_dsp::BandShape::kBell
                                             : focuseq_dsp::BandShape::kShelf;
                   });

  DECLARE_ATTR_DOUBLE(lmfreq, "Low-Mid Frequency (Hz)",
                      "Bell center frequency.", 500.0, 200.0, 2500.0,
                      [this](double v) { params_.lomid_freq = v; });

  DECLARE_ATTR_DOUBLE(lmgain, "Low-Mid Gain (dB)", "Boost or cut in dB.", 0.0,
                      -16.0, 16.0,
                      [this](double v) { params_.lomid_gain = v; });

  DECLARE_ATTR_DOUBLE(lmq, "Low-Mid Q",
                      "Quality factor. Proportional-Q is applied on top.", 0.5,
                      0.0, 1.0, [this](double v) { params_.lomid_q = v; });

  DECLARE_ATTR_DOUBLE(hmfreq, "High-Mid Frequency (Hz)",
                      "Bell center frequency.", 3000.0, 600.0, 7000.0,
                      [this](double v) { params_.himid_freq = v; });

  DECLARE_ATTR_DOUBLE(hmgain, "High-Mid Gain (dB)", "Boost or cut in dB.", 0.0,
                      -16.0, 16.0,
                      [this](double v) { params_.himid_gain = v; });

  DECLARE_ATTR_DOUBLE(hmq, "High-Mid Q",
                      "Quality factor. Proportional-Q is applied on top.", 0.5,
                      0.0, 1.0, [this](double v) { params_.himid_q = v; });

  DECLARE_ATTR_DOUBLE(highfreq, "High Band Frequency (Hz)",
                      "Shelf knee or bell center frequency.", 5000.0, 1500.0,
                      16000.0, [this](double v) { params_.high_freq = v; });

  DECLARE_ATTR_DOUBLE(highgain, "High Band Gain (dB)", "Boost or cut in dB.",
                      0.0, -16.0, 16.0,
                      [this](double v) { params_.high_gain = v; });

  DECLARE_ATTR_INT(hightype, "High Band Shape",
                   "0 = high shelf, 1 = bell (peaking EQ).", 0, 0, 1,
                   [this](int v) {
                     params_.high_shape = (v != 0)
                                              ? focuseq_dsp::BandShape::kBell
                                              : focuseq_dsp::BandShape::kShelf;
                   });

  DECLARE_ATTR_BOOL(eq_enable, "EQ Enable",
                    "EQ enable: 1 = active, 0 = bypassed.", true,
                    [this](bool v) { params_.eq_enable = v; });

  DECLARE_ATTR_BOOL(phase_inv, "Phase Invert",
                    "Phase invert: 1 = inverted, 0 = normal.", false,
                    [this](bool v) { params_.phase_inv = v; });

  DECLARE_ATTR_BOOL(sat_enable, "Saturation Enable",
                    "Toggles the Spiral2 soft-saturation stage.", false,
                    [this](bool v) { params_.sat_enable = v; });

  DECLARE_ATTR_DOUBLE(gain, "Saturation Input Gain",
                      "Input gain for the Spiral2 saturation stage.", 0.5, 0.0,
                      1.0, [this](double v) { params_.sat_gain = v; });
};

MIN_EXTERNAL(focuseq_tilde);
