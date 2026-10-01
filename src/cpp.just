# Building and installing the C++ side of Mata: the library itself and all binaries built from it
# (unit tests, integration benchmarks, examples).
#
# The recipes delegate to the root Makefile, which is a thin wrapper around CMake. Every build is
# configured out of source in 'build/<BUILD_MODE>/<CXX>'.

import '../just/common.just'

# Publish the compilation database of the MODE build at 'build/compile_commands.json', which is where
# '.clangd' and a bare 'clang-tidy' run look for it. Developers who drive the root Makefile directly
# build into 'build/' and get that file from CMake; the 'just' recipes build into
# 'build/<MODE>/<CXX>', so the database has to be published explicitly. Last build wins, which is
# what an editor should follow.
compile-db MODE=BUILD_MODE:
	#!/usr/bin/env bash
	set -euo pipefail
	db='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}/compile_commands.json'
	# A copy, not a symlink: CMake replaces a symlinked 'compile_commands.json' with a regular file of
	# its own, so a later 'make debug' (BUILD_DIR=build) would leave a link-shaped stale database
	# behind. A plain file is simply overwritten by whichever build ran last.
	if [[ -f "$db" ]]; then cp -f "$db" '{{ BUILD_ROOT }}/compile_commands.json'; fi

# Build the whole project (library, unit tests, integration tests, examples). MODE is any build target of the root Makefile, e.g. 'release' or 'debug-werror'.
[default]
build MODE=BUILD_MODE: && (compile-db MODE)
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Build only the library, without tests and examples.
build-lib MODE=BUILD_MODE: && (compile-db MODE)
	make -C '{{ ROOT }}' '{{ MODE }}-lib' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Build the whole project with compiler warnings treated as errors.
build-werror MODE=BUILD_MODE: && (compile-db MODE)
	make -C '{{ ROOT }}' '{{ MODE }}-werror' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Run cppcheck over the compilation database of the build.
check MODE=BUILD_MODE:
	make -C '{{ ROOT }}' check BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Run clang-tidy over the compilation database of the build. Extra ARGS go to run-clang-tidy, e.g. '-fix' to apply the fixes or '-checks=...' to override the '.clang-tidy' set.
lint MODE=BUILD_MODE *ARGS:
	#!/usr/bin/env bash
	# Which headers are analysed is decided by '.clang-tidy'; here we only pick the source files.
	set -euo pipefail
	db='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'
	if [[ ! -f "$db/compile_commands.json" ]]; then
		echo "No compilation database in '$db'. Run '{{ JUST }} cpp::build {{ MODE }}' first." >&2
		exit 1
	fi
	# clang-tidy does not inherit the system include paths of the compiler that produced the
	# database, so ask the compiler for them and forward them explicitly. Without this every '#include <...>' fails to resolve.
	extra_args=(-extra-arg=-Wno-unknown-warning-option)
	while read -r include_dir; do
		if [[ -d "$include_dir" ]]; then extra_args+=("-extra-arg=-isystem$include_dir"); fi
	done < <('{{ CXX }}' -E -x c++ -v /dev/null 2>&1 |
		sed -n '/^#include <\.\.\.>/,/^End of search/{ s/^ \([^ ]*\)$/\1/p }')
	# run-clang-tidy echoes the full clang-tidy command line for every translation unit; keep the
	# progress counter and drop the rest. 'pipefail' keeps the exit code of run-clang-tidy.
	run-clang-tidy -p "$db" -j '{{ JOBS }}' -quiet -use-color \
		"${extra_args[@]}" {{ ARGS }} '^{{ ROOT }}/src/.*\.cc$' |
		sed -u -E 's#^(\[[0-9]+/[0-9]+\]\[[^]]*\]) .*#\1#'

# Run clang-tidy as a gate: any reported diagnostic fails the recipe.
lint-werror MODE=BUILD_MODE *ARGS: (lint MODE "-warnings-as-errors=*" ARGS)

# Install the built library, headers, and CMake package files.
install MODE=BUILD_MODE:
	make -C '{{ ROOT }}' install BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Uninstall a previously installed library.
uninstall MODE=BUILD_MODE:
	make -C '{{ ROOT }}' uninstall BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Remove all build directories.
clean:
	rm -rf '{{ BUILD_ROOT }}'
