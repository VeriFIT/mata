{
  pkgs,
  lib,
  stdenv,
  version ? "dev",
}:
# Builds the Rust bindings (the `mata` crate and its `mata-sys` FFI layer) with the C++ library
# from this source tree, the same way the just recipes do.
pkgs.rustPlatform.buildRustPackage {
  pname = "mata-rust";
  inherit version;

  src =
    let
      root = ./..;
    in
    with lib.fileset;
    toSource {
      inherit root;
      fileset = gitTracked root;
    };

  # `cargoRoot` is where the vendoring hook finds Cargo.lock; `buildAndTestSubdir` is where cargo runs.
  cargoRoot = "bindings/rust";
  buildAndTestSubdir = "bindings/rust";

  # A library workspace installs nothing by itself; the examples give the package its binaries, and
  # exercise the whole stack (cxx bridge, C++ library, safe layer) at build time.
  cargoBuildFlags = [ "--examples" ];

  cargoLock.lockFile = ./../bindings/rust/Cargo.lock;
  postInstall = ''
    install -Dm555 -t $out/bin target/${stdenv.hostPlatform.config}/release/examples/{inclusion,regex_to_dot,stats}
  '';

  # The crate's own test suite runs in CI with the working tree it expects; here it would build a
  # second copy of the C++ library and take a long time without adding to the package.
  doCheck = false;

  # cmake for the C++ library that `mata-sys/build.rs` builds and installs into the crate.
  nativeBuildInputs = with pkgs; [
    cmake
  ];

  env = {
    # `build.rs` defaults to Release anyway; keep the choice explicit.
    MATA_CMAKE_BUILD_TYPE = "Release";
    # The source has no git tags, so CMake would declare 0.0.0; report the flake's version instead.
    MATA_VERSION = version;
  };

  meta = {
    description = "Rust bindings for the Mata automata library";
    homepage = "https://github.com/VeriFIT/mata";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix;
  };
}
