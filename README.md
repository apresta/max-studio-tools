# max-studio-tools

This is a suite of Max/MSP externals covering essential studio gear.

It includes the following device emulations:

- Air EQ: Transparent equalizer with broad high-frequency band
- Bloom EQ: Passive equalizer with dual boost/cut bands and tube saturation
- Contour EQ: Wide-shelf equalizer with two model variants for broad tonal shaping
- Focus EQ: Solid state equalizer with precise control and console saturation
- Silk EQ: Smooth, musical equalizer with transformer saturation
- Glide Compressor: Optical compressor with flexible controls for transparent shaping
- Level Compressor: Optical compressor with smooth leveling and program-dependent timing
- Punch Compressor: FET compressor with ultra-fast attack and aggressive character
- Smooth Compressor: Vari-mu compressor with vintage character and program-dependent compression
- Snap Compressor: VCA compressor with precise dynamic control and clean transient shaping

Each device supports optional oversampling up to 8x.

These externals form the core of multiple Max for Live devices by [Nyquist Limited](https://www.nyquistlimited.com).

Sound demos: [Studio EQs](https://www.youtube.com/watch?v=CGIqLPSuWRE) / [Studio Compressors](https://youtu.be/ZiccUu8VOns).

The build script has been tested on MacOS. Windows cross-compilation is
supported via mingw-w64.

Dependencies:

- <https://cmake.org>
- <https://github.com/Cycling74/min-devkit>
- <https://github.com/avaneev/r8brain-free-src>
- <https://clang.llvm.org/> (for MacOS build)
- <https://github.com/mstorsjo/llvm-mingw> (for Windows build)

This repository contains modified (or ported) code from the following projects:

- <https://github.com/airwindows/airwindows>
- <https://github.com/lkjbdsp/lkjb-plugins>
- <https://github.com/ABSounds/EQP-WDF-1A>
- <https://github.com/Chowdhury-DSP/chowdsp_wdf>
- <https://github.com/D4p0up/eq1979>
- <https://github.com/aim-qmul/4a2a>
- <https://github.com/JClones/JSFXClones>
