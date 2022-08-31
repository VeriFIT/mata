Nondeterministic Finite automata
==================================

Structures
----------

.. doxygenstruct:: Mata::Nfa::Nfa
   :members:

Operations
----------

.. doxygenfunction:: Mata::Nfa::uni(Nfa*, const Nfa&, const Nfa&)
.. doxygenfunction:: Mata::Nfa::uni(const Nfa&, const Nfa&)
.. doxygenfunction:: Mata::Nfa::intersection(Nfa*, const Nfa&, const Nfa&, Symbol, ProductMap*)
.. doxygenfunction:: Mata::Nfa::intersection(const Nfa&, const Nfa&, Symbol, ProductMap*)
.. doxygenfunction:: Mata::Nfa::determinize(Nfa*, const Nfa&, SubsetMap*)
.. doxygenfunction:: Mata::Nfa::determinize(const Nfa&, SubsetMap*)
.. doxygenfunction:: Mata::Nfa::revert(Nfa* result, const Nfa& aut)
.. doxygenfunction:: Mata::Nfa::minimize(Nfa*, const Nfa&)
.. doxygenfunction:: Mata::Nfa::minimize(const Nfa&)
.. doxygenfunction:: Mata::Nfa::is_lang_empty
.. doxygenfunction:: Mata::Nfa::is_in_lang
.. doxygenfunction:: Mata::Nfa::is_deterministic
.. doxygenfunction:: Mata::Nfa::is_complete

