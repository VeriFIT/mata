{
  lib,
  python3Packages,
  mata,
  graphviz,
  version ? "dev",
}:

let
  pep440Version =
    if builtins.match "[0-9]+\\.[0-9]+\\.[0-9]+.*" version != null then
      version
    else
      "0.0.0+${lib.strings.sanitizeDerivationName version}";
in
python3Packages.buildPythonPackage
# (finalAttrs:
{
  pname = "mata-py";
  inherit version;
  pyproject = true;
  # format = "setuptools";

  # doCheck = true;
  pythonImportsCheck = [
    "libmata"
    "libmata.nfa.nfa"
    "libmata.alphabets"
    "libmata.parser"
  ];

  src = mata.src;
    # let
    #   root = ./..;
    # in
    # with lib.fileset;
    # toSource {
    #   inherit root;
    #   fileset = difference (gitTracked root) (
    #     lib.fileset.unions [
    #       (maybeMissing ./../tests-integration/nfa-bench)
    #     ]
    #   );
    # };
  sourceRoot = "source/bindings/python";

  nativeBuildInputs = with python3Packages; [
    cython
    setuptools
    # wheel
  ];

  propagatedBuildInputs = [
    mata
    graphviz
    python3Packages.graphviz
  ]
  ++ (with python3Packages; [
    graphviz
  ])
  ;

  nativeCheckInputs = [
    graphviz
  ] ++ (with python3Packages; [
      # pytestCheckHook
      pytest-cov-stub
      papermill
      ipykernel
      ipython
      tabulate
      pandas
      seaborn
      networkx
  ]);

  # enabledTestPaths = [
  #   "tests/"
  # ];


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

  preCheck = ''
    make -C ../../bindings/python build-ext
  '';

  postCheck = ''
    bash ../../examples/notebooks/run_papermill_examples.sh
  '';

  meta = mata.meta // {
    description = "Python bindings for the Mata ${mata.meta.description}";
  };
}
# )
