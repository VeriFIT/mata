Nondeterministic Finite Transducers
===================================

[!WARNING] Mata provides an experimental (unstable) support for (non-)deterministic finite transducers (NFTs), defined in include/mata/nft/. The provided interface may change.

Types
-----
All typedefs and enums used by algorithms operating on NFTs.

.. doxygenfile:: nft/types.hh
   ::

NFT
---
NFT class and its methods.

.. doxygenfile:: nft/nft.hh

Delta
-----
The delta function of an NFT, which maps states and input symbols to sets of states.

.. doxygenfile:: nft/delta.hh

Builder
-------
Function to build predefined types of NFTs, to create them from regular expressions, and to load them from files.

.. doxygenfile:: nft/builder.hh

Algorithms
----------
Functions to operate on NFTs, such as inclusion and universality checking, etc.

.. doxygenfile:: nft/algorithms.hh

Strings
-------
NFT algorithms and classes used for solving string constraints.

.. doxygenfile:: nft/strings.hh
