"""Tests of error paths that currently crash the interpreter instead of raising.

A segfault in an in-process test takes down the whole pytest session: the remaining tests of the
file never run and no summary is printed. Every case here therefore runs in a fresh interpreter,
so that a crash becomes one ordinary test failure.
"""

import subprocess
import sys

import pytest

#: Statements that must raise a Python exception instead of killing the interpreter. The ones that
#: still crash are marked with the issue that tracks their cause; once it is fixed, the mark turns
#: the case into an XPASS and the mark (together with the expected exception type) can be tightened.
CASES = [
    pytest.param(
        "import libmata.nfa.nfa as m",
        "m.determinize(None)",
        marks=pytest.mark.xfail(
            reason="#730: typed extension arguments accept None",
            strict=True,
            raises=AssertionError,
        ),
        id="determinize-none",
    ),
    pytest.param(
        "import libmata.parser as p, libmata.alphabets as a",
        "p.from_mata('/nonexistent.mata', a.OnTheFlyAlphabet())",
        marks=pytest.mark.xfail(
            reason="#730: from_mata does not validate its input",
            strict=True,
            raises=AssertionError,
        ),
        id="from-mata-missing-file",
    ),
]


@pytest.mark.parametrize("imports, stmt", CASES)
def test_user_error_raises_instead_of_crashing(imports, stmt):
    """Runs @p stmt in a child interpreter and requires it to raise, not crash.

    The import of the module under test runs unguarded, so its failure fails the test
    instead of being mistaken for the expected exception.
    """
    code = f"{imports}\ntry:\n    {stmt}\nexcept Exception as e:\n    print(type(e).__name__)\n"
    result = subprocess.run(
        [sys.executable, "-c", code],
        capture_output=True,
        text=True,
        timeout=120,
        check=False,
    )
    assert result.returncode == 0, f"interpreter died with {result.returncode}\n{result.stderr[-2000:]}"
    assert result.stdout.strip(), f"no exception was raised by {stmt!r}"
