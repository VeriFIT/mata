"""Importing the bindings must not pull in the optional plotting/dataframe stack (issue #778)."""

import subprocess
import sys


def run_import_probe(module: str) -> set[str]:
    """Import @module in a fresh interpreter and report which optional packages it loaded."""
    code = (
        "import sys;"
        f"import {module};"
        "print(','.join(name for name in ('pandas', 'networkx', 'tabulate', 'graphviz')"
        " if name in sys.modules))"
    )
    result = subprocess.run(
        [sys.executable, "-c", code], capture_output=True, text=True, check=True
    )
    return {name for name in result.stdout.strip().split(",") if name}


def test_importing_nfa_does_not_import_optional_dependencies():
    assert run_import_probe("libmata.nfa.nfa") == set()


def test_importing_utils_does_not_import_tabulate():
    assert run_import_probe("libmata.utils") == set()
