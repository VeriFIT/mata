"""Shared fixtures for the testing of functionality of Python binding."""

import os
import shutil
import pytest
import libmata.nfa.nfa as mata
import libmata.alphabets as alphabets

__author__ = 'Tomas Fiedor'


@pytest.fixture(scope="function")
def cleandir():
    """Runs the test in the clean new dir, which is purged afterwards"""
    temp_path = tempfile.mkdtemp()
    os.chdir(temp_path)
    yield
    shutil.rmtree(temp_path)


@pytest.fixture(scope="function")
def dfa_one_state_uni():
    lhs = mata.Nfa(1)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 0)
    lhs.make_final_state(0)
    yield lhs


@pytest.fixture(scope="function")
def dfa_one_state_empty():
    lhs = mata.Nfa(1)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 0)
    yield lhs


@pytest.fixture(scope="function")
def nfa_two_states_uni():
    lhs = mata.Nfa(2)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 0)
    lhs.add_transition(0, 0, 1)
    lhs.add_transition(1, 0, 1)
    lhs.add_transition(1, 1, 1)
    lhs.make_final_state(1)
    yield lhs


def divisible_by(k: int):
    """
    Constructs automaton accepting strings containing ones divisible by "k"
    """
    assert k > 1
    lhs = mata.Nfa(k+1)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    for i in range(1, k + 1):
        lhs.add_transition(i - 1, 1, i)
        lhs.add_transition(i, 0, i)
    lhs.add_transition(k, 1, 1)
    lhs.make_final_state(k)
    return lhs


@pytest.fixture(scope="function")
def fa_one_divisible_by_two():
    yield divisible_by(2)


@pytest.fixture(scope="function")
def fa_one_divisible_by_four():
    yield divisible_by(4)


@pytest.fixture(scope="function")
def fa_one_divisible_by_eight():
    yield divisible_by(8)


@pytest.fixture(scope="function")
def fa_odd_ones():
    lhs = mata.Nfa(2)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 1)
    lhs.add_transition(1, 1, 0)
    lhs.add_transition(1, 0, 1)
    lhs.make_final_state(1)


@pytest.fixture(scope="function")
def fa_even_ones():
    lhs = mata.Nfa(2)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 1)
    lhs.add_transition(1, 1, 0)
    lhs.add_transition(1, 0, 1)
    lhs.make_final_state(0)


@pytest.fixture(scope="function")
def binary_alphabet():
    alph = alphabets.OnTheFlyAlphabet()
    alph.translate_symbol("0")
    alph.translate_symbol("1")
    yield alph


@pytest.fixture(scope="session")
def prepare_automaton_a():
    """
    Prepare Nfa as automaton A.
    """

    def _prepare_automaton_a():
        nfa = mata.Nfa(100)
        nfa.make_initial_states([1, 3])
        nfa.make_final_state(5)
        nfa.add_transition(1, ord('a'), 3)
        nfa.add_transition(1, ord('a'), 10)
        nfa.add_transition(1, ord('b'), 7)
        nfa.add_transition(3, ord('a'), 7)
        nfa.add_transition(3, ord('b'), 9)
        nfa.add_transition(9, ord('a'), 9)
        nfa.add_transition(7, ord('b'), 1)
        nfa.add_transition(7, ord('a'), 3)
        nfa.add_transition(7, ord('c'), 3)
        nfa.add_transition(10, ord('a'), 7)
        nfa.add_transition(10, ord('b'), 7)
        nfa.add_transition(10, ord('c'), 7)
        nfa.add_transition(7, ord('a'), 5)
        nfa.add_transition(5, ord('a'), 5)
        nfa.add_transition(5, ord('c'), 9)
        return nfa

    return _prepare_automaton_a


@pytest.fixture(scope="session")
def prepare_automaton_b():
    """
    Prepare Nfa as automaton B.
    """

    def _prepare_automaton_b():
        nfa = mata.Nfa(100)
        nfa.make_initial_states([4])
        nfa.make_final_states([2, 12])
        nfa.add_transition(4, ord('c'), 8)
        nfa.add_transition(4, ord('a'), 8)
        nfa.add_transition(8, ord('b'), 4)
        nfa.add_transition(4, ord('a'), 6)
        nfa.add_transition(4, ord('b'), 6)
        nfa.add_transition(6, ord('a'), 2)
        nfa.add_transition(2, ord('b'), 2)
        nfa.add_transition(2, ord('a'), 0)
        nfa.add_transition(0, ord('a'), 2)
        nfa.add_transition(2, ord('c'), 12)
        nfa.add_transition(12, ord('a'), 14)
        nfa.add_transition(14, ord('b'), 12)
        return nfa

    return _prepare_automaton_b
