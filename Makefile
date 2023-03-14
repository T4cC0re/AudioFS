ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

all: bin/audiofs_c_ffi.a bin/audiofs-cli

.PHONY: native/compile_commands.json
native/compile_commands.json:
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json
	GOFLAGS="-a" CC="$(ROOT_DIR)cc_wrapper $(ROOT_DIR)native gcc" $(MAKE) all
	jq -s add $(ROOT_DIR)native/compile_commands.tmp.json > $(ROOT_DIR)native/compile_commands.json
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json

.PHONY: bin/audiofs_c_ffi.a
bin/audiofs_c_ffi.a:
	go build --buildmode=c-archive -o bin/audiofs_c_ffi.a ./exports

.PHONY: bin/audiofs-cli
bin/audiofs-cli:
	go build -o bin/audiofs-cli ./cmd

.PHONY: clean
clean:
	rm -rf bin/

.PHONY: clang-format
check-format:
	cd native && clang-format-11 --verbose -Werror --dry-run *.c *.h
clang-format:
	cd native && clang-format-11 --verbose -i *.c *.h