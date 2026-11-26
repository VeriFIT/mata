{
  python3Packages,
  mata,
}:
mata.overrideAttrs (prev: {
  pname = "mata-docs";

  postBuild = ''
    make docs
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/share/doc/mata/html
    cp -r "$NIX_BUILD_TOP/$sourceRoot/docs/api"/* $out/share/doc/mata/html/
    runHook postInstall
  '';

  doCheck = false;

  meta.description = "Documentation for Mata:" + prev.meta.description;
})
