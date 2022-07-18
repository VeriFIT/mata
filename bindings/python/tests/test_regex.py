__author__ = 'Tomas Fiedor'

import pytest
import mata


def test_regex():
    """Test basic functionality of regex conversion"""
    symbol_map = {"a": ord("a"), "b": ord("b"), "c": ord("c")}
    lhs = mata.Nfa.from_regex("a|b")
    rhs = mata.Nfa.from_regex("b|c")

    union = mata.Nfa.union(lhs, rhs)
    assert mata.Nfa.is_in_lang(union, mata.Nfa.encode_word(symbol_map, "a"))
    assert mata.Nfa.is_in_lang(union, mata.Nfa.encode_word(symbol_map, "b"))
    assert mata.Nfa.is_in_lang(union, mata.Nfa.encode_word(symbol_map, "c"))

    intersection, _ = mata.Nfa.intersection(lhs, rhs)
    assert not mata.Nfa.is_in_lang(intersection, mata.Nfa.encode_word(symbol_map, "a"))
    assert mata.Nfa.is_in_lang(intersection, mata.Nfa.encode_word(symbol_map, "b"))
    assert not mata.Nfa.is_in_lang(intersection, mata.Nfa.encode_word(symbol_map, "c"))

