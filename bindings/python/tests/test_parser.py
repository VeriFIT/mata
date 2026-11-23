"""Tests of loading automata from .mata files with `parser.from_mata()`."""

import pytest
from libmata import alphabets, parser

#: Pairs of single-section automata per reader `from_mata()` uses: the fast reader of explicit alphabets, the bitvector
#:  reader, and the general reader, which reads the `%Initial` line in the body the fast reader gives up on. The first
#:  automaton accepts exactly one one-letter word, the second one the empty word.
SECTIONS = {
    "explicit": (
        "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n",
        "@NFA-explicit\n%Alphabet-auto\n%Initial p0\n%Final p0\np0 b p0\n",
    ),
    "bits": (
        "@NFA-bits\n%Initial q0\n%Final q1\nq0 (a0 & !a1) q1\n",
        "@NFA-bits\n%Initial q2\n%Final q2\nq2 (!a0 & a1) q3\nq3 (!a0 & a1) q3\n",
    ),
    "general": (
        "@NFA-explicit\n%Alphabet-auto\n%Final q1\nq0 a q1\n%Initial q0\n",
        "@NFA-explicit\n%Alphabet-auto\n%Final p0\np0 b p0\n%Initial p0\n",
    ),
}


def write_mata(path, *sections):
    path.write_text("\n".join(sections))
    return str(path)


def check_pair(first, second):
    assert not first.is_in_lang([])
    assert any(first.is_in_lang([symbol]) for symbol in first.get_symbols())
    assert second.is_in_lang([])


@pytest.mark.parametrize("kind", SECTIONS)
def test_single_path_rejects_several_automata(tmp_path, kind):
    """#852: only the first @-section used to be loaded."""
    src = write_mata(tmp_path / "two.mata", *SECTIONS[kind])
    with pytest.raises(RuntimeError, match="number of sections in the input file is '2'"):
        parser.from_mata(src, alphabets.OnTheFlyAlphabet())


@pytest.mark.parametrize("kind", SECTIONS)
def test_list_rejects_entry_with_several_automata(tmp_path, kind):
    """#852: only the first @-section of every file of a list used to be loaded."""
    one = write_mata(tmp_path / "one.mata", SECTIONS[kind][0])
    two = write_mata(tmp_path / "two.mata", *SECTIONS[kind])
    with pytest.raises(RuntimeError, match="number of sections in the input file") as excinfo:
        parser.from_mata([one, two], alphabets.OnTheFlyAlphabet())
    assert f"'{two}' is '2'" in str(excinfo.value)


def test_list_rejects_empty_entry(tmp_path):
    one = write_mata(tmp_path / "one.mata", SECTIONS["explicit"][0])
    empty = write_mata(tmp_path / "empty.mata")
    with pytest.raises(RuntimeError, match=f"'{empty}' is '0'"):
        parser.from_mata([one, empty], alphabets.OnTheFlyAlphabet())


@pytest.mark.parametrize("kind", SECTIONS)
def test_single_path_loads_one_automaton(tmp_path, kind):
    first = parser.from_mata(write_mata(tmp_path / "first.mata", SECTIONS[kind][0]), alphabets.OnTheFlyAlphabet())
    second = parser.from_mata(write_mata(tmp_path / "second.mata", SECTIONS[kind][1]), alphabets.OnTheFlyAlphabet())
    check_pair(first, second)


@pytest.mark.parametrize("kind", SECTIONS)
def test_list_loads_one_automaton_per_file(tmp_path, kind):
    first = write_mata(tmp_path / "first.mata", SECTIONS[kind][0])
    second = write_mata(tmp_path / "second.mata", SECTIONS[kind][1])
    automata = parser.from_mata([first, second], alphabets.OnTheFlyAlphabet())
    assert len(automata) == 2
    check_pair(*automata)
