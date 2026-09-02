# Nix flake of the Mata library: formatting, checks, packages, and development shells.
#
# All recipes run in the repository root, where the flake lives.

set working-directory := '..'

# Format the whole repository with treefmt.
[default]
fmt *ARGS:
	nix fmt {{ ARGS }}

# Evaluate and build all flake outputs.
check:
	nix flake check

# Build a flake package, e.g. 'just nix build mata-py'.
build PACKAGE="default":
	nix build '.#{{ PACKAGE }}'

# Enter a development shell, e.g. 'just nix develop clang'.
develop SHELL="default":
	nix develop '.#{{ SHELL }}'

# Update all flake inputs.
update:
	nix flake update
