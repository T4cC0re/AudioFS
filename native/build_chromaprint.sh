#!/usr/bin/env bash

set -eufo pipefail

NATIVE_ROOT="$(pwd)"
ROOT_DIR="${NATIVE_ROOT}/../"

export CC="${ROOT_DIR}cc_wrapper"
export CXX="${ROOT_DIR}cxx_wrapper"

sedi () {
  if [ "$(uname -s)" = "Darwin" ]; then
    sed -i "" "${@}"
  else
    sed -i "${@}"
  fi
}

DEP_DIR="${NATIVE_ROOT}/dependencies/non-release_$(uname -m)"
if [ "release" == "${1}" ]; then # when building release version, use fat LTO
  DEP_DIR="${NATIVE_ROOT}/dependencies/release_$(uname -m)"
  export CFLAGS='-flto -ffat-lto-objects'
  export CXXFLAGS='-flto -ffat-lto-objects'
  export LDFLAGS='-flto=full'
fi

mkdir -p "${DEP_DIR}/chromaprint"
wget -cO "${DEP_DIR}/chromaprint.tar.gz" "https://github.com/acoustid/chromaprint/releases/download/v1.5.1/chromaprint-1.5.1.tar.gz"
tar xf "${DEP_DIR}/chromaprint.tar.gz" -C "${DEP_DIR}/chromaprint" --strip-components 1
cd "${DEP_DIR}/chromaprint"

sedi 's;project(chromaprint ;project(chromaprint_audiofs ;' CMakeLists.txt
sedi 's;COMPONENT chromaprint;COMPONENT chromaprint_audiofs;' CMakeLists.txt
sedi 's;lchromaprint;lchromaprint_audiofs;' libchromaprint.pc.cmake
sedi 's;${CMAKE_CURRENT_BINARY_DIR}/libchromaprint.pc;${CMAKE_CURRENT_BINARY_DIR}/libchromaprint_audiofs.pc;' CMakeLists.txt

cmake -DCMAKE_INSTALL_PREFIX="${DEP_DIR}/built" -DCMAKE_BUILD_TYPE=Release -DFFT_LIB=fftw3 -DBUILD_SHARED_LIBS=OFF .
make
make install
mv -v "${DEP_DIR}/built/lib/libchromaprint.a" "${DEP_DIR}/built/lib/libchromaprint_audiofs.a"
