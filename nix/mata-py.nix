{
  lib,
  python3Packages,
  mata,
  version ? "dev",
}:

let
  pep440Version =
    if builtins.match "[0-9]+\\.[0-9]+\\.[0-9]+.*" version != null then
      version
    else
      "0.0.0+${lib.strings.sanitizeDerivationName version}";
in
python3Packages.buildPythonPackage {
  pname = "mata-py";
  inherit version;
  format = "setuptools";

  doCheck = true;
  pythonImportsCheck = [
    "libmata"
    "libmata.nfa.nfa"
  ];

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
  sourceRoot = "source/bindings/python";

  nativeBuildInputs = with python3Packages; [
    cython
    setuptools
    wheel
    # pytest
    # pytest-cov
  ];

  propagatedBuildInputs = with python3Packages; [
    graphviz
    ipython
    networkx
    pandas
    tabulate
  ];

  buildInputs = [
    mata
    # pkgs.graphviz
  ];

  postPatch = ''
    substituteInPlace setup.py \
      --replace-fail 'mata_build_dir = "build"' 'mata_build_dir = os.environ.get("MATA_BUILD_DIR", "build")' \
      --replace-fail 'version=get_version(),' 'version=(os.environ["MATA_VERSION"] if "MATA_VERSION" in os.environ else get_version()),' \
      --replace-fail 'self.execute(_build_mata, (), msg="Building libmata library")' 'if os.environ.get("MATA_SKIP_BUILD", "0") != "1": self.execute(_build_mata, (), msg="Building libmata library")'
  '';

  preBuild = ''
    mkdir -p "$TMPDIR/mata-build/src"
    ln -sf ${mata}/lib/libmata.a "$TMPDIR/mata-build/src/libmata.a"
    export MATA_SKIP_BUILD=1
    export MATA_VERSION="${pep440Version}"
    export MATA_BUILD_DIR="$TMPDIR/mata-build"
  '';

  meta = {
    description = "Python bindings for the Mata automata library";
    homepage = "https://github.com/VeriFIT/mata";
    license = lib.licenses.mit;
    maintainers = with lib.maintainers; [ adda ];
    platforms = lib.platforms.unix;
  };
}
