# Python bindings of the Mata library.
#
# The recipes delegate to the Makefile in this directory, which drives 'setup.py'. Building the
# bindings builds the release library into 'bindings/python/build/' on its own.

import '../../just/common.just'

# Build the bindings in place.
build:
	make build-ext jobs='{{ JOBS }}'

# Rebuild the bindings in place, even the up-to-date extension modules.
build-force:
	make build-ext-force jobs='{{ JOBS }}'

# Run the tests of the bindings with a coverage report.
test:
	make test

# Build the bindings and run their tests.
[default]
check: build test

# Build and install the bindings into the current Python environment.
install:
	make build-install

# Build a source distribution of the bindings.
dist:
	make build-dist

# Remove the generated sources, extension modules, and distributions.
clean:
	make clean
