{
  perSystem =
    { lib, ... }:
    let
      tomlPath = ../.treefmt.toml;
      tomlConfig =
        if builtins.pathExists tomlPath then builtins.fromTOML (builtins.readFile tomlPath) else { };

      # Remove "command" from each formatter since treefmt.programs.* handles that
      filterCommands = lib.mapAttrs (_name: formatter: builtins.removeAttrs formatter [ "command" ]);
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
        # TODO: Add clang-format or something similar for C++ code.

        inherit settings;
      };
    };
}
