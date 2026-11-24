{
  pkgs,
  lib,
  stdenv,
  version ? "dev",
}:
stdenv.mkDerivation {
  pname = "mata";
  inherit version;

  doCheck = true;

  src =
    let
      root = ./..;
    in
    with lib.fileset;
    toSource {
      inherit root;
      fileset = difference (gitTracked root) (
        lib.fileset.unions [
          (maybeMissing ./../tests-integration/nfa-bench)
        ]
      );
    };

  nativeBuildInputs = with pkgs; [
    cmake
    doxygen
  ];

  cmakeFlags = [
    "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    "-DMATA_USE_MIMALLOC=ON"
  ];

  buildInputs = with pkgs; [
    catch2_3
    graphviz
  ];

  # The installed CMake target links mimalloc into the final executable.
  propagatedBuildInputs = [ pkgs.mimalloc ];

  meta = {
    description = "A fast and simple automata library";
    homepage = "https://github.com/VeriFIT/mata";
    license = lib.licenses.mit;
    maintainers = with lib.maintainers; [ adda ];
    platforms = lib.platforms.unix;
  };
}
