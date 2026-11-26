# Documentation of the Mata library: the Doxygen reference of the C++ API (with the narrative pages).

import '../just/common.just'

# Generate the complete HTML documentation.
[default]
build: doxygen

# Generate the complete HTML documentation in the build directory of MODE.
# The root Makefile's 'docs' target configures its build as Debug, so the build of MODE is made here and only its
# 'docs' target is run.
doxygen MODE=BUILD_MODE:
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'
	cmake --build '{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' --target docs

# Remove the generated documentation.
clean:
	rm -rf api/ xml/ _build/
