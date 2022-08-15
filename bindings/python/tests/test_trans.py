"""Basic tests for utility package and sanity checks"""

import pytest
import mata

__author__ = 'Tomas Fiedor'


def test_trans():
    """Tests that the python interpreter can be obtained in reasonable format"""
    lhs = mata.Trans(0, 0, 0)
    rhs = mata.Trans(0, 1, 1)
    chs = mata.Trans(0, 0, 0)

    assert lhs != rhs
    assert lhs == chs


def test_transsymbstates():
    a = mata.TransSymbolStates(0, [0, 1])
    b = mata.TransSymbolStates(1, [1])
    c = mata.TransSymbolStates(0, [0])
    d = mata.TransSymbolStates(1, [])
    assert a.symbol == 0
    assert a.states_to == [0, 1]

    # Test getter and setters
    a.symbol = 1
    assert not a.symbol == 0
    assert a.symbol == 1
    a.states_to = []
    assert a.states_to == []

    # Test comparision
    assert not a < b
    assert not b < a
    assert c < a
    assert not a < c
    assert not d < b
    assert not b < d


def test_transition_operations():
    nfa = mata.Nfa(10)
    nfa.add_trans_raw(3, ord('c'), 4)
    assert nfa.has_trans_raw(3, ord('c'), 4)
    trans = mata.Trans(4, ord('c'), 5)
    nfa.add_trans(trans)
    assert nfa.has_trans(trans)

    nfa.remove_trans_raw(3, ord('c'), 4)
    assert not nfa.has_trans_raw(3, ord('c'), 4)
    nfa.remove_trans(trans)
    assert not nfa.has_trans(trans)

