# Documentation of the Mata library: Doxygen reference of the C++ API rendered by Sphinx.

import '../just/common.just'

# Generate the complete HTML documentation.
[default]
build: doxygen html

# Generate the Doxygen XML reference of the C++ API, in the build directory of MODE.
# The root Makefile's 'docs' target configures its build as Debug, so the build of MODE is made here and only its
# 'docs' target is run.
doxygen MODE=BUILD_MODE:
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'
	cmake --build '{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' --target docs

# Build the HTML documentation from the generated Doxygen XML.
html:
	make html

# Build the PDF documentation from the generated Doxygen XML.
latex:
	make latex

# Remove the generated documentation.
clean:
	make clean
