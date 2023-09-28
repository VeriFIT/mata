__author__ = 'Tomas Fiedor'

import libmata.parser as parser
import libmata.nfa.nfa as mata_nfa
import libmata.alphabets as mata_alphabets


def test_regex():
    """Test basic functionality of regex conversion"""
    alphabet = mata_alphabets.OnTheFlyAlphabet.from_symbol_map({"a": ord("a"), "b": ord("b"), "c": ord("c")})
    lhs = parser.from_regex("a|b")
    rhs = parser.from_regex("b|c")

    union = mata_nfa.union(lhs, rhs)
    assert union.is_in_lang(mata_nfa.encode_word(alphabet, "a"))
    assert union.is_in_lang(mata_nfa.encode_word(alphabet, "b"))
    assert union.is_in_lang(mata_nfa.encode_word(alphabet, "c"))

    intersection = mata_nfa.intersection(lhs, rhs)
    assert not intersection.is_in_lang(mata_nfa.encode_word(alphabet, "a"))
    assert intersection.is_in_lang(mata_nfa.encode_word(alphabet, "b"))
    assert not intersection.is_in_lang(mata_nfa.encode_word(alphabet, "c"))


def test_stars_concatenation():
    aut = parser.from_regex("(((c)*)((a)*))")
    expected = mata_nfa.Nfa(2)
    expected.make_initial_state(0)
    expected.make_final_states([0, 1])
    expected.add_transition(0, ord('c'), 0)
    expected.add_transition(0, ord('a'), 1)
    expected.add_transition(1, ord('a'), 1)
    assert mata_nfa.equivalence_check(aut, expected)
