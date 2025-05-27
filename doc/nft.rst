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
Concatenation
~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::concatenate

Union
~~~~~
.. doxygenfunction:: mata::nft::union_nondet

Intersection
~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::intersection(const Nft&, const Nft&, std::unordered_map<std::pair<State, State>, State>*, JumpMode, State, State)

Composition
~~~~~~~~~~~
.. doxygenfunction:: mata::nft::compose(const Nft&, const Nft&, const utils::OrdVector<Level>&, const utils::OrdVector<Level>&, bool, JumpMode)
.. doxygenfunction:: mata::nft::compose(const Nft&, const Nft&, Level, Level, bool, JumpMode)

Projection
~~~~~~~~~~
.. doxygenfunction:: mata::nft::project_out(const Nft&, const utils::OrdVector<Level>&, JumpMode)
.. doxygenfunction:: mata::nft::project_out(const Nft&, Level, JumpMode)
.. doxygenfunction:: mata::nft::project_to(const Nft&, const utils::OrdVector<Level>&, JumpMode)
.. doxygenfunction:: mata::nft::project_to(const Nft&, Level, JumpMode)

Determinization
~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::determinize

Complementation
~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::complement(const Nft&, const Alphabet&, const ParameterMap&)
.. doxygenfunction:: mata::nft::complement(const Nft&, const utils::OrdVector<Symbol>&, const ParameterMap&)

Minimization/Reduction
~~~~~~~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::minimize
.. doxygenfunction:: mata::nft::reduce

Revert
~~~~~~
.. doxygenfunction:: mata::nft::revert

Invert Levels
~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::invert_levels

Remove Epsilon
~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::remove_epsilon(const Nft&, Symbol)

Insert Levels
~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::insert_levels
.. doxygenfunction:: mata::nft::insert_level

Test Inclusion
~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::is_included(const Nft&, const Nft&, Run*, const Alphabet*, JumpMode, const ParameterMap&)
.. doxygenfunction:: mata::nft::is_included(const Nft&, const Nft&, const Alphabet* const, JumpMode, const ParameterMap&)

Test Equivalence
~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nft::are_equivalent(const Nft&, const Nft&, const Alphabet*, JumpMode, const ParameterMap&)
.. doxygenfunction:: mata::nft::are_equivalent(const Nft&, const Nft&, JumpMode, const ParameterMap&)
