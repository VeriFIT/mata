Nondeterministic Finite automata
==================================

Structures
----------

.. doxygenstruct:: mata::nfa::Nfa
   :members:

Operations
----------

.. doxygenfunction:: mata::nfa::uni(Nfa*, const Nfa&, const Nfa&)
.. doxygenfunction:: mata::nfa::uni(const Nfa&, const Nfa&)
.. doxygenfunction:: mata::nfa::intersection_preserving_epsilon_transitions(Nfa*, const Nfa&, const Nfa&, Symbol, ProductMap*)
.. doxygenfunction:: mata::nfa::intersection_preserving_epsilon_transitions(const Nfa&, const Nfa&, Symbol, ProductMap*)
.. doxygenfunction:: mata::nfa::determinize(Nfa*, const Nfa&, SubsetMap*)
.. doxygenfunction:: mata::nfa::determinize(const Nfa&, SubsetMap*)
.. doxygenfunction:: mata::nfa::revert(Nfa* result, const Nfa& aut)
.. doxygenfunction:: mata::nfa::minimize(Nfa*, const Nfa&)
.. doxygenfunction:: mata::nfa::minimize(const Nfa&)
.. doxygenfunction:: mata::nfa::is_lang_empty
.. doxygenfunction:: mata::nfa::is_in_lang
.. doxygenfunction:: mata::nfa::is_deterministic
.. doxygenfunction:: mata::nfa::is_complete
