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

  cmakeFlags = [ "-DFETCHCONTENT_FULLY_DISCONNECTED=ON" ];

  buildInputs = with pkgs; [
    catch2_3
    graphviz
  ];

  meta = {
    description = "A fast and simple automata library";
    homepage = "https://github.com/VeriFIT/mata";
    license = lib.licenses.mit;
    maintainers = with lib.maintainers; [ adda ];
    platforms = lib.platforms.unix;
  };
}
