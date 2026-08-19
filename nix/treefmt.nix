{
  perSystem =
    { lib, ... }:
    let
      tomlPath = ../.treefmt.toml;
      tomlConfig = if builtins.pathExists tomlPath then fromTOML (builtins.readFile tomlPath) else { };

      # Remove "command" from each formatter since treefmt.programs.* handles that
      filterCommands = lib.mapAttrs (_name: formatter: removeAttrs formatter [ "command" ]);
      settings = tomlConfig // {
        formatter = filterCommands (tomlConfig.formatter or { });
      };
    in
    {
      treefmt = {
        projectRootFile = ".git/config";

        programs.nixfmt.enable = true;
        programs.yamlfmt.enable = true;
        programs.taplo.enable = true;
        programs.prettier.enable = true;
        programs.clang-format.enable = true;

        inherit settings;
      };
    };
}
