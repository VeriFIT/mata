#!/bin/sh
BUILD_DIR=build
OUTPUT_DIR=cov_html
COV_FILE=coverage.info

rm ${OUTPUT_DIR}

# capture coverage info
lcov --directory ${BUILD_DIR} --capture --output-file ${COV_FILE}

# filter out system files
lcov --remove ${COV_FILE} '/usr/*' --output-file ${COV_FILE}
lcov --remove ${COV_FILE} '*/3rdparty/*' --output-file ${COV_FILE}

# debug info
lcov --list ${COV_FILE}

# generate HTML
genhtml ${COV_FILE} --output-directory ${OUTPUT_DIR}
