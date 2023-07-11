__author__ = 'Tomas Fiedor'

import pytest
import libmata.nfa as mata
import libmata.parser as parser


def test_regex():
    """Test basic functionality of regex conversion"""
    symbol_map = {"a": ord("a"), "b": ord("b"), "c": ord("c")}
    lhs = parser.from_regex("a|b")
    rhs = parser.from_regex("b|c")

    union = mata.Nfa.union(lhs, rhs)
    assert mata.Nfa.is_in_lang(union, mata.Nfa.encode_word(symbol_map, "a"))
    assert mata.Nfa.is_in_lang(union, mata.Nfa.encode_word(symbol_map, "b"))
    assert mata.Nfa.is_in_lang(union, mata.Nfa.encode_word(symbol_map, "c"))

    intersection = mata.Nfa.intersection(lhs, rhs)
    assert not mata.Nfa.is_in_lang(intersection, mata.Nfa.encode_word(symbol_map, "a"))
    assert mata.Nfa.is_in_lang(intersection, mata.Nfa.encode_word(symbol_map, "b"))
    assert not mata.Nfa.is_in_lang(intersection, mata.Nfa.encode_word(symbol_map, "c"))


def test_stars_concatenation():
    aut = parser.from_regex("(((c)*)((a)*))")
    expected = mata.Nfa(2)
    expected.make_initial_state(0)
    expected.make_final_states([0, 1])
    expected.add_transition(0, ord('c'), 0)
    expected.add_transition(0, ord('a'), 1)
    expected.add_transition(1, ord('a'), 1)
    assert mata.Nfa.equivalence_check(aut, expected)
