#pragma once

#include <cstring>
#include <mutex>

#include "c74_min.h"
#include "dsp_math.h"
#include "oversample.h"

// Base class for Max externals.
template <typename Derived>
class StereoOsBase : public c74::min::vector_operator<> {
 protected:
  oversample::OversampleStereo os_;
  std::mutex process_mutex_;
  double sr_{0.0};  // 0 until first dspsetup
  int vs_{0};       // 0 until first dspsetup

  // Prepare the oversampler, then the DSP engine.
  // Returns host-rate latency in samples.
  // Must be called with process_mutex_ held (or before audio is running).
  int PrepareAll(double sr, int vs, int os_idx) {
    const int latency = os_.Prepare(sr, vs, os_idx);
    static_cast<Derived*>(this)->PrepareEngine(os_.os_sr());
    return latency;
  }

  // Live oversample-attribute setter helper.
  // Skips reconfiguration until dspsetup has run at least once.
  // Returns the new host-rate latency, or -1 if not yet initialized.
  int ReconfigureOversample(int v) {
    if (sr_ <= 0.0 || vs_ <= 0) return -1;
    std::lock_guard<std::mutex> lock(process_mutex_);
    return PrepareAll(sr_, vs_, v);
  }

  // Call once from dspsetup (on the audio thread) to configure the
  // oversampler and enable hardware flush-to-zero for this thread.
  int DspSetup(double sr, int vs, int os_idx) {
    sr_ = sr;
    vs_ = vs;
    dsp::SetFlushToZero();
    return PrepareAll(sr, vs, os_idx);
  }

 public:
  // Locked operator() owned by the base class.
  // Calls the derived PreProcess() hook before the lock (safe for param
  // copies), then ProcessBlock() inside the lock at the oversampled rate.
  void operator()(c74::min::audio_bundle input,
                  c74::min::audio_bundle output) override {
    static_cast<Derived*>(this)->PreProcess();
    {
      std::lock_guard<std::mutex> lock(process_mutex_);
      const int frames = static_cast<int>(input.frame_count());
      const c74::min::sample* in_l = input.samples(0);
      const c74::min::sample* in_r = input.samples(1);
      c74::min::sample* out_l = output.samples(0);
      c74::min::sample* out_r = output.samples(1);
      const std::size_t bytes =
          sizeof(c74::min::sample) * static_cast<std::size_t>(frames);

      // memmove is safe for overlapping ranges (memcpy is not).
      if (in_l != out_l) std::memmove(out_l, in_l, bytes);
      if (in_r != out_r) std::memmove(out_r, in_r, bytes);

      os_.Process(out_l, out_r, frames, [this](double* l, double* r, int n) {
        static_cast<Derived*>(this)->ProcessBlock(l, r, n);
      });
    }
  }
};
