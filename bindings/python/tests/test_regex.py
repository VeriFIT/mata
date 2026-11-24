__author__ = "Tomas Fiedor"

import libmata.alphabets as mata_alphabets
import libmata.nfa.nfa as mata_nfa
import pytest
from libmata import parser


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
    expected.add_transition(0, ord("c"), 0)
    expected.add_transition(0, ord("a"), 1)
    expected.add_transition(1, ord("a"), 1)
    assert mata_nfa.equivalence_check(aut, expected)


def test_optional_reduction():
    for regex in ["a*b|c(de)*", "(?:[a-z]b|c[d-f]){20}", "((a+b)*a)*"]:
        plain = parser.from_regex(regex, reduce=False)
        reduced = parser.from_regex(regex)
        assert mata_nfa.equivalence_check(plain, reduced)
        assert reduced.num_of_states() <= plain.num_of_states()
    assert (
        parser.from_regex("a*b|c(de)*").num_of_states() < parser.from_regex("a*b|c(de)*", reduce=False).num_of_states()
    )


def test_post_processing_options():
    with_epsilon = parser.from_regex("(a)(b)", reduce=False, keep_epsilon=True)
    assert mata_nfa.epsilon() in with_epsilon.get_symbols()
    without_epsilon = parser.from_regex("(a)(b)", reduce=False)
    assert mata_nfa.epsilon() not in without_epsilon.get_symbols()
    with_epsilon.remove_epsilon_inplace()
    assert mata_nfa.equivalence_check(with_epsilon, without_epsilon)

    # The 'a' transition leads to a state that cannot reach a final state.
    untrimmed = parser.from_regex("a[^\\x00-\\xff]", reduce=False, trim=False)
    assert untrimmed.num_of_states() > 0
    assert untrimmed.is_lang_empty()
    assert parser.from_regex("a[^\\x00-\\xff]", reduce=False).num_of_states() == 0


def test_nfa_to_regex_str_round_trip():
    aut = parser.from_regex("a*b|c(de)*")
    fast = aut.to_regex_str(mode=mata_nfa.RegexPrintMode.Fast)
    readable = aut.to_regex_str(mode=mata_nfa.RegexPrintMode.Readable)

    assert aut.to_regex_str() == fast
    assert len(readable) <= len(fast)
    for regex in (fast, readable):
        parsed = parser.from_regex(regex)
        assert mata_nfa.equivalence_check(aut, parsed)


def test_nfa_to_regex_str_explicit_alphabet():
    aut = mata_nfa.Nfa(2)
    aut.make_initial_state(0)
    aut.make_final_states([1])
    aut.add_transition(0, ord("a"), 1)
    alphabet = mata_alphabets.OnTheFlyAlphabet.from_symbol_map({"x": ord("a")})

    parsed = parser.from_regex(aut.to_regex_str(alphabet=alphabet))
    assert mata_nfa.equivalence_check(parsed, parser.from_regex("x"))


@pytest.mark.parametrize("mode", [-1, 2])
def test_nfa_to_regex_str_invalid_mode(mode):
    with pytest.raises(ValueError):
        parser.from_regex("a").to_regex_str(mode=mode)
