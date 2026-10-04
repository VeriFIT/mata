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

# Build Mata with profile-guided optimization. Runs the instrumented 'generate' stage, trains it
# with TRAIN (a command run from the build directory), and rebuilds with the recorded profile. The
# result is left in 'build/<MODE>/<CXX>-pgo'. Record the compiler, the flags and the corpus when
# reporting numbers from it; a profile belongs to exactly one compiler and source tree.
pgo MODE='release' TRAIN='./tests/tests':
	#!/usr/bin/env bash
	set -euo pipefail
	build='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}-pgo'
	profile="$build/pgo"
	rm -rf "$profile"
	mkdir -p "$profile"
	echo "=== PGO stage 1/3: instrumented build in $build"
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR="$build" JOBS='{{ JOBS }}' \
		MATA_PGO=generate
	echo "=== PGO stage 2/3: training with '{{ TRAIN }}'"
	( cd "$build" && {{ TRAIN }} )
	# Detect the compiler from the CMakeCXXCompiler.cmake file, not from the CXX variable;
	#  an absolute path like /opt/homebrew/bin/clang++ does not start with 'clang'.
	compiler_id=$(grep -h '^set(CMAKE_CXX_COMPILER_ID' "$build"/CMakeFiles/*/CMakeCXXCompiler.cmake 2>/dev/null | head -1 | sed -n 's/^set(CMAKE_CXX_COMPILER_ID "\([^"]*\)").*/\1/p')
	if [[ -z "$compiler_id" ]]; then
		echo "error: could not determine CMAKE_CXX_COMPILER_ID from the generate-stage build" >&2
		exit 1
	fi
	case "$compiler_id" in
		Clang|AppleClang)
			profraw_count=$(find "$profile" -name "*.profraw" 2>/dev/null | wc -l)
			if [[ $profraw_count -eq 0 ]]; then
				echo "error: no raw profiles (.profraw) found in $profile; the training command may not have run an instrumented binary" >&2
				exit 1
			fi
			llvm-profdata merge -output="$profile/mata.profdata" "$profile"/*.profraw
			;;
		GNU)
			gcda_count=$(find "$build" -name "*.gcda" 2>/dev/null | wc -l)
			if [[ $gcda_count -eq 0 ]]; then
				echo "error: no GCC profile data (.gcda) found under $build; the training command may not have run an instrumented binary" >&2
				exit 1
			fi
			;;
		*)
			echo "error: unsupported compiler '$compiler_id' (only GNU and Clang/AppleClang are supported)" >&2
			exit 1
			;;
	esac
	echo "=== PGO stage 3/3: rebuilding with the recorded profile"
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR="$build" JOBS='{{ JOBS }}' \
		MATA_PGO=use
	echo "=== PGO build ready in $build"

# Install the built library, headers, and CMake package files.
install MODE=BUILD_MODE:
	make -C '{{ ROOT }}' install BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Uninstall a previously installed library.
uninstall MODE=BUILD_MODE:
	make -C '{{ ROOT }}' uninstall BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Remove all build directories.
clean:
	rm -rf '{{ BUILD_ROOT }}'
