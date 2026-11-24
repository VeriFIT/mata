# Python environment of the development shells, built from uv.lock so that Nix and `uv sync` resolve the same versions.
#  Workspace members are installed editable from $REPO_ROOT, which the shell hook sets.
{
  inputs,
  lib,
  python3,
  callPackage,
}:
let
  inherit (inputs.omniflake.unified) uv2nix pyproject-nix build-system-pkgs;

  workspace = uv2nix.lib.workspace.loadWorkspace { workspaceRoot = ../.; };

  pythonSet =
    (callPackage pyproject-nix.build.packages {
      python = python3;
    }).overrideScope
      (
        lib.composeManyExtensions [
          build-system-pkgs.overlays.wheel
          (workspace.mkPyprojectOverlay { sourcePreference = "wheel"; })
          (workspace.mkEditablePyprojectOverlay { root = "$REPO_ROOT"; })
          (_final: prev: {
            # setup.py needs the whole repository and builds the C++ library with CMake. The editable install only maps
            #  the `libmata` package onto $REPO_ROOT; `just bindings python build` builds its extensions there.
            libmata = prev.libmata.overrideAttrs (old: {
              postPatch = (old.postPatch or "") + ''
                rm setup.py
                substituteInPlace pyproject.toml --replace-fail 'dynamic = ["version"]' 'version = "0.0.0"'
              '';
            });
          })
        ]
      );
in
pythonSet.mkVirtualEnv "mata-dev-env" workspace.deps.all
