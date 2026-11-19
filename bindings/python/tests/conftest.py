"""Shared fixtures for the testing of functionality of Python binding."""

import os
import shutil
import sys
import tempfile
import threading
import time

import libmata.nfa.nfa as mata_nfa
import pytest
from libmata import alphabets

__author__ = "Tomas Fiedor"


@pytest.fixture(scope="function")
def cleandir():
    """Runs the test in the clean new dir, which is purged afterwards"""
    original_path = os.getcwd()
    temp_path = tempfile.mkdtemp()
    os.chdir(temp_path)
    try:
        yield temp_path
    finally:
        os.chdir(original_path)
        shutil.rmtree(temp_path)


@pytest.fixture(scope="function")
def runs_without_the_gil():
    """Returns whether the main thread runs Python code while a given callable runs in another thread."""
    if not getattr(sys, "_is_gil_enabled", lambda: True)():
        pytest.skip("the interpreter has no GIL")

    def _runs_without_the_gil(operation, budget_seconds=1.0):
        """Return True iff this thread runs Python code while `operation` is still running.

        The previous version polled `finished` exactly once after `entered.wait()` returned. That
        is a single scheduling slot: a fast or short operation can complete before the main thread
        wakes up again even when the call releases the GIL, which made the test flaky on macOS.
        Poll in a loop for a bounded budget instead, so any window where the worker still has not
        finished (because the GIL was released) is counted.
        """
        entered = threading.Event()
        finished = threading.Event()
        # Force at least one observation window between "operation started" and "operation perhaps finished"
        #  so the loop below cannot skip the question entirely on very fast operations.
        ready = threading.Event()

        def run():
            entered.set()
            ready.wait()
            operation()
            finished.set()

        switch_interval = sys.getswitchinterval()
        # Threads then switch only where one blocks or releases the GIL: the worker holds it from
        #  `entered.set()` into `operation()`, so the main thread wakes up only if the call lets it.
        sys.setswitchinterval(60)
        try:
            worker = threading.Thread(target=run, daemon=True)
            worker.start()
            entered.wait()
            # Let the worker start `operation()`; main will run only if/when that call releases the
            #  GIL. A short sleep forces the scheduler to consider re-waking us before the worker
            #  has finished, so the polling loop below cannot miss the window.
            time.sleep(0.001)
            ready.set()
            deadline = time.monotonic() + budget_seconds
            saw_running = False
            # Each poll iteration where the worker still has not finished means the main thread
            #  advanced while the operation was running, i.e. the GIL was released.
            while not finished.is_set() and time.monotonic() < deadline:
                saw_running = True
                time.sleep(0)
            worker.join()
            return saw_running
        finally:
            sys.setswitchinterval(switch_interval)

    return _runs_without_the_gil


@pytest.fixture(scope="function")
def dfa_one_state_uni():
    lhs = mata_nfa.Nfa(1)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 0)
    lhs.make_final_state(0)
    yield lhs


@pytest.fixture(scope="function")
def dfa_one_state_empty():
    lhs = mata_nfa.Nfa(1)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 0)
    yield lhs


@pytest.fixture(scope="function")
def nfa_two_states_uni():
    lhs = mata_nfa.Nfa(2)
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
    lhs = mata_nfa.Nfa(k + 1)
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
    lhs = mata_nfa.Nfa(2)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 1)
    lhs.add_transition(1, 1, 0)
    lhs.add_transition(1, 0, 1)
    lhs.make_final_state(1)
    yield lhs


@pytest.fixture(scope="function")
def fa_even_ones():
    lhs = mata_nfa.Nfa(2)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    lhs.add_transition(0, 1, 1)
    lhs.add_transition(1, 1, 0)
    lhs.add_transition(1, 0, 1)
    lhs.make_final_state(0)
    yield lhs


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
        nfa = mata_nfa.Nfa(100)
        nfa.make_initial_states([1, 3])
        nfa.make_final_state(5)
        nfa.add_transition(1, ord("a"), 3)
        nfa.add_transition(1, ord("a"), 10)
        nfa.add_transition(1, ord("b"), 7)
        nfa.add_transition(3, ord("a"), 7)
        nfa.add_transition(3, ord("b"), 9)
        nfa.add_transition(9, ord("a"), 9)
        nfa.add_transition(7, ord("b"), 1)
        nfa.add_transition(7, ord("a"), 3)
        nfa.add_transition(7, ord("c"), 3)
        nfa.add_transition(10, ord("a"), 7)
        nfa.add_transition(10, ord("b"), 7)
        nfa.add_transition(10, ord("c"), 7)
        nfa.add_transition(7, ord("a"), 5)
        nfa.add_transition(5, ord("a"), 5)
        nfa.add_transition(5, ord("c"), 9)
        return nfa

    return _prepare_automaton_a


@pytest.fixture(scope="session")
def prepare_automaton_b():
    """
    Prepare Nfa as automaton B.
    """

    def _prepare_automaton_b():
        nfa = mata_nfa.Nfa(100)
        nfa.make_initial_states([4])
        nfa.make_final_states([2, 12])
        nfa.add_transition(4, ord("c"), 8)
        nfa.add_transition(4, ord("a"), 8)
        nfa.add_transition(8, ord("b"), 4)
        nfa.add_transition(4, ord("a"), 6)
        nfa.add_transition(4, ord("b"), 6)
        nfa.add_transition(6, ord("a"), 2)
        nfa.add_transition(2, ord("b"), 2)
        nfa.add_transition(2, ord("a"), 0)
        nfa.add_transition(0, ord("a"), 2)
        nfa.add_transition(2, ord("c"), 12)
        nfa.add_transition(12, ord("a"), 14)
        nfa.add_transition(14, ord("b"), 12)
        return nfa

    return _prepare_automaton_b
