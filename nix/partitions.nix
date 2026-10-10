{ inputs, ... }:
{
  imports = [
    inputs.omniflake.unified.flake-parts.flakeModules.partitions
  ];

  partitionedAttrs = {
    devShells = "dev";
    formatter = "dev";
    checks = "dev";
    apps = "dev";
  };

  # Keeps evaluating `packages` from fetching the development-only flakes.
  partitions.dev.module =
    { inputs, ... }:
    {
      imports = [
        inputs.omniflake.unified.treefmt-nix.flakeModule
        ./shells.nix
        ./treefmt.nix
        ./utils.nix
      ];
    };
}
