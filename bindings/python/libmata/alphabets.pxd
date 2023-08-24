from libc.stdint cimport uintptr_t
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.unordered_map cimport unordered_map as umap
from libmata.utils cimport COrdVector


cdef extern from "mata/alphabet.hh" namespace "mata":
    ctypedef uintptr_t Symbol

    cdef cppclass CAlphabet "mata::Alphabet":
        CAlphabet() except +

        Symbol translate_symb(string)
        string reverse_translate_symbol(Symbol)

    cdef cppclass CIntAlphabet "mata::IntAlphabet" (CAlphabet):
        COrdVector[Symbol] get_alphabet_symbols()

    cdef cppclass COnTheFlyAlphabet "mata::OnTheFlyAlphabet" (CAlphabet):
        ctypedef umap[string, Symbol] StringToSymbolMap

        StringToSymbolMap symbol_map

        COnTheFlyAlphabet(StringToSymbolMap) except +
        COnTheFlyAlphabet(Symbol) except +
        COnTheFlyAlphabet(COnTheFlyAlphabet) except +
        COnTheFlyAlphabet(vector[string]) except +
        COrdVector[Symbol] get_alphabet_symbols()
        StringToSymbolMap get_symbol_map()
        void add_symbols_from(StringToSymbolMap)
        void add_symbols_from(vector[string])


cdef class Alphabet:
    cdef CAlphabet* as_base(self)
    cdef get_symbols(self)
