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
"""Tests for bulk transition import (issue #796)."""

import libmata.nfa.nfa as mata_nfa
import pytest


def test_add_transitions_columnar_basic():
    """Test basic columnar bulk import."""
    nfa = mata_nfa.Nfa(5)
    sources = [0, 1, 2, 1, 2]
    symbols = [ord('a'), ord('b'), ord('c'), ord('d'), ord('e')]
    targets = [1, 2, 3, 4, 0]
    
    nfa.delta.add_transitions(sources, symbols, targets)
    
    assert nfa.delta.num_of_transitions() == 5
    assert nfa.delta.contains(0, ord('a'), 1)
    assert nfa.delta.contains(1, ord('b'), 2)
    assert nfa.delta.contains(2, ord('c'), 3)
    assert nfa.delta.contains(1, ord('d'), 4)
    assert nfa.delta.contains(2, ord('e'), 0)


def test_add_transitions_from_triples():
    """Test triple-iterable bulk import."""
    nfa = mata_nfa.Nfa(5)
    triples = [
        (0, ord('a'), 1),
        (1, ord('b'), 2),
        (2, ord('c'), 3),
    ]
    
    nfa.delta.add_transitions_from(triples)
    
    assert nfa.delta.num_of_transitions() == 3
    assert nfa.delta.contains(0, ord('a'), 1)
    assert nfa.delta.contains(1, ord('b'), 2)
    assert nfa.delta.contains(2, ord('c'), 3)


def test_add_transitions_duplicates_collapse():
    """Test that duplicates are collapsed in bulk import."""
    nfa = mata_nfa.Nfa(5)
    sources = [0, 0, 0, 1, 1]
    symbols = [ord('a'), ord('a'), ord('a'), ord('b'), ord('b')]
    targets = [1, 1, 1, 2, 2]
    
    nfa.delta.add_transitions(sources, symbols, targets)
    
    # Duplicates should collapse to 2 unique transitions
    assert nfa.delta.num_of_transitions() == 2
    assert nfa.delta.contains(0, ord('a'), 1)
    assert nfa.delta.contains(1, ord('b'), 2)


def test_add_transitions_max_state_allocated():
    """Test that states up to the largest ID are allocated."""
    nfa = mata_nfa.Nfa(0)
    sources = [0, 5, 3]
    symbols = [ord('a'), ord('b'), ord('c')]
    targets = [2, 7, 4]
    
    nfa.delta.add_transitions(sources, symbols, targets)
    
    # Should allocate up to state 7
    assert nfa.delta.num_of_states() == 8


def test_add_transitions_merges_nonempty_delta():
    """Test that import into a non-empty delta merges."""
    nfa = mata_nfa.Nfa(5)
    
    # Add some transitions first
    nfa.delta.add(0, ord('x'), 1)
    assert nfa.delta.num_of_transitions() == 1
    
    # Bulk add more
    nfa.delta.add_transitions([1, 2], [ord('a'), ord('b')], [2, 3])
    
    # Should merge to 3 total transitions
    assert nfa.delta.num_of_transitions() == 3
    assert nfa.delta.contains(0, ord('x'), 1)
    assert nfa.delta.contains(1, ord('a'), 2)
    assert nfa.delta.contains(2, ord('b'), 3)


def test_add_transitions_length_mismatch_raises():
    """Test that length mismatch raises without modifying automaton."""
    nfa = mata_nfa.Nfa(5)
    nfa.delta.add(0, ord('x'), 1)
    initial_trans = list(nfa.delta)
    
    with pytest.raises(ValueError, match="equal length"):
        nfa.delta.add_transitions([0, 1], [ord('a')], [1, 2])
    
    # Automaton should be unmodified
    assert list(nfa.delta) == initial_trans


def test_add_transitions_invalid_source_raises():
    """Test that invalid source state raises without modifying automaton."""
    nfa = mata_nfa.Nfa(5)
    nfa.delta.add(0, ord('x'), 1)
    initial_trans = list(nfa.delta)
    
    with pytest.raises((TypeError, OverflowError)):
        nfa.delta.add_transitions(["not_a_number", 1], [ord('a'), ord('b')], [1, 2])
    
    # Automaton should be unmodified
    assert list(nfa.delta) == initial_trans


def test_add_transitions_invalid_symbol_raises():
    """Test that invalid symbol raises without modifying automaton."""
    nfa = mata_nfa.Nfa(5)
    nfa.delta.add(0, ord('x'), 1)
    initial_trans = list(nfa.delta)
    
    with pytest.raises((TypeError, OverflowError)):
        nfa.delta.add_transitions([0, 1], ["not_a_number", ord('b')], [1, 2])
    
    # Automaton should be unmodified
    assert list(nfa.delta) == initial_trans


def test_add_transitions_invalid_target_raises():
    """Test that invalid target state raises without modifying automaton."""
    nfa = mata_nfa.Nfa(5)
    nfa.delta.add(0, ord('x'), 1)
    initial_trans = list(nfa.delta)
    
    with pytest.raises((TypeError, OverflowError)):
        nfa.delta.add_transitions([0, 1], [ord('a'), ord('b')], [1, "not_a_number"])
    
    # Automaton should be unmodified
    assert list(nfa.delta) == initial_trans


def test_add_transitions_from_bad_triple_arity_raises():
    """Test that bad triple arity raises without modifying automaton."""
    nfa = mata_nfa.Nfa(5)
    nfa.delta.add(0, ord('x'), 1)
    initial_trans = list(nfa.delta)
    
    with pytest.raises((ValueError, TypeError)):
        nfa.delta.add_transitions_from([(0, ord('a')), (1, ord('b'), 2)])
    
    # Automaton should be unmodified
    assert list(nfa.delta) == initial_trans


def test_add_transitions_from_invalid_value_raises():
    """Test that invalid values in triples raise without modifying automaton."""
    nfa = mata_nfa.Nfa(5)
    nfa.delta.add(0, ord('x'), 1)
    initial_trans = list(nfa.delta)
    
    with pytest.raises((TypeError, OverflowError, ValueError)):
        nfa.delta.add_transitions_from([(0, ord('a'), 1), ("bad", ord('b'), 2)])
    
    # Automaton should be unmodified
    assert list(nfa.delta) == initial_trans


def test_iter_transitions_from_generator():
    """Test that iter_transitions_from is a generator."""
    nfa = mata_nfa.Nfa(5)
    nfa.add_transition(0, ord('a'), 1)
    nfa.add_transition(0, ord('b'), 2)
    nfa.add_transition(0, ord('c'), 3)
    nfa.add_transition(1, ord('d'), 2)
    
    # iter_transitions_from should yield transitions for state 0
    gen = nfa.iter_transitions_from(0)
    trans = list(gen)
    
    assert len(trans) == 3
    assert all(t.source == 0 for t in trans)
    assert {(t.symbol, t.target) for t in trans} == {
        (ord('a'), 1), (ord('b'), 2), (ord('c'), 3)
    }


def test_get_trans_from_state_as_sequence_via_generator():
    """Test that get_trans_from_state_as_sequence materializes iter_transitions_from."""
    nfa = mata_nfa.Nfa(5)
    nfa.add_transition(0, ord('a'), 1)
    nfa.add_transition(0, ord('b'), 2)
    nfa.add_transition(1, ord('c'), 3)
    
    # Should return a list
    result = nfa.get_trans_from_state_as_sequence(0)
    assert isinstance(result, list)
    assert len(result) == 2
    assert all(t.source == 0 for t in result)


def test_lifetime_iterate_survives_nfa_deletion():
    """Test that an iterate generator keeps the automaton alive."""
    nfa = mata_nfa.Nfa(3)
    nfa.add_transition(0, ord('a'), 1)
    nfa.add_transition(1, ord('b'), 2)
    
    it = nfa.iterate()
    del nfa  # Delete the owning NFA
    
    # Generator should still be able to iterate
    trans = list(it)
    assert len(trans) == 2
    assert trans[0].source == 0
    assert trans[1].source == 1


def test_lifetime_iter_transitions_from_survives_nfa_deletion():
    """Test that an iter_transitions_from generator keeps the automaton alive."""
    nfa = mata_nfa.Nfa(3)
    nfa.add_transition(0, ord('a'), 1)
    nfa.add_transition(0, ord('b'), 2)
    
    it = nfa.iter_transitions_from(0)
    del nfa  # Delete the owning NFA
    
    # Generator should still be able to iterate
    trans = list(it)
    assert len(trans) == 2
    assert all(t.source == 0 for t in trans)


def test_lifetime_delta_survives_nfa_deletion_bulk_add():
    """Test that a Delta view keeps automaton alive during bulk add."""
    nfa = mata_nfa.Nfa(5)
    delta = nfa.delta
    del nfa  # Delete the owning NFA
    
    # Delta should still work for bulk add
    delta.add_transitions([0, 1], [ord('a'), ord('b')], [1, 2])
    
    assert delta.num_of_transitions() == 2
    assert delta.contains(0, ord('a'), 1)
    assert delta.contains(1, ord('b'), 2)


def test_lifetime_transition_owns_heap():
    """Test that a Transition owns its own heap object."""
    trans = mata_nfa.Transition(0, ord('a'), 1)
    assert trans.source == 0
    assert trans.symbol == ord('a')
    assert trans.target == 1
    # Transition is independent of any automaton


def test_bulk_equals_per_transition():
    """Test that bulk import produces identical delta to per-transition add."""
    # Build with per-transition
    nfa1 = mata_nfa.Nfa(10)
    trans_list = [
        (0, ord('a'), 1),
        (1, ord('b'), 2),
        (2, ord('c'), 3),
        (3, ord('d'), 4),
        (0, ord('e'), 5),
        (1, ord('f'), 6),
        (0, ord('a'), 1),  # Duplicate
    ]
    for src, sym, tgt in trans_list:
        nfa1.add_transition(src, sym, tgt)
    
    # Build with bulk
    nfa2 = mata_nfa.Nfa(10)
    sources, symbols, targets = zip(*trans_list) if trans_list else ([], [], [])
    nfa2.delta.add_transitions(sources, symbols, targets)
    
    # Should be identical
    assert nfa1.delta == nfa2.delta
    assert list(nfa1.delta) == list(nfa2.delta)


def test_bulk_with_out_of_order_sources():
    """Test bulk import with out-of-order sources (batched import sorts)."""
    nfa = mata_nfa.Nfa(10)
    
    # Add in non-monotonic order
    nfa.delta.add_transitions(
        [3, 1, 2, 1],
        [ord('a'), ord('b'), ord('c'), ord('d')],
        [4, 2, 3, 5]
    )
    
    assert nfa.delta.num_of_transitions() == 4
    assert nfa.delta.contains(1, ord('b'), 2)
    assert nfa.delta.contains(1, ord('d'), 5)
    assert nfa.delta.contains(2, ord('c'), 3)
    assert nfa.delta.contains(3, ord('a'), 4)


def test_delta_iter_transitions_from_consistency():
    """Test that Delta.iter_transitions_from matches get_transitions_from."""
    nfa = mata_nfa.Nfa(10)
    nfa.delta.add_transitions(
        [0, 0, 1, 1, 2],
        [ord('a'), ord('b'), ord('c'), ord('d'), ord('e')],
        [1, 2, 3, 4, 5]
    )
    
    # Compare streaming vs list for each state
    for state in range(3):
        streaming = list(nfa.delta.iter_transitions_from(state))
        list_form = nfa.delta.get_transitions_from(state)
        assert streaming == list_form
