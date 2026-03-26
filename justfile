#!/usr/bin/env -S just --justfile

# Compiler to use. Options: "g++", "clang", ...
CXX := env_var_or_default("CXX", "g++")

# Number of cores for parallel compilation.
JOBS := env_var_or_default("JOBS", "6")

alias t := test
[default]
test *ARGS:
  just build "debug"
  just test-run "debug" {{ARGS}}

  just build "release"
  just test-run "release" {{ARGS}}

test-run BUILD_MODE="debug" *ARGS:
  ./build/{{BUILD_MODE}}/{{CXX}}/tests/tests {{ARGS}}

alias b := build
build BUILD_MODE="debug":
  make {{BUILD_MODE}} BUILD_DIR="build/{{BUILD_MODE}}/{{CXX}}"

alias w := wip
wip BUILD_DIR BUILD_MODE="debug" *ARGS:
  make {{BUILD_MODE}} BUILD_DIR="build/{{BUILD_DIR}}/{{CXX}}"
  ./build/{{BUILD_DIR}}/{{CXX}}/tests/tests {{ARGS}}

alias tp := test-python
[working-directory: "bindings/python/"]
test-python:
  # source .venv/bin/activate.fish &&
  make -j {{JOBS}} BUILD_DIR=build/bindings/python
  make -j {{JOBS}} test
  ../../examples/notebooks/run_papermill_examples.sh
  # ; deactivate

alias vc := valgrind-callgrind
valgrind-callgrind +ARGS:
  valgrind --tool=callgrind {{ARGS}}

alias c := clean
clean:
  make clean

alias r := release
release: (build "release")

alias rd := release-debuginfo
release-debuginfo: (build "release-debuginfo")

alias d := docs
docs:
  make docs BUILD_DIR="build/debug/{{CXX}}"
  make -C docs/ html

# TODO: Implement.
ci:
  @! echo "Unimplemented"

alias h := help
help:
  just --list --justfile {{justfile()}}

alias f := fmt
fmt:
  nix fmt

[working-directory: "examples/notebooks/"]
notebook-sync:
  jupytext --sync examples/notebooks/*.ipynb
