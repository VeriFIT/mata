#!/bin/sh
TRAVIS_BUILD_DIR=build

cd ${TRAVIS_BUILD_DIR}

# capture coverage info
lcov --directory . --capture --output-file coverage.info

# filter out system
lcov --remove coverage.info '/usr/*' --output-file coverage.info

# debug info
lcov --list coverage.info
