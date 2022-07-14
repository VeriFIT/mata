__author__ = 'Tomas Fiedor'

import pytest
import pynfa


def test_regex():
    """Test basic functionality of regex conversion"""
    symbol_map = {"a": ord("a"), "b": ord("b"), "c": ord("c")}
    lhs = pynfa.Nfa.from_regex("a|b")
    rhs = pynfa.Nfa.from_regex("b|c")

    union = pynfa.Nfa.union(lhs, rhs)
    assert pynfa.Nfa.is_in_lang(union, pynfa.Nfa.encode_word(symbol_map, "a"))
    assert pynfa.Nfa.is_in_lang(union, pynfa.Nfa.encode_word(symbol_map, "b"))
    assert pynfa.Nfa.is_in_lang(union, pynfa.Nfa.encode_word(symbol_map, "c"))

    intersection, _ = pynfa.Nfa.intersection(lhs, rhs)
    assert not pynfa.Nfa.is_in_lang(intersection, pynfa.Nfa.encode_word(symbol_map, "a"))
    assert pynfa.Nfa.is_in_lang(intersection, pynfa.Nfa.encode_word(symbol_map, "b"))
    assert not pynfa.Nfa.is_in_lang(intersection, pynfa.Nfa.encode_word(symbol_map, "c"))

