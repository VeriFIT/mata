# flake-compat at the revision `omniflake.unified.flake-compat` resolves to. That attribute needs flakes to evaluate, so the
#  pin is read from omniflake's index, with omniflake itself fetched at the revision in flake.lock.
let
  fetchGitHub =
    locked:
    fetchTarball {
      url = "https://github.com/${locked.owner}/${locked.repo}/archive/${locked.rev}.tar.gz";
      sha256 = locked.narHash;
    };
  lock = builtins.fromJSON (builtins.readFile ../flake.lock);
  omniflake = fetchGitHub lock.nodes.${lock.nodes.${lock.root}.inputs.omniflake}.locked;
  index = builtins.fromJSON (builtins.readFile "${omniflake}/index.json");
in
import (fetchGitHub index.flake-compat.locked) { src = ../.; }
