Nondeterministic Finite automata
================================

An ``mata::nfa::Nfa`` holds the transition relation ``delta`` together with the ``initial`` and
``final`` state sets (both ``mata::utils::SparseSet`` over states), all three inherited from
``mata::Automaton``, plus an optional shared ``alphabet``. All of them appear among the members
below. ``mata::nfa::Delta`` itself is documented in :doc:`core`.

.. doxygenpage:: nfa

Types
-----
.. doxygenfile:: nfa/types.hh

NFA
---
.. doxygenclass:: mata::nfa::Nfa
   :members:

Functions
---------
.. doxygenfile:: nfa/nfa.hh
   :sections: func var typedef enum define

Builder
-------
.. doxygenfile:: nfa/builder.hh

Algorithms
----------
.. doxygenfile:: nfa/algorithms.hh

Plumbing
--------
.. doxygenfile:: nfa/plumbing.hh
