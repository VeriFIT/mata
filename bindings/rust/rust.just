# Rust bindings of the Mata library.
#
# The build script of 'mata-sys' builds the library from the sources of this repository into Cargo's target
# directory, 'bindings/rust/target/'.

import '../../just/common.just'

# Build the bindings.
build:
	cargo build --workspace --all-features --jobs '{{ JOBS }}'

# Run the tests of the bindings.
test:
	cargo test --workspace --all-features --jobs '{{ JOBS }}'

# Check the formatting and run Clippy.
lint:
	cargo fmt --all --check
	cargo clippy --workspace --all-targets --all-features --jobs '{{ JOBS }}' -- -D warnings

# Build the bindings and run their tests.
[default]
check: build test

# Remove the build artifacts.
clean:
	cargo clean

# Package the crates for crates.io and build the packages, as 'cargo publish' does. ARGS go to 'cargo package'.
package *ARGS: (_with-sources "package" ARGS)

# Publish both crates to crates.io; raise their versions first, also in the workspace 'Cargo.toml'. ARGS go to 'cargo publish'.
publish *ARGS: (_with-sources "publish" ARGS)

# Run 'cargo COMMAND' on the crates with the committed Mata sources in 'mata-sys/mata/', which the package
# of 'mata-sys' carries.
_with-sources COMMAND ARGS:
	#!/usr/bin/env bash
	set -euo pipefail
	# Cargo counts the copied sources as uncommitted changes, so it gets '--allow-dirty' and this check instead.
	# The whole repository: the package carries the C++ sources, so uncommitted changes anywhere ship stale ones.
	if [[ -n "$(git -C '{{ ROOT }}' status --porcelain)" ]]; then
		echo "The repository has uncommitted changes. Commit them first." >&2
		exit 1
	fi
	sources=mata-sys/mata
	rm -rf "$sources"
	trap 'rm -rf "$sources"' EXIT
	mkdir "$sources"
	git -C '{{ ROOT }}' archive HEAD CMakeLists.txt cmake_uninstall.cmake.in Doxyfile.in LICENSE cmake include src 3rdparty | tar -x -C "$sources"
	# CMake reads the version from this file, or from git tags, which the package has none of.
	if version=$(git -C '{{ ROOT }}' describe --tags --abbrev=0 2>/dev/null); then echo "$version" > "$sources/VERSION"; fi
	cargo {{ COMMAND }} --workspace --allow-dirty --jobs '{{ JOBS }}' {{ ARGS }}
