#pragma once

#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

class ScopedDenormalGuard {
 public:
  ScopedDenormalGuard() noexcept {
#if defined(_M_X64) || defined(__x86_64__) || defined(__i386__)
    original_state_ = _mm_getcsr();

    // Bit 15 = Flush-to-Zero (FTZ), Bit 6 = Denormals-are-Zero (DAZ)
    _mm_setcsr(original_state_ | 0x8040);
#elif defined(__ARM_NEON) || defined(__aarch64__)
    asm volatile("mrs %0, fpcr" : "=r"(original_state_));

    // Bit 24 = Flush-to-Zero (FZ)
    asm volatile("msr fpcr, %0" ::"r"(original_state_ | (1 << 24)));
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
