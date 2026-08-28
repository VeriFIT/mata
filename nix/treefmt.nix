{
  perSystem =
    {
      config,
      lib,
      pkgs,
      ...
    }:
    {
      treefmt = {
        projectRootFile = ".git/config";
        settings = {
          excludes = [ "3rdparty/*" ];
          allow-missing-formatter = true;
        };
        programs = {
          prettier.enable = true;

          clang-format.enable = true;
          nixfmt.enable = true;
          yamlfmt.enable = true;
          taplo.enable = true;

          ruff-check.enable = true;
          ruff-format = {
            enable = true;
            lineLength = 120;
            priority = 1;
          };
        };
      };

      apps.write-treefmt-toml = {
        type = "app";
        program = toString (
          pkgs.writeShellScript "write-treefmt-toml" ''
            set -euo pipefail
            root="$(${lib.getExe pkgs.git} rev-parse --show-toplevel)"
            {
              echo "# GENERATED FILE. DO NOT EDIT."
              echo "# Source of truth is the \`treefmt\` option in nix/treefmt.nix. Regenerate with: nix run .#write-treefmt-toml"
              echo
              ${pkgs.gnused}/bin/sed -E 's|^command = "/nix/store/[^/]+/bin/([^"]+)"|command = "\1"|' ${config.treefmt.build.configFile}
            } > "$root/.treefmt.toml"
            ${lib.getExe config.treefmt.build.wrapper} --working-dir "$root" "$root/.treefmt.toml"
            echo "Wrote $root/.treefmt.toml"
          ''
        );
      };
    };
}
