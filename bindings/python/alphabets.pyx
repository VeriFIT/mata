cimport alphabets

from libcpp.vector cimport vector
from libcpp.list cimport list as clist
from libcpp.set cimport set as cset
from libcpp.utility cimport pair
from libcpp.memory cimport shared_ptr, make_shared
from cython.operator import dereference, postincrement as postinc, preincrement as preinc
from libcpp.unordered_map cimport unordered_map as umap

cdef class Alphabet:
    """
    Base class for alphabets
    """
    cdef CAlphabet* as_base(self):
        pass

    def translate_symbol(self, str symbol):
        pass

    def reverse_translate_symbol(self, Symbol symbol):
        pass

    cdef get_symbols(self):
        pass

cdef class CharAlphabet(Alphabet):
    """
    CharAlphabet translates characters in quotes, such as 'a' or "b" to their ordinal values.
    """
    cdef alphabets.CCharAlphabet *thisptr

    def __cinit__(self):
        self.thisptr = new alphabets.CCharAlphabet()

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        """Translates character to its ordinal value. If the character is not in quotes,
        it is interpreted as 0 byte

        :param str symbol: translated symbol
        :return: ordinal value of the symbol
        """
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol):
        """Translates the ordinal value back to the character

        :param Symbol symbol: integer symbol
        :return: symbol as a character
        """
        return "'" + chr(symbol) + "'"

    cpdef get_symbols(self):
        """Returns list of supported symbols

        :return: list of symbols
        """
        cdef clist[Symbol] symbols = self.thisptr.get_symbols()
        return [s for s in symbols]

    cdef alphabets.CAlphabet* as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as CAlphabet*
        """
        return <alphabets.CAlphabet*> self.thisptr


cdef class EnumAlphabet(Alphabet):
    """
    EnumAlphabet represents alphabet that has fixed number of possible values
    """

    cdef alphabets.CEnumAlphabet *thisptr
    cdef vector[string] enums_as_strings

    def __cinit__(self, enums):
        self.enums_as_strings = [e.encode('utf-8') for e in enums]
        self.thisptr = new alphabets.CEnumAlphabet(
            self.enums_as_strings.begin(), self.enums_as_strings.end()
        )

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        """Translates the symbol ot its position in the enumeration

        :param str symbol: translated symbol
        :return: symbol as an position in the enumeration
        """
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol):
        """Translates the symbol back to its string representation

        :param Symbol symbol: integer symbol (position in enumeration)
        :return: symbol as the original string
        """
        if symbol < len(self.enums_as_strings):
            return self.enums_as_strings[symbol].decode('utf-8')
        else:
            raise IndexError(f"{symbol} is out of range of enumeration")

    cpdef get_symbols(self):
        """Returns list of supported symbols

        :return: list of supported symbols
        """
        cdef clist[Symbol] symbols = self.thisptr.get_symbols()
        return [s for s in symbols]

    cdef alphabets.CAlphabet* as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as CAlphabet*
        """
        return <alphabets.CAlphabet*> self.thisptr


cdef class OnTheFlyAlphabet(Alphabet):
    """
    OnTheFlyAlphabet represents alphabet that is not known before hand and is constructed on-the-fly
    """
    cdef alphabets.COnTheFlyAlphabet *thisptr
    cdef StringToSymbolMap string_to_symbol_map

    def __cinit__(self, Symbol initial_symbol = 0):
        self.thisptr = new alphabets.COnTheFlyAlphabet(&self.string_to_symbol_map, initial_symbol)

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol):
        """Translates symbol back to its string representation

        :param Symbol symbol: integer symbol
        :return: original string
        """
        cdef umap[string, Symbol].iterator it = self.string_to_symbol_map.begin()
        cdef umap[string, Symbol].iterator end = self.string_to_symbol_map.end()
        while it != end:
            key = dereference(it).first
            value = dereference(it).second
            if value == symbol:
                return key.decode('utf-8')
            postinc(it)
        raise IndexError(f"{symbol} is out of range of enumeration")


    cpdef get_symbols(self):
        """Returns list of supported symbols

        :return: list of supported symbols
        """
        cdef clist[Symbol] symbols = self.thisptr.get_symbols()
        return [s for s in symbols]

    cdef alphabets.CAlphabet* as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as CAlphabet*
        """
        return <alphabets.CAlphabet*> self.thisptr
