# Language bindings of the Mata library.

mod python 'python/python.just'
mod rust 'rust/rust.just'

# Build all bindings.
[default]
build: python::build rust::build

# Build all bindings and run their tests.
test: python::check rust::check

# Remove the build artifacts of all bindings.
clean: python::clean rust::clean
