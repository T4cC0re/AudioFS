#!/usr/bin/env bash

set -eufo pipefail

CFLAGS='-Werror=unused-result -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic -fPIC -flto -ffat-lto-objects -Bstatic'
LDFLAGS='-framework CoreFoundation -framework CoreMedia -framework CoreServices -liconv -lswresample-audiofs -lavformat-audiofs -lavutil-audiofs -lavcodec-audiofs -lavfilter-audiofs -lchromaprint_audiofs -lfftw3 -ljansson -lstdc++ -lm -lz -Bstatic -flto=full'
CFLAGS="${CFLAGS} -O0 -g"

if [[ $(uname -s ) == Darwin ]]; then
    CFLAGS="${CFLAGS} -Idependencies/non-release_arm64/built/include -I/opt/homebrew/include"
    LDFLAGS="${LDFLAGS} -Ldependencies/non-release_arm64/built/lib -L/opt/homebrew/lib"
fi

export CFLAGS
export LDFLAGS

clang -v -o ../bin/native $(find . -type file -maxdepth 1 -name '*.c') ${CFLAGS} ${LDFLAGS}
