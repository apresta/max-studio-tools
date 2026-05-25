#!/bin/bash

set -euo pipefail

MAX_VERSION=9
PACKAGE_NAME=max-studio-tools
EXTERNALS_PATH="${HOME}/Documents/Max ${MAX_VERSION}/Packages/${PACKAGE_NAME}/externals"

LLVM_MINGW_DIR="deps/llvm-mingw"
LLVM_MINGW_SENTINEL="${LLVM_MINGW_DIR}/bin/x86_64-w64-mingw32-clang"

if [[ "${CLEAN:-0}" == "1" ]]; then
  echo "==> Cleaning build artifacts"
  rm -rf build/
fi

echo "==> Updating submodules"
git submodule update --init --recursive

if [[ ! -x "${LLVM_MINGW_SENTINEL}" ]]; then
  echo "==> Fetching llvm-mingw"
  LLVM_MINGW_VERSION=$(
    curl -fsSL https://api.github.com/repos/mstorsjo/llvm-mingw/releases/latest \
      | grep '"tag_name"' | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/'
  )
  ARCHIVE="llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-macos-universal.tar.xz"
  URL="https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSION}/${ARCHIVE}"
  TMP="$(mktemp -t llvm-mingw-XXXXXX).tar.xz"
  trap 'rm -f "${TMP}"' EXIT
  curl -fL --progress-bar "${URL}" -o "${TMP}"
  mkdir -p "${LLVM_MINGW_DIR}"
  tar -xJf "${TMP}" -C "${LLVM_MINGW_DIR}" --strip-components=1
  [[ -x "${LLVM_MINGW_SENTINEL}" ]] || {
    echo "ERROR: llvm-mingw sentinel not found" >&2
    exit 1
  }
fi

echo "==> Building"
mkdir -p build "${EXTERNALS_PATH}"
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLLVM_MINGW_PREFIX="$(pwd)/${LLVM_MINGW_DIR}" \
  -DMAX_EXTERNALS_DIR="${EXTERNALS_PATH}"
cmake --build build --config Release

echo "==> Code-signing macOS bundles"
for mxo in "${EXTERNALS_PATH}"/*.mxo; do
  codesign --force --deep --sign - "${mxo}"
done

echo
echo "Done."
echo "  macOS:   ${EXTERNALS_PATH}/aireq~.mxo  ${EXTERNALS_PATH}/silkeq~.mxo  ${EXTERNALS_PATH}/bloomeq~.mxo  ${EXTERNALS_PATH}/focuseq~.mxo  ${EXTERNALS_PATH}/contoureq~.mxo"
echo "  Windows: ${EXTERNALS_PATH}/aireq~.mxe64 ${EXTERNALS_PATH}/silkeq~.mxe64 ${EXTERNALS_PATH}/bloomeq~.mxe64 ${EXTERNALS_PATH}/focuseq~.mxe64 ${EXTERNALS_PATH}/contoureq~.mxe64"
