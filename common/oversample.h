#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

#include "CDSPResampler.h"

namespace base {

static constexpr int kMaxOversampleIndex = 3;

// Sentinel returned by OversampleBase::PrepareCommon() when oversampling
// resolves to a 1x ratio (disabled). Distinct from
// StereoOsBase::kNotYetInitialized in stereo_os_base.h, which is a
// different -1 sentinel meaning "dspsetup hasn't run yet".
inline constexpr int kOversamplingDisabled = -1;

inline int IndexToRatio(int idx) noexcept {
  if (idx <= 0) return 1;
  idx = std::min(idx, kMaxOversampleIndex);
  return 1 << idx;
}

// Create a CDSPResampler24 with the project-standard transition bandwidth.
inline std::unique_ptr<r8b::CDSPResampler24> MakeResampler(double from_sr,
                                                           double to_sr,
                                                           int max_in_frames) {
  constexpr double kTransition = 2.0;
  return std::make_unique<r8b::CDSPResampler24>(from_sr, to_sr, max_in_frames,
                                                kTransition);
}

// A small FIFO used ONLY at the boundary where a fixed number of samples
// per call is a hard external requirement -- i.e. handing samples back to
// a real-time host audio callback, which always wants exactly num_frames
// samples, no more, no less.
//
// r8brain's own process() does not promise a fixed output length per call,
// so real output is queued here as it's produced and exactly num_frames
// are popped off the front each call, zero-filling any shortfall.
//
// IMPORTANT: this must NOT be used between resampler stages (up -> down).
// Zero-filling at an intermediate boundary would inject fictitious zero
// samples into the next stage's input, burning down its own latency
// countdown on samples that were never real. Between stages, pass each
// resampler's actual variable-length output straight into the next as-is.
//
// Backed by a flat std::vector with a logical size_ rather than
// std::deque, to avoid std::deque's per-push node allocation in this
// audio-adjacent path. Reallocation only happens when the buffer grows
// past its current capacity, which is bounded by the pipeline-priming
// period.
class SampleFifo {
 public:
  void Push(const double* src, int n) {
    if (n <= 0) return;
    EnsureCapacity(size_ + n);
    std::copy(src, src + n, buf_.begin() + size_);
    size_ += n;
  }

  // Pops exactly n samples into dst. If fewer than n samples are
  // currently buffered, the shortfall is zero-filled. This only happens
  // during the initial pipeline-priming period -- which IS the reported
  // round-trip latency -- and does not recur once the pipeline has
  // primed, since real output accumulates at least as fast as it's
  // drained in steady state.
  void PopExact(double* dst, int n) {
    const int take = std::min(size_, n);

    std::copy(buf_.begin(), buf_.begin() + take, dst);

    if (take < n) { std::fill(dst + take, dst + n, 0.0); }

    if (take > 0) {
      std::copy(buf_.begin() + take, buf_.begin() + size_, buf_.begin());
      size_ -= take;
    }
  }

  void Clear() noexcept { size_ = 0; }

 private:
  void EnsureCapacity(int needed) {
    if (needed <= static_cast<int>(buf_.size())) return;
    buf_.resize(std::max(needed, static_cast<int>(buf_.size()) * 2));
  }

  std::vector<double> buf_;
  int size_ = 0;
};

// Shared bookkeeping for the host<->oversampled-rate upsample stage common
// to both OversampleStereo (which also downsamples back to host rate) and
// OversampleSidechain (upsample-only).
class OversampleBase {
 public:
  OversampleBase() = default;
  ~OversampleBase() = default;

  OversampleBase(const OversampleBase&) = delete;
  OversampleBase& operator=(const OversampleBase&) = delete;

  int Ratio() const noexcept { return ratio_; }

  bool Enabled() const noexcept { return enabled_; }

  double OsSr() const noexcept { return os_sr_; }

  int Latency() const noexcept { return latency_; }

 protected:
  // Resolves ratio_/host_sr_/os_sr_/enabled_ and builds the upsampler
  // pair. If oversampling ends up disabled, shared state is left
  // untouched and kOversamplingDisabled is returned -- the caller should
  // run its own Reset() and return early.
  //
  // Otherwise returns the WORST-CASE number of samples a single
  // up_l_/up_r_ process() call can produce, i.e. up_l_->getMaxOutLen().
  // This is r8brain's own derived bound (computed from the aMaxInLen
  // given to the constructor, propagated through every internal stage)
  // -- it is NOT guaranteed to equal max_frames * ratio_ on every call,
  // particularly right as the upsampler's own startup latency is being
  // absorbed. Any downstream stage that consumes this upsampler's output
  // (e.g. a downsampler's own aMaxInLen) must be sized off this value,
  // not off max_frames * ratio_, or it's being fed input the resampler
  // library never promised to keep within bounds.
  int PrepareCommon(double host_sr, int max_frames, int os_index) {
    ratio_ = IndexToRatio(os_index);
    host_sr_ = host_sr;
    os_sr_ = host_sr_ * ratio_;
    enabled_ = ratio_ > 1;

    if (!enabled_) return kOversamplingDisabled;

    up_l_ = MakeResampler(host_sr_, os_sr_, max_frames);
    up_r_ = MakeResampler(host_sr_, os_sr_, max_frames);

    return up_l_->getMaxOutLen(max_frames);
  }

  // Resets the upsampler pair and shared latency. Derived classes with
  // their own extra state (e.g. downsamplers, output FIFOs) should reset
  // that first and then call this for the shared part.
  void ResetCommon() noexcept {
    up_l_.reset();
    up_r_.reset();
    latency_ = 0;
  }

  int ratio_ = 1;
  bool enabled_ = false;

  double host_sr_ = 44100.0;
  double os_sr_ = 44100.0;

  int latency_ = 0;

  std::unique_ptr<r8b::CDSPResampler24> up_l_;
  std::unique_ptr<r8b::CDSPResampler24> up_r_;
};

class OversampleStereo : public OversampleBase {
 public:
  OversampleStereo() = default;
  ~OversampleStereo() = default;

  int Prepare(double host_sr, int max_frames, int os_index) {
    const int os_max = PrepareCommon(host_sr, max_frames, os_index);
    if (os_max == kOversamplingDisabled) {
      Reset();
      return 0;
    }

    // The downsampler must accept up to os_max input samples per call --
    // the true worst case coming out of the upsampler (see
    // PrepareCommon), not max_frames * ratio_. Undersizing this would
    // mean process() is sometimes called with more samples than the
    // aMaxInLen the downsampler was constructed with, which is the
    // exact contract r8brain relies on for its own internal buffer
    // sizing.
    dn_l_ = MakeResampler(os_sr_, host_sr_, os_max);
    dn_r_ = MakeResampler(os_sr_, host_sr_, os_max);

    out_fifo_l_.Clear();
    out_fifo_r_.Clear();

    latency_ = up_l_->getInLenBeforeOutStart() +
               (dn_l_->getInLenBeforeOutStart() / static_cast<double>(ratio_));

    // Sanity-check: L and R resamplers must have identical latency since we
    // only report a single latency value to the host.
    assert(up_l_->getInLenBeforeOutStart() == up_r_->getInLenBeforeOutStart());
    assert(dn_l_->getInLenBeforeOutStart() == dn_r_->getInLenBeforeOutStart());

    return latency_;
  }

  template <typename Fn>
  void Process(double* l, double* r, int num_frames, Fn&& fn) {
    if (!enabled_) {
      fn(l, r, num_frames);
      return;
    }

    assert(up_l_ && up_r_ && dn_l_ && dn_r_);

    // Upsample. `n` is whatever r8brain actually produced this call --
    // NOT forced to num_frames * ratio_. It can be 0 (while the
    // upsampler's own startup latency is still being absorbed) or vary
    // slightly call to call. up_l_/up_r_ are driven by identical input
    // lengths with identical parameters, so their output lengths always
    // agree (checked below).
    double* up_ptr_l = nullptr;
    double* up_ptr_r = nullptr;

    const int n = up_l_->process(l, num_frames, up_ptr_l);
    [[maybe_unused]] const int n_r = up_r_->process(r, num_frames, up_ptr_r);
    assert(n == n_r);

    // DSP at oversampled rate, on exactly the n real samples produced.
    // No zero padding, no truncation -- this operates directly on the
    // upsamplers' own internal buffers (valid until each one's next
    // process() call, which doesn't happen until the next Process()
    // call on *this* object).
    fn(up_ptr_l, up_ptr_r, n);

    // Downsample exactly those n samples -- whatever fn() left there.
    double* dn_ptr_l = nullptr;
    double* dn_ptr_r = nullptr;
    int dn_out_l = 0;
    int dn_out_r = 0;

    if (n > 0) {
      dn_out_l = dn_l_->process(up_ptr_l, n, dn_ptr_l);
      dn_out_r = dn_r_->process(up_ptr_r, n, dn_ptr_r);
    }

    // Queue the downsampler's real output (length varies block to block:
    // 0 during startup, occasionally a larger catch-up burst) and hand
    // the host back exactly num_frames samples, zero-filling only the
    // still-unfilled startup period. This is the ONLY place zero-padding
    // happens, and it corresponds to the genuine round-trip latency --
    // not to a mid-pipeline buffering artifact.
    out_fifo_l_.Push(dn_ptr_l, dn_out_l);
    out_fifo_r_.Push(dn_ptr_r, dn_out_r);

    out_fifo_l_.PopExact(l, num_frames);
    out_fifo_r_.PopExact(r, num_frames);
  }

 private:
  void Reset() noexcept {
    dn_l_.reset();
    dn_r_.reset();
    out_fifo_l_.Clear();
    out_fifo_r_.Clear();
    ResetCommon();
  }

  std::unique_ptr<r8b::CDSPResampler24> dn_l_;
  std::unique_ptr<r8b::CDSPResampler24> dn_r_;

  SampleFifo out_fifo_l_;
  SampleFifo out_fifo_r_;
};

// Stereo upsampler for an external sidechain that the DSP engine consumes
// for level detection but never writes back to a host-rate output.
// Only the upsample path is present; there is no downsample step, and
// thus no fixed-size host-rate output requirement -- so no FIFO is
// needed here at all.
//
// Using CDSPResampler24 with the same parameters as OversampleStereo's
// upsamplers, fed the same per-call input length, guarantees the same
// per-call output length n as OversampleStereo::Process() produces (both
// are deterministic given identical construction parameters and
// identical input), so the upsampled sidechain block stays sample-aligned
// with the upsampled main-signal block with no extra synchronization
// logic required.
class OversampleSidechain : public OversampleBase {
 public:
  OversampleSidechain() = default;
  ~OversampleSidechain() = default;

  // Configure the upsampler. Returns the one-way group delay in host-rate
  // samples (provided for symmetry with OversampleStereo::Prepare; the
  // caller typically discards it since the stereo oversampler's round-trip
  // latency is what gets reported to the host for PDC).
  int Prepare(double host_sr, int max_frames, int os_index) {
    const int os_max = PrepareCommon(host_sr, max_frames, os_index);
    if (os_max == kOversamplingDisabled) {
      Reset();
      return 0;
    }

    latency_ = up_l_->getInLenBeforeOutStart();

    // Sanity-check: L and R resamplers must have identical latency since we
    // only report a single latency value to the host.
    assert(up_l_->getInLenBeforeOutStart() == up_r_->getInLenBeforeOutStart());

    return latency_;
  }

  // Upsample l/r (num_frames samples at host rate each) and invoke
  // fn(upsampled_l_ptr, upsampled_r_ptr, n), where n is r8brain's actual
  // per-call output length (see class comment for why this stays aligned
  // with OversampleStereo::Process() automatically). The pointers passed
  // to fn point into internal buffers and remain valid until the next
  // call to Process(). l and r themselves are not modified.
  template <typename Fn>
  void Process(double* l, double* r, int num_frames, Fn&& fn) {
    if (!enabled_) {
      fn(l, r, num_frames);
      return;
    }

    assert(up_l_ && up_r_);

    double* up_ptr_l = nullptr;
    double* up_ptr_r = nullptr;

    const int n = up_l_->process(l, num_frames, up_ptr_l);
    [[maybe_unused]] const int n_r = up_r_->process(r, num_frames, up_ptr_r);
    assert(n == n_r);

    fn(up_ptr_l, up_ptr_r, n);
  }

 private:
  void Reset() noexcept { ResetCommon(); }
};

}  // namespace base
