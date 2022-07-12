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
    lhs = pynfa.Nfa(3)
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


def test_post(binary_alphabet):
    """Test various cases of getting post of the states
    :return:
    """
    lhs = pynfa.Nfa(3)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_trans_raw(0, 1, 2)
    lhs.add_trans_raw(1, 0, 0)
    lhs.add_trans_raw(2, 1, 2)
    lhs.add_trans_raw(2, 0, 2)
    lhs.add_final_state(2)

    assert lhs.post_of({0}, 0) == {1}
    assert lhs.post_of({0, 1}, 0) == {0, 1}
    assert lhs.post_of({0, 1, 2}, 1) == {2}
    assert lhs.post_of({0, 1, 2}, 0) == {0, 1, 2}

    assert lhs.post_map_of(0, binary_alphabet) == {0: {1}, 1: {2}}
    assert lhs.post_map_of(1, binary_alphabet) == {0: {0}, 1: {2}}
    assert lhs.post_map_of(2, binary_alphabet) == {0: {2}, 1: {2}}


def test_determinisation(nfa_two_states_uni, dfa_one_state_uni):
    """
    Tests determinisation
    """
    lhs = nfa_two_states_uni
    assert not pynfa.Nfa.is_deterministic(lhs)
    rhs = dfa_one_state_uni
    assert pynfa.Nfa.is_deterministic(rhs)

    chs, sm_map = pynfa.Nfa.determinize(lhs)
    assert pynfa.Nfa.is_deterministic(chs)
    assert sm_map == {(0, ): 0, (0, 1): 1}


def test_forward_reach_states(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight, binary_alphabet
):
    assert pynfa.Nfa.get_forward_reachable_states(fa_one_divisible_by_two, binary_alphabet) == set(range(0, 3))
    assert pynfa.Nfa.get_forward_reachable_states(fa_one_divisible_by_four, binary_alphabet) == set(range(0, 5))
    assert pynfa.Nfa.get_forward_reachable_states(fa_one_divisible_by_eight, binary_alphabet) == set(range(0, 9))


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

    lhs = pynfa.Nfa(4)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 0, 2)
    lhs.add_trans_raw(2, 0, 3)
    assert pynfa.Nfa.is_lang_empty_word_counterexample(lhs) == (True, [])


def test_universality(fa_one_divisible_by_two):
    alph = pynfa.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    assert pynfa.Nfa.is_universal(fa_one_divisible_by_two, alph) == False

    l = pynfa.Nfa(1)
    l.add_initial_state(0)
    l.add_trans_raw(0, 0, 0)
    l.add_trans_raw(0, 1, 0)
    l.add_final_state(0)
    assert pynfa.Nfa.is_universal(l, alph) == True

def test_inclusion(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = pynfa.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    result, cex = pynfa.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_four, alph)
    assert not result
    assert cex == [1, 1]

    result, cex = pynfa.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_two, alph)
    assert result
    assert cex == []

    assert pynfa.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_two, alph)[0]
    assert pynfa.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_four, alph)[0]
    assert not pynfa.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_eight, alph)[0]
    assert not pynfa.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_eight, alph)[0]


def test_completeness(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = pynfa.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    assert pynfa.Nfa.is_complete(fa_one_divisible_by_two, alph)
    assert pynfa.Nfa.is_complete(fa_one_divisible_by_four, alph)
    assert pynfa.Nfa.is_complete(fa_one_divisible_by_eight, alph)

    l = pynfa.Nfa(1)
    l.add_initial_state(0)
    l.add_trans_raw(0,0,0)
    assert not pynfa.Nfa.is_complete(l, alph)
    l.add_trans_raw(0,1,0)
    assert pynfa.Nfa.is_complete(l, alph)

    r = pynfa.Nfa(1)
    r.add_initial_state(0)
    r.add_trans_raw(0,0,0)
    assert not pynfa.Nfa.is_complete(r, alph)
    pynfa.Nfa.make_complete(r, 1, alph)
    assert pynfa.Nfa.is_complete(r, alph)


def test_in_language(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert pynfa.Nfa.is_in_lang(fa_one_divisible_by_two, [1, 1])
    assert not pynfa.Nfa.is_in_lang(fa_one_divisible_by_two, [1, 1, 1])

    assert pynfa.Nfa.is_prefix_in_lang(fa_one_divisible_by_four, [1, 1, 1, 1, 0])
    assert not pynfa.Nfa.is_prefix_in_lang(fa_one_divisible_by_four, [1, 1, 1, 0, 0])
    assert not pynfa.Nfa.accepts_epsilon(fa_one_divisible_by_four)

    lhs = pynfa.Nfa(2)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 1)
    assert not pynfa.Nfa.accepts_epsilon(lhs)
    lhs.add_final_state(1)
    assert not pynfa.Nfa.accepts_epsilon(lhs)
    lhs.add_final_state(0)
    assert pynfa.Nfa.accepts_epsilon(lhs)

def test_union(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = pynfa.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    assert pynfa.Nfa.is_in_lang(fa_one_divisible_by_two, [1, 1])
    assert not pynfa.Nfa.is_in_lang(fa_one_divisible_by_four, [1, 1])
    uni = pynfa.Nfa.union(fa_one_divisible_by_two, fa_one_divisible_by_four)
    assert pynfa.Nfa.is_in_lang(uni, [1, 1])
    assert pynfa.Nfa.is_in_lang(uni, [1, 1, 1, 1])
    assert pynfa.Nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1])
    assert pynfa.Nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1, 1, 1,])
    assert pynfa.Nfa.is_included(fa_one_divisible_by_two, uni, alph)[0]
    assert pynfa.Nfa.is_included(fa_one_divisible_by_four, uni, alph)[0]

def test_intersection(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = pynfa.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    inter, map = pynfa.Nfa.intersection(fa_one_divisible_by_two, fa_one_divisible_by_four)

    assert not pynfa.Nfa.is_in_lang(inter, [1, 1])
    assert pynfa.Nfa.is_in_lang(inter, [1, 1, 1, 1])
    assert not pynfa.Nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1])
    assert pynfa.Nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1, 1, 1,])
    assert pynfa.Nfa.is_included(inter, fa_one_divisible_by_two, alph)[0]
    assert pynfa.Nfa.is_included(inter, fa_one_divisible_by_four, alph)[0]
    assert map == {(0,0): 0, (1,1): 1, (1,3): 3, (2,2): 2, (2, 4): 4}

def test_complement(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = pynfa.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    res, subset_map = pynfa.Nfa.complement(fa_one_divisible_by_two, alph)
    assert not pynfa.Nfa.is_in_lang(res, [1,1])
    assert pynfa.Nfa.is_in_lang(res, [1,1,1])
    assert not pynfa.Nfa.is_in_lang(res, [1,1,1,1])
    assert subset_map == {(): 4, (0,): 0, (1,): 1, (2,): 2}


def test_revert():
    lhs = pynfa.Nfa(3)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_final_state(2)
    assert pynfa.Nfa.is_in_lang(lhs, [0, 1])
    assert not pynfa.Nfa.is_in_lang(lhs, [1, 0])

    rhs = pynfa.Nfa.revert(lhs)
    assert not pynfa.Nfa.is_in_lang(rhs, [0, 1])
    assert pynfa.Nfa.is_in_lang(rhs, [1, 0])

def test_removing_epsilon():
    lhs = pynfa.Nfa(3)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_trans_raw(0, 2, 2)
    lhs.add_final_state(2)

    rhs = pynfa.Nfa.remove_epsilon(lhs, 2)
    assert rhs.has_trans_raw(0, 0, 1)
    assert rhs.has_trans_raw(1, 1, 2)
    assert not rhs.has_trans_raw(0, 2, 2)


def test_minimize(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    minimized = pynfa.Nfa.minimize(fa_one_divisible_by_two)
    assert minimized.trans_size() <= fa_one_divisible_by_two.trans_size()
    minimized = pynfa.Nfa.minimize(fa_one_divisible_by_four)
    assert minimized.trans_size() <= fa_one_divisible_by_four.trans_size()
    minimized = pynfa.Nfa.minimize(fa_one_divisible_by_eight)
    assert minimized.trans_size() <= fa_one_divisible_by_eight.trans_size()

    lhs = pynfa.Nfa(11)
    lhs.add_initial_state(0)
    for i in range(0, 10):
        lhs.add_trans_raw(i, 0, i+1)
        lhs.add_final_state(i)
    lhs.add_trans_raw(10, 0, 10)
    lhs.add_final_state(10)
    assert lhs.trans_size() == 11

    minimized = pynfa.Nfa.minimize(lhs)
    assert minimized.trans_size() == 1
