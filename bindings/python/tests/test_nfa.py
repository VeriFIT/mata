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
    lhs.make_initial_state(0)
    assert lhs.has_initial_state(0)
    assert not lhs.has_final_state(0)

    # Test adding range of states
    for i in range(1, 5):
        assert not lhs.has_initial_state(i)
        assert not lhs.has_final_state(i)

    lhs.make_initial_states([1, 2, 3, 4])
    for i in range(0, 5):
        assert lhs.has_initial_state(i)
        assert not lhs.has_final_state(i)

    # Test adding final states
    lhs.make_final_state(0)
    assert lhs.has_final_state(0)

    rhs = mata.Nfa()
    assert rhs.get_num_of_states() == 0
    state = rhs.add_new_state()
    assert state == 0
    assert rhs.get_num_of_states() == 1
    state = rhs.add_new_state()
    assert state == 1
    assert rhs.get_num_of_states() == 2

    rhs.resize(10)
    assert rhs.get_num_of_states() == 10
    for i in range(0, 10):
        assert rhs.is_state(i)

    rhs.resize(1)
    assert rhs.get_num_of_states() == 1
    assert rhs.is_state(0)
    assert not rhs.is_state(1)

    with pytest.raises(OverflowError):
        rhs.resize(-10)

    rhs.resize_for_state(11)
    assert rhs.get_num_of_states() == 12
    assert rhs.is_state(11)
    assert not rhs.is_state(12)

    with pytest.raises(OverflowError):
        rhs.resize_for_state(-10)


def test_making_initial_and_final_states():
    """Test making states in the automaton initial and final."""
    nfa = mata.Nfa(10)
    assert len(nfa.initial_states) == 0
    assert len(nfa.final_states) == 0

    nfa.make_initial_state(0)
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(0)
    assert len(nfa.final_states) == 0

    nfa.make_final_state(0)
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(0)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(0)

    nfa.make_initial_states([1, 2, 3])
    assert len(nfa.initial_states) == 4
    assert nfa.has_initial_state(0)
    assert nfa.has_initial_state(1)
    assert nfa.has_initial_state(2)
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(0)

    nfa.make_final_states([1, 2, 3])
    assert len(nfa.initial_states) == 4
    assert nfa.has_initial_state(0)
    assert nfa.has_initial_state(1)
    assert nfa.has_initial_state(2)
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 4
    assert nfa.has_final_state(0)
    assert nfa.has_final_state(1)
    assert nfa.has_final_state(2)
    assert nfa.has_final_state(3)

    nfa.remove_initial_state(0)
    assert len(nfa.initial_states) == 3
    assert not nfa.has_initial_state(0)
    assert nfa.has_initial_state(1)
    assert nfa.has_initial_state(2)
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 4
    assert nfa.has_final_state(0)
    assert nfa.has_final_state(1)
    assert nfa.has_final_state(2)
    assert nfa.has_final_state(3)

    nfa.remove_final_state(0)
    assert len(nfa.initial_states) == 3
    assert not nfa.has_initial_state(0)
    assert nfa.has_initial_state(1)
    assert nfa.has_initial_state(2)
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 3
    assert not nfa.has_final_state(0)
    assert nfa.has_final_state(1)
    assert nfa.has_final_state(2)
    assert nfa.has_final_state(3)

    nfa.remove_initial_states([1, 2])
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 3
    assert not nfa.has_final_state(0)
    assert nfa.has_final_state(1)
    assert nfa.has_final_state(2)
    assert nfa.has_final_state(3)

    nfa.remove_final_states([1, 2])
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(3)

    nfa.reset_initial_state(4)
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(4)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(3)

    nfa.reset_final_state(4)
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(4)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(4)

    nfa.reset_initial_states([5, 6])
    assert len(nfa.initial_states) == 2
    assert nfa.has_initial_state(5)
    assert nfa.has_initial_state(6)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(4)

    nfa.reset_initial_states([5, 6])
    assert len(nfa.initial_states) == 2
    assert nfa.has_initial_state(5)
    assert nfa.has_initial_state(6)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(4)

    nfa.clear_initial()
    assert len(nfa.initial_states) == 0
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(4)

    nfa.clear_final()
    assert len(nfa.initial_states) == 0
    assert len(nfa.final_states) == 0


def test_transitions():
    """Test adding transitions to automaton"""
    lhs = mata.Nfa(3)
    t1 = mata.Trans(0, 0, 0)
    t2 = mata.Trans(0, 1, 0)
    t3 = mata.Trans(1, 1, 1)
    t4 = mata.Trans(2, 2, 2)

    # Test adding transition
    assert lhs.trans_empty()
    assert lhs.get_num_of_trans() == 0
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
    lhs.make_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_trans_raw(0, 1, 2)
    lhs.add_trans_raw(1, 0, 0)
    lhs.add_trans_raw(2, 1, 2)
    lhs.add_trans_raw(2, 0, 2)
    lhs.make_final_state(2)

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
    assert sm_map == {(0,): 0, (0, 1): 1}


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
    lhs.make_initial_state(0)
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
    l.make_initial_state(0)
    l.add_trans_raw(0, 0, 0)
    l.add_trans_raw(0, 1, 0)
    l.make_final_state(0)
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
    result, cex = mata.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_four)
    assert not result
    assert cex == [1, 1]

    result, cex = mata.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_two, alph)
    assert result
    assert cex == []
    result, cex = mata.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_two)
    assert result
    assert cex == []

    assert mata.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_two, alph)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_four, alph)[0]
    assert not mata.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_eight, alph)[0]
    assert not mata.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_eight, alph)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_two)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_four)[0]
    assert not mata.Nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_eight)[0]
    assert not mata.Nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_eight)[0]

    # Test equivalence of two NFAs.
    smaller = mata.Nfa(10)
    bigger = mata.Nfa(16)
    alph = ["a", "b"]
    smaller.make_initial_state(1)
    smaller.make_final_state(1)
    smaller.add_trans_raw(1, ord('a'), 1)
    smaller.add_trans_raw(1, ord('b'), 1)

    bigger.make_initial_state(11)
    bigger.make_final_states([11, 12, 13, 14, 15])

    bigger.add_trans_raw(11, ord('a'), 12)
    bigger.add_trans_raw(11, ord('b'), 12)
    bigger.add_trans_raw(12, ord('a'), 13)
    bigger.add_trans_raw(12, ord('b'), 13)

    bigger.add_trans_raw(13, ord('a'), 14)
    bigger.add_trans_raw(14, ord('a'), 14)

    bigger.add_trans_raw(13, ord('b'), 15)
    bigger.add_trans_raw(15, ord('b'), 15)

    assert not mata.Nfa.equivalence_check(smaller, bigger, mata.EnumAlphabet(alph))
    assert not mata.Nfa.equivalence_check(smaller, bigger)
    assert not mata.Nfa.equivalence_check(bigger, smaller, mata.EnumAlphabet(alph))
    assert not mata.Nfa.equivalence_check(bigger, smaller)

    smaller = mata.Nfa(10)
    bigger = mata.Nfa(16)
    alph = []
    smaller.initial_states = [1]
    smaller.final_states = [1]
    bigger.initial_states = [11]
    bigger.final_states = [11]

    assert mata.Nfa.equivalence_check(smaller, bigger, mata.EnumAlphabet(alph))
    assert mata.Nfa.equivalence_check(smaller, bigger)
    assert mata.Nfa.equivalence_check(bigger, smaller, mata.EnumAlphabet(alph))
    assert mata.Nfa.equivalence_check(bigger, smaller)


def test_concatenate():
    lhs = mata.Nfa(2)
    lhs.make_initial_state(0)
    lhs.make_final_state(1)
    lhs.add_trans_raw(0, ord('b'), 1)

    rhs = mata.Nfa(2)
    rhs.make_initial_state(0)
    rhs.make_final_state(1)
    rhs.add_trans_raw(0, ord('a'), 1)

    result = mata.Nfa.concatenate(lhs, rhs)

    assert result.has_initial_state(0)
    assert result.has_final_state(2)
    assert result.get_num_of_states() == 3
    assert result.has_trans_raw(0, ord('b'), 1)
    assert result.has_trans_raw(1, ord('a'), 2)

    shortest_words = result.get_shortest_words()
    assert len(shortest_words) == 1
    assert [ord('b'), ord('a')] in shortest_words


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
    l.make_initial_state(0)
    l.add_trans_raw(0, 0, 0)
    assert not mata.Nfa.is_complete(l, alph)
    l.add_trans_raw(0, 1, 0)
    assert mata.Nfa.is_complete(l, alph)

    r = mata.Nfa(1)
    r.make_initial_state(0)
    r.add_trans_raw(0, 0, 0)
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
    lhs.make_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 1)
    assert not mata.Nfa.accepts_epsilon(lhs)
    lhs.make_final_state(1)
    assert not mata.Nfa.accepts_epsilon(lhs)
    lhs.make_final_state(0)
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
    assert mata.Nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1, 1, 1, ])
    assert mata.Nfa.is_included(fa_one_divisible_by_two, uni, alph)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_four, uni, alph)[0]

    assert mata.Nfa.is_included(fa_one_divisible_by_two, uni)[0]
    assert mata.Nfa.is_included(fa_one_divisible_by_four, uni)[0]


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
    assert mata.Nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1, 1, 1, ])
    assert mata.Nfa.is_included(inter, fa_one_divisible_by_two, alph)[0]
    assert mata.Nfa.is_included(inter, fa_one_divisible_by_four, alph)[0]
    assert mata.Nfa.is_included(inter, fa_one_divisible_by_two)[0]
    assert mata.Nfa.is_included(inter, fa_one_divisible_by_four)[0]
    assert map == {(0, 0): 0, (1, 1): 1, (1, 3): 3, (2, 2): 2, (2, 4): 4}


def test_intersection_preserving_epsilon_transitions():
    epsilon = ord('e')
    a = mata.Nfa(6)
    a.make_initial_state(0)
    a.make_final_states([1, 4, 5])
    a.add_trans_raw(0, epsilon, 1)
    a.add_trans_raw(1, ord('a'), 1)
    a.add_trans_raw(1, ord('b'), 1)
    a.add_trans_raw(1, ord('c'), 2)
    a.add_trans_raw(2, ord('b'), 4)
    a.add_trans_raw(2, epsilon, 3)
    a.add_trans_raw(3, ord('a'), 5)

    b = mata.Nfa(10)
    b.make_initial_state(0)
    b.make_final_states([2, 4, 8, 7])
    b.add_trans_raw(0, ord('b'), 1)
    b.add_trans_raw(0, ord('a'), 2)
    b.add_trans_raw(2, ord('a'), 4)
    b.add_trans_raw(2, epsilon, 3)
    b.add_trans_raw(3, ord('b'), 4)
    b.add_trans_raw(0, ord('c'), 5)
    b.add_trans_raw(5, ord('a'), 8)
    b.add_trans_raw(5, epsilon, 6)
    b.add_trans_raw(6, ord('a'), 9)
    b.add_trans_raw(6, ord('b'), 7)

    result, prod_map = mata.Nfa.intersection_preserving_epsilon_transitions(a, b, epsilon)

    # Check states.
    assert result.get_num_of_states() == 13
    assert result.is_state(prod_map[(0, 0)])
    assert result.is_state(prod_map[(1, 0)])
    assert result.is_state(prod_map[(1, 1)])
    assert result.is_state(prod_map[(1, 2)])
    assert result.is_state(prod_map[(1, 3)])
    assert result.is_state(prod_map[(1, 4)])
    assert result.is_state(prod_map[(2, 5)])
    assert result.is_state(prod_map[(3, 5)])
    assert result.is_state(prod_map[(2, 6)])
    assert result.is_state(prod_map[(3, 6)])
    assert result.is_state(prod_map[(4, 7)])
    assert result.is_state(prod_map[(5, 9)])
    assert result.is_state(prod_map[(5, 8)])

    assert result.has_initial_state(prod_map[(0, 0)])
    assert len(result.initial_states) == 1
    assert result.has_final_state(prod_map[(1, 2)])
    assert result.has_final_state(prod_map[(1, 4)])
    assert result.has_final_state(prod_map[(4, 7)])
    assert result.has_final_state(prod_map[(5, 8)])
    assert len(result.final_states) == 4

    # Check transitions.
    assert result.get_num_of_trans() == 15

    assert result.has_trans_raw(prod_map[(0, 0)], epsilon, prod_map[(1, 0)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(0, 0)])) == 1

    assert result.has_trans_raw(prod_map[(1, 0)], ord('b'), prod_map[(1, 1)])
    assert result.has_trans_raw(prod_map[(1, 0)], ord('a'), prod_map[(1, 2)])
    assert result.has_trans_raw(prod_map[(1, 0)], ord('c'), prod_map[(2, 5)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(1, 0)])) == 3

    assert len(result.get_trans_from_state_as_sequence(prod_map[(1, 1)])) == 0

    assert result.has_trans_raw(prod_map[(1, 2)], epsilon, prod_map[(1, 3)])
    assert result.has_trans_raw(prod_map[(1, 2)], ord('a'), prod_map[(1, 4)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(1, 2)])) == 2

    assert result.has_trans_raw(prod_map[(1, 3)], ord('b'), prod_map[(1, 4)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(1, 3)])) == 1

    assert len(result.get_trans_from_state_as_sequence(prod_map[(1, 4)])) == 0

    assert result.has_trans_raw(prod_map[(2, 5)], epsilon, prod_map[(3, 5)])
    assert result.has_trans_raw(prod_map[(2, 5)], epsilon, prod_map[(2, 6)])
    assert result.has_trans_raw(prod_map[(2, 5)], epsilon, prod_map[(3, 6)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(2, 5)])) == 3

    assert result.has_trans_raw(prod_map[(3, 5)], ord('a'), prod_map[(5, 8)])
    assert result.has_trans_raw(prod_map[(3, 5)], epsilon, prod_map[(3, 6)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(3, 5)])) == 2

    assert result.has_trans_raw(prod_map[(2, 6)], ord('b'), prod_map[(4, 7)])
    assert result.has_trans_raw(prod_map[(2, 6)], epsilon, prod_map[(3, 6)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(2, 6)])) == 2

    assert result.has_trans_raw(prod_map[(3, 6)], ord('a'), prod_map[(5, 9)])
    assert len(result.get_trans_from_state_as_sequence(prod_map[(3, 6)])) == 1

    assert len(result.get_trans_from_state_as_sequence(prod_map[(4, 7)])) == 0

    assert len(result.get_trans_from_state_as_sequence(prod_map[(5, 9)])) == 0

    assert len(result.get_trans_from_state_as_sequence(prod_map[(5, 8)])) == 0


def test_complement(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = mata.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    res, subset_map = mata.Nfa.complement(fa_one_divisible_by_two, alph)
    assert not mata.Nfa.is_in_lang(res, [1, 1])
    assert mata.Nfa.is_in_lang(res, [1, 1, 1])
    assert not mata.Nfa.is_in_lang(res, [1, 1, 1, 1])
    assert subset_map == {(): 4, (0,): 0, (1,): 1, (2,): 2}


def test_revert():
    lhs = mata.Nfa(3)
    lhs.make_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.make_final_state(2)
    assert mata.Nfa.is_in_lang(lhs, [0, 1])
    assert not mata.Nfa.is_in_lang(lhs, [1, 0])

    rhs = mata.Nfa.revert(lhs)
    assert not mata.Nfa.is_in_lang(rhs, [0, 1])
    assert mata.Nfa.is_in_lang(rhs, [1, 0])


def test_removing_epsilon():
    lhs = mata.Nfa(3)
    lhs.make_initial_state(0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 1, 2)
    lhs.add_trans_raw(0, 2, 2)
    lhs.make_final_state(2)

    rhs = mata.Nfa.remove_epsilon(lhs, 2)
    assert rhs.has_trans_raw(0, 0, 1)
    assert rhs.has_trans_raw(1, 1, 2)
    assert not rhs.has_trans_raw(0, 2, 2)


def test_minimize(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    minimized = mata.Nfa.minimize(fa_one_divisible_by_two)
    assert minimized.get_num_of_trans() <= fa_one_divisible_by_two.get_num_of_trans()
    minimized = mata.Nfa.minimize(fa_one_divisible_by_four)
    assert minimized.get_num_of_trans() <= fa_one_divisible_by_four.get_num_of_trans()
    minimized = mata.Nfa.minimize(fa_one_divisible_by_eight)
    assert minimized.get_num_of_trans() <= fa_one_divisible_by_eight.get_num_of_trans()

    lhs = mata.Nfa(11)
    lhs.make_initial_state(0)
    for i in range(0, 10):
        lhs.add_trans_raw(i, 0, i + 1)
        lhs.make_final_state(i)
    lhs.add_trans_raw(10, 0, 10)
    lhs.make_final_state(10)
    assert lhs.get_num_of_trans() == 11

    minimized = mata.Nfa.minimize(lhs)
    assert minimized.get_num_of_trans() == 1


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
    lhs.make_initial_state(0)
    lhs.make_final_state(1)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 1)
    expected = "initial_states: [0]\nfinal_states: [1]\ntransitions:\n0-[0]→0\n0-[1]→1\n"
    assert str(lhs) == expected


def test_shortest(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    shortest = lhs.get_shortest_words()
    assert shortest == [[1, 1]]


def test_get_trans(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    t = lhs.get_transitions_from_state(0)
    assert sorted(t) == sorted([mata.TransSymbolStates(0, [0]), mata.TransSymbolStates(1, [1])])
    tt = lhs.get_transitions_from_state(1)
    assert sorted(tt) == sorted([mata.TransSymbolStates(0, [1]), mata.TransSymbolStates(1, [2])])


def test_trim(prepare_automaton_a):
    """Test trimming the automaton."""
    nfa = prepare_automaton_a()
    nfa.remove_trans_raw(1, ord('a'), 10)

    old_nfa = prepare_automaton_a()
    old_nfa.remove_trans_raw(1, ord('a'), 10)

    nfa.trim()

    assert len(nfa.initial_states) == len(old_nfa.initial_states)
    assert len(nfa.final_states) == len(old_nfa.final_states)

    for word in old_nfa.get_shortest_words():
        assert mata.Nfa.is_in_lang(nfa, word)

    nfa.remove_final_state(2)  # '2' is the new final state in the earlier trimmed automaton.
    nfa.trim()
    assert nfa.trans_empty()
    assert nfa.get_num_of_states() == 0


def test_get_digraph(prepare_automaton_a):
    """Test creating digraph from an automaton."""
    abstract_symbol = ord('x')
    nfa = prepare_automaton_a()
    old_nfa = prepare_automaton_a()

    digraph = nfa.get_digraph()

    assert digraph.get_num_of_states() == nfa.get_num_of_states()
    assert digraph.get_num_of_trans() == 12
    assert digraph.has_trans_raw(1, abstract_symbol, 10)
    assert digraph.has_trans_raw(10, abstract_symbol, 7)
    assert not digraph.has_trans_raw(10, ord('a'), 7)
    assert not digraph.has_trans_raw(10, ord('b'), 7)
    assert not digraph.has_trans_raw(10, ord('c'), 7)


def test_simulation(fa_one_divisible_by_four):
    lhs = fa_one_divisible_by_four
    rel = mata.Nfa.compute_relation(lhs)
    assert rel.size() == 5
    assert rel.to_matrix() == [
        [True, False, False, False, True],
        [False, True, False, False, False],
        [False, False, True, False, False],
        [False, False, False, True, False],
        [False, False, False, False, True],
    ]

    # Test reseting the relation
    rel.reset()
    assert rel.to_matrix() == [
        [False for _ in range(0, 5)],
        [False for _ in range(0, 5)],
        [False for _ in range(0, 5)],
        [False for _ in range(0, 5)],
        [False for _ in range(0, 5)],
    ]

    rel.reset(defValue=True)
    assert rel.to_matrix() == [
        [True for _ in range(0, 5)],
        [True for _ in range(0, 5)],
        [True for _ in range(0, 5)],
        [True for _ in range(0, 5)],
        [True for _ in range(0, 5)],
    ]


def test_simulation_other_features(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    rel = mata.Nfa.compute_relation(lhs)
    assert rel.to_matrix() == [
        [True, False, True],
        [False, True, False],
        [False, False, True]
    ]

    # Testing transposition
    trans_rel = rel.transpose()
    assert trans_rel.to_matrix() == [
        [True, False, False],
        [False, True, False],
        [True, False, True]
    ]

    assert not rel.is_symmetric_at(0, 2)
    assert not rel.is_symmetric_at(1, 2)
    rel.split(0)
    assert rel.to_matrix() == [
        [True, False, True, True],
        [False, True, False, False],
        [False, False, True, False],
        [True, False, True, True],
    ]
    size = rel.alloc()
    assert rel.to_matrix() == [
        [True, False, True, True, False],
        [False, True, False, False, False],
        [False, False, True, False, False],
        [True, False, True, True, False],
        [False, False, False, False, False],
    ]

    projection = rel.get_quotient_projection()
    assert projection == [0, 1, 0, 0, 4]

    rel.restrict_to_symmetric()
    for i in range(0, rel.size()):
        for j in range(0, rel.size()):
            if rel.get(i, j) or rel.get(j, i):
                assert rel.is_symmetric_at(i, j)


def test_simulation_equivalence():
    r = mata.BinaryRelation(3, True, 3)
    classes, heads = r.build_equivalence_classes()
    assert classes == [0, 0, 0]
    assert heads == [0]
    r.reset(False)
    classes, heads = r.build_equivalence_classes()
    assert classes == [0, 1, 2]
    assert heads == [0, 1, 2]


def test_simulation_indexes(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    rel = mata.Nfa.compute_relation(lhs)
    index = rel.build_index()
    assert sorted(index) == [[0, 2], [1], [2]]
    inv_index = rel.build_inverse_index()
    assert sorted(inv_index) == [[0], [0, 2], [1]]
    index, inv_index = rel.build_indexes()
    assert sorted(index) == [[0, 2], [1], [2]]
    assert sorted(inv_index) == [[0], [0, 2], [1]]


def test_get_states(prepare_automaton_a, prepare_automaton_b):
    nfa = prepare_automaton_a()

    nfa.remove_trans_raw(3, ord('b'), 9)
    nfa.remove_trans_raw(5, ord('c'), 9)
    nfa.remove_trans_raw(1, ord('a'), 10)

    reachable = nfa.get_reachable_states()

    assert 0 not in reachable
    assert 1 in reachable
    assert 2 not in reachable
    assert 3 in reachable
    assert 4 not in reachable
    assert 5 in reachable
    assert 6 not in reachable
    assert 7 in reachable
    assert 8 not in reachable
    assert 9 not in reachable
    assert 10 not in reachable

    nfa.remove_initial_state(1)
    nfa.remove_initial_state(3)

    reachable = nfa.get_reachable_states()
    assert len(reachable) == 0

    nfa = prepare_automaton_b()

    nfa.remove_trans_raw(2, ord('c'), 12)
    nfa.remove_trans_raw(4, ord('c'), 8)
    nfa.remove_trans_raw(4, ord('a'), 8)

    reachable = nfa.get_reachable_states()
    assert 0 in reachable
    assert 1 not in reachable
    assert 2 in reachable
    assert 3 not in reachable
    assert 4 in reachable
    assert 5 not in reachable
    assert 6 in reachable
    assert 7 not in reachable
    assert 8 not in reachable
    assert 9 not in reachable
    assert 10 not in reachable
    assert 11 not in reachable
    assert 12 not in reachable
    assert 13 not in reachable
    assert 14 not in reachable

    nfa.remove_final_state(2)
    reachable = nfa.get_reachable_states()
    assert len(reachable) == 4
    assert 0 in reachable
    assert 2 in reachable
    assert 4 in reachable
    assert 6 in reachable

    useful = nfa.get_useful_states()
    assert len(useful) == 0

    nfa.make_final_state(4)
    useful = nfa.get_useful_states()
    assert len(useful) == 1
    assert 4 in useful


def test_segmentation(prepare_automaton_a):
    nfa = prepare_automaton_a()
    epsilon = ord('c')

    segmentation = mata.Segmentation(nfa, epsilon)
    epsilon_depths = segmentation.get_epsilon_depths()
    assert len(epsilon_depths) == 1
    assert 0 in epsilon_depths
    assert len(epsilon_depths[0]) == 3
    assert mata.Trans(10, epsilon, 7) in epsilon_depths[0]
    assert mata.Trans(7, epsilon, 3) in epsilon_depths[0]
    assert mata.Trans(5, epsilon, 9) in epsilon_depths[0]

    nfa = mata.Nfa(ord('q') + 1)
    nfa.make_initial_state(1)
    nfa.make_final_state(8)
    nfa.add_trans_raw(1, epsilon, 2)
    nfa.add_trans_raw(2, ord('a'), 3)
    nfa.add_trans_raw(2, ord('b'), 4)
    nfa.add_trans_raw(3, ord('b'), 6)
    nfa.add_trans_raw(4, ord('a'), 6)
    nfa.add_trans_raw(6, epsilon, 7)
    nfa.add_trans_raw(7, epsilon, 8)

    segmentation = mata.Segmentation(nfa, epsilon)
    epsilon_depths = segmentation.get_epsilon_depths()
    assert len(epsilon_depths) == 3
    assert 0 in epsilon_depths
    assert 1 in epsilon_depths
    assert 2 in epsilon_depths
    assert len(epsilon_depths[0]) == 1
    assert len(epsilon_depths[1]) == 1
    assert len(epsilon_depths[2]) == 1
    assert mata.Trans(1, epsilon, 2) in epsilon_depths[0]
    assert mata.Trans(6, epsilon, 7) in epsilon_depths[1]
    assert mata.Trans(7, epsilon, 8) in epsilon_depths[2]


def test_reduce():
    """Test reducing the automaton."""
    nfa = mata.Nfa()

    # Test the reduction of an empty automaton.
    result, state_map = mata.Nfa.reduce(nfa)
    assert result.trans_empty()
    assert len(result.initial_states) == 0
    assert len(result.final_states) == 0

    # Test the reduction of a simple automaton.
    nfa.resize(3)
    nfa.make_initial_state(1)
    nfa.make_final_state(2)
    result, state_map = mata.Nfa.reduce(nfa)
    assert result.trans_empty()
    assert result.get_num_of_states() == 2
    assert result.has_initial_state(state_map[1])
    assert result.has_final_state(state_map[2])
    assert state_map[1] == state_map[0]
    assert state_map[2] != state_map[0]

    # Test the reduction of a bigger automaton.
    nfa.resize(10)
    nfa.initial_states = {1, 2}
    nfa.final_states = {3, 9}
    nfa.add_trans_raw(1, ord('a'), 2)
    nfa.add_trans_raw(1, ord('a'), 3)
    nfa.add_trans_raw(1, ord('b'), 4)
    nfa.add_trans_raw(2, ord('a'), 2)
    nfa.add_trans_raw(2, ord('b'), 2)
    nfa.add_trans_raw(2, ord('a'), 3)
    nfa.add_trans_raw(2, ord('b'), 4)
    nfa.add_trans_raw(3, ord('b'), 4)
    nfa.add_trans_raw(3, ord('c'), 7)
    nfa.add_trans_raw(3, ord('b'), 2)
    nfa.add_trans_raw(5, ord('c'), 3)
    nfa.add_trans_raw(7, ord('a'), 8)
    nfa.add_trans_raw(9, ord('b'), 2)
    nfa.add_trans_raw(9, ord('c'), 0)
    nfa.add_trans_raw(0, ord('a'), 4)

    result, state_map = mata.Nfa.reduce(nfa)
    assert result.get_num_of_states() == 6
    assert result.has_initial_state(state_map[1])
    assert result.has_initial_state(state_map[2])
    assert result.has_final_state(state_map[9])
    assert result.has_final_state(state_map[3])
    assert result.has_trans_raw(state_map[9], ord('c'), state_map[0])
    assert result.has_trans_raw(state_map[9], ord('c'), state_map[7])
    assert result.has_trans_raw(state_map[3], ord('c'), state_map[0])
    assert result.has_trans_raw(state_map[0], ord('a'), state_map[8])
    assert result.has_trans_raw(state_map[7], ord('a'), state_map[4])
    assert result.has_trans_raw(state_map[1], ord('a'), state_map[3])
    assert not result.has_trans_raw(state_map[3], ord('b'), state_map[4])
    assert result.has_trans_raw(state_map[2], ord('a'), state_map[2])


def test_noodlify():
    """Test noodlification."""
    left1 = mata.Nfa(3)
    left1.make_initial_state(0)
    left1.make_final_states([1, 2])
    left1.add_trans_raw(0, ord('a'), 1)
    left1.add_trans_raw(0, ord('b'), 2)

    left2 = mata.Nfa(2)
    left2.make_initial_state(0)
    left2.make_final_state(1)
    left2.add_trans_raw(0, ord('a'), 1)

    left3 = mata.Nfa(2)
    left3.make_initial_state(0)
    left3.make_final_state(1)
    left3.add_trans_raw(0, ord('b'), 1)

    noodle1 = mata.Nfa(6)
    noodle1.make_initial_state(0)
    noodle1.make_final_state(5)
    noodle1.add_trans_raw(0, ord('a'), 1)
    noodle1.add_trans_raw(1, ord('c'), 2)  # The automatically chosen epsilon symbol(one larger than ord('b')).
    noodle1.add_trans_raw(2, ord('a'), 3)
    noodle1.add_trans_raw(3, ord('c'), 4)
    noodle1.add_trans_raw(4, ord('b'), 5)

    noodle2 = mata.Nfa(6)
    noodle2.make_initial_state(0)
    noodle2.make_final_state(5)
    noodle2.add_trans_raw(0, ord('b'), 1)
    noodle2.add_trans_raw(1, ord('c'), 2)
    noodle2.add_trans_raw(2, ord('a'), 3)
    noodle2.add_trans_raw(3, ord('c'), 4)
    noodle2.add_trans_raw(4, ord('b'), 5)

    left_side: list[Nfa] = [left1, left2, left3]

    right_side = mata.Nfa(7)
    right_side.make_initial_state(0)
    right_side.add_trans_raw(0, ord('a'), 1)
    right_side.add_trans_raw(1, ord('a'), 2)
    right_side.add_trans_raw(2, ord('b'), 3)
    right_side.add_trans_raw(0, ord('b'), 4)
    right_side.add_trans_raw(4, ord('a'), 5)
    right_side.add_trans_raw(5, ord('b'), 6)
    right_side.make_final_states([3, 6])

    result = mata.Nfa.noodlify_for_equation(left_side, right_side)
    assert len(result) == 2
    assert mata.Nfa.equivalence_check(result[0], noodle1)
    assert mata.Nfa.equivalence_check(result[1], noodle2)
