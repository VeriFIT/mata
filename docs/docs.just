# Documentation of the Mata library: Doxygen reference of the C++ API rendered by Sphinx.

import '../just/common.just'

# Generate the complete HTML documentation.
[default]
build: doxygen html

# Generate the Doxygen XML reference of the C++ API.
doxygen MODE=BUILD_MODE:
	make -C '{{ ROOT }}' docs BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Build the HTML documentation from the generated Doxygen XML.
html:
	make html

# Build the PDF documentation from the generated Doxygen XML.
latex:
	make latex

# Remove the generated documentation.
clean:
	make clean
