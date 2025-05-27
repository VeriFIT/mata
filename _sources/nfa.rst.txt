Nondeterministic Finite automata
==================================

Types
-----
All typedefs and enums used by algorithms operating on NFAs.

.. doxygenfile:: nfa/types.hh

NFA
---
NFA class and its methods.

.. doxygenfile:: nfa/nfa.hh

Delta
-----
The delta function of an NFA, which maps states and input symbols to sets of states.

.. doxygenfile:: nfa/delta.hh

Builder
-------
Function to build predefined types of NFAs, to create them from regular expressions, and to load them from files.

.. doxygenfile:: nfa/builder.hh

Algorithms
----------
Functions to operate on NFAs, such as inclusion and universality checking, minimization, etc.

.. doxygenfile:: nfa/algorithms.hh

Strings
-------
NFA algorithms and classes used for solving string constraints.

.. doxygenfile:: nfa/strings.hh
