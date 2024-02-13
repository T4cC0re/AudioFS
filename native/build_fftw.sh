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
  export LDFLAGS='-flto'
fi

mkdir -p "${DEP_DIR}/fftw3"
wget -cO "${DEP_DIR}/fftw3.tar.gz" "https://fftw.org/fftw-3.3.10.tar.gz"
tar xf "${DEP_DIR}/fftw3.tar.gz" -C "${DEP_DIR}/fftw3" --strip-components 1
cd "${DEP_DIR}/fftw3"

./configure --prefix="${DEP_DIR}/built" --enable-threads --with-combined-threads --program-suffix=audiofs
# --enable-avx --enable-avx2 --enable-sse2
make
make install
ln -s "${DEP_DIR}/built/lib/libfftw3.a" "${DEP_DIR}/built/lib/libfftw3_audiofs.a"
sedi 's/-lfftw3/-lfftw3_audiofs/g' "${DEP_DIR}/built/lib/pkgconfig/fftw3.pc"
