"""Basic tests for utility package and sanity checks"""

import os
import pytest

import libmata.alphabets as alphabets
import libmata.nfa.nfa as mata_nfa
import libmata.nfa.strings as mata_strings
import libmata.utils as mata_utils

__author__ = 'Tomas Fiedor'


def test_adding_states():
    """Test nfa"""
    lhs = mata_nfa.Nfa(5)

    # Test adding states
    assert lhs.size() == 5
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

    rhs = mata_nfa.Nfa()
    assert rhs.size() == 0
    state = rhs.add_new_state()
    assert state == 0
    assert rhs.size() == 1
    state = rhs.add_new_state()
    assert state == 1
    assert rhs.size() == 2

    rhs.add_state(9)
    assert rhs.size() == 10
    for i in range(0, 10):
        assert rhs.is_state(i)

    assert rhs.size() == 10
    rhs.clear()
    assert rhs.size() == 0

    rhs.add_state(0)
    assert rhs.size() == 1
    assert rhs.is_state(0)
    assert not rhs.is_state(1)

    with pytest.raises(OverflowError):
        rhs.add_state(-10)

    rhs.add_state(11)
    assert rhs.size() == 12
    assert rhs.is_state(11)
    assert not rhs.is_state(12)

    with pytest.raises(OverflowError):
        rhs.add_state(-10)


def test_making_initial_and_final_states():
    """Test making states in the automaton initial and final."""
    nfa = mata_nfa.Nfa(10)
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

    nfa.remove_initial_state(1)
    nfa.remove_initial_state(2)
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 3
    assert not nfa.has_final_state(0)
    assert nfa.has_final_state(1)
    assert nfa.has_final_state(2)
    assert nfa.has_final_state(3)

    nfa.remove_final_state(1)
    nfa.remove_final_state(2)
    assert len(nfa.initial_states) == 1
    assert nfa.has_initial_state(3)
    assert len(nfa.final_states) == 1
    assert nfa.has_final_state(3)

    nfa.clear_initial()
    assert len(nfa.initial_states) == 0
    assert len(nfa.final_states) == 1

    nfa.clear_final()
    assert len(nfa.initial_states) == 0
    assert len(nfa.final_states) == 0


def test_post(binary_alphabet):
    """Test various cases of getting post of the states
    :return:
    """
    lhs = mata_nfa.Nfa(3)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 1)
    lhs.add_transition(1, 1, 2)
    lhs.add_transition(0, 1, 2)
    lhs.add_transition(1, 0, 0)
    lhs.add_transition(2, 1, 2)
    lhs.add_transition(2, 0, 2)
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
    assert not mata_nfa.is_deterministic(lhs)
    rhs = dfa_one_state_uni
    assert mata_nfa.is_deterministic(rhs)

    chs, sm_map = mata_nfa.determinize_with_subset_map(lhs)
    assert mata_nfa.is_deterministic(chs)
    assert sm_map == {(0,): 0, (0, 1): 1}


def test_forward_reach_states(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert fa_one_divisible_by_two.get_reachable_states() == set(range(0, 3))
    assert fa_one_divisible_by_four.get_reachable_states() == set(range(0, 5))
    assert fa_one_divisible_by_eight.get_reachable_states() == set(range(0, 9))


def test_get_word_for_path(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert mata_nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2]) == ([1, 1], True)
    assert mata_nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2, 0]) == ([], False)
    assert mata_nfa.get_word_for_path(fa_one_divisible_by_two, [0, 1, 2, 2]) == ([1, 1, 0], True)
    assert mata_nfa.get_word_for_path(
        fa_one_divisible_by_four, [0, 1, 2, 3, 4]
    ) == ([1, 1, 1, 1], True)
    assert mata_nfa.get_word_for_path(
        fa_one_divisible_by_eight, [0, 1, 2, 3, 4, 5, 6, 7, 8]
    ) == ([1, 1, 1, 1, 1, 1, 1, 1], True)


def test_encode_word():
    assert mata_nfa.encode_word(alphabets.OnTheFlyAlphabet.from_symbol_map({'a': 1, 'b': 2, "c": 0}), "abca") \
           == [1, 2, 0, 1]


def test_language_emptiness(fa_one_divisible_by_two):
    cex = mata_nfa.Run()
    assert not mata_nfa.is_lang_empty(fa_one_divisible_by_two, cex)
    assert cex.path == [0, 1, 2]
    assert cex.word == [1, 1]

    lhs = mata_nfa.Nfa(4)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 1)
    lhs.add_transition(1, 0, 2)
    lhs.add_transition(2, 0, 3)
    cex = mata_nfa.Run()
    assert mata_nfa.is_lang_empty(lhs, cex)
    assert cex.word == []
    assert cex.path == []


def test_universality(fa_one_divisible_by_two):
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    assert mata_nfa.is_universal(fa_one_divisible_by_two, alph) == False

    l = mata_nfa.Nfa(1)
    l.make_initial_state(0)
    l.add_transition(0, 0, 0)
    l.add_transition(0, 1, 0)
    l.make_final_state(0)
    assert mata_nfa.is_universal(l, alph) == True


def test_inclusion(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    result, cex = mata_nfa.is_included_with_cex(fa_one_divisible_by_two, fa_one_divisible_by_four, alph)
    assert not result
    assert cex.word == [1, 1]
    result, cex = mata_nfa.is_included_with_cex(fa_one_divisible_by_two, fa_one_divisible_by_four)
    assert not result
    assert cex.word == [1, 1]

    result, cex = mata_nfa.is_included_with_cex(fa_one_divisible_by_four, fa_one_divisible_by_two, alph)
    assert result
    assert cex.word == []
    result, cex = mata_nfa.is_included_with_cex(fa_one_divisible_by_four, fa_one_divisible_by_two)
    assert result
    assert cex.word == []

    assert mata_nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_two, alph)
    assert mata_nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_four, alph)
    assert not mata_nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_eight, alph)
    assert not mata_nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_eight, alph)
    assert mata_nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_two)
    assert mata_nfa.is_included(fa_one_divisible_by_eight, fa_one_divisible_by_four)
    assert not mata_nfa.is_included(fa_one_divisible_by_two, fa_one_divisible_by_eight)
    assert not mata_nfa.is_included(fa_one_divisible_by_four, fa_one_divisible_by_eight)

    # Test equivalence of two NFAs.
    smaller = mata_nfa.Nfa(10)
    bigger = mata_nfa.Nfa(16)
    symbol_map = {"a": ord("a"), "b": ord("b")}
    smaller.make_initial_state(1)
    smaller.make_final_state(1)
    smaller.add_transition(1, ord('a'), 1)
    smaller.add_transition(1, ord('b'), 1)

    bigger.make_initial_state(11)
    bigger.make_final_states([11, 12, 13, 14, 15])

    bigger.add_transition(11, ord('a'), 12)
    bigger.add_transition(11, ord('b'), 12)
    bigger.add_transition(12, ord('a'), 13)
    bigger.add_transition(12, ord('b'), 13)

    bigger.add_transition(13, ord('a'), 14)
    bigger.add_transition(14, ord('a'), 14)

    bigger.add_transition(13, ord('b'), 15)
    bigger.add_transition(15, ord('b'), 15)

    assert not mata_nfa.equivalence_check(smaller, bigger, alphabets.OnTheFlyAlphabet.from_symbol_map(symbol_map))
    assert not mata_nfa.equivalence_check(smaller, bigger)
    assert not mata_nfa.equivalence_check(bigger, smaller, alphabets.OnTheFlyAlphabet.from_symbol_map(symbol_map))
    assert not mata_nfa.equivalence_check(bigger, smaller)

    smaller = mata_nfa.Nfa(10)
    bigger = mata_nfa.Nfa(16)
    symbol_map = {}
    smaller.initial_states = [1]
    smaller.final_states = [1]
    bigger.initial_states = [11]
    bigger.final_states = [11]

    assert mata_nfa.equivalence_check(smaller, bigger, alphabets.OnTheFlyAlphabet.from_symbol_map(symbol_map))
    assert mata_nfa.equivalence_check(smaller, bigger)
    assert mata_nfa.equivalence_check(bigger, smaller, alphabets.OnTheFlyAlphabet.from_symbol_map(symbol_map))
    assert mata_nfa.equivalence_check(bigger, smaller)


def test_concatenate():
    lhs = mata_nfa.Nfa(2)
    lhs.make_initial_state(0)
    lhs.make_final_state(1)
    lhs.add_transition(0, ord('b'), 1)

    rhs = mata_nfa.Nfa(2)
    rhs.make_initial_state(0)
    rhs.make_final_state(1)
    rhs.add_transition(0, ord('a'), 1)

    result = mata_nfa.concatenate(lhs, rhs)

    assert not mata_nfa.is_lang_empty(result)
    shortest_words = mata_strings.get_shortest_words(result)
    assert len(shortest_words) == 1
    assert [ord('b'), ord('a')] in shortest_words

    result = mata_nfa.concatenate(lhs, rhs, True)
    assert result.has_initial_state(0)
    assert result.has_final_state(3)
    assert result.size() == 4
    assert result.has_transition(0, ord('b'), 1)
    assert result.has_transition(1, mata_nfa.epsilon(), 2)
    assert result.has_transition(2, ord('a'), 3)

    result, _, rhs_map = mata_nfa.concatenate_with_result_state_maps(lhs, rhs, True)
    assert result.has_initial_state(0)
    assert result.has_final_state(3)
    assert result.size() == 4
    assert result.has_transition(0, ord('b'), 1)
    assert result.has_transition(1, mata_nfa.epsilon(), 2)
    assert result.has_transition(2, ord('a'), 3)


def test_completeness(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")
    assert mata_nfa.is_complete(fa_one_divisible_by_two, alph)
    assert mata_nfa.is_complete(fa_one_divisible_by_four, alph)
    assert mata_nfa.is_complete(fa_one_divisible_by_eight, alph)

    l = mata_nfa.Nfa(1)
    l.make_initial_state(0)
    l.add_transition(0, 0, 0)
    assert not mata_nfa.is_complete(l, alph)
    l.add_transition(0, 1, 0)
    assert mata_nfa.is_complete(l, alph)

    r = mata_nfa.Nfa(1)
    r.make_initial_state(0)
    r.add_transition(0, 0, 0)
    assert not mata_nfa.is_complete(r, alph)
    mata_nfa.make_complete(r, 1, alph)
    assert mata_nfa.is_complete(r, alph)


def test_in_language(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    assert mata_nfa.is_in_lang(fa_one_divisible_by_two, [1, 1])
    assert not mata_nfa.is_in_lang(fa_one_divisible_by_two, [1, 1, 1])

    assert mata_nfa.is_prefix_in_lang(fa_one_divisible_by_four, [1, 1, 1, 1, 0])
    assert not mata_nfa.is_prefix_in_lang(fa_one_divisible_by_four, [1, 1, 1, 0, 0])
    assert not mata_nfa.accepts_epsilon(fa_one_divisible_by_four)

    lhs = mata_nfa.Nfa(2)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 1)
    assert not mata_nfa.accepts_epsilon(lhs)
    lhs.make_final_state(1)
    assert not mata_nfa.accepts_epsilon(lhs)
    lhs.make_final_state(0)
    assert mata_nfa.accepts_epsilon(lhs)


def test_union(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    assert mata_nfa.is_in_lang(fa_one_divisible_by_two, [1, 1])
    assert not mata_nfa.is_in_lang(fa_one_divisible_by_four, [1, 1])
    uni = mata_nfa.union(fa_one_divisible_by_two, fa_one_divisible_by_four)
    assert mata_nfa.is_in_lang(uni, [1, 1])
    assert mata_nfa.is_in_lang(uni, [1, 1, 1, 1])
    assert mata_nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1])
    assert mata_nfa.is_in_lang(uni, [1, 1, 1, 1, 1, 1, 1, 1, ])
    assert mata_nfa.is_included(fa_one_divisible_by_two, uni, alph)
    assert mata_nfa.is_included(fa_one_divisible_by_four, uni, alph)

    assert mata_nfa.is_included(fa_one_divisible_by_two, uni)
    assert mata_nfa.is_included(fa_one_divisible_by_four, uni)


def test_intersection(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    inter, product_map = mata_nfa.intersection_with_product_map(fa_one_divisible_by_two, fa_one_divisible_by_four)

    assert not mata_nfa.is_in_lang(inter, [1, 1])
    assert mata_nfa.is_in_lang(inter, [1, 1, 1, 1])
    assert not mata_nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1])
    assert mata_nfa.is_in_lang(inter, [1, 1, 1, 1, 1, 1, 1, 1, ])
    assert mata_nfa.is_included(inter, fa_one_divisible_by_two, alph)
    assert mata_nfa.is_included(inter, fa_one_divisible_by_four, alph)
    assert mata_nfa.is_included(inter, fa_one_divisible_by_two)
    assert mata_nfa.is_included(inter, fa_one_divisible_by_four)
    assert product_map == {(0, 0): 0, (1, 1): 1, (1, 3): 3, (2, 2): 2, (2, 4): 4}


def test_intersection_preserving_epsilon_transitions():
    a = mata_nfa.Nfa(6)
    a.make_initial_state(0)
    a.make_final_states([1, 4, 5])
    a.add_transition(0, mata_nfa.epsilon(), 1)
    a.add_transition(1, ord('a'), 1)
    a.add_transition(1, ord('b'), 1)
    a.add_transition(1, ord('c'), 2)
    a.add_transition(2, ord('b'), 4)
    a.add_transition(2, mata_nfa.epsilon(), 3)
    a.add_transition(3, ord('a'), 5)

    b = mata_nfa.Nfa(10)
    b.make_initial_state(0)
    b.make_final_states([2, 4, 8, 7])
    b.add_transition(0, ord('b'), 1)
    b.add_transition(0, ord('a'), 2)
    b.add_transition(2, ord('a'), 4)
    b.add_transition(2, mata_nfa.epsilon(), 3)
    b.add_transition(3, ord('b'), 4)
    b.add_transition(0, ord('c'), 5)
    b.add_transition(5, ord('a'), 8)
    b.add_transition(5, mata_nfa.epsilon(), 6)
    b.add_transition(6, ord('a'), 9)
    b.add_transition(6, ord('b'), 7)

    result, product_map = mata_nfa.intersection_with_product_map(a, b, True)

    # Check states.
    assert result.size() == 13
    assert result.is_state(product_map[(0, 0)])
    assert result.is_state(product_map[(1, 0)])
    assert result.is_state(product_map[(1, 1)])
    assert result.is_state(product_map[(1, 2)])
    assert result.is_state(product_map[(1, 3)])
    assert result.is_state(product_map[(1, 4)])
    assert result.is_state(product_map[(2, 5)])
    assert result.is_state(product_map[(3, 5)])
    assert result.is_state(product_map[(2, 6)])
    assert result.is_state(product_map[(3, 6)])
    assert result.is_state(product_map[(4, 7)])
    assert result.is_state(product_map[(5, 9)])
    assert result.is_state(product_map[(5, 8)])

    assert result.has_initial_state(product_map[(0, 0)])
    assert len(result.initial_states) == 1
    assert result.has_final_state(product_map[(1, 2)])
    assert result.has_final_state(product_map[(1, 4)])
    assert result.has_final_state(product_map[(4, 7)])
    assert result.has_final_state(product_map[(5, 8)])
    assert len(result.final_states) == 4

    # Check transitions.
    assert result.get_num_of_transitions() == 15

    assert result.has_transition(product_map[(0, 0)], mata_nfa.epsilon(), product_map[(1, 0)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(0, 0)])) == 1

    assert result.has_transition(product_map[(1, 0)], ord('b'), product_map[(1, 1)])
    assert result.has_transition(product_map[(1, 0)], ord('a'), product_map[(1, 2)])
    assert result.has_transition(product_map[(1, 0)], ord('c'), product_map[(2, 5)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(1, 0)])) == 3

    assert len(result.get_trans_from_state_as_sequence(product_map[(1, 1)])) == 0

    assert result.has_transition(product_map[(1, 2)], mata_nfa.epsilon(), product_map[(1, 3)])
    assert result.has_transition(product_map[(1, 2)], ord('a'), product_map[(1, 4)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(1, 2)])) == 2

    assert result.has_transition(product_map[(1, 3)], ord('b'), product_map[(1, 4)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(1, 3)])) == 1

    assert len(result.get_trans_from_state_as_sequence(product_map[(1, 4)])) == 0

    assert result.has_transition(product_map[(2, 5)], mata_nfa.epsilon(), product_map[(3, 5)])
    assert result.has_transition(product_map[(2, 5)], mata_nfa.epsilon(), product_map[(2, 6)])
    assert result.has_transition(product_map[(2, 5)], mata_nfa.epsilon(), product_map[(3, 6)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(2, 5)])) == 3

    assert result.has_transition(product_map[(3, 5)], ord('a'), product_map[(5, 8)])
    assert result.has_transition(product_map[(3, 5)], mata_nfa.epsilon(), product_map[(3, 6)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(3, 5)])) == 2

    assert result.has_transition(product_map[(2, 6)], ord('b'), product_map[(4, 7)])
    assert result.has_transition(product_map[(2, 6)], mata_nfa.epsilon(), product_map[(3, 6)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(2, 6)])) == 2

    assert result.has_transition(product_map[(3, 6)], ord('a'), product_map[(5, 9)])
    assert len(result.get_trans_from_state_as_sequence(product_map[(3, 6)])) == 1

    assert len(result.get_trans_from_state_as_sequence(product_map[(4, 7)])) == 0

    assert len(result.get_trans_from_state_as_sequence(product_map[(5, 9)])) == 0

    assert len(result.get_trans_from_state_as_sequence(product_map[(5, 8)])) == 0


def test_complement(
        fa_one_divisible_by_two
):
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("a")
    alph.translate_symbol("b")

    res = mata_nfa.complement(fa_one_divisible_by_two, alph)
    assert not mata_nfa.is_in_lang(res, [1, 1])
    assert mata_nfa.is_in_lang(res, [1, 1, 1])
    assert not mata_nfa.is_in_lang(res, [1, 1, 1, 1])


def test_revert():
    lhs = mata_nfa.Nfa(3)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 1)
    lhs.add_transition(1, 1, 2)
    lhs.make_final_state(2)
    assert mata_nfa.is_in_lang(lhs, [0, 1])
    assert not mata_nfa.is_in_lang(lhs, [1, 0])

    rhs = mata_nfa.revert(lhs)
    assert not mata_nfa.is_in_lang(rhs, [0, 1])
    assert mata_nfa.is_in_lang(rhs, [1, 0])


def test_removing_epsilon():
    lhs = mata_nfa.Nfa(3)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 1)
    lhs.add_transition(1, 1, 2)
    lhs.add_transition(0, 2, 2)
    lhs.make_final_state(2)

    rhs = mata_nfa.remove_epsilon(lhs, 2)
    assert rhs.has_transition(0, 0, 1)
    assert rhs.has_transition(1, 1, 2)
    assert not rhs.has_transition(0, 2, 2)


def test_minimize(
        fa_one_divisible_by_two, fa_one_divisible_by_four, fa_one_divisible_by_eight
):
    minimized = mata_nfa.minimize(fa_one_divisible_by_two)
    assert minimized.get_num_of_transitions() <= fa_one_divisible_by_two.get_num_of_transitions()
    minimized = mata_nfa.minimize(fa_one_divisible_by_four)
    assert minimized.get_num_of_transitions() <= fa_one_divisible_by_four.get_num_of_transitions()
    minimized = mata_nfa.minimize(fa_one_divisible_by_eight)
    assert minimized.get_num_of_transitions() <= fa_one_divisible_by_eight.get_num_of_transitions()

    lhs = mata_nfa.Nfa(11)
    lhs.make_initial_state(0)
    for i in range(0, 10):
        lhs.add_transition(i, 0, i + 1)
        lhs.make_final_state(i)
    lhs.add_transition(10, 0, 10)
    lhs.make_final_state(10)
    assert lhs.get_num_of_transitions() == 11

    minimized = mata_nfa.minimize(lhs)
    assert minimized.get_num_of_transitions() == 1


def test_to_dot():
    lhs = mata_nfa.Nfa()
    expected = "digraph finiteAutomaton {\nnode [shape=circle];\nnode [shape=none, label=\"\"];\n}\n"
    assert lhs.to_dot_str() == expected

    lhs.to_dot_file('test.dot')
    assert 'test.dot.pdf' in os.listdir('.')
    assert 'test.dot' in os.listdir('.')
    with open('test.dot', 'r') as test_handle:
        lines = test_handle.read()
    assert lines == expected


def test_to_str():
    lhs = mata_nfa.Nfa(2)
    lhs.make_initial_state(0)
    lhs.make_final_state(1)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 1)
    expected = "initial_states: [0]\nfinal_states: [1]\ntransitions:\n0-[0]→0\n0-[1]→1\n"
    assert str(lhs) == expected


def test_shortest(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    shortest = mata_strings.get_shortest_words(lhs)
    assert shortest == [[1, 1]]


def test_get_trans(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    t = lhs.get_transitions_from_state(0)
    assert sorted(t) == sorted([mata_nfa.SymbolPost(0, [0]), mata_nfa.SymbolPost(1, [1])])
    tt = lhs.get_transitions_from_state(1)
    assert sorted(tt) == sorted([mata_nfa.SymbolPost(0, [1]), mata_nfa.SymbolPost(1, [2])])


def test_trim(prepare_automaton_a):
    """Test trimming the automaton."""
    nfa = prepare_automaton_a()
    nfa.remove_trans_raw(1, ord('a'), 10)

    old_nfa = prepare_automaton_a()
    old_nfa.remove_trans_raw(1, ord('a'), 10)

    nfa.trim()

    assert len(nfa.initial_states) == len(old_nfa.initial_states)
    assert len(nfa.final_states) == len(old_nfa.final_states)

    for word in mata_strings.get_shortest_words(old_nfa):
        assert mata_nfa.is_in_lang(nfa, word)

    nfa.remove_final_state(2)  # '2' is the new final state in the earlier trimmed automaton.
    nfa.trim()
    assert nfa.get_num_of_transitions() == 0
    assert nfa.size() == 0


def test_get_one_letter_automaton(prepare_automaton_a):
    """Test creating one letter automaton from an input automaton."""
    abstract_symbol = ord('x')
    nfa = prepare_automaton_a()

    one_letter_automaton = nfa.get_one_letter_aut()

    assert one_letter_automaton.size() == nfa.size()
    assert one_letter_automaton.get_num_of_transitions() == 12
    assert one_letter_automaton.has_transition(1, abstract_symbol, 10)
    assert one_letter_automaton.has_transition(10, abstract_symbol, 7)
    assert not one_letter_automaton.has_transition(10, ord('a'), 7)
    assert not one_letter_automaton.has_transition(10, ord('b'), 7)
    assert not one_letter_automaton.has_transition(10, ord('c'), 7)


def test_simulation(fa_one_divisible_by_four):
    lhs = fa_one_divisible_by_four
    rel = mata_nfa.compute_relation(lhs)
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
    rel = mata_nfa.compute_relation(lhs)
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
    rel.alloc()
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
    r = mata_utils.BinaryRelation(3, True, 3)
    classes, heads = r.build_equivalence_classes()
    assert classes == [0, 0, 0]
    assert heads == [0]
    r.reset(False)
    classes, heads = r.build_equivalence_classes()
    assert classes == [0, 1, 2]
    assert heads == [0, 1, 2]


def test_simulation_indexes(fa_one_divisible_by_two):
    lhs = fa_one_divisible_by_two
    rel = mata_nfa.compute_relation(lhs)
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

    useful_states = nfa.get_useful_states()
    assert len(useful_states) == 0

    nfa.make_final_state(4)
    useful_states = nfa.get_useful_states()
    assert len(useful_states) == 1
    assert 4 in useful_states


def test_segmentation(prepare_automaton_a):
    nfa = prepare_automaton_a()
    epsilon = ord('c')

    segmentation = mata_strings.Segmentation(nfa, [epsilon])
    epsilon_depths = segmentation.get_epsilon_depths()
    assert len(epsilon_depths) == 1
    assert 0 in epsilon_depths
    assert len(epsilon_depths[0]) == 3
    assert mata_nfa.Transition(10, epsilon, 7) in epsilon_depths[0]
    assert mata_nfa.Transition(7, epsilon, 3) in epsilon_depths[0]
    assert mata_nfa.Transition(5, epsilon, 9) in epsilon_depths[0]

    nfa = mata_nfa.Nfa(ord('q') + 1)
    nfa.make_initial_state(1)
    nfa.make_final_state(8)
    nfa.add_transition(1, epsilon, 2)
    nfa.add_transition(2, ord('a'), 3)
    nfa.add_transition(2, ord('b'), 4)
    nfa.add_transition(3, ord('b'), 6)
    nfa.add_transition(4, ord('a'), 6)
    nfa.add_transition(6, epsilon, 7)
    nfa.add_transition(7, epsilon, 8)

    segmentation = mata_strings.Segmentation(nfa, [epsilon])
    epsilon_depths = segmentation.get_epsilon_depths()
    assert len(epsilon_depths) == 3
    assert 0 in epsilon_depths
    assert 1 in epsilon_depths
    assert 2 in epsilon_depths
    assert len(epsilon_depths[0]) == 1
    assert len(epsilon_depths[1]) == 1
    assert len(epsilon_depths[2]) == 1
    assert mata_nfa.Transition(1, epsilon, 2) in epsilon_depths[0]
    assert mata_nfa.Transition(6, epsilon, 7) in epsilon_depths[1]
    assert mata_nfa.Transition(7, epsilon, 8) in epsilon_depths[2]


def test_reduce():
    """Test reducing the automaton."""
    nfa = mata_nfa.Nfa()

    # Test the reduction of an empty automaton.
    result, state_map = mata_nfa.reduce_with_state_map(nfa)
    assert result.get_num_of_transitions() == 0
    assert len(result.initial_states) == 0
    assert len(result.final_states) == 0

    # Test the reduction of a simple automaton.
    nfa.add_state(2)
    nfa.make_initial_state(1)
    nfa.make_final_state(2)
    result, state_map = mata_nfa.reduce_with_state_map(nfa)
    assert result.get_num_of_transitions() == 0
    assert result.size() == 2
    assert result.has_initial_state(state_map[1])
    assert result.has_final_state(state_map[2])
    assert state_map[1] == state_map[0]
    assert state_map[2] != state_map[0]

    result, state_map = mata_nfa.reduce_with_state_map(nfa.trim())
    assert result.get_num_of_transitions() == 0
    assert result.size() == 0

    # Test the reduction of a bigger automaton.
    nfa.add_state(9)
    nfa.initial_states = {1, 2}
    nfa.final_states = {3, 9}
    nfa.add_transition(1, ord('a'), 2)
    nfa.add_transition(1, ord('a'), 3)
    nfa.add_transition(1, ord('b'), 4)
    nfa.add_transition(2, ord('a'), 2)
    nfa.add_transition(2, ord('b'), 2)
    nfa.add_transition(2, ord('a'), 3)
    nfa.add_transition(2, ord('b'), 4)
    nfa.add_transition(3, ord('b'), 4)
    nfa.add_transition(3, ord('c'), 7)
    nfa.add_transition(3, ord('b'), 2)
    nfa.add_transition(5, ord('c'), 3)
    nfa.add_transition(7, ord('a'), 8)
    nfa.add_transition(9, ord('b'), 2)
    nfa.add_transition(9, ord('c'), 0)
    nfa.add_transition(0, ord('a'), 4)

    result, state_map = mata_nfa.reduce_with_state_map(nfa)
    assert result.size() == 6
    assert result.has_initial_state(state_map[1])
    assert result.has_initial_state(state_map[2])
    assert result.has_final_state(state_map[9])
    assert result.has_final_state(state_map[3])
    assert result.has_transition(state_map[9], ord('c'), state_map[0])
    assert result.has_transition(state_map[9], ord('c'), state_map[7])
    assert result.has_transition(state_map[3], ord('c'), state_map[0])
    assert result.has_transition(state_map[0], ord('a'), state_map[8])
    assert result.has_transition(state_map[7], ord('a'), state_map[4])
    assert result.has_transition(state_map[1], ord('a'), state_map[3])
    assert not result.has_transition(state_map[3], ord('b'), state_map[4])
    assert result.has_transition(state_map[2], ord('a'), state_map[2])


def test_noodlify():
    """Test noodlification."""
    left1 = mata_nfa.Nfa(3)
    left1.make_initial_state(0)
    left1.make_final_states([1, 2])
    left1.add_transition(0, ord('a'), 1)
    left1.add_transition(0, ord('b'), 2)

    left2 = mata_nfa.Nfa(2)
    left2.make_initial_state(0)
    left2.make_final_state(1)
    left2.add_transition(0, ord('a'), 1)

    left3 = mata_nfa.Nfa(2)
    left3.make_initial_state(0)
    left3.make_final_state(1)
    left3.add_transition(0, ord('b'), 1)

    noodle1_segment1 = mata_nfa.Nfa(2)
    noodle1_segment1.make_initial_state(0)
    noodle1_segment1.make_final_state(1)
    noodle1_segment1.add_transition(0, ord('a'), 1)

    noodle1_segment2 = mata_nfa.Nfa(2)
    noodle1_segment2.make_initial_state(0)
    noodle1_segment2.make_final_state(1)
    noodle1_segment2.add_transition(0, ord('a'), 1)

    noodle1_segment3 = mata_nfa.Nfa(2)
    noodle1_segment3.make_initial_state(0)
    noodle1_segment3.make_final_state(1)
    noodle1_segment3.add_transition(0, ord('b'), 1)

    noodle2_segment1 = mata_nfa.Nfa(2)
    noodle2_segment1.make_initial_state(0)
    noodle2_segment1.make_final_state(1)
    noodle2_segment1.add_transition(0, ord('b'), 1)

    noodle2_segment2 = mata_nfa.Nfa(2)
    noodle2_segment2.make_initial_state(0)
    noodle2_segment2.make_final_state(1)
    noodle2_segment2.add_transition(0, ord('a'), 1)

    noodle2_segment3 = mata_nfa.Nfa(2)
    noodle2_segment3.make_initial_state(0)
    noodle2_segment3.make_final_state(1)
    noodle2_segment3.add_transition(0, ord('b'), 1)

    right_side = mata_nfa.Nfa(7)
    right_side.make_initial_state(0)
    right_side.add_transition(0, ord('a'), 1)
    right_side.add_transition(1, ord('a'), 2)
    right_side.add_transition(2, ord('b'), 3)
    right_side.add_transition(0, ord('b'), 4)
    right_side.add_transition(4, ord('a'), 5)
    right_side.add_transition(5, ord('b'), 6)
    right_side.make_final_states([3, 6])

    left_side: list[Nfa] = [left1, left2, left3]
    result = mata_strings.noodlify_for_equation(left_side, right_side)
    assert len(result) == 2
    assert mata_nfa.equivalence_check(result[0][0], noodle1_segment1)
    assert mata_nfa.equivalence_check(result[0][1], noodle1_segment2)
    assert mata_nfa.equivalence_check(result[0][2], noodle1_segment3)

    assert mata_nfa.equivalence_check(result[1][0], noodle2_segment1)
    assert mata_nfa.equivalence_check(result[1][1], noodle2_segment2)
    assert mata_nfa.equivalence_check(result[1][2], noodle2_segment3)


def test_unify():
    nfa = mata_nfa.Nfa(10)
    nfa.make_initial_state(0)
    nfa.make_initial_state(1)
    nfa.add_transition(0, 1, 2)
    nfa.unify_initial()
    assert nfa.has_initial_state(10)
    assert nfa.has_transition(10, 1, 2)


def test_epsilon_symbol_posts():
    nfa = mata_nfa.Nfa(10)
    nfa.make_initial_state(0)
    nfa.make_final_state(1)

    nfa.add_transition(1, 1, 2)
    nfa.add_transition(1, 2, 2)
    nfa.add_transition(1, mata_nfa.epsilon(), 2)
    nfa.add_transition(1, mata_nfa.epsilon(), 3)
    epsilon_symbol_posts = nfa.epsilon_symbol_posts(1)
    assert epsilon_symbol_posts.symbol == mata_nfa.epsilon()
    assert epsilon_symbol_posts.targets == [2, 3]

    nfa.add_transition(0, 1, 2)
    nfa.add_transition(0, 2, 2)
    nfa.add_transition(0, 8, 5)
    nfa.add_transition(0, 8, 6)
    epsilon_symbol_posts = nfa.epsilon_symbol_posts(0, 8)
    assert epsilon_symbol_posts.symbol == 8
    assert epsilon_symbol_posts.targets == [5, 6]

    assert nfa.epsilon_symbol_posts(5) is None


def test_is_epsilon():
    nfa = mata_nfa.Nfa()
    assert nfa.is_epsilon(mata_nfa.epsilon())
    assert not nfa.is_epsilon(0)

    # TODO: Add checks for user-specified epsilons when user-specified epsilons are implemented.
