from typing import Self

from libmata.nfa.nfa import State

Symbol = int

class Alphabet:
    """Base class for alphabets."""
    def __init__(self, *args, **kwargs) -> None:
        ...
    def translate_symbol(self, symbol: str) -> None:
        pass
    def reverse_translate_symbol(self, symbol: Symbol) -> None:
        pass

class OnTheFlyAlphabet(Alphabet):
    """OnTheFlyAlphabet represents alphabet that is not known before hand and is constructed on-the-fly."""
    def __init__(self, initial_symbol: State = 0) -> None:
        ...
    @classmethod
    def from_symbol_map(cls, symbol_map: dict[str, int]) -> Self:
        """Create on the fly alphabet filled with symbol_map.

        :param symbol_map: Map mapping symbol names to symbol values.
        :return: On the fly alphabet.
        """
        ...
    @classmethod
    def for_symbol_names(cls, symbol_map: list[str]) -> Self:
        ...
    def add_symbols_from_symbol_map(self, symbol_map: dict[str, int]) -> None:
        """Add symbols from symbol_map to the current alphabet.

        :param symbol_map: Map mapping strings to symbols.
        """
        ...
    def add_symbols_for_names(self, symbol_names: list[str]) -> None:
        """Add symbols for symbol names to the current alphabet.

        :param symbol_names: Vector of symbol names.
        """
        ...
    def get_symbol_map(self) -> dict[str, int]:
        """Get map mapping strings to symbols.

        :return: Map of strings to symbols.
        """
        ...
    def translate_symbol(self, symbol: str) -> Symbol:
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        ...
    def reverse_translate_symbol(self, symbol: Symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        Throw an exception when the symbol is missing in the alphabet.
        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
        ...
    def get_alphabet_symbols(self) -> set[Symbol]:
        """Returns a set of supported symbols.

        :return: Set of supported symbols.
        """
        ...

class IntAlphabet(Alphabet):
    """IntAlphabet represents integer alphabet that directly maps integer string to their values."""
    def __init__(self, *args, **kwargs) -> None:
        ...
    def translate_symbol(self, symbol: str) -> Symbol:
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        ...
    def reverse_translate_symbol(self, symbol: Symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
        ...
