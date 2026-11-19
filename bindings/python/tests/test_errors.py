"""Tests of error paths that must raise instead of crashing the interpreter.

A segfault in an in-process test takes down the whole pytest session: the remaining tests of the
file never run and no summary is printed. Every case here therefore runs in a fresh interpreter,
so that a crash becomes one ordinary test failure.
"""

import os
import signal
import sys
import tempfile
import time
from types import SimpleNamespace

import pytest

#: The fixture path is resolved here: the child inherits pytest's working directory, which may be anywhere.
AUT0 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "automata", "aut0.mata")

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
    pytest.param('mn.minimize(aut(), {"algorithm": "moore"})', id="minimize-unknown-algorithm"),
    pytest.param(
        'mn.compute_relation(aut(), {"relation": "simulation"})',
        id="compute-relation-missing-key",
    ),
    pytest.param('alph.IntAlphabet().translate_symbol("abc")', id="int-alphabet-translate-symbol"),
    pytest.param("mn.determinize(None)", id="determinize-none"),
    pytest.param("mn.concatenate(aut(), None)", id="concatenate-none"),
    pytest.param(
        'parser.from_mata("/nonexistent.mata", alph.OnTheFlyAlphabet())',
        id="from-mata-missing-file",
    ),
    pytest.param("parser.from_mata([], alph.OnTheFlyAlphabet())", id="from-mata-empty-list"),
    pytest.param("strings.noodlify_for_equation([None], aut())", id="noodlify-for-equation-none"),
    pytest.param("mn.complement(aut(), alph.Alphabet())", id="complement-base-alphabet"),
    pytest.param("parser.from_regex('(ab')", id="from-regex-syntax-error"),
    pytest.param("parser.from_regex('(?:ab|cd){1000}' * 200)", id="from-regex-too-large"),
]

#: Statements that must simply run to completion. They used to read freed memory. Every check raises
#:  explicitly instead of using `assert`, so it still runs under `PYTHONOPTIMIZE`.
SURVIVING_CASES = [
    pytest.param(
        "s = strings.Segmentation(seg_aut(), {100}); gc.collect();"
        " junk = [mn.Nfa(10) for _ in range(1000)];"
        " n = len(s.get_segments())\n"
        "if n != 2: raise SystemExit(f'expected 2 segments, got {n}')\n"
        "print('ok')",
        id="segmentation-of-a-temporary",
    ),
    pytest.param(
        'n = mn.Nfa(2, alphabet=alph.OnTheFlyAlphabet.from_symbol_map({"a": 0, "b": 1}));'
        " n.add_transition(0, 0, 1); gc.collect();"
        " junk = [bytearray(64) for _ in range(10000)]\n"
        "if not str(n): raise SystemExit('empty string form')\n"
        "print('ok')",
        id="nfa-with-a-temporary-alphabet",
    ),
    pytest.param(
        "t = mn.Transition(0, 1, 2)\nif t == None: raise SystemExit('Transition compared equal to None')\nprint('ok')",
        id="transition-eq-none",
    ),
    pytest.param(
        "t = mn.Transition(0, 1, 2)\n"
        "if t not in [None, t]: raise SystemExit('Transition not found in a list holding it')\n"
        "print('ok')",
        id="transition-in-list-with-none",
    ),
    pytest.param(
        f"a = parser.from_mata({AUT0!r})\n"
        "if a.num_of_states() == 0: raise SystemExit('the fixture loaded as an empty automaton')\n"
        "print('ok')",
        id="from-mata-without-alphabet",
    ),
    pytest.param(
        # A generator is iterated lazily: the missing file is reported before the generator is resumed.
        "def paths():\n"
        "    yield '/nonexistent.mata'\n"
        "    raise RuntimeError('resumed past the missing file')\n"
        "try:\n"
        "    parser.from_mata(paths())\n"
        "except FileNotFoundError:\n"
        "    print('ok')",
        id="from-mata-generator-is-lazy",
    ),
    pytest.param(
        # Runs in a child so that a regression to a native crash fails this case instead of killing pytest.
        "try:\n"
        "    parser.from_mata(iter(()))\n"
        "except ValueError as e:\n"
        "    if 'at least one source' not in str(e): raise SystemExit(f'unexpected message: {e}')\n"
        "    print('ok')\n"
        "else:\n"
        "    raise SystemExit('returned normally instead of raising')",
        id="from-mata-empty-iterator",
    ),
]


def _run(statement):
    """Runs @p statement in a child interpreter and returns its result."""
    with tempfile.TemporaryDirectory(prefix="mata-user-error-") as directory:
        stdout = os.path.join(directory, "stdout")
        stderr = os.path.join(directory, "stderr")
        prelude = f"import sys\nsys.stdout = open({stdout!r}, 'w')\nsys.stderr = open({stderr!r}, 'w')\n"
        child = os.posix_spawn(
            sys.executable,
            [sys.executable, "-c", prelude + PRELUDE + statement],
            os.environ.copy(),
        )
        deadline = time.monotonic() + 120
        status = 0
        while True:
            waited, status = os.waitpid(child, os.WNOHANG)
            if waited == child:
                break
            if time.monotonic() > deadline:
                os.kill(child, signal.SIGKILL)
                os.waitpid(child, 0)
                raise TimeoutError("child interpreter timed out after 120 seconds")
            time.sleep(0.01)
        with open(stdout) as output, open(stderr) as error:
            return SimpleNamespace(
                returncode=os.waitstatus_to_exitcode(status),
                stdout=output.read(),
                stderr=error.read(),
            )


@pytest.mark.parametrize("stmt", RAISING_CASES)
def test_user_error_raises_instead_of_crashing(stmt):
    """@p stmt must raise a Python exception, not kill the interpreter or return normally."""
    result = _run(
        f"try:\n    {stmt}\nexcept Exception as e:\n    print(type(e).__name__)\n"
        "else:\n    raise SystemExit('returned normally instead of raising')\n"
    )
    assert result.returncode == 0, f"interpreter exited with {result.returncode}\n{result.stderr[-2000:]}"
    assert result.stdout.strip(), f"no exception was raised by {stmt!r}"


@pytest.mark.parametrize("stmt", SURVIVING_CASES)
def test_statement_survives(stmt):
    """@p stmt must run to completion; it used to read freed memory or reject a valid argument."""
    result = _run(stmt)
    assert result.returncode == 0, f"interpreter died with {result.returncode}\n{result.stderr[-2000:]}"
    assert result.stdout.strip() == "ok", f"{stmt!r} printed {result.stdout!r} instead of its success marker"
