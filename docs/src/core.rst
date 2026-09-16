Core
====

The generic machinery: the post templates, the walks and the cursor, the concepts, and the
structural operations that belong to no particular automaton class. Nothing here names a concrete
relation — ``mata::Delta`` is not available from ``core/`` alone, deliberately, so that a third party
building their own relation compiles all of this without inheriting the one this library ships.

That one lives in :ref:`relation`, and :doc:`nfa` and :doc:`nft` re-export it through their own
seams, so ``mata::nfa::Delta`` and ``mata::Delta`` currently name the same type.

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
and ``key_arity`` off it, so the two cannot disagree. It is a base class rather than one
instantiated directly, and its members are documented as part of :doc:`nfa` and :doc:`nft`, which is
where you will use them. The alias every automaton in the tree actually derives from,
``mata::Automaton``, is in :ref:`relation` with the relation it is built on.

.. doxygenfile:: core/automaton.hh

.. _relation:

The shipped relation
--------------------
Everything above is generic. This is the one instantiation the library ships — ``mata::Delta`` and
the aliases around it, depth 2, symbols keying states — kept out of ``core/`` so that "does core
name a concrete relation?" has an answer, and the answer is no.

.. doxygenfile:: relation.hh

Concepts
--------
What a transition relation has to provide for ``mata::AutomatonBase`` to work over it, plus the key
vocabulary the relation itself needs: where a level's reserved keys begin.

.. doxygenfile:: core/concepts.hh

``doxygenfile`` does not render concept declarations, so each one is named explicitly below.

.. doxygenconcept:: mata::WalkableRange
.. doxygenconcept:: mata::TargetSetLike
.. doxygenconcept:: mata::PostEntryLike
.. doxygenconcept:: mata::ReservedKeysLike
.. doxygenconcept:: mata::ReservedKeysAtTail
.. doxygenconcept:: mata::PostLike
.. doxygenconcept:: mata::DeltaLike
.. doxygenconcept:: mata::ExtensibleAlphabet
.. doxygenconcept:: mata::Printable
.. doxygenconcept:: mata::AutomatonWithRuns
