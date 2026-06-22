#include "dsp/aireq_dsp.h"
#include "stereo_os_base.h"

class aireq_tilde : public base::StereoOsBase<aireq_tilde> {
 public:
  MIN_DESCRIPTION{"Air EQ: Analog emulation EQ with air band."};
  MIN_AUTHOR{"Alessandro Presta"};
  MIN_TAGS{"audio, eq, filter"};
  MIN_RELATED{"equalizer~, filtercoeff~, biquad~"};

 private:
  aireq_dsp::Processor proc_;
  aireq_dsp::Params params_;

 public:
  void PrepareEngine(double os_sr) { proc_.Prepare(os_sr); }

  void PreProcess() { proc_.SetParams(params_); }

  void ProcessBlock(double* l, double* r, const double* /*sc_l*/,
                    const double* /*sc_r*/, int num_frames) {
    proc_.ProcessBlock(l, r, num_frames);
  }

  DECLARE_ATTR_DOUBLE(gain10, "10 Hz Gain (dB)",
                      "Bell gain for the 10 Hz band.", 0.0, -10.0, 10.0,
                      [this](double v) {
                        params_.gains[aireq_dsp::BandType::kBand10] = v;
                      });

  DECLARE_ATTR_DOUBLE(gain40, "40 Hz Gain (dB)",
                      "Bell gain for the 40 Hz band.", 0.0, -10.0, 10.0,
                      [this](double v) {
                        params_.gains[aireq_dsp::BandType::kBand40] = v;
                      });

  DECLARE_ATTR_DOUBLE(gain160, "160 Hz Gain (dB)",
                      "Bell gain for the 160 Hz band.", 0.0, -10.0, 10.0,
                      [this](double v) {
                        params_.gains[aireq_dsp::BandType::kBand160] = v;
                      });

  DECLARE_ATTR_DOUBLE(gain640, "640 Hz Gain (dB)",
                      "Bell gain for the 640 Hz band.", 0.0, -10.0, 10.0,
                      [this](double v) {
                        params_.gains[aireq_dsp::BandType::kBand640] = v;
                      });

  DECLARE_ATTR_DOUBLE(gain2k5, "2.5 kHz Shelf Gain (dB)",
                      "High-shelf gain at 2.5 kHz.", 0.0, -10.0, 10.0,
                      [this](double v) {
                        params_.gains[aireq_dsp::BandType::kShelf2k5] = v;
                      });

  DECLARE_ATTR_DOUBLE(gainhi, "Air Shelf Gain (dB)",
                      "Gain for the variable-frequency air shelf.", 0.0, 0.0,
                      10.0, [this](double v) {
                        params_.gains[aireq_dsp::BandType::kShelfHi] = v;
                      });

  DECLARE_ATTR_INT(
      hitype, "Air Shelf Frequency",
      "Corner frequency of the air shelf. "
      "0 = Off, 1 = 2.5kHz, 2 = 5kHz, 3 = 10kHz, 4 = 20kHz, 5 = 40kHz",
      0, 0, static_cast<int>(aireq_dsp::HighShelf::kNumHighShelves) - 1,
      [this](int v) {
        params_.high_shelf = static_cast<aireq_dsp::HighShelf>(v);
      });

  DECLARE_ATTR_BOOL(keepgain, "Keep Gain",
                    "When 1, output level is compensated to preserve loudness.",
                    false, [this](bool v) { params_.keep_gain = v; });

  DECLARE_ATTR_BOOL(phase_inv, "Phase Invert",
                    "Phase invert: 1 = inverted, 0 = normal.", false,
                    [this](bool v) { params_.phase_inv = v; });
};

MIN_EXTERNAL(aireq_tilde);
