{ inputs, ... }:
{
  flake.overlays.default = final: _prev: {
    mata = inputs.self.packages.${final.stdenv.hostPlatform.system}.mata;
    mataGcc = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataGcc;
    mataClang = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataClang;
    mataPy = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataPy;
    mataPyGcc = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataPyGcc;
    mataPyClang = inputs.self.packages.${final.stdenv.hostPlatform.system}.mataPyClang;
  };
}
