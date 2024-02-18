ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
ARCH := $(shell uname -m)

ifeq ($(shell uname -s),Darwin)
ifeq ($(ARCH),x86_64)
GO := /usr/local/bin/go
BREW := /usr/local/bin/brew
endif
ifeq ($(ARCH),arm64)
GO := /opt/homebrew/bin/go
BREW := /opt/homebrew/bin/brew
endif
else
GO := go
BREW := brew
endif

NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu)

CGO_ENABLED := 0
export CGO_ENABLED

.PHONY: all
all: bin/non-release_$(ARCH)/audiofs-cli

.PHONY: release
ifeq ($(shell uname -s),Darwin)
release:
	arch -arch x86_64 make -j$(NPROC) bin/release_x86_64/audiofs-cli
	arch -arch arm64 make -j$(NPROC) bin/release_arm64/audiofs-cli
	mkdir -p bin/release_universal
	lipo -create -output bin/release_universal/audiofs-cli bin/release_arm64/audiofs-cli bin/release_x86_64/audiofs-cli
	lipo -create -output bin/release_universal/native bin/release_arm64/native bin/release_x86_64/native
else
release: bin/release_$(ARCH)/audiofs-cli
endif
# The compile_commands.json file can be used to open native code in an IDE which supports them (such as CLion)
.PHONY: native/compile_commands.json
native/compile_commands.json:
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json
	$(MAKE) clean
	GOFLAGS="-a" CC="$(ROOT_DIR)cc_wrapper" $(MAKE) all
	jq -s add $(ROOT_DIR)native/compile_commands.tmp.json > $(ROOT_DIR)native/compile_commands.json
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json

native/dependencies/release_$(ARCH)/built/lib/libavcodec-audiofs.a: native/dependencies/release_$(ARCH)/built/lib/libchromaprint_audiofs.a
	cd native && ./build_ffmpeg.sh release

native/dependencies/non-release_$(ARCH)/built/lib/libavcodec-audiofs.a: native/dependencies/non-release_$(ARCH)/built/lib/libchromaprint_audiofs.a
	cd native && ./build_ffmpeg.sh non-release

native/dependencies/release_$(ARCH)/built/lib/libchromaprint_audiofs.a: native/dependencies/release_$(ARCH)/built/lib/libfftw3_audiofs.a
	cd native && ./build_chromaprint.sh release

native/dependencies/non-release_$(ARCH)/built/lib/libchromaprint_audiofs.a: native/dependencies/non-release_$(ARCH)/built/lib/libfftw3_audiofs.a
	cd native && ./build_chromaprint.sh non-release
	
native/dependencies/release_$(ARCH)/built/lib/libfftw3_audiofs.a:
	cd native && ./build_fftw.sh release

native/dependencies/non-release_$(ARCH)/built/lib/libfftw3_audiofs.a:
	cd native && ./build_fftw.sh non-release

.PHONY: bin/non-release_$(ARCH)/native
bin/non-release_$(ARCH)/native: native/dependencies/non-release_$(ARCH)/built/lib/libavcodec-audiofs.a
	mkdir -p bin/non-release_$(ARCH)
	cd native && ./build_native.sh non-release "$(ROOT_DIR)$@"

.PHONY: bin/non-release_$(ARCH)/audiofs-cli
bin/non-release_$(ARCH)/audiofs-cli: native/dependencies/non-release_$(ARCH)/built/lib/libavcodec-audiofs.a bin/non-release_$(ARCH)/native
	$(GO) build -o $@ ./cmd

.PHONY: bin/release_$(ARCH)/native
bin/release_$(ARCH)/native: native/dependencies/release_$(ARCH)/built/lib/libavcodec-audiofs.a
	mkdir -p bin/release_$(ARCH)
	cd native && ./build_native.sh release "$(ROOT_DIR)$@"
	strip "$@"


.PHONY: bin/release_$(ARCH)/audiofs-cli
bin/release_$(ARCH)/audiofs-cli: native/dependencies/release_$(ARCH)/built/lib/libavcodec-audiofs.a bin/release_$(ARCH)/native
	$(GO) build -tags release -trimpath=true -buildvcs=true -ldflags="-s -w" -o $@ ./cmd

.PHONY: clean
clean:
	rm -rf bin/
	rm -rf native/dependencies/

.PHONY: check-format
check-format:
	cd native && clang-format-11 --verbose -Werror --dry-run *.c *.h

.PHONY: clang-format
clang-format:
	cd native && clang-format-11 --verbose -i *.c *.h

macos_packages:
	$(BREW) install cmake wget fftw jq clang-format@11 jansson go

packages:
	apt install cmake libfftw3-dev libfftw3-3 nasm libjansson-dev libz-dev build-essential clang-format-11 jq git wget tar
