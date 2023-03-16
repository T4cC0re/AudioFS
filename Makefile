ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

all: bin/audiofs.a bin/audiofs-cli

# The compile_commands.json file can be used to open native code in an IDE which supports them (such as CLion)
.PHONY: native/compile_commands.json
native/compile_commands.json:
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json
	GOFLAGS="-a" CC="$(ROOT_DIR)cc_wrapper $(ROOT_DIR)native gcc" $(MAKE) all
	jq -s add $(ROOT_DIR)native/compile_commands.tmp.json > $(ROOT_DIR)native/compile_commands.json
	rm -f $(ROOT_DIR)native/compile_commands.tmp.json

.PHONY: bin/audiofs.a bin/audiofs.h
bin/audiofs.a:
	go build --buildmode=c-archive -o $@ ./exports

.PHONY: bin/audiofs-cli
bin/audiofs-cli:
	go build -o $@ ./cmd

.PHONY: clean
clean:
	rm -rf bin/

.PHONY: check-format
check-format:
	cd native && clang-format-11 --verbose -Werror --dry-run *.c *.h

.PHONY: clang-format
clang-format:
	cd native && clang-format-11 --verbose -i *.c *.h
