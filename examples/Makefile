# Makefile wrapper for building/running selected MATA examples via CMake.

ROOT_DIR ?= ..
BUILD_DIR ?= $(ROOT_DIR)/build
CMAKE_BUILD_TYPE ?= Debug
JOBS ?= 6

EXAMPLE ?= example01-simple

.PHONY: all configure build run clean

all: build

configure:
	cmake -S $(ROOT_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) --parallel $(JOBS)

run: build
	$(BUILD_DIR)/examples/$(EXAMPLE)

clean:
	rm -f $(BUILD_DIR)/examples/$(EXAMPLE)
