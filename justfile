#!/usr/bin/env -S just --justfile
# ^ A shebang isn't required, but allows a justfile to be executed
#   like a script, with `./.justfile test`, for example.
#
#
#default: lint build test
# default recipe to display help information
[default]
help:
  just --list

alias t := test
alias tp := test-python
alias vc := valgrind-callgrind
alias c := clean
alias r := release
alias rd := release-debuginfo
alias w := wip
alias b := build
alias d := docs
alias h := help

CXX := env_var_or_default("CXX", "g++")

# Number of cores for parallel compilation.
JOBS := env_var_or_default("JOBS", "6")

test *ARGS:
    just build "debug"
    just test-run "debug" {{ARGS}}

    just build "release"
    just test-run "release" {{ARGS}}

test-run BUILD_MODE="debug" *ARGS:
    ./build/{{BUILD_MODE}}/{{CXX}}/tests/tests {{ARGS}}

build BUILD_MODE="debug":
    make {{BUILD_MODE}} BUILD_DIR="build/{{BUILD_MODE}}/{{CXX}}"

wip BUILD_DIR BUILD_MODE="debug" *ARGS:
    make {{BUILD_MODE}} BUILD_DIR="build/{{BUILD_DIR}}/{{CXX}}"
    ./build/{{BUILD_DIR}}/{{CXX}}/tests/tests {{ARGS}}

test-python:
    # source .venv/bin/activate.fish &&
    make -j {{JOBS}} -C bindings/python BUILD_DIR=build/bindings/python && make -j {{JOBS}} -C bindings/python test && ./run_papermill_examples.sh
    # ; deactivate

valgrind-callgrind +ARGS:
    valgrind --tool=callgrind {{ARGS}}

clean:
    make clean

release:
    just build "release"

release-debuginfo:
    just build "release-debuginfo"

docs:
    make doc
    make -C doc html
