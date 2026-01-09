{ inputs, ... }:
{
  perSystem =
    { pkgs, ... }:
    let
      version = inputs.self.shortRev or inputs.self.dirtyShortRev or "dev";

      mataGcc = pkgs.callPackage ./package.nix {
        inherit version;
        stdenv = pkgs.gccStdenv;
      };

      mataClang = pkgs.callPackage ./package.nix {
        inherit version;
        stdenv = pkgs.clangStdenv;
      };

      mata = mataGcc;
    in
    {
      packages = {
        inherit mata mataGcc mataClang;
        default = mata;
      };
    };
}
