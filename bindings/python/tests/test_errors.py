"""Tests of error paths that must raise instead of crashing the interpreter.

A segfault in an in-process test takes down the whole pytest session: the remaining tests of the
file never run and no summary is printed. Every case here therefore runs in a fresh interpreter,
so that a crash becomes one ordinary test failure.
"""

import subprocess
import sys

import pytest

#: Code every case may rely on. Each case runs in its own interpreter, so this is cheap.
PRELUDE = """
import gc
import libmata.alphabets as alph
import libmata.nfa.nfa as mn
import libmata.nfa.strings as strings
import libmata.parser as parser

def aut():
    a = mn.Nfa(3); a.make_initial_state(0); a.make_final_state(2)
    a.add_transition(0, 0, 1); a.add_transition(1, 1, 2)
    return a

def seg_aut():  # Two segments joined by symbol 100.
    a = mn.Nfa(4); a.make_initial_state(0); a.make_final_state(3)
    a.add_transition(0, 0, 1); a.add_transition(1, 100, 2); a.add_transition(2, 1, 3)
    return a
"""

#: Statements that must raise a Python exception instead of killing the interpreter. Every one of
#: them used to end in `std::terminate` or a segfault (#730, #729).
RAISING_CASES = [
    pytest.param(
        'mn.minimize(aut(), {"algorithm": "moore"})', id="minimize-unknown-algorithm"
    ),
    pytest.param(
        'mn.compute_relation(aut(), {"relation": "simulation"})',
        id="compute-relation-missing-key",
    ),
    pytest.param(
        'alph.IntAlphabet().translate_symbol("abc")', id="int-alphabet-translate-symbol"
    ),
    pytest.param("mn.determinize(None)", id="determinize-none"),
    pytest.param("mn.concatenate(aut(), None)", id="concatenate-none"),
    pytest.param(
        'parser.from_mata("/nonexistent.mata", alph.OnTheFlyAlphabet())',
        id="from-mata-missing-file",
    ),
    pytest.param(
        "parser.from_mata([], alph.OnTheFlyAlphabet())", id="from-mata-empty-list"
    ),
    pytest.param(
        "strings.noodlify_for_equation([None], aut())", id="noodlify-for-equation-none"
    ),
    pytest.param(
        "mn.complement(aut(), alph.Alphabet())", id="complement-base-alphabet"
    ),
    pytest.param("parser.from_regex('(ab')", id="from-regex-syntax-error"),
    pytest.param(
        "parser.from_regex('(?:ab|cd){1000}' * 200)", id="from-regex-too-large"
    ),
]

#: Statements that must simply run to completion. They used to read freed memory.
SURVIVING_CASES = [
    pytest.param(
        "s = strings.Segmentation(seg_aut(), {100}); gc.collect();"
        " junk = [mn.Nfa(10) for _ in range(1000)]; print(len(s.get_segments()))",
        id="segmentation-of-a-temporary",
    ),
    pytest.param(
        'n = mn.Nfa(2, alphabet=alph.OnTheFlyAlphabet.from_symbol_map({"a": 0, "b": 1}));'
        " n.add_transition(0, 0, 1); gc.collect();"
        " junk = [bytearray(64) for _ in range(10000)]; print(len(str(n)))",
        id="nfa-with-a-temporary-alphabet",
    ),
    pytest.param("print(mn.Transition(0, 1, 2) == None)", id="transition-eq-none"),
    pytest.param(
        "t = mn.Transition(0, 1, 2); print(t in [None, t])",
        id="transition-in-list-with-none",
    ),
    pytest.param(
        'print(parser.from_mata("tests/automata/aut0.mata").num_of_states() > 0)',
        id="from-mata-without-alphabet",
    ),
    pytest.param(
        "import libmata.parser as p",
        "p.from_regex('(ab')",
        id="from-regex-syntax-error",
    ),
    pytest.param(
        "import libmata.parser as p",
        "p.from_regex('(?:ab|cd){1000}' * 200)",
        id="from-regex-too-large",
    ),
]


def _run(statement):
    """Runs @p statement in a child interpreter and returns its result."""
    return subprocess.run(
        [sys.executable, "-c", PRELUDE + statement],
        capture_output=True,
        text=True,
        timeout=120,
        check=False,
    )


@pytest.mark.parametrize("stmt", RAISING_CASES)
def test_user_error_raises_instead_of_crashing(stmt):
    """@p stmt must raise a Python exception, not kill the interpreter."""
    result = _run(
        f"try:\n    {stmt}\nexcept Exception as e:\n    print(type(e).__name__)\n"
    )
    assert result.returncode == 0, (
        f"interpreter died with {result.returncode}\n{result.stderr[-2000:]}"
    )
    assert result.stdout.strip(), f"no exception was raised by {stmt!r}"


@pytest.mark.parametrize("stmt", SURVIVING_CASES)
def test_statement_survives(stmt):
    """@p stmt must run to completion; it used to read freed memory or reject a valid argument."""
    result = _run(stmt)
    assert result.returncode == 0, (
        f"interpreter died with {result.returncode}\n{result.stderr[-2000:]}"
    )
    assert result.stdout.strip(), f"{stmt!r} produced no output"
