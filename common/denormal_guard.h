#pragma once

#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

namespace dsp {

class ScopedDenormalGuard {
 public:
#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
  static constexpr unsigned int kFlushToZeroBit = 0x8000;       // bit 15 (FTZ)
  static constexpr unsigned int kDenormalsAreZeroBit = 0x0040;  // bit 6 (DAZ)
#elif defined(__ARM_NEON) || defined(__aarch64__)
  static constexpr unsigned long long kFlushToZeroBit = 1ULL
                                                          << 24;  // bit 24 (FZ)
#endif

  ScopedDenormalGuard() noexcept {
#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
    original_state_ = _mm_getcsr();
    _mm_setcsr(original_state_ | (kFlushToZeroBit | kDenormalsAreZeroBit));
#elif defined(__ARM_NEON) || defined(__aarch64__)
    asm volatile("mrs %0, fpcr" : "=r"(original_state_));
    asm volatile("msr fpcr, %0" ::"r"(original_state_ | kFlushToZeroBit));
#endif
  }

  ~ScopedDenormalGuard() noexcept {
#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
    _mm_setcsr(original_state_);
#elif defined(__ARM_NEON) || defined(__aarch64__)
    asm volatile("msr fpcr, %0" ::"r"(original_state_));
#endif
  }

  ScopedDenormalGuard(const ScopedDenormalGuard&) = delete;
  ScopedDenormalGuard& operator=(const ScopedDenormalGuard&) = delete;
  ScopedDenormalGuard(ScopedDenormalGuard&&) = delete;
  ScopedDenormalGuard& operator=(ScopedDenormalGuard&&) = delete;

 private:
#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
  unsigned int original_state_{0};
#elif defined(__ARM_NEON) || defined(__aarch64__)
  unsigned long long original_state_{0};
#else
  int original_state_{0};
#endif
};

}  // namespace dsp
