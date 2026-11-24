{ mataRust }:
# The `mata` command-line interface: the same Cargo workspace and C++ build as the Rust bindings, built
# for the `mata-cli` crate instead of the examples.
mataRust.overrideAttrs (prev: {
  pname = "mata-cli";

  cargoBuildFlags = [
    "--package"
    "mata-cli"
  ];
  # The Cargo install hook copies the crate's binaries; the examples of `mataRust` are not built.
  postInstall = "";

  meta = prev.meta // {
    description = "Mata Script interpreter and Mata automata command-line interface";
    mainProgram = "mata";
  };
})
