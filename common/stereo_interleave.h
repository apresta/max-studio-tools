#pragma once

// Stereo planar <-> interleaved conversion helpers.
//
// Interleaved layout: { L[0], R[0], L[1], R[1], ... }
// Planar layout:      L[0..n-1]  and  R[0..n-1]  in separate buffers.
//
// Both functions are in-place-safe as long as the planar pointers alias
// non-overlapping halves of the interleaved buffer.

#include <cstddef>

#include "vec.h"

namespace dsp {

// Interleave planar L / R into a pre-allocated interleaved buffer of
// length 2 * num_frames.
inline void Interleave(const double* L, const double* R, double* interleaved,
                       int num_frames) noexcept {
  for (int i = 0; i < num_frames; ++i) {
#if defined(DSP_USE_SIMDE)
    _mm_storeu_pd(&interleaved[2 * i], _mm_set_pd(R[i], L[i]));
#else
    interleaved[2 * i] = L[i];
    interleaved[2 * i + 1] = R[i];
#endif
  }
}

// De-interleave an interleaved buffer of length 2 * num_frames into
// separate planar L / R buffers.
inline void Deinterleave(const double* interleaved, double* L, double* R,
                         int num_frames) noexcept {
  for (int i = 0; i < num_frames; ++i) {
#if defined(DSP_USE_SIMDE)
    const __m128d v = _mm_loadu_pd(&interleaved[2 * i]);
    _mm_storel_pd(&L[i], v);
    _mm_storeh_pd(&R[i], v);
#else
    L[i] = interleaved[2 * i];
    R[i] = interleaved[2 * i + 1];
#endif
  }
}

// Interleave, run fn(double* interleaved, int frames), deinterleave.
// Caller must supply an interleaved scratch buffer of at least 2*num_frames.
template <typename Fn>
inline void ProcessInterleaved(double* L, double* R, int num_frames,
                               double* scratch, Fn&& fn) noexcept {
  Interleave(L, R, scratch, num_frames);
  fn(scratch, num_frames);
  Deinterleave(scratch, L, R, num_frames);
}

}  // namespace dsp
