# Building and installing the C++ side of Mata: the library itself and all binaries built from it
# (unit tests, integration benchmarks, examples).
#
# The recipes delegate to the root Makefile, which is a thin wrapper around CMake. Every build is
# configured out of source in 'build/<BUILD_MODE>/<CXX>'.

import '../just/common.just'

# Build the whole project (library, unit tests, integration tests, examples). MODE is any build target of the root Makefile, e.g. 'release' or 'debug-werror'.
[default]
build MODE=BUILD_MODE:
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Build only the library, without tests and examples.
build-lib MODE=BUILD_MODE:
	make -C '{{ ROOT }}' '{{ MODE }}-lib' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Build the whole project with compiler warnings treated as errors.
build-werror MODE=BUILD_MODE:
	make -C '{{ ROOT }}' '{{ MODE }}-werror' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Run cppcheck over the compilation database of the build.
check MODE=BUILD_MODE:
	make -C '{{ ROOT }}' check BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Install the built library, headers, and CMake package files.
install MODE=BUILD_MODE:
	make -C '{{ ROOT }}' install BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Uninstall a previously installed library.
uninstall MODE=BUILD_MODE:
	make -C '{{ ROOT }}' uninstall BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Remove all build directories.
clean:
	rm -rf '{{ BUILD_ROOT }}'
