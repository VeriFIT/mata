#!/usr/bin/env sh

set -euo pipefail

papermill ./example-01-ws1s-formulae.ipynb tmp_out.ipynb
papermill ./example-02-redos-attacks.ipynb tmp_out.ipynb
papermill ./example-03-exploring-maze.ipynb tmp_out.ipynb
papermill ./example-04-visualization.ipynb tmp_out.ipynb
rm tmp_out.ipynb
