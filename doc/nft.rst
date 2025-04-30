Nondeterministic Finite Transducers
===================================

[!WARNING] Mata provides an experimental (unstable) support for (non-)deterministic finite transducers (NFTs), defined in include/mata/nft/. The provided interface may change.

Constants and Types
-------------------

Symbol
~~~~~~
.. doxygenvariable:: mata::nft::EPSILON
.. doxygenvariable:: mata::nft::DONT_CARE

State
~~~~~
.. doxygentypedef:: mata::nft::State
.. doxygentypedef:: mata::nft::StateSet
.. doxygentypedef:: mata::nft::Limits
.. doxygentypedef:: mata::nft::StateRenaming

Levels
~~~~~~
.. doxygentypedef:: mata::nft::Level
.. doxygenvariable:: mata::nft::DEFAULT_LEVEL
.. doxygenvariable:: mata::nft::DEFAULT_NUM_OF_LEVELS

Parameters
~~~~~~~~~~
.. doxygentypedef:: mata::nft::ParameterMap
.. doxygentypedef:: mata::nft::EpsilonClosureOpt
.. doxygentypedef:: mata::nft::ProductFinalStateCondition
.. doxygenenum:: mata::nft::JumpMode

Structures
----------
.. doxygenclass:: mata::nft::Nft
   :members:
.. doxygenclass:: mata::nft::Levels
   :members:
.. doxygentypedef:: mata::nft::Delta
.. doxygentypedef:: mata::nft::StatePost
.. doxygentypedef:: mata::nft::SymbolPost
.. doxygentypedef:: mata::nft::Move
.. doxygentypedef:: mata::nft::Transition
.. doxygentypedef:: mata::nft::Run



Operations
----------
