# Language bindings of the Mata library.

mod python 'python/python.just'

# Build all bindings.
[default]
build: python::build

# Build all bindings and run their tests.
test: python::check

# Remove the build artifacts of all bindings.
clean: python::clean
