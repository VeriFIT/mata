{ inputs, ... }:
{
  flake.overlays.default = final: _prev: {
    mata = inputs.self.packages.${final.stdenv.hostPlatform.system}.mata;
    mataGcc = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataGcc;
    mataClang = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataClang;
  };
}
