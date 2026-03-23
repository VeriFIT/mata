{
  python3Packages,
  mata,
}:
mata.overrideAttrs (prev: {
  pname = "mata-docs";

  nativeBuildInputs =
    (prev.nativeBuildInputs or [ ])
    ++ (with python3Packages; [
      sphinx
      breathe
      myst-parser
    ]);

  postBuild = ''
    make docs
    make -C "$NIX_BUILD_TOP/$sourceRoot/docs" html
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/share/doc/mata/html
    cp -r "$NIX_BUILD_TOP/$sourceRoot/docs/_build/html"/* $out/share/doc/mata/html/
    runHook postInstall
  '';

  doCheck = false;

  meta.description = "Documentation for Mata:" + prev.meta.description;
})
