{ inputs, ... }:
{
  systems = inputs.nixpkgs.lib.systems.flakeExposed;

  imports = [
    ./overlays.nix
    ./partitions.nix
    ./mata.nix
  ];
}
