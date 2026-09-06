Nondeterministic Finite Transducers
===================================

An ``mata::nft::Nft`` holds the same ``delta``, ``initial`` and ``final`` members as an NFA, all
three inherited from ``mata::Automaton``, plus ``levels``, which gives each state its tape level,
and ``alphabets``, which holds one alphabet per level. All of them appear among the members below.
``mata::nft::Delta`` itself is documented in :doc:`core`.

.. doxygenpage:: nft

Types
-----
.. doxygenfile:: nft/types.hh

NFT
---
.. doxygenclass:: mata::nft::Nft
   :members:

Functions
---------
.. doxygenfile:: nft/nft.hh
   :sections: func var typedef enum define

Builder
-------
.. doxygenfile:: nft/builder.hh

Algorithms
----------
.. doxygenfile:: nft/algorithms.hh

Plumbing
--------
.. doxygenfile:: nft/plumbing.hh
