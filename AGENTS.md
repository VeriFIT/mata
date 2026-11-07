# Instructions for coding agents

This file only points to the documentation written for human developers; that documentation is the single source of
truth. Read the linked file before doing the related task, and follow it as if you were a human contributor.

- For building, installing, and using the library (C++, Python, Rust), read [README.md](README.md).
- For contribution workflow, code style, naming conventions, TODO comment format, testing rules, formatting, commit
  message format, and referencing issues (`Part of #N` vs. `Fixes #N`), read [CONTRIBUTING.md](CONTRIBUTING.md) before
  writing code, committing, or opening a pull request.
- For branches (`devel`, `master`) and where to base changes, read [docs/src/branches.md](docs/src/branches.md).
- For available build, test, lint, and format commands, read the [justfile](justfile) (and the `*.just` files it
  imports), or run `just help`.
- For benchmarking, profiling, and performance comparisons, read [BENCHMARKING.md](BENCHMARKING.md) and
  [tests-integration/README.md](tests-integration/README.md).
- For the automata file format (`.mata`), read [AUTOMATAFORMAT.md](AUTOMATAFORMAT.md).
- For the Python binding, read [bindings/python/README.md](bindings/python/README.md).
- For the Rust binding, read [bindings/rust/README.md](bindings/rust/README.md).
- For building to WebAssembly, read [README-WASM.md](README-WASM.md).
- For building the documentation, read [docs/docs.just](docs/docs.just).
- For the API, start from the headers `include/mata/nfa/nfa.hh`, `include/mata/nft/nft.hh`, etc.
