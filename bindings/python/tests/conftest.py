"""Shared fixtures for the testing of functionality of Python binding."""

import os
import shutil
import pytest
import pynfa

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
    lhs = pynfa.Nfa()
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 0)
    lhs.add_final_state(0)
    yield lhs


@pytest.fixture(scope="function")
def dfa_one_state_empty():
    lhs = pynfa.Nfa()
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 0)
    yield lhs


@pytest.fixture(scope="function")
def nfa_two_states_uni():
    lhs = pynfa.Nfa()
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    lhs.add_trans_raw(0, 1, 0)
    lhs.add_trans_raw(0, 0, 1)
    lhs.add_trans_raw(1, 0, 1)
    lhs.add_trans_raw(1, 1, 1)
    lhs.add_final_state(1)
    yield lhs
