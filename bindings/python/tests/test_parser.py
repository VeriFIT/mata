"""Tests of loading automata from .mata files."""

import pytest
from libmata import alphabets, parser

SINGLE_AUTOMATON = """@NFA-explicit
%Alphabet-auto
%Initial q0
%Final q1
q0 a q1
"""

SECOND_AUTOMATON = """@NFA-explicit
%Alphabet-auto
%Initial p0
%Final p0
p0 b p0
"""


def write_mata(path, *sections):
    path.write_text("\n".join(sections))
    return str(path)


def test_from_mata_rejects_file_with_several_automata(tmp_path):
    """#852: the second @-section used to be dropped without a word."""
    src = write_mata(tmp_path / "two.mata", SINGLE_AUTOMATON, SECOND_AUTOMATON)
    with pytest.raises(ValueError) as excinfo:
        parser.from_mata(src, alphabets.OnTheFlyAlphabet())
    assert src in str(excinfo.value)
    assert "2" in str(excinfo.value)


def test_from_mata_rejects_list_entry_with_several_automata(tmp_path):
    src = write_mata(tmp_path / "one.mata", SINGLE_AUTOMATON)
    two = write_mata(tmp_path / "two.mata", SINGLE_AUTOMATON, SECOND_AUTOMATON)
    with pytest.raises(ValueError) as excinfo:
        parser.from_mata([src, two], alphabets.OnTheFlyAlphabet())
    assert two in str(excinfo.value)
    assert "2" in str(excinfo.value)


def test_from_mata_loads_single_automaton_from_path(tmp_path):
    src = write_mata(tmp_path / "one.mata", SINGLE_AUTOMATON)
    nfa = parser.from_mata(src, alphabets.OnTheFlyAlphabet())
    symbols = nfa.get_symbols()
    assert len(symbols) == 1
    # The only automaton of the file accepts exactly the one-letter word "a".
    assert nfa.is_in_lang(list(symbols))
    assert not nfa.is_in_lang([])


def test_from_mata_loads_list_of_single_automaton_files(tmp_path):
    first = write_mata(tmp_path / "first.mata", SINGLE_AUTOMATON)
    second = write_mata(tmp_path / "second.mata", SECOND_AUTOMATON)
    automata = parser.from_mata([first, second], alphabets.OnTheFlyAlphabet())
    assert len(automata) == 2
    # The first automaton rejects the empty word, the second one accepts it: its only state is
    # both initial and final.
    assert [nfa.is_in_lang([]) for nfa in automata] == [False, True]
    assert [len(nfa.get_symbols()) for nfa in automata] == [1, 1]
