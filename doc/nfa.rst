Nondeterministic Finite automata
==================================

Constants and Types
-------------------

Symbol
~~~~~~
.. doxygenvariable:: mata::nfa::Limits::min_symbol
.. doxygenvariable:: mata::nfa::Limits::max_symbol
.. doxygenvariable:: mata::nfa::EPSILON

State
~~~~~
.. doxygentypedef:: mata::nfa::State
.. doxygentypedef:: mata::nfa::StateSet
.. doxygenvariable:: mata::nfa::Limits::min_state
.. doxygenvariable:: mata::nfa::Limits::max_state
.. doxygentypedef:: mata::nfa::StateRenaming

Parameters
~~~~~~~~~~
.. doxygentypedef:: mata::nfa::ParameterMap
.. doxygenenum:: mata::nfa::EpsilonClosureOpt
.. doxygenenum:: mata::nfa::ProductFinalStateCondition

Structures
----------
.. doxygenclass:: mata::nfa::Nfa
   :members:
.. doxygenclass:: mata::nfa::Delta
   :members:
.. doxygenclass:: mata::nfa::StatePost
   :members:
.. doxygenclass:: mata::nfa::SymbolPost
   :members:
.. doxygenclass:: mata::nfa::Move
   :members:
.. doxygenstruct:: mata::nfa::Transition
   :members:
.. doxygenstruct:: mata::nfa::Run
   :members:

Operations
----------

Concatenation
~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::concatenate

Union
~~~~~
.. doxygenfunction:: mata::nfa::union_nondet
.. doxygenfunction:: mata::nfa::union_det_complete

Intersection/Product
~~~~~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::product
.. doxygenfunction:: mata::nfa::intersection

Determinization
~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::determinize

Complementation
~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::complement(const Nfa&, const Alphabet&, const ParameterMap&)
.. doxygenfunction:: mata::nfa::complement(const Nfa&, const utils::OrdVector<Symbol>&, const ParameterMap&)
.. doxygenfunction:: mata::nfa::algorithms::complement_classical(const Nfa&, const mata::utils::OrdVector<Symbol>&)
.. doxygenfunction:: mata::nfa::algorithms::complement_brzozowski(const Nfa&, const mata::utils::OrdVector<Symbol>&)

Minimization/Reduction
~~~~~~~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::minimize
.. doxygenfunction:: mata::nfa::algorithms::minimize_brzozowski
.. doxygenfunction:: mata::nfa::algorithms::minimize_hopcroft
.. doxygenfunction:: mata::nfa::reduce
.. doxygenfunction:: mata::nfa::algorithms::reduce_simulation
.. doxygenfunction:: mata::nfa::algorithms::reduce_residual
.. doxygenfunction:: mata::nfa::algorithms::reduce_residual_with
.. doxygenfunction:: mata::nfa::algorithms::reduce_residual_after

Revert
~~~~~~
.. doxygenfunction:: mata::nfa::revert

Remove Epsilon
~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::remove_epsilon(const Nfa&, Symbol)

Language Difference
~~~~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::lang_difference
.. doxygenfunction:: mata::nfa::get_word_from_lang_difference

Test Inclusion
~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::is_included(const Nfa&, const Nfa&, Run*, const Alphabet*, const ParameterMap&)
.. doxygenfunction:: mata::nfa::is_included(const Nfa&, const Nfa&, const Alphabet *const, const ParameterMap&)
.. doxygenfunction:: mata::nfa::algorithms::is_included_naive
.. doxygenfunction:: mata::nfa::algorithms::is_included_antichains

Test Equivalence
~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::are_equivalent(const Nfa&, const Nfa&, const Alphabet*, const ParameterMap&)
.. doxygenfunction:: mata::nfa::are_equivalent(const Nfa&, const Nfa&, const ParameterMap&)

Test Universality
~~~~~~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::algorithms::is_universal_naive
.. doxygenfunction:: mata::nfa::algorithms::is_universal_antichains

Get Alphabet
~~~~~~~~~~~~
.. doxygenfunction:: mata::nfa::get_symbols_to_work_with
.. doxygenfunction:: mata::nfa::create_alphabet(const std::vector<std::reference_wrapper<const Nfa>>&)
.. doxygenfunction:: mata::nfa::create_alphabet(const std::vector<std::reference_wrapper<Nfa>>&)
.. doxygenfunction:: mata::nfa::create_alphabet(const std::vector<Nfa*>&)
.. doxygenfunction:: mata::nfa::create_alphabet(const std::vector<const Nfa*>&)
