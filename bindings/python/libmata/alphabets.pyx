cimport libmata.alphabets as alph

from libmata.alphabets cimport CAlphabet, CIntAlphabet, COnTheFlyAlphabet
from libmata.nfa.nfa cimport State

cdef class Alphabet:
    """Base class for alphabets."""

    def __cinit__(self):

        pass

    cdef CAlphabet* as_base(self):
        pass

    def translate_symbol(self, str symbol):
        pass

    def reverse_translate_symbol(self, Symbol symbol):
        pass

    cdef get_symbols(self):
        pass

    def __dealloc__(self):
        pass


cdef class OnTheFlyAlphabet(Alphabet):
    """OnTheFlyAlphabet represents alphabet that is not known before hand and is constructed on-the-fly."""

    cdef COnTheFlyAlphabet *thisptr

    def __cinit__(self, State initial_symbol = 0):
        self.thisptr = new COnTheFlyAlphabet(initial_symbol)

    @classmethod
    def from_symbol_map(cls, symbol_map: dict[str, int]) -> OnTheFlyAlphabet:
        """Create on the fly alphabet filled with symbol_map.

        :param symbol_map: Map mapping symbol names to symbol values.
        :return: On the fly alphabet.
        """
        alphabet = cls()
        alphabet.add_symbols_from_symbol_map(symbol_map)
        return alphabet

    @classmethod
    def for_symbol_names(cls, symbol_map: list[str]) -> OnTheFlyAlphabet:
        alphabet = cls()
        alphabet.add_symbols_for_names(symbol_map)
        return alphabet

    def add_symbols_from_symbol_map(self, symbol_map: dict[str, int]) -> None:
        """Add symbols from symbol_map to the current alphabet.

        :param symbol_map: Map mapping strings to symbols.
        """
        cdef COnTheFlyAlphabet.StringToSymbolMap c_symbol_map
        for symbol, value in symbol_map.items():
            c_symbol_map[symbol.encode('utf-8')] = value
        self.thisptr.add_symbols_from(<COnTheFlyAlphabet.StringToSymbolMap>c_symbol_map)

    def add_symbols_for_names(self, symbol_names: list[str]) -> None:
        """Add symbols for symbol names to the current alphabet.

        :param symbol_names: Vector of symbol names.
        """
        cdef vector[string] c_symbol_names
        for symbol_name in symbol_names:
            c_symbol_names.push_back(symbol_name.encode('utf-8'))
        self.thisptr.add_symbols_from(c_symbol_names)

    def __dealloc__(self):
        del self.thisptr

    def get_symbol_map(self) -> dict[str, int]:
        """Get map mapping strings to symbols.

        :return: Map of strings to symbols.
        """
        cdef umap[string, Symbol] c_symbol_map = self.thisptr.get_symbol_map()
        symbol_map = {}
        for symbol, value in c_symbol_map:
            symbol_map[symbol.decode('utf-8')] = value
        return symbol_map

    def translate_symbol(self, str symbol):
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        Throw an exception when the symbol is missing in the alphabet.
        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
        return self.thisptr.reverse_translate_symbol(symbol).decode('utf-8')

    cpdef get_alphabet_symbols(self):
        """Returns a set of supported symbols.

        :return: Set of supported symbols.
        """
        cdef COrdVector[Symbol] symbols = self.thisptr.get_alphabet_symbols()
        return {s for s in symbols}

    cdef CAlphabet* as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as CAlphabet*
        """
        return <CAlphabet*> self.thisptr


cdef class IntAlphabet(Alphabet):
    """IntAlphabet represents integer alphabet that directly maps integer string to their values."""

    cdef CIntAlphabet *thisptr

    def __cinit__(self):
        self.thisptr = new CIntAlphabet()

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
        return self.thisptr.reverse_translate_symbol(symbol).decode('utf-8')

    cdef CAlphabet* as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as CAlphabet*
        """
        return <CAlphabet*> self.thisptr
