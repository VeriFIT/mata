"""Importing the bindings must not pull in the optional plotting/dataframe stack (issue #778)."""

import os
import sys
import tempfile
from types import SimpleNamespace

OPTIONAL_PACKAGES = ("pandas", "networkx", "tabulate", "graphviz", "IPython")


def run_isolated(code: str) -> SimpleNamespace:
    """Run fixed test scripts in a fresh interpreter without any shell or subprocess wrappers."""
    with tempfile.TemporaryDirectory(prefix="mata-lazy-imports-") as directory:
        stdout = os.path.join(directory, "stdout")
        stderr = os.path.join(directory, "stderr")
        prelude = f"import sys\nsys.stdout = open({stdout!r}, 'w')\nsys.stderr = open({stderr!r}, 'w')\n"
        child = os.posix_spawn(sys.executable, [sys.executable, "-c", prelude + code], os.environ.copy())
        _, status = os.waitpid(child, 0)
        with open(stdout) as output, open(stderr) as error:
            return SimpleNamespace(
                returncode=os.waitstatus_to_exitcode(status),
                stdout=output.read(),
                stderr=error.read(),
            )


def run_import_probe(module: str) -> set[str]:
    """Import @module in a fresh interpreter and report which optional packages it loaded."""
    code = f"import sys;import {module};print(','.join(name for name in {OPTIONAL_PACKAGES!r} if name in sys.modules))"
    result = run_isolated(code)
    if result.returncode != 0:
        raise RuntimeError(result.stderr)
    return {name for name in result.stdout.strip().split(",") if name}


def test_importing_nfa_does_not_import_optional_dependencies():
    assert run_import_probe("libmata.nfa.nfa") == set()


def test_importing_utils_does_not_import_tabulate():
    assert run_import_probe("libmata.utils") == set()


def test_importing_plotting_does_not_import_optional_dependencies():
    assert run_import_probe("libmata.plotting") == set()


def run_with_module_blocked(module: str, statement: str) -> SimpleNamespace:
    """Run @statement in a fresh interpreter in which importing @module fails."""
    code = (
        "import sys\n"
        "import importlib.abc\n"
        "class _Blocker(importlib.abc.MetaPathFinder):\n"
        f"    def find_spec(self, name, path=None, target=None):\n"
        f"        if name == {module!r}:\n"
        "            raise ImportError(f'blocked for the test: {name}')\n"
        f"sys.meta_path.insert(0, _Blocker())\n"
        f"{statement}\n"
    )
    return run_isolated(code)


def test_to_dataframe_reports_the_extra_when_pandas_is_missing():
    result = run_with_module_blocked("pandas", "import libmata.nfa.nfa as nfa;nfa.Nfa(1).to_dataframe()")
    assert isinstance(result.stderr, str)
    assert "ImportError: Nfa.to_dataframe()" in result.stderr
    assert "pip install libmata[pandas]" in result.stderr


def test_to_networkx_graph_reports_the_extra_when_networkx_is_missing():
    result = run_with_module_blocked("networkx", "import libmata.nfa.nfa as nfa;nfa.Nfa(1).to_networkx_graph()")
    assert "ImportError: Nfa.to_networkx_graph()" in result.stderr
    assert "pip install libmata[plotting]" in result.stderr
