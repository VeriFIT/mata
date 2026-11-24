{ inputs, ... }:
{
  perSystem =
    { pkgs, ... }:
    let
      version = inputs.self.shortRev or inputs.self.dirtyShortRev or "dev";

      mataGcc = pkgs.callPackage ./mata-cpp.nix {
        inherit version;
        stdenv = pkgs.gccStdenv;
      };
      mataClang = pkgs.callPackage ./mata-cpp.nix {
        inherit version;
        stdenv = pkgs.clangStdenv;
      };
      mata = mataGcc;

      mataPyGcc = pkgs.callPackage ./mata-py.nix {
        inherit version;
        mata = mataGcc;
      };
      mataPyClang = pkgs.callPackage ./mata-py.nix {
        inherit version;
        mata = mataClang;
      };
      mataPy = mataPyGcc;

      mataRust = pkgs.callPackage ./mata-rust.nix {
        inherit version;
      };
      mataCli = pkgs.callPackage ./mata-cli.nix { inherit mataRust; };

      mataDocs = pkgs.callPackage ./mata-docs.nix {
        mata = mata;
      };
    in
    {
      packages = {
        inherit
          mata
          mataGcc
          mataClang

          mataPy
          mataPyGcc
          mataPyClang

          mataRust
          mataCli
          mataDocs
          ;
        default = mata;
      };

      checks = {
        inherit
          mataGcc
          mataClang

          mataPyGcc
          mataPyClang

          mataRust
          mataCli
          mataDocs
          ;
      };
    };
}
