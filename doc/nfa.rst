Nondeterministic Finite automata
==================================

Structures
----------

.. doxygenstruct:: Mata::Nfa::Nfa
   :members:

Operations
----------

.. doxygenfunction:: Mata2::Nfa::union_norename(Nfa*, const Nfa&, const Nfa&)
.. doxygenfunction:: Mata2::Nfa::union_norename(const Nfa&, const Nfa&)
.. doxygenfunction:: Mata2::Nfa::intersection(Nfa*, const Nfa&, const Nfa&, ProductMap*)
.. doxygenfunction:: Mata2::Nfa::intersection(const Nfa&, const Nfa&, ProductMap*)
.. doxygenfunction:: Mata2::Nfa::determinize(Nfa*, const Nfa&, SubsetMap*, State*)
.. doxygenfunction:: Mata2::Nfa::revert(Nfa* result, const Nfa& aut)
.. doxygenfunction:: Mata2::Nfa::minimize(Nfa*, const Nfa&, const StringDict&)
.. doxygenfunction:: Mata2::Nfa::is_lang_empty
.. doxygenfunction:: Mata2::Nfa::is_in_lang
.. doxygenfunction:: Mata2::Nfa::is_deterministic
.. doxygenfunction:: Mata2::Nfa::is_complete
.. doxygenfunction:: Mata2::Nfa::accepts_epsilon

