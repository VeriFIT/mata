"""Basic tests for utility package and sanity checks"""

import pytest
import pynfa

__author__ = 'Tomas Fiedor'


def test_adding_states():
    """Test nfa"""
    lhs = pynfa.Nfa()

    # Test adding states
    assert not lhs.has_initial_state(0)
    assert not lhs.has_final_state(0)
    lhs.add_initial_state(0)
    assert lhs.has_initial_state(0)
    assert not lhs.has_final_state(0)

    # Test adding range of states
    for i in range(1, 5):
        assert not lhs.has_initial_state(i)
        assert not lhs.has_final_state(i)

    lhs.add_initial_states([1, 2, 3, 4])
    for i in range(0, 5):
        assert lhs.has_initial_state(i)
        assert not lhs.has_final_state(i)

    # Test adding final states
    lhs.add_final_state(0)
    assert lhs.has_final_state(0)


def test_transitions():
    """Test adding transitions to automaton"""
    lhs = pynfa.Nfa()
    t1 = pynfa.Trans(0, 0, 0)
    t2 = pynfa.Trans(0, 1, 0)
    t3 = pynfa.Trans(1, 1, 1)
    t4 = pynfa.Trans(2, 2, 2)

    # Test adding transition
    assert lhs.trans_empty()
    assert lhs.trans_size() == 0
    lhs.add_trans(t1)
    assert not lhs.trans_empty()
    assert lhs.trans_size() == 1

    lhs.add_trans(t2)
    assert lhs.trans_size() == 2

    # Test adding add-hoc transition
    lhs.add_trans_raw(1, 1, 1)
    assert lhs.trans_size() == 3
    assert lhs.has_trans(t3)
    assert not lhs.has_trans_raw(2, 2, 2)
    lhs.add_trans(t4)
    assert lhs.has_trans_raw(2, 2, 2)
    assert lhs.trans_size() == 4

    # Test that transitions are not duplicated
    lhs.add_trans_raw(1, 1, 1)
    assert lhs.trans_size() == 4

    assert [t for t in lhs.iterate()] == [t4, t3, t2, t1]


def test_post():
    """Test various cases of getting post of the states
    :return:
    """
    lhs = pynfa.Nfa()
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_trans_raw(0, 1, 2)
    lhs.add_trans_raw(1, 0, 0)
    lhs.add_trans_raw(2, 1, 2)
    lhs.add_trans_raw(2, 0, 2)
    lhs.add_final_state(2)

    assert lhs.post_map_of(0) == {0: {1}, 1: {2}}
    assert lhs.post_map_of(1) == {0: {0}, 1: {2}}
    assert lhs.post_map_of(2) == {0: {2}, 1: {2}}

    assert lhs.post_of({0}, 0) == {1}
    assert lhs.post_of({0, 1}, 0) == {0, 1}
    assert lhs.post_of({0, 1, 2}, 1) == {2}
    assert lhs.post_of({0, 1, 2}, 0) == {0, 1, 2}


def test_determinisation(nfa_two_states_uni, dfa_one_state_uni):
    """
    Tests determinisation
    """
    lhs = nfa_two_states_uni
    assert not pynfa.Nfa.is_deterministic(lhs)
    rhs = dfa_one_state_uni
    assert pynfa.Nfa.is_deterministic(rhs)

    chs, sm_map, last_state = pynfa.Nfa.determinize(lhs)
    assert pynfa.Nfa.is_deterministic(chs)
    assert sm_map == {(0, ): 0, (0, 1): 1}
    assert last_state == 1


def test_forward_reach_states(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert pynfa.Nfa.get_forward_reachable_states(fa_one_divisible_by_two) == set(range(0, 3))
    assert pynfa.Nfa.get_forward_reachable_states(fa_one_divisible_by_four) == set(range(0, 5))
    assert pynfa.Nfa.get_forward_reachable_states(fa_one_divisible_by_eight) == set(range(0, 9))


def test_get_word_for_path(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert pynfa.Nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2]) == ([1, 1], True)
    assert pynfa.Nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2, 0]) == ([], False)
    assert pynfa.Nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2, 2]) == ([1, 1, 0], True)
    assert pynfa.Nfa.get_word_for_path(
        fa_one_divisible_by_four, [0, 1, 2, 3, 4]
    ) == ([1, 1, 1, 1], True)
    assert pynfa.Nfa.get_word_for_path(
        fa_one_divisible_by_eight, [0, 1, 2, 3, 4, 5, 6, 7, 8]
    ) == ([1, 1, 1, 1, 1, 1, 1, 1], True)


def test_encode_word():
    assert pynfa.Nfa.encode_word({'a': 1, 'b': 2, "c": 0}, "abca") == [1, 2, 0, 1]

def test_language_emptiness(fa_one_divisible_by_two):
    assert pynfa.Nfa.is_lang_empty_path_counterexample(fa_one_divisible_by_two) == (False, [0, 1, 2])
    assert pynfa.Nfa.is_lang_empty_word_counterexample(fa_one_divisible_by_two) == (False, [1, 1])

    lhs = pynfa.Nfa()
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 0, 2)
    lhs.add_trans_raw(2, 0, 3)
    assert pynfa.Nfa.is_lang_empty_word_counterexample(lhs) == (True, [])
