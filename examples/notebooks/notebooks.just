# Jupyter notebook examples of using the Mata Python bindings.

import '../../just/common.just'

# Build the Python bindings the notebooks import and execute all notebooks with Papermill. Fails whenever a notebook raises.
[default]
run:
	{{ JUST }} bindings::python::build
	./run_papermill_examples.sh

# Synchronize the notebooks with their paired Python scripts (jupytext).
sync:
	jupytext --sync *.ipynb
