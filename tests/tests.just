# Unit tests of the Mata C++ library (Catch2).

import '../just/common.just'

# Build and run the unit tests in both debug and release mode.
[default]
check *ARGS:
	{{ JUST }} cpp::build 'debug'
	{{ JUST }} tests-cpp::run 'debug' {{ ARGS }}
	{{ JUST }} cpp::build 'release'
	{{ JUST }} tests-cpp::run 'release' {{ ARGS }}

# Run the already built unit tests.
run MODE=BUILD_MODE *ARGS:
	'{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}/tests/tests' {{ ARGS }}

# Run all tests registered in the build (unit tests and integration binaries) through CTest.
ctest MODE=BUILD_MODE:
	make -C '{{ ROOT }}' test BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Build with coverage instrumentation, run the tests, and generate a coverage report.
coverage:
	make -C '{{ ROOT }}' coverage BUILD_DIR='{{ BUILD_ROOT }}/coverage/{{ CXX }}' JOBS='{{ JOBS }}'
	make -C '{{ ROOT }}' test-coverage BUILD_DIR='{{ BUILD_ROOT }}/coverage/{{ CXX }}'

# Build and run the unit tests in a scratch build directory 'build/<DIR>/<CXX>'.
wip DIR MODE=BUILD_MODE *ARGS:
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR='{{ BUILD_ROOT }}/{{ DIR }}/{{ CXX }}' JOBS='{{ JOBS }}'
	'{{ BUILD_ROOT }}/{{ DIR }}/{{ CXX }}/tests/tests' {{ ARGS }}

# Run a command under Valgrind Callgrind.
callgrind +ARGS:
	valgrind --tool=callgrind {{ ARGS }}
