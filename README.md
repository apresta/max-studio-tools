# max-studio-tools

This is a suite of Max/MSP externals covering essential studio gear.

It includes the following device emulations:

- Air EQ: Transparent equalizer with broad high-frequency band
- Bloom EQ: Passive equalizer with dual boost/cut bands and tube saturation
- Contour EQ: Wide-shelf equalizer with two model variants for broad tonal shaping
- Focus EQ: Solid state equalizer with precise control and console saturation
- Silk EQ: Smooth, musical equalizer with transformer saturation

Each device supports vectorized computations on modern CPUs, and optional oversampling.

The build script has been tested on MacOS. Windows cross-compilation is
supported via mingw-w64.

Dependencies:

- <https://cmake.org>
- <https://github.com/Cycling74/min-devkit>
- <https://github.com/simd-everywhere/simde>
- <https://github.com/avaneev/r8brain-free-src>
- <https://clang.llvm.org/> (for MacOS build)
- <https://github.com/mstorsjo/llvm-mingw> (for Windows build)

This repository contains modified (or ported) code from the following projects:

- <https://github.com/airwindows/airwindows> (Baxandall, Baxandall3, Coils, Spiral2, Tube2)
- <https://github.com/lkjbdsp/lkjb-plugins> (Luftikus)
- <https://github.com/ABSounds/EQP-WDF-1A>
- <https://github.com/Chowdhury-DSP/chowdsp_wdf>
- <https://github.com/D4p0up/eq1979>
