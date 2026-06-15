{ inputs, ... }:
{
  perSystem =
    { pkgs, lib, ... }:
    {
      devShells =
        let
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

                inputsFrom = with inputs.self.packages.${pkgs.stdenv.hostPlatform.system}; [
                  mata
                  mataPy
                  mataDocs
                ];

                packages = with inputs.self.packages.${pkgs.stdenv.hostPlatform.system}; [
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
                    sphinx
                    sphinxygen
                    python3Packages.myst-parser
                    python3Packages.breathe
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

                    (pkgs.python3.withPackages (
                      ps: with ps; [
                        pytest
                        pytest-cov
                        papermill
                        jupytext
                      ]
                      #  ++ [
                      #   inputs.self.packages.${pkgs.stdenv.hostPlatform.system}.mataPy
                      # ]
                    ))
                  ]
                  ++ (
                    if pkgs.stdenv.hostPlatform.system != inputs.flake-utils.lib.system.aarch64-darwin then
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
