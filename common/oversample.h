#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "CDSPResampler.h"

namespace oversample {

static constexpr int kMaxOversampleIndex = 3;

inline int IndexToRatio(int idx) noexcept {
  if (idx <= 0) return 1;
  if (idx > kMaxOversampleIndex) idx = kMaxOversampleIndex;
  return 1 << idx;
}

class OversampleStereo {
 public:
  OversampleStereo() = default;
  ~OversampleStereo() = default;

  OversampleStereo(const OversampleStereo&) = delete;
  OversampleStereo& operator=(const OversampleStereo&) = delete;

  int Prepare(double host_sr, int max_frames, int os_index) {
    ratio_ = IndexToRatio(os_index);

    host_sr_ = host_sr;
    os_sr_ = host_sr_ * ratio_;

    enabled_ = ratio_ > 1;

    if (!enabled_) {
      Reset();
      return 0;
    }

    const int os_frames = max_frames * ratio_;

    buf_l_.resize(static_cast<size_t>(os_frames));
    buf_r_.resize(static_cast<size_t>(os_frames));

    constexpr double kTransition = 2.0;

    up_l_ = std::make_unique<r8b::CDSPResampler24>(host_sr_, os_sr_, max_frames,
                                                   kTransition);

    up_r_ = std::make_unique<r8b::CDSPResampler24>(host_sr_, os_sr_, max_frames,
                                                   kTransition);

    dn_l_ = std::make_unique<r8b::CDSPResampler24>(os_sr_, host_sr_, os_frames,
                                                   kTransition);

    dn_r_ = std::make_unique<r8b::CDSPResampler24>(os_sr_, host_sr_, os_frames,
                                                   kTransition);

    latency_ = static_cast<int>(std::ceil(
        up_l_->getInLenBeforeOutStart() +
        (dn_l_->getInLenBeforeOutStart() / static_cast<double>(ratio_))));

    // Sanity-check: L and R resamplers must have identical latency since we
    // only report a single latency value to the host.
    assert(up_l_->getInLenBeforeOutStart() == up_r_->getInLenBeforeOutStart());
    assert(dn_l_->getInLenBeforeOutStart() == dn_r_->getInLenBeforeOutStart());

    return latency_;
  }

  template <typename Fn>
  void Process(double* l, double* r, int frames, Fn&& fn) {
    if (!enabled_) {
      fn(l, r, frames);
      return;
    }

    assert(up_l_ && up_r_ && dn_l_ && dn_r_);

    const int os_frames = frames * ratio_;

    // Upsample.
    double* up_ptr_l = nullptr;
    double* up_ptr_r = nullptr;

    const int up_out_l = up_l_->process(l, frames, up_ptr_l);

    const int up_out_r = up_r_->process(r, frames, up_ptr_r);

    if (up_out_l > 0) {
      std::copy(up_ptr_l, up_ptr_l + std::min(up_out_l, os_frames),
                buf_l_.begin());
    }

    if (up_out_r > 0) {
      std::copy(up_ptr_r, up_ptr_r + std::min(up_out_r, os_frames),
                buf_r_.begin());
    }

    if (up_out_l < os_frames) {
      std::fill(buf_l_.begin() + up_out_l, buf_l_.end(), 0.0);
    }

    if (up_out_r < os_frames) {
      std::fill(buf_r_.begin() + up_out_r, buf_r_.end(), 0.0);
    }

    // DSP at oversampled rate.
    fn(buf_l_.data(), buf_r_.data(), os_frames);

    // Downsample.
    double* dn_ptr_l = nullptr;
    double* dn_ptr_r = nullptr;

    const int dn_out_l = dn_l_->process(buf_l_.data(), os_frames, dn_ptr_l);

    const int dn_out_r = dn_r_->process(buf_r_.data(), os_frames, dn_ptr_r);

    const int n_l = std::min(frames, dn_out_l);
    const int n_r = std::min(frames, dn_out_r);

    if (n_l > 0) { std::copy(dn_ptr_l, dn_ptr_l + n_l, l); }

    if (n_r > 0) { std::copy(dn_ptr_r, dn_ptr_r + n_r, r); }

    if (n_l < frames) { std::fill(l + n_l, l + frames, 0.0); }

    if (n_r < frames) { std::fill(r + n_r, r + frames, 0.0); }
  }

  int ratio() const noexcept { return ratio_; }

  bool enabled() const noexcept { return enabled_; }

  double os_sr() const noexcept { return os_sr_; }

  int latency() const noexcept { return latency_; }

 private:
  void Reset() {
    up_l_.reset();
    up_r_.reset();

    dn_l_.reset();
    dn_r_.reset();

    buf_l_.clear();
    buf_r_.clear();

    latency_ = 0;
  }

 private:
  int ratio_ = 1;
  bool enabled_ = false;

  double host_sr_ = 44100.0;
  double os_sr_ = 44100.0;

  int latency_ = 0;

  std::unique_ptr<r8b::CDSPResampler24> up_l_;
  std::unique_ptr<r8b::CDSPResampler24> up_r_;

  std::unique_ptr<r8b::CDSPResampler24> dn_l_;
  std::unique_ptr<r8b::CDSPResampler24> dn_r_;

  std::vector<double> buf_l_;
  std::vector<double> buf_r_;
};

}  // namespace oversample
