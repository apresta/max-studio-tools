#pragma once

#include <cstring>
#include <mutex>
#include <type_traits>
#include <vector>

#include "attr_helpers.h"
#include "c74_min.h"
#include "oversample.h"

namespace base {

// Base class for Max externals that process stereo audio with optional
// oversampling and an optional external stereo sidechain.
//
// Every StereoOsBase-derived external gets four signal inlets (main L/R and
// a stereo sidechain pair) and the "sidechain" attribute, whether or not
// the engine actually uses the sidechain. If you don't need it, just
// ignore sc_l/sc_r in ProcessBlock and leave inlets 3/4 unpatched in the
// Max patch -- Max feeds unpatched signal inlets silence, so there's no
// behavioral difference from not having the inlets at all.
//
// The sidechain is carried as a stereo pair (rather than summed/mono) so
// that any pre-detector processing in the engine -- gain, filtering, etc.
// -- can be applied to it through exactly the same per-channel code path
// as the main signal, with no special-casing for a single shared lane.
//
// Derived must implement:
//   void PrepareEngine(double os_sr);
//   void PreProcess();
//   void ProcessBlock(double* l, double* r, const double* sc_l,
//                     const double* sc_r, int num_frames);
// sc_l/sc_r are both null whenever the sidechain attribute is off (default)
// or the sidechain inlets are unpatched/silent; otherwise both are non-null
// together. Engines that don't care about the sidechain can just ignore
// the last two parameters.
template <typename Derived>
class StereoOsBase : public c74::min::object<Derived>,
                     public c74::min::vector_operator<> {
 public:
  c74::min::inlet<> in_left{this, "(signal) left input", "signal"};
  c74::min::inlet<> in_right{this, "(signal) right input", "signal"};
  c74::min::inlet<> in_sidechain_left{
      this, "(signal) sidechain left input (optional)", "signal"};
  c74::min::inlet<> in_sidechain_right{
      this, "(signal) sidechain right input (optional)", "signal"};

  c74::min::outlet<> out_left{this, "(signal) left output", "signal"};
  c74::min::outlet<> out_right{this, "(signal) right output", "signal"};
  c74::min::outlet<> out_status{this, "latency <n>"};

  DECLARE_ATTR_INT(oversample, "Oversampling",
                   "Oversampling ratio: 0=off, 1=2x, 2=4x, 3=8x.", 0, 0,
                   kMaxOversampleIndex, [this](int v) {
                     const int latency = ReconfigureOversample(v);
                     if (latency >= 0) out_status.send("latency", latency);
                   });

  DECLARE_ATTR_INT(
      sidechain, "Sidechain",
      "Use the sidechain inlets (3-4) as the detector input instead of the "
      "main signal in inlets 1-2. Has no effect on engines that don't "
      "implement sidechain detection.",
      0, 0, 1, [this](int v) { sidechain_enabled_ = (v != 0); });

  // clang-format off
  c74::min::message<> dspsetup{
    this, "dspsetup",
    MIN_FUNCTION{
      const int latency = DspSetup(static_cast<double>(args[0]),
                                   static_cast<int>(args[1]),
                                   static_cast<int>(oversample));
      out_status.send("latency", latency);
      return {latency};
    }
  };  // clang-format on

  // Calls the derived PreProcess() hook before the lock (safe for param
  // copies), then ProcessBlock() inside the lock at the oversampled
  // rate, upsampling the sidechain pair first when the sidechain
  // attribute is on.
  void operator()(c74::min::audio_bundle input,
                  c74::min::audio_bundle output) override {
    static_assert(std::is_trivially_copyable_v<c74::min::sample>,
                  "memmove/memcpy below assume a trivially copyable sample "
                  "type");

    static_cast<Derived*>(this)->PreProcess();
    std::lock_guard<std::mutex> lock(process_mutex_);

    const int num_frames = static_cast<int>(input.frame_count());
    const c74::min::sample* in_l = input.samples(0);
    const c74::min::sample* in_r = input.samples(1);
    c74::min::sample* out_l = output.samples(0);
    c74::min::sample* out_r = output.samples(1);
    const std::size_t bytes =
        sizeof(c74::min::sample) * static_cast<std::size_t>(num_frames);

    // memmove is safe for overlapping ranges (memcpy is not).
    if (in_l != out_l) std::memmove(out_l, in_l, bytes);
    if (in_r != out_r) std::memmove(out_r, in_r, bytes);

    if (!sidechain_enabled_) {
      os_.Process(out_l, out_r, num_frames,
                 [this](double* l, double* r, int n) {
                   static_cast<Derived*>(this)->ProcessBlock(l, r, nullptr,
                                                             nullptr, n);
                 });
      return;
    }

    // Sidechain inlets have no outlets to alias, so copy them into scratch
    // space before upsampling.
    std::memcpy(sc_scratch_l_.data(), input.samples(2), bytes);
    std::memcpy(sc_scratch_r_.data(), input.samples(3), bytes);

    // Upsample the stereo sidechain pair together. os_sidechain_ uses the
    // same CDSPResampler24 parameters as os_, so the upsampled sidechain
    // block has the same group delay and stays sample-aligned with the
    // upsampled main block. The pointers handed to the lambda point into
    // os_sidechain_'s internal buffers, valid until the next call to
    // os_sidechain_.Process(), i.e. for the rest of this block.
    double* sc_up_l = nullptr;
    double* sc_up_r = nullptr;
    os_sidechain_.Process(sc_scratch_l_.data(), sc_scratch_r_.data(),
                          num_frames, [&](double* sc_l, double* sc_r, int) {
                            sc_up_l = sc_l;
                            sc_up_r = sc_r;
                          });

    os_.Process(out_l, out_r, num_frames,
               [this, sc_up_l, sc_up_r](double* l, double* r, int n) {
                 static_cast<Derived*>(this)->ProcessBlock(l, r, sc_up_l,
                                                           sc_up_r, n);
               });
  }

 protected:
  // Sentinel returned by ReconfigureOversample() before the first
  // dspsetup call. Distinct from base::kOversamplingDisabled in
  // oversample.h, which is a different -1 sentinel meaning "ratio
  // resolved to 1x".
  static constexpr int kNotYetInitialized = -1;

  // Prepare both oversamplers, then the DSP engine.
  // Returns host-rate latency in samples.
  // Must be called with process_mutex_ held (or before audio is running).
  int PrepareAll(double sr, int vs, int os_idx) {
    const int latency = os_.Prepare(sr, vs, os_idx);
    os_sidechain_.Prepare(sr, vs, os_idx);
    sc_scratch_l_.assign(static_cast<std::size_t>(vs),
                         static_cast<c74::min::sample>(0));
    sc_scratch_r_.assign(static_cast<std::size_t>(vs),
                         static_cast<c74::min::sample>(0));
    static_cast<Derived*>(this)->PrepareEngine(os_.OsSr());
    return latency;
  }

  // Live oversample-attribute setter helper.
  // Skips reconfiguration until dspsetup has run at least once.
  // Returns the new host-rate latency, or kNotYetInitialized.
  int ReconfigureOversample(int v) {
    if (sr_ <= 0.0 || vs_ <= 0) return kNotYetInitialized;
    std::lock_guard<std::mutex> lock(process_mutex_);
    return PrepareAll(sr_, vs_, v);
  }

  // Call once from dspsetup (on the audio thread) to configure the
  // oversampler and enable hardware flush-to-zero for this thread.
  int DspSetup(double sr, int vs, int os_idx) {
    sr_ = sr;
    vs_ = vs;
    return PrepareAll(sr, vs, os_idx);
  }

  OversampleStereo os_;

  // Always configured with the same sr/vs/os_idx as os_. Uses the same
  // CDSPResampler24 parameters so the upsampled sidechain block has
  // identical group delay to the upsampled main block -- the two stay
  // sample-aligned. Upsample-only (no downsample step): the sidechain is
  // used solely for level detection and is never written back to a
  // host-rate output.
  OversampleSidechain os_sidechain_;

  std::mutex process_mutex_;
  double sr_{0.0};  // 0 until first dspsetup
  int vs_{0};       // 0 until first dspsetup
  bool sidechain_enabled_{false};

  // Host-rate scratch space for the stereo sidechain input. The inlets have
  // no corresponding outlets to alias, so they are copied here before being
  // handed to os_sidechain_.
  std::vector<c74::min::sample> sc_scratch_l_;
  std::vector<c74::min::sample> sc_scratch_r_;
};

}  // namespace base
