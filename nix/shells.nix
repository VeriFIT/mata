{ inputs, ... }:
{
  perSystem =
    { pkgs, lib, ... }:
    {
      devShells =
        let
          pythonEnv = pkgs.callPackage ./python-env.nix { inherit inputs; };
          devShellEnv =
            {
              compiler ? "gcc",
            }:
            pkgs.mkShell.override
              {
                # Override stdenv in order to change compiler.
                stdenv =
                  if compiler == "clang" then
                    pkgs.clangStdenv
                  else if compiler == "gcc" then
                    pkgs.gccStdenv
                  else
                    throw "Invalid compiler value";
              }
              {
                name = "mata-${compiler}-dev";

                LD_LIBRARY_PATH = lib.makeLibraryPath [ pkgs.stdenv.cc.cc ];

                env = {
                  # `uv run` uses the Nix-built `pythonEnv` as the project environment. It is read-only, so uv must not
                  #  sync it; re-entering the shell installs a changed uv.lock.
                  UV_PROJECT_ENVIRONMENT = "${pythonEnv}";
                  UV_NO_SYNC = "1";
                  UV_PYTHON = pkgs.python3.interpreter;
                  UV_PYTHON_DOWNLOADS = "never";
                };

                # The Python packages of the Nix build inputs would shadow the locked ones in `pythonEnv`.
                shellHook = ''
                  unset PYTHONPATH
                  REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || jj root)"
                  export REPO_ROOT

                  # Prepend a clangd allowed to query the dev shells' Nix cc-wrappers for system include
                  # paths. Stock clangd refuses to execute the wrapper named in 'compile_commands.json',
                  # so diagnostics in editors cannot resolve the compiler's bundled headers
                  # (#include <memory>, etc.). Both wrappers are allowed, so a build directory configured
                  # in either shell works; nothing else is, since clangd executes whatever allowed driver
                  # any opened 'compile_commands.json' names. Both 'clang' (in the stdenv of the 'clang'
                  # dev shell, ahead of buildInputs) and 'clang-tools' ship their own 'clangd', so an
                  # entry in buildInputs cannot win; prepending in shellHook does.
                  export PATH=${
                    lib.makeBinPath [
                      (pkgs.writeShellScriptBin "clangd" ''
                        exec ${lib.getExe' pkgs.clang-tools "clangd"} --query-driver=${
                          lib.escapeShellArg (
                            lib.concatMapStringsSep "," (cc: "${cc}/bin/*") [
                              pkgs.gccStdenv.cc
                              pkgs.clangStdenv.cc
                            ]
                          )
                        } "$@"
                      '')
                    ]
                  }:$PATH
                '';

                inputsFrom = with inputs.self.packages.${pkgs.stdenv.hostPlatform.system}; [
                  mata
                ];

                # Ahead of `buildInputs` on PATH, whose tools bring their own bare `python3`.
                packages = with inputs.self.packages.${pkgs.stdenv.hostPlatform.system}; [
                  pythonEnv
                  # mata
                  # mataPy
                ];

                buildInputs =
                  with pkgs;
                  [
                    lldb
                    gcovr
                    just
                    valgrind

                    # git
                    # coreutils
                    # gnumake
                    pkgs.gcc
                    pkgs.clang

                    # TODO: Enable building documentation through Nix.
                    sphinxygen
                    # lcov
                    # libgcc

                    # Formatting and linting.
                    clang-tools
                    cppcheck
                    nixd
                    nixfmt
                    yamlfmt
                    taplo
                    prettier
                    prettierd

                    uv
                  ]
                  ++ (
                    if
                      pkgs.stdenv.hostPlatform.system != inputs.omniflake.unified.flake-utils.lib.system.aarch64-darwin
                    then
                      [
                        gdb
                      ]
                    else
                      [ ]
                  );
              };
          clang = devShellEnv { compiler = "clang"; };
          gcc = devShellEnv { compiler = "gcc"; };
        in
        {
          inherit gcc clang;
          default = gcc;
        };
    };
}
