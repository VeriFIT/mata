# The website of the Mata library: the landing page and the playground in this directory, deployed with the
# documentation by .github/workflows/website.yml.

import '../just/common.just'

# Release of Viz.js, the build of Graphviz that draws automata in the playground (keep it in sync with website.yml).
VIZ_VERSION := "3.31.0"

# Build directory of the playground, compiled to WebAssembly.
WASM_BUILD := BUILD_ROOT / "wasm"

# Serve the website at http://localhost:PORT. The playground needs the files from `just site playground` first.
[default]
serve PORT="8000":
	python3 -m http.server '{{ PORT }}' --bind 127.0.0.1

# Build Mata for the playground with Emscripten, test the build under Node, and download Viz.js.
playground: wasm viz

# Build Mata for the playground with Emscripten (`emcmake` on PATH), and test the build under Node.
wasm:
	emcmake cmake -S '{{ ROOT }}/bindings/wasm' -B '{{ WASM_BUILD }}' -DCMAKE_BUILD_TYPE=Release
	cmake --build '{{ WASM_BUILD }}' --parallel '{{ JOBS }}'
	node '{{ ROOT }}/bindings/wasm/smoke-test.mjs' '{{ WASM_BUILD }}/mata-playground.js'
	cp '{{ WASM_BUILD }}/mata-playground.js' '{{ WASM_BUILD }}/mata-playground.wasm' playground/

# Download Viz.js to playground/vendor/.
viz:
	#!/usr/bin/env bash
	set -euo pipefail
	tmp="$(mktemp -d)"
	trap 'rm -rf "$tmp"' EXIT
	npm pack '@viz-js/viz@{{ VIZ_VERSION }}' --pack-destination "$tmp"
	tar -xzf "$tmp/viz-js-viz-{{ VIZ_VERSION }}.tgz" -C "$tmp"
	mkdir -p playground/vendor
	cp "$tmp/package/dist/viz.js" playground/vendor/viz.js

# Remove the generated files of the playground.
clean:
	rm -rf playground/mata-playground.js playground/mata-playground.wasm playground/vendor '{{ WASM_BUILD }}'
