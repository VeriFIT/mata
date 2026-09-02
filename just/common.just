# Shared configuration for all Mata just modules.
#
# Imported by the root justfile and by every module justfile. Note that `justfile()` and
# `justfile_directory()` always refer to the *root* justfile, even when this file is imported from a
# submodule. Recipes of a submodule are, on the other hand, run with the working directory set to
# the directory of the submodule.

# Repository root.
ROOT := justfile_directory()

# Directory with out-of-source CMake builds. Each build lives in 'build/<BUILD_MODE>/<CXX>'.
BUILD_ROOT := justfile_directory() / "build"

# Compiler to use. Options: "g++", "clang++", ...
CXX := env("CXX", "g++")

# Build mode used by recipes which do not take one explicitly.
# Options: "debug", "release", "release-debuginfo", "coverage", ...
BUILD_MODE := env("BUILD_MODE", "debug")

# Number of cores for parallel compilation.
JOBS := env("JOBS", "6")

# Invocation of the root justfile. A module sees only its own submodules, so recipes of sibling
# modules (such as 'src::build') have to be called through the root justfile.
JUST := "just --justfile '" + justfile() + "'"
