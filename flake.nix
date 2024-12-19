{
  description = "A Nix flake for Mata: A Fast and Simple Automata Library";

  inputs = {
    # Inputs
    # https://nixos.org/manual/nix/unstable/command-ref/new-cli/nix3-flake.html#flake-inputs

    # The flake in the current directory.
    # inputs.currentDir.url = ".";

    # A flake in some other directory.
    # inputs.otherDir.url = "/home/alice/src/patchelf";

    # A flake in some absolute path
    # inputs.otherDir.url = "path:/home/alice/src/patchelf";

    nixpkgs.url = "nixpkgs/nixos-unstable";

    # Flake utils.
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = inputs@{ self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        packageName = "mata";

        buildInputPackages = with pkgs; [
            git
            coreutils
            gnumake
            gcc
            clang-tools
            clang
            gdb
            lldb
            catch2_3
            cmake
            cppcheck
            doxygen
            lcov
            libgcc
            gcovr
            graphviz
        ];

        pkgs = nixpkgs.legacyPackages.${system};

        devShellEnv = { compiler ? "gcc" }:
          pkgs.mkShell.override {
            # Override stdenv in order to change compiler:
            stdenv =
              if compiler == "clang" then
                pkgs.clangStdenv
              else if compiler == "gcc" then
                pkgs.gccStdenv
              else
                throw "Invalid compiler value"
              ;
          } {
            LD_LIBRARY_PATH = nixpkgs.lib.makeLibraryPath [ pkgs.stdenv.cc.cc ];

            buildInputs = with pkgs; [
              just
              valgrind
            ]
              ++ buildInputPackages
              ++ (if system != flake-utils.lib.system.aarch64-darwin then [ gdb ] else [])
            ;
          };
      in {
        # Utilized by `nix flake check`
        #checks.x86_64-linux.test = c-hello.checks.x86_64-linux.test;

        packages.${packageName} = pkgs.stdenvNoCC.mkDerivation rec {
            name = "mata";
            src = nixpkgs.lib.fileset.toSource {
              root = ./.;
              fileset = nixpkgs.lib.fileset.fileFilter (file: ! nixpkgs.lib.hasPrefix "/build/" file.name) ./.;
            };
            buildInputs = with pkgs; [
            ] ++ buildInputPackages;
            phases = ["unpackPhase" "buildPhase" "installPhase"];
            buildPhase = ''
              # export PATH="${pkgs.lib.makeBinPath buildInputs}";
              # mkdir -p .cache/texmf-var
              # env TEXMFHOME=.cache TEXMFVAR=.cache/texmf-var \
                # latexmk -interaction=nonstopmode -pdf -lualatex \
                # main.tex
              # make debug
              # rm CmakeCache.txt
              make release

              # make test
            '';
            installPhase = ''
              mkdir -p $out/lib
              mkdir -p $out/include/mata/{cudd,simlib}
              # cp build/tests/tests $out/bin/tests
              cp build/src/libmata.a $out/lib/libmata.a
              cp -r include/* $out/include/
              cp -r 3rdparty/simlib/include/* $out/include/
              # cp -r 3rdparty/re2/* $out/include/mata/re2/
              cp -r 3rdparty/cudd/include/* $out/include/
            '';
          };

        # Utilized by `nix build .`
        defaultPackage = self.packages.${system}.${packageName};


        # Utilized by `nix build`
        packages.x86_64-linux.mata = self.packages.x86_64-linux.${packageName};
        packages.x86_64-linux.default = self.packages.x86_64-linux.${packageName};

        # Utilized by `nix run .#<name>`
        #apps.x86_64-linux.hello = {
        #  type = "app";
        #  program = c-hello.packages.x86_64-linux.hello;
        #};

        ## Utilized by `nix bundle -- .#<name>` (should be a .drv input, not program path?)
        #bundlers.x86_64-linux.example = nix-bundle.bundlers.x86_64-linux.toArx;

        ## Utilized by `nix bundle -- .#<name>`
        #defaultBundler.x86_64-linux = self.bundlers.x86_64-linux.example;

        ## Utilized by `nix run . -- <args?>`
        #defaultApp.x86_64-linux = self.apps.x86_64-linux.hello;

        ## Utilized for nixpkgs packages, also utilized by `nix build .#<name>`
        #legacyPackages.x86_64-linux.hello = c-hello.defaultPackage.x86_64-linux;

        ## Default overlay, for use in dependent flakes
        #overlay = final: prev: { };

        ## # Same idea as overlay but a list or attrset of them.
        #overlays = { exampleOverlay = self.overlay; };


        ## Utilized by `nix develop`
        #devShell.${system} = rust-web-server.devShell.x86_64-linux;

        ## Utilized by `nix develop .#<name>`
        #devShells.x86_64-linux.example = self.devShell.x86_64-linux;

        devShells = {
          clang = devShellEnv { compiler = "clang";};
          gcc = devShellEnv {compiler = "gcc";};
          default = devShellEnv {compiler = "gcc";};
        };

        ## Utilized by `nix flake init -t <flake>`
        #defaultTemplate = {
        #  path = c-hello;
        #  description = "template description";
        #};

        ## Utilized by `nix flake init -t <flake>#<name>`
        #templates.example = self.defaultTemplate;
      });
}
