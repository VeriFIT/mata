__author__ = 'Tomas Fiedor'

import pytest

import libmata.parser as parser
import libmata.nfa.nfa as mata_nfa


def test_regex():
    """Test basic functionality of regex conversion"""
    symbol_map = {"a": ord("a"), "b": ord("b"), "c": ord("c")}
    lhs = parser.from_regex("a|b")
    rhs = parser.from_regex("b|c")

    union = mata_nfa.Nfa.union(lhs, rhs)
    assert mata_nfa.Nfa.is_in_lang(union, mata_nfa.Nfa.encode_word(symbol_map, "a"))
    assert mata_nfa.Nfa.is_in_lang(union, mata_nfa.Nfa.encode_word(symbol_map, "b"))
    assert mata_nfa.Nfa.is_in_lang(union, mata_nfa.Nfa.encode_word(symbol_map, "c"))

    intersection = mata_nfa.Nfa.intersection(lhs, rhs)
    assert not mata_nfa.Nfa.is_in_lang(intersection, mata_nfa.Nfa.encode_word(symbol_map, "a"))
    assert mata_nfa.Nfa.is_in_lang(intersection, mata_nfa.Nfa.encode_word(symbol_map, "b"))
    assert not mata_nfa.Nfa.is_in_lang(intersection, mata_nfa.Nfa.encode_word(symbol_map, "c"))


def test_stars_concatenation():
    aut = parser.from_regex("(((c)*)((a)*))")
    expected = mata_nfa.Nfa(2)
    expected.make_initial_state(0)
    expected.make_final_states([0, 1])
    expected.add_transition(0, ord('c'), 0)
    expected.add_transition(0, ord('a'), 1)
    expected.add_transition(1, ord('a'), 1)
    assert mata_nfa.Nfa.equivalence_check(aut, expected)
