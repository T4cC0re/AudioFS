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
extra_CFLAGS=""
extra_LDFLAGS=""
extra_CONFIG="--enable-debug"
if [ "release" == "${1}" ]; then # when building release version, use fat LTO
  DEP_DIR="${NATIVE_ROOT}/dependencies/release_$(uname -m)"
  extra_CFLAGS="-flto -ffat-lto-objects"
  extra_LDFLAGS="-flto=full"
  extra_CONFIG="--enable-lto"
fi

if [ "$(uname -s)" = "Darwin" ]; then
  STATIC_FLAG='-Bstatic'
  if [ "$(uname -m)" = "arm64" ]; then
    extra_LDFLAGS="${extra_LDFLAGS} -L/opt/homebrew/lib"
    extra_CFLAGS="${extra_CFLAGS} -I/opt/homebrew/include"
  else
    extra_LDFLAGS="${extra_LDFLAGS} -L/usr/local/lib"
    extra_CFLAGS="${extra_CFLAGS} -I/usr/local/include"
  fi
  extra_CONFIG="${extra_CONFIG} --disable-x86asm" # x86asm can break on Apple Silicon
else
  STATIC_FLAG='-static'
fi

#sudo apt install -y libfftw3-3 libfftw3-dev

mkdir -p "${DEP_DIR}/ffmpeg"
wget -cO "${DEP_DIR}/ffmpeg".tar.gz "https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.gz"
tar xf "${DEP_DIR}/ffmpeg".tar.gz -C "${DEP_DIR}/ffmpeg" --strip-components 1
cd "${DEP_DIR}/ffmpeg"

export PKG_CONFIG_PATH=${DEP_DIR}/built/lib/pkgconfig
export PKG_CONFIG_LIBDIR=${DEP_DIR}/built/lib/pkgconfig
export build_suffix=-audiofs
export extra_version=audiofs_git

sedi 's;lchromaprint;lchromaprint_audiofs;' configure
sedi 's;libchromaprint;libchromaprint_audiofs;' configure

decoders() {
  cat ${NATIVE_ROOT}/decoders.list | sort -Vu | xargs -r -I% echo --enable-decoder=%
}
encoders() {
  cat ${NATIVE_ROOT}/encoders.list | sort -Vu | xargs -r -I% echo --enable-encoder=%
}
demuxers() {
  cat ${NATIVE_ROOT}/demuxers.list | sort -Vu | xargs -r -I% echo --enable-demuxer=%
}
muxers() {
  cat ${NATIVE_ROOT}/muxers.list | sort -Vu | xargs -r -I% echo --enable-muxer=%
}
filters() {
  cat ${NATIVE_ROOT}/filters.list | sort -Vu | xargs -r -I% echo --enable-filter=%
}

echo "running configure. This might take a moment..."
# shellcheck disable=SC2046
./configure \
  --cc="${CC}"\
  --cxx="${CXX}"\
  --enable-static\
  --enable-runtime-cpudetect\
  --disable-doc\
  --disable-ffplay\
  --disable-ffmpeg\
  --disable-shared\
  --disable-avdevice\
  --disable-swscale\
  --disable-everything\
  --disable-vdpau\
  --disable-v4l2-m2m \
  --disable-vaapi\
  --disable-vdpau\
  --disable-videotoolbox\
  --disable-vulkan\
  --disable-cuda-llvm\
  --disable-cuvid\
  --disable-d3d11va\
  --disable-dxva2\
  --disable-ffnvcodec\
  --disable-nvdec\
  --disable-nvenc\
  --disable-amf\
  $(decoders) \
  $(demuxers) \
  $(muxers) \
  $(encoders) \
  $(filters) \
  --enable-protocol=pipe,file\
  --enable-pic\
  --enable-version3\
  --enable-gpl\
  --enable-ffprobe\
  --prefix="${DEP_DIR}/built"\
  --enable-pthreads\
  --disable-stripping\
  --enable-chromaprint\
  ${extra_CONFIG}\
  --pkg-config-flags=--static \
  --extra-cxxflags="${STATIC_FLAG} -static-libstdc++ -static-libgcc" \
  --extra-libs='-ldl -lpthread -lchromaprint_audiofs -lfftw3 -lm -lstdc++' \
  --extra-cflags="${extra_CFLAGS} ${STATIC_FLAG} -I${DEP_DIR}/built/include -static-libstdc++ -static-libgcc -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic -fPIC"\
  --extra-ldflags="${extra_LDFLAGS} -fPIE -fstack-protector ${STATIC_FLAG} -L${DEP_DIR}/built/lib"

#exit 0
make
make install
