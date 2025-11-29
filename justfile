#!/usr/bin/env -S just --justfile
# ^ A shebang isn't required, but allows a justfile to be executed
#   like a script, with `./.justfile test`, for example.
#
#
#default: lint build test
# default recipe to display help information
default:
  @just --list

alias t := test
alias tp := test-python
alias vc := valgrind-callgrind
alias c := clean
alias rd := release-debuginfo
alias w := wip

#check:

CXX := env_var_or_default("CXX", "g++")

# Number of cores for parallel compilation.
JOBS := env_var_or_default("JOBS", "6")

test:
    make debug BUILD_DIR="build/debug/${CXX}"
    ./build/debug/${CXX}/tests/tests

    make release BUILD_DIR="build/release/${CXX}"
    ./build/release/${CXX}/tests/tests

wip BUILD_DIR_PATH BUILD_MODE="debug":
    make {{BUILD_MODE}} BUILD_DIR="build/{{BUILD_DIR_PATH}}/${CXX}"
    ./build/{{BUILD_DIR_PATH}}/${CXX}/tests/tests

test-python:
    source .venv/bin/activate && make -j ${JOBS} -C bindings/python BUILD_DIR=build/bindings/python && make -j ${JOBS} -C bindings/python test && ./run_papermill_examples.sh ; deactivate

valgrind-callgrind +ARGS:
    valgrind --tool=callgrind {{ARGS}}

clean:
    make clean

release-debuginfo:
    make release-debuginfo BUILD_DIR="build/release-debuginfo"
