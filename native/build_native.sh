#!/usr/bin/env bash

set -eufo pipefail

NATIVE_ROOT="$(pwd)"
ROOT_DIR="${NATIVE_ROOT}/../"

export CC="${ROOT_DIR}cc_wrapper"
OUTPUT="${2}"

CFLAGS='-Werror=unused-result -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic -fPIC -static-libstdc++'
LDFLAGS='-lswresample-audiofs -lavformat-audiofs -lavutil-audiofs -lavcodec-audiofs -lavfilter-audiofs -lchromaprint_audiofs -lfftw3_audiofs -ljansson -lstdc++ -lm -lz'
STATIC_FLAG='-static' 

if [[ "arm64" == "$(uname -m)" ]]; then
    HOMEBREW='/opt/homebrew'
else
    HOMEBREW='/usr/local'
fi

if [[ "release" == "${1}" ]]; then
  echo 'Building release binary'

DEP_DIR="${NATIVE_ROOT}/dependencies/release_$(uname -m)"
  CFLAGS="${CFLAGS} -DAUDIOFS_NO_TRACE=1 -O3 -flto -ffat-lto-objects"
  LDFLAGS="${LDFLAGS} -flto=full"
else
    echo 'building debug binary'
DEP_DIR="${NATIVE_ROOT}/dependencies/non-release_$(uname -m)"
CFLAGS="${CFLAGS} -O0 -g"

fi

if [[ $(uname -s ) == Darwin ]]; then
STATIC_FLAG='-Bstatic'
    CFLAGS="${CFLAGS} -I${HOMEBREW}/include"
    LDFLAGS="${LDFLAGS} -framework CoreFoundation -framework CoreMedia -framework CoreServices -liconv -L${HOMEBREW}/lib"
else
    CFLAGS="${CFLAGS} -fuse-ld=lld"
    LDFLAGS="-Wl,--start-group ${LDFLAGS} -Wl,--end-group"
fi

CFLAGS="${STATIC_FLAG} -I${DEP_DIR}/built/include ${CFLAGS}"
LDFLAGS="${STATIC_FLAG} -L${DEP_DIR}/built/lib ${LDFLAGS}"

export CFLAGS
export LDFLAGS

"${CC}" -o "${OUTPUT}" ${CFLAGS} ${LDFLAGS} $(find . -type f -maxdepth 1 -name '*.c')
