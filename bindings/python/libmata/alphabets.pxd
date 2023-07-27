from libc.stdint cimport uintptr_t
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.unordered_map cimport unordered_map as umap
from libmata.utils cimport COrdVector


cdef extern from "mata/alphabet.hh" namespace "Mata":
    ctypedef uintptr_t Symbol

    cdef cppclass CAlphabet "Mata::Alphabet":
        CAlphabet() except +

        Symbol translate_symb(string)
        string reverse_translate_symbol(Symbol)

    cdef cppclass CIntAlphabet "Mata::IntAlphabet" (CAlphabet):
        COrdVector[Symbol] get_alphabet_symbols()

    cdef cppclass COnTheFlyAlphabet "Mata::OnTheFlyAlphabet" (CAlphabet):
        umap[string, Symbol] symbol_map
        COnTheFlyAlphabet(umap[string, Symbol]) except +
        COnTheFlyAlphabet(Symbol) except +
        COnTheFlyAlphabet(COnTheFlyAlphabet) except +
        COnTheFlyAlphabet(vector[string]) except +
        COrdVector[Symbol] get_alphabet_symbols()
        umap[string, Symbol] get_symbol_map()
        umap[string, Symbol] add_symbols_from(umap[string, Symbol])
        umap[string, Symbol] add_symbols_from(vector[string])


cdef class Alphabet:
    cdef CAlphabet* as_base(self)
    cdef get_symbols(self)
