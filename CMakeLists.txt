cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
project(max_studio_tools VERSION 1.0.0)

# Options

set(MAX_VERSION 9 CACHE STRING "Max version")
set(MIN_DEVKIT_PATH "${CMAKE_SOURCE_DIR}/deps/min-devkit" CACHE PATH
  "Min-DevKit root")
set(MIN_API_PATH    "${MIN_DEVKIT_PATH}/source/min-api" CACHE PATH
  "Min API root")
set(MAX_EXTERNALS_DIR
  "$ENV{HOME}/Documents/Max ${MAX_VERSION}/Packages/max-studio-tools/externals"
  CACHE PATH "Output directory for Max externals")
set(LLVM_MINGW_PREFIX "${CMAKE_SOURCE_DIR}/deps/llvm-mingw" CACHE PATH
  "llvm-mingw cross-compiler prefix")
set(R8BRAIN_PATH "${CMAKE_SOURCE_DIR}/deps/r8brain-free-src" CACHE PATH
  "Path to r8brain headers")

option(SUPERBUILD  "Build macOS + Windows in one pass" ON)
option(BUILD_AIREQ      "Build aireq~"      ON)
option(BUILD_SILKEQ     "Build silkeq~"     ON)
option(BUILD_BLOOMEQ    "Build bloomeq~"    ON)
option(BUILD_FOCUSEQ    "Build focuseq~"    ON)
option(BUILD_CONTOUREQ  "Build contoureq~"  ON)
option(BUILD_LEVELCOMP  "Build levelcomp~"  ON)
option(BUILD_PUNCHCOMP  "Build punchcomp~"  ON)
option(BUILD_SMOOTHCOMP "Build smoothcomp~" ON)
option(BUILD_GLIDECOMP  "Build glidecomp~"  ON)
option(BUILD_SNAPCOMP   "Build snapcomp~"   ON)

# Superbuild

if(SUPERBUILD AND NOT CMAKE_CROSSCOMPILING)
  include(ExternalProject)
  set(_fwd
    -DSUPERBUILD=OFF
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS}
        -DMIN_DEVKIT_PATH=${MIN_DEVKIT_PATH}
        -DMAX_EXTERNALS_DIR=${MAX_EXTERNALS_DIR}
        -DBUILD_AIREQ=${BUILD_AIREQ}
        -DBUILD_SILKEQ=${BUILD_SILKEQ}
        -DBUILD_BLOOMEQ=${BUILD_BLOOMEQ}
        -DBUILD_FOCUSEQ=${BUILD_FOCUSEQ}
        -DBUILD_CONTOUREQ=${BUILD_CONTOUREQ}
        -DBUILD_LEVELCOMP=${BUILD_LEVELCOMP}
        -DBUILD_PUNCHCOMP=${BUILD_PUNCHCOMP}
        -DBUILD_SMOOTHCOMP=${BUILD_SMOOTHCOMP}
        -DBUILD_GLIDECOMP=${BUILD_GLIDECOMP}
        -DBUILD_SNAPCOMP=${BUILD_SNAPCOMP}
  )
  ExternalProject_Add(max_studio_tools_mac
    PREFIX ${CMAKE_BINARY_DIR}/mac SOURCE_DIR ${CMAKE_SOURCE_DIR}
    BINARY_DIR ${CMAKE_BINARY_DIR}/mac/build CMAKE_ARGS ${_fwd}
    BUILD_ALWAYS TRUE INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
  )
  ExternalProject_Add(max_studio_tools_win
    PREFIX ${CMAKE_BINARY_DIR}/win SOURCE_DIR ${CMAKE_SOURCE_DIR}
    BINARY_DIR ${CMAKE_BINARY_DIR}/win/build
    CMAKE_ARGS
    ${_fwd}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/toolchain/toolchain-mingw.cmake
            -DLLVM_MINGW_PREFIX=${LLVM_MINGW_PREFIX}
    BUILD_ALWAYS TRUE INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
  )
  return()
endif()

# Sanity check

if(NOT EXISTS "${MIN_API_PATH}/include/c74_min.h")
  message(FATAL_ERROR
    "Min API not found at '${MIN_API_PATH}'.\n"
    "Run build.sh to initialize submodules, or pass -DMIN_DEVKIT_PATH=<path>.")
endif()

if(APPLE AND NOT EXISTS "${R8BRAIN_PATH}/r8bbase.h")
  message(FATAL_ERROR
    "r8brain-free-src not found at '${R8BRAIN_PATH}'.\n"
    "Run: git submodule update --init --recursive")
endif()

# Shared settings helper

set(_MIN_INCLUDES
  "${MIN_API_PATH}/include"
  "${MIN_API_PATH}/max-sdk-base/c74support"
  "${MIN_API_PATH}/max-sdk-base/c74support/max-includes"
  "${MIN_API_PATH}/max-sdk-base/c74support/msp-includes"
)

function(configure_target target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_definitions(${target} PRIVATE NOMINMAX)
  target_compile_options(${target} PRIVATE
    -O3 -ffast-math -fno-finite-math-only -Wall -Wextra -Wno-unused-parameter)
  target_include_directories(${target} SYSTEM PRIVATE ${_MIN_INCLUDES}
    ${R8BRAIN_PATH})

  if(APPLE)
    set_target_properties(${target} PROPERTIES
      BUNDLE TRUE BUNDLE_EXTENSION "mxo"
      MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    target_link_options(${target} PRIVATE -undefined dynamic_lookup)
    target_link_libraries(${target} PRIVATE "-framework CoreFoundation")
  endif()

  if(WIN32)
    set_target_properties(${target} PROPERTIES SUFFIX ".mxe64")
    target_compile_options(${target} PRIVATE
      -include "${CMAKE_SOURCE_DIR}/toolchain/win_prefix.h"
      -stdlib=libc++ -ffunction-sections -fdata-sections -flto)
    target_link_options(${target} PRIVATE
      -stdlib=libc++
      -Wl,-Bstatic,-lc++,-lunwind,-lwinpthread,-Bdynamic
      -Wl,--strip-all -Wl,--gc-sections -flto)
    target_link_libraries(${target} PRIVATE
      "${MIN_API_PATH}/max-sdk-base/c74support/max-includes/x64/MaxAPI.lib"
      "${MIN_API_PATH}/max-sdk-base/c74support/msp-includes/x64/MaxAudio.lib")
  endif()

  set_target_properties(${target} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY         "${MAX_EXTERNALS_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${MAX_EXTERNALS_DIR}"
    RUNTIME_OUTPUT_DIRECTORY         "${MAX_EXTERNALS_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MAX_EXTERNALS_DIR}")
endfunction()

# aireq~

if(BUILD_AIREQ)
  add_library(aireq_tilde MODULE
    aireq/aireq_tilde.cpp
    aireq/dsp/aireq_dsp.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:aireq/aireq_tilde.def>
  )
  set_target_properties(aireq_tilde PROPERTIES PREFIX "" OUTPUT_NAME "aireq~")
  target_include_directories(aireq_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/aireq/dsp"
    "${CMAKE_SOURCE_DIR}/common")
  configure_target(aireq_tilde)
endif()

# silkeq~

if(BUILD_SILKEQ)
  add_library(silkeq_tilde MODULE
    silkeq/silkeq_tilde.cpp
    silkeq/dsp/silkeq_dsp.cpp
    saturation/Coils.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
  )
  set_target_properties(silkeq_tilde PROPERTIES PREFIX "" OUTPUT_NAME "silkeq~")
  target_include_directories(silkeq_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/silkeq/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(silkeq_tilde)

  if(APPLE)
    message(STATUS "silkeq~: NEON/SSE2 via Xcode universal target")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
    message(STATUS "silkeq~: NEON (AArch64)")
  else()
    target_compile_options(silkeq_tilde PRIVATE -msse2)
    message(STATUS "silkeq~: SSE2 (x86-64)")
  endif()
endif()

# bloomeq~

if(BUILD_BLOOMEQ)
  add_library(bloomeq_tilde MODULE
    bloomeq/bloomeq_tilde.cpp
    bloomeq/dsp/bloomeq_dsp.cpp
    saturation/Tube2.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:bloomeq/bloomeq_tilde.def>
  )
  set_target_properties(bloomeq_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "bloomeq~")
  target_include_directories(bloomeq_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/bloomeq/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(bloomeq_tilde)
endif()

# focuseq~

if(BUILD_FOCUSEQ)
  add_library(focuseq_tilde MODULE
    focuseq/focuseq_tilde.cpp
    focuseq/dsp/focuseq_dsp.cpp
    saturation/Spiral2.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:focuseq/focuseq_tilde.def>
  )
  set_target_properties(focuseq_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "focuseq~")
  target_include_directories(focuseq_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/focuseq/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(focuseq_tilde)
endif()

# contoureq~

if(BUILD_CONTOUREQ)
  add_library(contoureq_tilde MODULE
    contoureq/contoureq_tilde.cpp
    contoureq/dsp/contoureq_dsp.cpp
    contoureq/dsp/Baxandall.cpp
    contoureq/dsp/Baxandall3.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:contoureq/contoureq_tilde.def>
  )
  set_target_properties(contoureq_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "contoureq~")
  target_include_directories(contoureq_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/contoureq/dsp"
    "${CMAKE_SOURCE_DIR}/common")
  configure_target(contoureq_tilde)
endif()

# levelcomp~

if(BUILD_LEVELCOMP)
  add_library(levelcomp_tilde MODULE
    levelcomp/levelcomp_tilde.cpp
    levelcomp/dsp/levelcomp_dsp.cpp
    saturation/SingleEndedTriode.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:levelcomp/levelcomp_tilde.def>
  )
  set_target_properties(levelcomp_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "levelcomp~")
  target_include_directories(levelcomp_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/levelcomp/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(levelcomp_tilde)
endif()

# punchcomp~

if(BUILD_PUNCHCOMP)
  add_library(punchcomp_tilde MODULE
    punchcomp/punchcomp_tilde.cpp
    punchcomp/dsp/punchcomp_dsp.cpp
    saturation/Coils.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:punchcomp/punchcomp_tilde.def>
  )
  set_target_properties(punchcomp_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "punchcomp~")
  target_include_directories(punchcomp_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/punchcomp/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(punchcomp_tilde)
endif()

# smoothcomp~

if(BUILD_SMOOTHCOMP)
  add_library(smoothcomp_tilde MODULE
    smoothcomp/smoothcomp_tilde.cpp
    smoothcomp/dsp/smoothcomp_dsp.cpp
    saturation/Tube2.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:smoothcomp/smoothcomp_tilde.def>
  )
  set_target_properties(smoothcomp_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "smoothcomp~")
  target_include_directories(smoothcomp_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/smoothcomp/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(smoothcomp_tilde)
endif()

# glidecomp~

if(BUILD_GLIDECOMP)
  add_library(glidecomp_tilde MODULE
    glidecomp/glidecomp_tilde.cpp
    glidecomp/dsp/glidecomp_dsp.cpp
    saturation/PurestDrive.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:glidecomp/glidecomp_tilde.def>
  )
  set_target_properties(glidecomp_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "glidecomp~")
  target_include_directories(glidecomp_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/glidecomp/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(glidecomp_tilde)
endif()

# snapcomp~

if(BUILD_SNAPCOMP)
  add_library(snapcomp_tilde MODULE
    snapcomp/snapcomp_tilde.cpp
    snapcomp/dsp/snapcomp_dsp.cpp
    saturation/Spiral2.cpp
    $<$<BOOL:${WIN32}>:toolchain/win_jit_stub.cpp>
    $<$<BOOL:${WIN32}>:snapcomp/snapcomp_tilde.def>
  )
  set_target_properties(snapcomp_tilde PROPERTIES PREFIX "" OUTPUT_NAME
    "snapcomp~")
  target_include_directories(snapcomp_tilde PRIVATE
    "${CMAKE_SOURCE_DIR}/snapcomp/dsp"
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/saturation")
  configure_target(snapcomp_tilde)
endif()

set(_targets "")
if(BUILD_AIREQ)
  list(APPEND _targets aireq_tilde)
endif()
if(BUILD_SILKEQ)
  list(APPEND _targets silkeq_tilde)
endif()
if(BUILD_BLOOMEQ)
  list(APPEND _targets bloomeq_tilde)
endif()
if(BUILD_FOCUSEQ)
  list(APPEND _targets focuseq_tilde)
endif()
if(BUILD_CONTOUREQ)
  list(APPEND _targets contoureq_tilde)
endif()
if(BUILD_LEVELCOMP)
  list(APPEND _targets levelcomp_tilde)
endif()
if(BUILD_PUNCHCOMP)
  list(APPEND _targets punchcomp_tilde)
endif()
if(BUILD_SMOOTHCOMP)
  list(APPEND _targets smoothcomp_tilde)
endif()
if(BUILD_GLIDECOMP)
  list(APPEND _targets glidecomp_tilde)
endif()
if(BUILD_SNAPCOMP)
  list(APPEND _targets snapcomp_tilde)
endif()
if(_targets)
  install(TARGETS ${_targets}
    LIBRARY DESTINATION "${MAX_EXTERNALS_DIR}"
    RUNTIME DESTINATION "${MAX_EXTERNALS_DIR}")
endif()
