#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$0")"

TMP_OUT="$(mktemp --tmpdir mata-papermill-XXXXXX.ipynb)"
trap 'rm -f "$TMP_OUT"' EXIT

for file in "$SCRIPT_DIR"/*.ipynb; do
    echo "Running: $file."
    papermill "$file" "$TMP_OUT"
done
