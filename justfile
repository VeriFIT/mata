#!/usr/bin/env -S just --justfile

# Root justfile of the Mata library. Every subdirectory owns its recipes in '<dir>/<dir>.just' and is
# imported here as a submodule. Run a module recipe as 'just tests run release', a module default
# recipe as 'just docs', and list the recipes of a single module as 'just --list tests'.

import 'just/common.just'

mod bench 'tests-integration/tests-integration.just'
mod bindings 'bindings/bindings.just'
mod cpp 'src/src.just'
mod docs 'docs/docs.just'
mod examples 'examples/examples.just'
mod nix 'nix/nix.just'
mod tests-cpp 'tests/tests.just'

alias t := test
alias tc := tests-cpp
alias tp := bindings::python
alias ti := bench::performance
alias b := cpp::build
alias w := tests-cpp::wip
alias vc := tests-cpp::callgrind
alias d := docs::build
alias f := nix::fmt
alias r := release
alias rd := release-debuginfo
alias c := clean
alias h := help

# Run the whole test suite: C++ unit tests in debug and release mode, Python bindings, and notebooks.
[default]
test *ARGS: (tests-cpp::check ARGS) bindings::test examples::notebooks::run

# Build the whole project in release mode.
release: (cpp::build "release")

# Build the whole project in release mode with debug information.
release-debuginfo: (cpp::build "release-debuginfo")

# Remove the build artifacts of the project, the bindings, and the documentation.
clean: cpp::clean bindings::clean docs::clean

# TODO: Implement.
ci:
	@! echo "Unimplemented"

# List all recipes, including the recipes of all modules.
help:
	@just --list --list-submodules --justfile '{{ justfile() }}'

# Update and synchronize the locked Python dependencies of the workspace.
uv-update:
	uv lock --upgrade
	uv sync
