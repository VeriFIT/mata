__author__ = 'Tomas Fiedor'

import pynfa
import pytest


def test_char_alphabet():
    """Tests character alphabet

    CharacterAlphabet translates escaped characters ('a' or "a") into their ascii
    representation. Integers are kept as they are, other strings are translated to 0.
    """
    alphabet = pynfa.CharAlphabet()
    assert alphabet.translate_symbol("'a'") == 97
    assert alphabet.translate_symbol("'b'") == 98
    assert alphabet.translate_symbol("b") == 0
    assert alphabet.translate_symbol("1") == 1
    assert alphabet.translate_symbol("10") == 10
    assert alphabet.translate_symbol("ahoj") == 0
    assert alphabet.translate_symbol('"a"') == 97
    assert alphabet.translate_symbol('"0"') == 48


def test_enum_alphabet():
    """Tests enumerated alphabet

    EnumAlphabet translates integers based on fixed list of values.
    """
    alphabet = pynfa.EnumAlphabet(['a', 'b', 'c'])
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('b') == 1
    assert alphabet.translate_symbol('c') == 2
    with pytest.raises(RuntimeError):
        assert alphabet.translate_symbol('d') == 2
    with pytest.raises(RuntimeError):
        _ = pynfa.EnumAlphabet(['a', 'a', 'c'])


def test_on_the_fly_alphabet():
    """Tests on the fly alphabet

    OnTheFlyAlphabet translates the symbols into values on-the-fly,
    based on a given counter.
    """
    alphabet = pynfa.OnTheFlyAlphabet()
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('b') == 1
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('c') == 2

    alphabet = pynfa.OnTheFlyAlphabet(3)
    assert alphabet.translate_symbol('a') == 3
    assert alphabet.translate_symbol('b') == 4
    assert alphabet.translate_symbol('c') == 5
    assert alphabet.translate_symbol('a') == 3
