__author__ = 'Tomas Fiedor'

from libmata import alphabets as alph, parser


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
    alphabet = alph.OnTheFlyAlphabet.from_symbol_map({'a': 0, 'b': 1, 'c': 2})
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('b') == 1
    assert alphabet.translate_symbol('c') == 2


def test_on_the_fly_alphabet():
    """Tests on the fly alphabet

    OnTheFlyAlphabet translates the symbols into values on-the-fly,
    based on a given counter.
    """
    alphabet = alph.OnTheFlyAlphabet()
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('b') == 1
    assert alphabet.translate_symbol('a') == 0
    assert alphabet.translate_symbol('c') == 2

    alphabet = alph.OnTheFlyAlphabet(3)
    assert alphabet.translate_symbol('a') == 3
    assert alphabet.translate_symbol('b') == 4
    assert alphabet.translate_symbol('c') == 5
    assert alphabet.translate_symbol('a') == 3


def test_int_alphabet():
    alphabet = alph.IntAlphabet()
    assert alphabet.translate_symbol('4') == 4
    assert alphabet.reverse_translate_symbol(4) == '4'


def test_alphabet_get_symbol_map():
    alpha = alph.OnTheFlyAlphabet.from_symbol_map({'a': 0})
    symbol_map = alpha.get_symbol_map()
    assert len(symbol_map) > 0
    assert symbol_map['a'] == 0


def test_get_symbols():
    alpha = alph.OnTheFlyAlphabet()
    aut = parser.from_mata("tests/automata/aut_get_symbols_from_aut.mata", alpha)
    assert len(alpha.get_alphabet_symbols()) == 78
    assert alpha.get_alphabet_symbols() == aut.get_symbols()
