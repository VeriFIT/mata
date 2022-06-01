Nondeterministic Finite automata
==================================

Structures
----------

.. doxygenstruct:: Vata2::Nfa::Nfa
   :members:

Operations
----------

.. doxygenfunction:: Vata2::Nfa::union_norename(Nfa*, const Nfa&, const Nfa&)
.. doxygenfunction:: Vata2::Nfa::union_norename(const Nfa&, const Nfa&)
.. doxygenfunction:: Vata2::Nfa::intersection(Nfa*, const Nfa&, const Nfa&, ProductMap*)
.. doxygenfunction:: Vata2::Nfa::intersection(const Nfa&, const Nfa&, ProductMap*)
.. doxygenfunction:: Vata2::Nfa::determinize(Nfa*, const Nfa&, SubsetMap*, State*)
.. doxygenfunction:: Vata2::Nfa::revert(Nfa* result, const Nfa& aut)
.. doxygenfunction:: Vata2::Nfa::minimize(Nfa*, const Nfa&, const StringDict&)
.. doxygenfunction:: Vata2::Nfa::is_lang_empty
.. doxygenfunction:: Vata2::Nfa::is_in_lang
.. doxygenfunction:: Vata2::Nfa::is_deterministic
.. doxygenfunction:: Vata2::Nfa::is_complete
.. doxygenfunction:: Vata2::Nfa::accepts_epsilon

