// This file is derived from the original SingleEndedTriode by Airwindows.
// Copyright (c) Airwindows (MIT license).

#pragma once

namespace saturation {

class SingleEndedTriode {
 public:
  // Triode saturation amount. 0 = bypassed, 1 = maximum even-harmonic drive.
  void SetTriodeDrive(double value) noexcept;

  double GetTriodeDrive() const noexcept { return triode_drive_; }

  // Class AB soft crossover depth. 0 = bypassed, 1 = heavy crossover notch.
  void SetClassAB(double value) noexcept;

  double GetClassAB() const noexcept { return class_ab_; }

  // Class B hard crossover depth. 0 = bypassed, 1 = severe dead band.
  void SetClassB(double value) noexcept;

  double GetClassB() const noexcept { return class_b_; }

  // Process one stereo block in-place.
  void ProcessBlock(double* left, double* right,
                    int num_samples) const noexcept;

 private:
  double triode_drive_{0.0};
  double class_ab_{0.0};
  double class_b_{0.0};
};

}  // namespace saturation
