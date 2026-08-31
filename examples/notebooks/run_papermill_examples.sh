#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# The notebooks import 'libmata' from the in-place build of the Python bindings, so make it
# importable regardless of the directory this script is called from.
export PYTHONPATH="$SCRIPT_DIR/../../bindings/python${PYTHONPATH:+:$PYTHONPATH}"

TMP_OUT="$(mktemp --tmpdir mata-papermill-XXXXXX.ipynb)"
trap 'rm -f "$TMP_OUT"' EXIT

for file in "$SCRIPT_DIR"/*.ipynb; do
    echo "Running: $file."
    papermill "$file" "$TMP_OUT"
done
