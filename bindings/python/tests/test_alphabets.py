__author__ = "Tomas Fiedor"

import pytest
from libmata import alphabets as alph
from libmata import parser


def test_on_the_fly_alphabet_with_character_symbols():
    """
    Tests on the fly alphabet with character symbols.

    OnTheFlyAlphabet translates the symbols into values on-the-fly, based on a given counter.
    """
    alphabet = alph.OnTheFlyAlphabet()
    assert alphabet.translate_symbol("'a'") == 0
    assert alphabet.translate_symbol("'b'") == 1
    assert alphabet.translate_symbol("b") == 2
    assert alphabet.translate_symbol("1") == 3
    assert alphabet.translate_symbol("10") == 4
    assert alphabet.translate_symbol("ahoj") == 5
    assert alphabet.translate_symbol('"a"') == 6
    assert alphabet.translate_symbol('"0"') == 7


def test_on_the_fly_alphabet_with_enumeration_of_symbols():
    """
    Tests on the fly alphabet.

    OnTheFlyAlphabet translates the symbols into values on-the-fly, based on a given counter.
    """
    alphabet = alph.OnTheFlyAlphabet.from_symbol_map({"a": 0, "b": 1, "c": 2})
    assert alphabet.translate_symbol("a") == 0
    assert alphabet.translate_symbol("b") == 1
    assert alphabet.translate_symbol("c") == 2


def test_on_the_fly_alphabet():
    """Tests on the fly alphabet

    OnTheFlyAlphabet translates the symbols into values on-the-fly,
    based on a given counter.
    """
    alphabet = alph.OnTheFlyAlphabet()
    assert alphabet.translate_symbol("a") == 0
    assert alphabet.translate_symbol("a") == 0
    assert alphabet.translate_symbol("b") == 1
    assert alphabet.translate_symbol("a") == 0
    assert alphabet.translate_symbol("c") == 2

    alphabet = alph.OnTheFlyAlphabet(3)
    assert alphabet.translate_symbol("a") == 3
    assert alphabet.translate_symbol("b") == 4
    assert alphabet.translate_symbol("c") == 5
    assert alphabet.translate_symbol("a") == 3


def test_int_alphabet():
    alphabet = alph.IntAlphabet()
    assert alphabet.translate_symbol("4") == 4
    assert alphabet.reverse_translate_symbol(4) == "4"


def test_alphabet_get_symbol_map():
    alpha = alph.OnTheFlyAlphabet.from_symbol_map({"a": 0})
    symbol_map = alpha.get_symbol_map()
    assert len(symbol_map) > 0
    assert symbol_map["a"] == 0


def test_get_symbols():
    alpha = alph.OnTheFlyAlphabet()
    aut = parser.from_mata("tests/automata/aut_get_symbols_from_aut.mata", alpha)
    assert len(alpha.get_alphabet_symbols()) == 78
    assert alpha.get_alphabet_symbols() == aut.get_symbols()


def test_alphabet_gap_fill_parity():
    """Tests get_complement()/is_empty()/clear() added to Alphabet and its subclasses."""
    alpha = alph.OnTheFlyAlphabet.for_symbol_names(["a", "b", "c"])
    assert not alpha.is_empty()
    alpha.clear()
    assert alpha.is_empty()

    int_alphabet = alph.IntAlphabet()
    assert not int_alphabet.is_empty()


def test_enum_alphabet():
    """Tests EnumAlphabet, a direct alphabet maintaining an explicit, ordered set of symbols."""
    alphabet = alph.EnumAlphabet({0, 4, 6, 8, 9})
    assert alphabet.translate_symbol("6") == 6
    try:
        alphabet.translate_symbol("5")
        assert False, "Expected an exception for an unknown symbol."
    except Exception:
        pass
    assert alphabet.get_complement({0, 6, 9}) == {4, 8}
    assert alphabet.get_alphabet_symbols() == {0, 4, 6, 8, 9}
    assert not alphabet.is_empty()

    alphabet.add_new_symbol(10)
    assert 10 in alphabet.get_alphabet_symbols()
    alphabet.add_new_symbol("11")
    assert alphabet.translate_symbol("11") == 11

    assert alphabet.erase(10) == 1
    assert 10 not in alphabet.get_alphabet_symbols()

    alphabet.clear()
    assert alphabet.is_empty()


def test_alphabet_levels_multi_level_mode():
    """Tests AlphabetLevels in MultiLevel mode: distinct per-level alphabets, indexed by level."""
    level0 = alph.EnumAlphabet({1, 2, 3})
    level1 = alph.IntAlphabet()
    alphabets = alph.AlphabetLevels([level0, level1])

    assert alphabets.mode == alph.AlphabetLevelsMode.MultiLevel
    assert len(alphabets) == 2
    assert alphabets.for_level(0).get_alphabet_symbols() == {1, 2, 3}
    assert alphabets[0] == level0
    assert alphabets.at(1) == level1

    try:
        alphabets.for_level(None)
        assert False, "Expected an exception: MultiLevel requires an explicit level."
    except Exception:
        pass
    try:
        alphabets.for_level(5)
        assert False, "Expected an exception: level 5 is out of range."
    except Exception:
        pass

    alphabets.push_back(alph.IntAlphabet())
    assert len(alphabets) == 3
    alphabets.erase(2)
    assert len(alphabets) == 2


def test_alphabet_levels_bounds_and_type_checks():
    """push_back/insert/pop_back/erase must reject bad input instead of relying on unchecked C++ vector access."""
    alphabets = alph.AlphabetLevels([alph.IntAlphabet(), alph.IntAlphabet()])

    with pytest.raises(TypeError):
        alphabets.push_back("not an alphabet")
    with pytest.raises(TypeError):
        alphabets.insert(0, "not an alphabet")

    with pytest.raises(IndexError):
        alphabets.insert(10, alph.IntAlphabet())
    with pytest.raises(IndexError):
        alphabets.erase(10)
    with pytest.raises(IndexError):
        alphabets.erase(0, 10)
    with pytest.raises(IndexError):
        alphabets.erase(1, 0)

    empty = alph.AlphabetLevels([])
    with pytest.raises(IndexError):
        empty.pop_back()

    # None is still accepted (a null slot), and valid indices still work.
    alphabets.insert(1, None)
    assert len(alphabets) == 3
    alphabets.erase(1)
    assert len(alphabets) == 2
    alphabets.pop_back()
    assert len(alphabets) == 1


def test_alphabet_levels_global_mode():
    """Tests AlphabetLevels in Global mode: a single shared alphabet used for every level."""
    shared = alph.EnumAlphabet({7, 8})
    alphabets = alph.AlphabetLevels.global_mode(shared)

    assert alphabets.mode == alph.AlphabetLevelsMode.Global
    assert alphabets.for_level(0).get_alphabet_symbols() == {7, 8}
    # Level argument is ignored in Global mode.
    assert alphabets.for_level(123).get_alphabet_symbols() == {7, 8}

    other = alph.IntAlphabet()
    alphabets.set_multi_level_mode([shared, other])
    assert alphabets.mode == alph.AlphabetLevelsMode.MultiLevel
    assert len(alphabets) == 2

    alphabets.set_global_mode(other := alph.IntAlphabet())
    assert alphabets.mode == alph.AlphabetLevelsMode.Global
    assert alphabets.for_level() == other


def test_alphabet_levels_equality():
    shared = alph.IntAlphabet()
    a = alph.AlphabetLevels.global_mode(shared)
    b = alph.AlphabetLevels.global_mode(shared)
    c = alph.AlphabetLevels.global_mode(alph.IntAlphabet())

    assert a == b
    assert a != c
