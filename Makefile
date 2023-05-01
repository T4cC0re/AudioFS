ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: all
all: bin/non-release/audiofs.a bin/non-release/audiofs-cli

.PHONY: release
release: bin/release/audiofs.a bin/release/audiofs-cli

# The compile_commands.json file can be used to open native code in an IDE which supports them (such as CLion)
.PHONY: native/compile_commands.json
native/compile_commands.json:
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json
	$(MAKE) clean
	GOFLAGS="-a" CC="$(ROOT_DIR)cc_wrapper" $(MAKE) all
	jq -s add $(ROOT_DIR)native/compile_commands.tmp.json > $(ROOT_DIR)native/compile_commands.json
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json

native/dependencies/release/built/lib/libavcodec-audiofs.a: native/dependencies/release/built/lib/libchromaprint_audiofs.a
	cd native && ./build_ffmpeg.sh release

native/dependencies/non-release/built/lib/libavcodec-audiofs.a: native/dependencies/non-release/built/lib/libchromaprint_audiofs.a
	cd native && ./build_ffmpeg.sh non-release

native/dependencies/release/built/lib/libchromaprint_audiofs.a:
	cd native && ./build_chromaprint.sh release

native/dependencies/non-release/built/lib/libchromaprint_audiofs.a:
	cd native && ./build_chromaprint.sh non-release

.PHONY: bin/non-release/audiofs.a bin/non-release/audiofs.h
bin/non-release/audiofs.a bin/non-release/audiofs.h: native/dependencies/non-release/built/lib/libavcodec-audiofs.a
	go build --buildmode=c-archive -o $@ ./exports

.PHONY: bin/non-release/audiofs-cli
bin/non-release/audiofs-cli: native/dependencies/non-release/built/lib/libavcodec-audiofs.a
	go build -o $@ ./cmd

.PHONY: bin/release/audiofs.a bin/release/audiofs.h
bin/release/audiofs.a bin/release/audiofs.h: native/dependencies/release/built/lib/libavcodec-audiofs.a
	go build -tags release --buildmode=c-archive -o $@ ./exports

.PHONY: bin/release/audiofs-cli
bin/release/audiofs-cli: native/dependencies/release/built/lib/libavcodec-audiofs.a
	go build -tags release -o $@ ./cmd

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
