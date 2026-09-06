Core
====

The types, the transition relation, and the structural operations that belong to no particular
automaton class. :doc:`nfa` and :doc:`nft` re-export the parts they use, so ``mata::nfa::Delta``
and ``mata::Delta`` currently name the same type.

Types
-----
.. doxygenfile:: core/types.hh

Delta
-----
.. doxygenfile:: core/delta.hh

Automaton
---------
``mata::Automaton`` owns ``delta``, ``initial`` and ``final`` together with the graph-only
operations over them. It is a base class rather than one instantiated directly. Its members are
documented as part of :doc:`nfa` and :doc:`nft`, which is where you will use them.

.. doxygenfile:: core/automaton.hh
