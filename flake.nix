{
  description = "A Nix flake for Mata: A fast and simple automata library";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    omniflake = {
      url = "github:fzakaria/omniflake";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = inputs: inputs.omniflake.unified.flake-parts.lib.mkFlake { inherit inputs; } ./nix;
}
