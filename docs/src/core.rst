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
``mata::AutomatonBase`` owns ``delta``, ``initial`` and ``final`` together with the graph-only
operations over them. It is parameterised by the transition relation and reads ``State``, ``Target``
and ``key_arity`` off it, so the two cannot disagree. ``mata::Automaton`` is the alias for the
depth-2 relation that every automaton in the tree derives from. It is a base class rather than one
instantiated directly, and its members are documented as part of :doc:`nfa` and :doc:`nft`, which is
where you will use them.

.. doxygenfile:: core/automaton.hh

Concepts
--------
What a transition relation has to provide for ``mata::AutomatonBase`` to work over it.

.. doxygenfile:: core/concepts.hh

``doxygenfile`` does not render concept declarations, so each one is named explicitly below.

.. doxygenconcept:: mata::WalkableRange
.. doxygenconcept:: mata::TargetSetLike
.. doxygenconcept:: mata::PostEntryLike
.. doxygenconcept:: mata::PostLike
.. doxygenconcept:: mata::DeltaLike
.. doxygenconcept:: mata::AutomatonWithRuns
