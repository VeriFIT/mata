"""Regression tests for #768: symbol width, unify_final chaining, str decoding, and noodlify_for_equation copying."""

import pytest

import libmata.alphabets as alphabets
import libmata.nfa.nfa as mata_nfa
import libmata.nfa.strings as mata_strings


def test_add_transition_rejects_symbol_beyond_32_bits():
    """mata::Symbol is 32-bit; passing a larger symbol must raise instead of truncating to low bits."""
    nfa = mata_nfa.Nfa(2)
    with pytest.raises(OverflowError):
        nfa.add_transition(0, 2**32 + 5, 1)
    assert nfa.get_symbols() == set()


def test_unify_final_returns_self_like_unify_initial():
    nfa = mata_nfa.Nfa(3)
    nfa.make_initial_state(0)
    nfa.make_final_state(1)
    nfa.make_final_state(2)
    assert nfa.unify_initial() is nfa
    assert nfa.unify_final() is nfa


def test_str_decodes_symbol_names():
    alphabet = alphabets.OnTheFlyAlphabet.from_symbol_map({"a": 0})
    nfa = mata_nfa.Nfa(2, alphabet=alphabet)
    nfa.add_transition(0, 0, 1)
    text = str(nfa)
    assert "-[a]→" in text
    assert "b'" not in text


def test_noodlify_for_equation_with_reduce_does_not_mutate_callers_automata():
    lhs = mata_nfa.Nfa(3)
    lhs.make_initial_state(0)
    lhs.make_initial_state(1)
    lhs.make_final_state(2)
    lhs.add_transition(0, ord("a"), 2)
    lhs.add_transition(1, ord("b"), 2)
    rhs = mata_nfa.Nfa(2)
    rhs.make_initial_state(0)
    rhs.make_final_state(1)
    rhs.add_transition(0, ord("a"), 1)
    rhs.add_transition(0, ord("b"), 1)
    states_before, initial_before = lhs.num_of_states(), sorted(lhs.initial_states)
    noodles = mata_strings.noodlify_for_equation([lhs], rhs, params={"reduce": "forward"})
    assert (lhs.num_of_states(), sorted(lhs.initial_states)) == (states_before, initial_before)
    assert noodles and all(isinstance(segment, list) for segment in noodles)
