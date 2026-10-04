# Examples of using Mata: standalone C++ programs and Jupyter notebooks.

import '../just/common.just'

mod notebooks 'notebooks/notebooks.just'

# The build goes through the root Makefile, which always configures the build directory with
# '-DMATA_BUILD_EXAMPLES=ON'; configuring it here directly would silently inherit a cached
# 'MATA_BUILD_EXAMPLES=OFF' left behind by 'just cpp::build-lib' and build no examples at all.
# The '*-lib' targets of the root Makefile are the exception: they set MATA_BUILD_EXAMPLES=OFF on
# purpose, so building examples with them would report success and produce nothing.

# Build all the C++ examples. MODE is any non-'*-lib' build target of the root Makefile.
[default]
build MODE=BUILD_MODE:
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ '{{ MODE }}' == *-lib ]]; then
		echo "Build mode '{{ MODE }}' builds only the library and disables the examples. Use '{{ trim_end_match(MODE, "-lib") }}' instead." >&2
		exit 1
	fi
	make -C '{{ ROOT }}' '{{ MODE }}' BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}' JOBS='{{ JOBS }}'

# Build and run a single C++ example, 'example01-simple' by default.
run EXAMPLE="example01-simple" MODE=BUILD_MODE: (build MODE)
	#!/usr/bin/env bash
	set -euo pipefail
	example='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}/examples/{{ EXAMPLE }}'
	if [[ ! -x "$example" ]]; then
		echo "No such example: '{{ EXAMPLE }}'. Built examples live in '$(dirname "$example")'." >&2
		exit 1
	fi
	"$example"

# Remove the built C++ examples.
clean MODE=BUILD_MODE:
	#!/usr/bin/env bash
	set -euo pipefail
	# MODE ends up in an 'rm -rf' path, so refuse anything that could escape the build directory
	# ('just examples::clean ..' would otherwise resolve to the source directory 'examples/').
	if [[ ! '{{ MODE }}' =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ || '{{ MODE }}' == *..* ]]; then
		echo "Refusing to clean: '{{ MODE }}' is not a build mode name." >&2
		exit 1
	fi
	rm -rf '{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}/examples'
