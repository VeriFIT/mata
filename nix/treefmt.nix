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

          just = {
            enable = true;
            indentation = "\t";
          };

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
              # Strip the store path of every executable, both in 'command =' and inside inlined formatter scripts, so
              #  that the committed config resolves the tools from PATH.
              ${lib.getExe pkgs.gnused} -E 's|/nix/store/[^/"[:space:]]+/bin/||g' ${config.treefmt.build.configFile}
            } > "$root/.treefmt.toml"
            ${lib.getExe config.treefmt.build.wrapper} --working-dir "$root" "$root/.treefmt.toml"
            echo "Wrote $root/.treefmt.toml"
          ''
        );
      };
    };
}
