"""Basic tests for utility package and sanity checks"""

import libmata.nfa.nfa as mata_nfa

__author__ = "Tomas Fiedor"


def test_trans():
    """Tests that the python interpreter can be obtained in reasonable format"""
    lhs = mata_nfa.Transition(0, 0, 0)
    rhs = mata_nfa.Transition(0, 1, 1)
    chs = mata_nfa.Transition(0, 0, 0)

    assert lhs != rhs
    assert lhs == chs


def test_move():
    a = mata_nfa.SymbolPost(0, [0, 1])
    b = mata_nfa.SymbolPost(1, [1])
    c = mata_nfa.SymbolPost(0, [0])
    d = mata_nfa.SymbolPost(1, [])
    assert a.symbol == 0
    assert a.targets == [0, 1]

    # Test getter and setters
    a.symbol = 1
    assert a.symbol != 0
    assert a.symbol == 1
    a.targets = []
    assert a.targets == []

    # Test comparision
    assert not a < b
    assert not b < a
    assert c < a
    assert not a < c
    assert not d < b
    assert not b < d


def test_transition_operations(prepare_automaton_a):
    nfa = mata_nfa.Nfa(10)
    nfa.add_transition(3, ord("c"), 4)
    assert nfa.has_transition(3, ord("c"), 4)
    trans = mata_nfa.Transition(4, ord("c"), 5)
    nfa.add_transition_object(trans)
    assert nfa.has_transition(trans.source, trans.symbol, trans.target)

    nfa.remove_trans_raw(3, ord("c"), 4)
    assert not nfa.has_transition(3, ord("c"), 4)
    nfa.remove_trans(trans)
    assert not nfa.has_transition(trans.source, trans.symbol, trans.target)

    nfa = prepare_automaton_a()

    expected_trans = [
        mata_nfa.Transition(3, ord("b"), 9),
        mata_nfa.Transition(5, ord("c"), 9),
        mata_nfa.Transition(9, ord("a"), 9),
    ]

    trans = nfa.get_transitions_to_state(9)
    assert trans == expected_trans

    trans = nfa.get_trans_as_sequence()
    expected_trans = [
        mata_nfa.Transition(1, ord("a"), 3),
        mata_nfa.Transition(1, ord("a"), 10),
        mata_nfa.Transition(1, ord("b"), 7),
        mata_nfa.Transition(3, ord("a"), 7),
        mata_nfa.Transition(3, ord("b"), 9),
        mata_nfa.Transition(5, ord("a"), 5),
        mata_nfa.Transition(5, ord("c"), 9),
        mata_nfa.Transition(7, ord("a"), 3),
        mata_nfa.Transition(7, ord("a"), 5),
        mata_nfa.Transition(7, ord("b"), 1),
        mata_nfa.Transition(7, ord("c"), 3),
        mata_nfa.Transition(9, ord("a"), 9),
        mata_nfa.Transition(10, ord("a"), 7),
        mata_nfa.Transition(10, ord("b"), 7),
        mata_nfa.Transition(10, ord("c"), 7),
    ]
    assert trans == expected_trans

    trans = nfa.get_trans_from_state_as_sequence(1)
    expected_trans = [
        mata_nfa.Transition(1, ord("a"), 3),
        mata_nfa.Transition(1, ord("a"), 10),
        mata_nfa.Transition(1, ord("b"), 7),
    ]
    assert trans == expected_trans


def test_transitions():
    """Test adding transitions to automaton"""
    lhs = mata_nfa.Nfa(3)
    t1 = mata_nfa.Transition(0, 0, 0)
    t2 = mata_nfa.Transition(0, 1, 0)
    t3 = mata_nfa.Transition(1, 1, 1)
    t4 = mata_nfa.Transition(2, 2, 2)

    # Test adding transition.
    assert lhs.get_num_of_transitions() == 0
    lhs.add_transition(0, 0, 0)
    assert lhs.get_num_of_transitions() == 1
    assert lhs.has_transition(t1.source, t1.symbol, t1.target)

    lhs.add_transition_object(t2)
    assert lhs.get_num_of_transitions() == 2
    assert lhs.has_transition(t2.source, t2.symbol, t2.target)

    # Test adding add-hoc transition.
    lhs.add_transition(1, 1, 1)
    assert lhs.get_num_of_transitions() == 3
    assert lhs.has_transition(t3.source, t3.symbol, t3.target)
    assert not lhs.has_transition(2, 2, 2)
    lhs.add_transition_object(t4)
    assert lhs.get_num_of_transitions() == 4
    assert lhs.has_transition(2, 2, 2)

    # Test that transitions are not duplicated.
    lhs.add_transition_object(t3)
    assert [t for t in lhs.iterate()] == [t1, t2, t3, t4]


def test_delta_add_remove_contains():
    """Tests basic Delta operations mirroring the C++ interface, e.g. nfa.delta.add()."""
    nfa = mata_nfa.Nfa(5)
    assert isinstance(nfa.delta, mata_nfa.Delta)
    assert nfa.delta.empty()
    assert len(nfa.delta) == 0

    nfa.delta.add(0, ord("a"), 1)
    assert not nfa.delta.empty()
    assert len(nfa.delta) == 1
    assert nfa.delta.contains(0, ord("a"), 1)
    assert (0, ord("a"), 1) in nfa.delta

    tr = mata_nfa.Transition(1, ord("b"), 2)
    nfa.delta.add(tr)
    assert nfa.delta.contains(tr)
    assert tr in nfa.delta

    # Adding a transition to multiple targets at once.
    nfa.delta.add(2, ord("c"), {3, 4})
    assert nfa.delta.contains(2, ord("c"), 3)
    assert nfa.delta.contains(2, ord("c"), 4)
    assert len(nfa.delta) == 4

    nfa.delta.remove(0, ord("a"), 1)
    assert not nfa.delta.contains(0, ord("a"), 1)
    nfa.delta.remove(tr)
    assert not nfa.delta.contains(tr)


def test_delta_views(prepare_automaton_a):
    """Tests that Delta exposes the transitions similarly to the corresponding Nfa methods."""
    nfa = prepare_automaton_a()

    assert nfa.delta.num_of_transitions() == nfa.get_num_of_transitions()
    assert list(nfa.delta) == nfa.get_trans_as_sequence()
    assert nfa.delta.get_transitions_from(1) == nfa.get_trans_from_state_as_sequence(1)
    assert nfa.delta.get_transitions_to(9) == nfa.get_transitions_to_state(9)
    assert nfa.delta.get_used_symbols() == nfa.get_symbols()
    assert nfa.delta[1] == nfa.get_transitions_from_state(1)


def test_delta_survives_nfa_deletion():
    """Tests that a kept Delta reference stays valid after the owning Nfa is garbage collected."""
    nfa = mata_nfa.Nfa(2)
    nfa.delta.add(0, 0, 1)
    delta = nfa.delta
    del nfa
    assert len(delta) == 1
    assert delta.contains(0, 0, 1)


def test_delta_equality():
    lhs = mata_nfa.Nfa(2)
    lhs.add_transition(0, 0, 1)
    rhs = mata_nfa.Nfa(2)
    rhs.add_transition(0, 0, 1)

    assert lhs.delta == rhs.delta
    rhs.delta.add(1, 1, 1)
    assert lhs.delta != rhs.delta
