"""Basic tests for utility package and sanity checks"""

import pytest
import pynfa

__author__ = 'Tomas Fiedor'


def test_trans():
    """Tests that the python interpreter can be obtained in reasonable format"""
    lhs = pynfa.Trans(0, 0, 0)
    rhs = pynfa.Trans(0, 1, 1)
    chs = pynfa.Trans(0, 0, 0)

    assert lhs != rhs
    assert lhs == chs
