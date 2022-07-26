"""Basic tests for utility package and sanity checks"""

import pytest
import mata
import os

__author__ = 'Tomas Fiedor'


def test_adding_states():
    """Test nfa"""
    lhs = mata.Nfa(5)

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

    rhs = mata.Nfa()
    assert rhs.state_size() == 0
    state = rhs.add_new_state()
    assert state == 0
    assert rhs.state_size() == 1
    state = rhs.add_new_state()
    assert state == 1
    assert rhs.state_size() == 2


def test_transitions():
    """Test adding transitions to automaton"""
    lhs = mata.Nfa(3)
    t1 = mata.Trans(0, 0, 0)
    t2 = mata.Trans(0, 1, 0)
    t3 = mata.Trans(1, 1, 1)
    t4 = mata.Trans(2, 2, 2)

    # Test adding transition
    assert lhs.trans_empty()
    assert lhs.trans_size() == 3
    lhs.add_trans(t1)
    assert not lhs.trans_empty()
    assert lhs.has_trans(t1)

    lhs.add_trans(t2)
    assert lhs.has_trans(t2)

    # Test adding add-hoc transition
    lhs.add_trans_raw(1, 1, 1)
    assert lhs.has_trans(t3)
    assert not lhs.has_trans_raw(2, 2, 2)
    lhs.add_trans(t4)
    assert lhs.has_trans_raw(2, 2, 2)

    # Test that transitions are not duplicated
    lhs.add_trans_raw(1, 1, 1)

    assert [t for t in lhs.iterate()] == [t1, t2, t3, t4]


def test_post(binary_alphabet):
    """Test various cases of getting post of the states
    :return:
    """
    lhs = mata.Nfa(3)
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
    assert not mata.Nfa.is_deterministic(lhs)
    rhs = dfa_one_state_uni
    assert mata.Nfa.is_deterministic(rhs)

    chs, sm_map = mata.Nfa.determinize(lhs)
    assert mata.Nfa.is_deterministic(chs)
    assert sm_map == {(0, ): 0, (0, 1): 1}


def test_forward_reach_states(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight, binary_alphabet
):
    assert mata.Nfa.get_forward_reachable_states(fa_one_divisible_by_two, binary_alphabet) == set(range(0, 3))
    assert mata.Nfa.get_forward_reachable_states(fa_one_divisible_by_four, binary_alphabet) == set(range(0, 5))
    assert mata.Nfa.get_forward_reachable_states(fa_one_divisible_by_eight, binary_alphabet) == set(range(0, 9))


def test_get_word_for_path(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert mata.Nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2]) == ([1, 1], True)
    assert mata.Nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2, 0]) == ([], False)
    assert mata.Nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2, 2]) == ([1, 1, 0], True)
    assert mata.Nfa.get_word_for_path(
        fa_one_divisible_by_four, [0, 1, 2, 3, 4]
    ) == ([1, 1, 1, 1], True)
    assert mata.Nfa.get_word_for_path(
        fa_one_divisible_by_eight, [0, 1, 2, 3, 4, 5, 6, 7, 8]
    ) == ([1, 1, 1, 1, 1, 1, 1, 1], True)


def test_encode_word():
    assert mata.Nfa.encode_word({'a': 1, 'b': 2, "c": 0}, "abca") == [1, 2, 0, 1]


def test_language_emptiness(fa_one_divisible_by_two):
    assert mata.Nfa.is_lang_empty_path_counterexample(fa_one_divisible_by_two) == (False, [0, 1, 2])
    assert mata.Nfa.is_lang_empty_word_counterexample(fa_one_divisible_by_two) == (False, [1, 1])

    lhs = mata.Nfa(4)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 0, 2)
    lhs.add_trans_raw(2, 0, 3)
    assert mata.Nfa.is_lang_empty_word_counterexample(lhs) == (True, [])


def test_universality(fa_one_divisible_by_two):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    assert mata.Nfa.is_universal(fa_one_divisible_by_two, alph) == False

    l = mata.Nfa(1)
    l.add_initial_state(0)
    l.add_trans_raw(0, 0, 0)
    l.add_trans_raw(0, 1, 0)
    l.add_final_state(0)
    assert mata.Nfa.is_universal(l, alph) == True

def test_inclusion(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    result, cex = mata.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_four, alph)
    assert not result
    assert cex == [1, 1]

    result, cex = mata.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_two, alph)
    assert result
    assert cex == []

    assert mata.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_two, alph)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_four, alph)[0]
    assert not mata.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_eight, alph)[0]
    assert not mata.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_eight, alph)[0]


def test_completeness(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    assert mata.Nfa.is_complete(fa_one_divisible_by_two, alph)
    assert mata.Nfa.is_complete(fa_one_divisible_by_four, alph)
    assert mata.Nfa.is_complete(fa_one_divisible_by_eight, alph)

    l = mata.Nfa(1)
    l.add_initial_state(0)
    l.add_trans_raw(0,0,0)
    assert not mata.Nfa.is_complete(l, alph)
    l.add_trans_raw(0,1,0)
    assert mata.Nfa.is_complete(l, alph)

    r = mata.Nfa(1)
    r.add_initial_state(0)
    r.add_trans_raw(0,0,0)
    assert not mata.Nfa.is_complete(r, alph)
    mata.Nfa.make_complete(r, 1, alph)
    assert mata.Nfa.is_complete(r, alph)


def test_in_language(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert mata.Nfa.is_in_lang(fa_one_divisible_by_two, [1, 1])
    assert not mata.Nfa.is_in_lang(fa_one_divisible_by_two, [1, 1, 1])

    assert mata.Nfa.is_prefix_in_lang(fa_one_divisible_by_four, [1, 1, 1, 1, 0])
    assert not mata.Nfa.is_prefix_in_lang(fa_one_divisible_by_four, [1, 1, 1, 0, 0])
    assert not mata.Nfa.accepts_epsilon(fa_one_divisible_by_four)

    lhs = mata.Nfa(2)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 1)
    assert not mata.Nfa.accepts_epsilon(lhs)
    lhs.add_final_state(1)
    assert not mata.Nfa.accepts_epsilon(lhs)
    lhs.add_final_state(0)
    assert mata.Nfa.accepts_epsilon(lhs)

def test_union(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    assert mata.Nfa.is_in_lang(fa_one_divisible_by_two, [1, 1])
    assert not mata.Nfa.is_in_lang(fa_one_divisible_by_four, [1, 1])
    uni = mata.Nfa.union(fa_one_divisible_by_two, fa_one_divisible_by_four)
    assert mata.Nfa.is_in_lang(uni, [1, 1])
    assert mata.Nfa.is_in_lang(uni, [1, 1, 1, 1])
    assert mata.Nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1])
    assert mata.Nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1, 1, 1,])
    assert mata.Nfa.is_included(fa_one_divisible_by_two, uni, alph)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_four, uni, alph)[0]

def test_intersection(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    inter, map = mata.Nfa.intersection(fa_one_divisible_by_two, fa_one_divisible_by_four)

    assert not mata.Nfa.is_in_lang(inter, [1, 1])
    assert mata.Nfa.is_in_lang(inter, [1, 1, 1, 1])
    assert not mata.Nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1])
    assert mata.Nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1, 1, 1,])
    assert mata.Nfa.is_included(inter, fa_one_divisible_by_two, alph)[0]
    assert mata.Nfa.is_included(inter, fa_one_divisible_by_four, alph)[0]
    assert map == {(0,0): 0, (1,1): 1, (1,3): 3, (2,2): 2, (2, 4): 4}

def test_complement(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    res, subset_map = mata.Nfa.complement(fa_one_divisible_by_two, alph)
    assert not mata.Nfa.is_in_lang(res, [1,1])
    assert mata.Nfa.is_in_lang(res, [1,1,1])
    assert not mata.Nfa.is_in_lang(res, [1,1,1,1])
    assert subset_map == {(): 4, (0,): 0, (1,): 1, (2,): 2}


def test_revert():
    lhs = mata.Nfa(3)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_final_state(2)
    assert mata.Nfa.is_in_lang(lhs, [0, 1])
    assert not mata.Nfa.is_in_lang(lhs, [1, 0])

    rhs = mata.Nfa.revert(lhs)
    assert not mata.Nfa.is_in_lang(rhs, [0, 1])
    assert mata.Nfa.is_in_lang(rhs, [1, 0])

def test_removing_epsilon():
    lhs = mata.Nfa(3)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_trans_raw(0, 2, 2)
    lhs.add_final_state(2)

    rhs = mata.Nfa.remove_epsilon(lhs, 2)
    assert rhs.has_trans_raw(0, 0, 1)
    assert rhs.has_trans_raw(1, 1, 2)
    assert not rhs.has_trans_raw(0, 2, 2)


def test_minimize(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    minimized = mata.Nfa.minimize(fa_one_divisible_by_two)
    assert minimized.trans_size() <= fa_one_divisible_by_two.trans_size()
    minimized = mata.Nfa.minimize(fa_one_divisible_by_four)
    assert minimized.trans_size() <= fa_one_divisible_by_four.trans_size()
    minimized = mata.Nfa.minimize(fa_one_divisible_by_eight)
    assert minimized.trans_size() <= fa_one_divisible_by_eight.trans_size()

    lhs = mata.Nfa(11)
    lhs.add_initial_state(0)
    for i in range(0, 10):
        lhs.add_trans_raw(i, 0, i+1)
        lhs.add_final_state(i)
    lhs.add_trans_raw(10, 0, 10)
    lhs.add_final_state(10)
    assert lhs.trans_size() == 11

    minimized = mata.Nfa.minimize(lhs)
    assert minimized.trans_size() == 1

def test_to_dot():
    lhs = mata.Nfa()
    expected = "digraph finiteAutomaton {\nnode [shape=circle];\nnode [shape=none, label=\"\"];\n}\n"
    assert lhs.to_dot_str() == expected

    lhs.to_dot_file('test.dot')
    assert 'test.dot.pdf' in os.listdir('.')
    assert 'test.dot' in os.listdir('.')
    with open('test.dot', 'r') as test_handle:
        lines = test_handle.read()
    assert lines == expected

def test_to_str():
    lhs = mata.Nfa(2)
    lhs.add_initial_state(0)
    lhs.add_final_state(1)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 1)
    expected = "initial_states: [0]\nfinal_states: [1]\ntransitions:\n0-[0]→0\n0-[1]→1\n"
    assert str(lhs) == expected
