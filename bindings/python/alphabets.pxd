from libcpp cimport bool
from libcpp.set cimport set as cset
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.string cimport string
from libcpp.list cimport list as clist
from libcpp.pair cimport pair
from libc.stdint cimport uintptr_t

cdef extern from "mata/nfa.hh" namespace "Mata::Nfa":
    # Typedefs
    ctypedef uintptr_t Symbol
    ctypedef vector[Symbol] Word
    ctypedef cset[Word] WordSet
    ctypedef umap[string, Symbol] StringToSymbolMap
    ctypedef umap[Symbol, string] SymbolToStringMap
    ctypedef umap[string, string] StringDict

    # Alphabets
    cdef cppclass CAlphabet "Mata::Nfa::Alphabet":
        CAlphabet() except +

    cdef cppclass CCharAlphabet "Mata::Nfa::CharAlphabet" (CAlphabet):
        CCharAlphabet() except +
        Symbol translate_symb(string)
        clist[Symbol] get_symbols()

    cdef cppclass CDirectAlphabet "Mata::Nfa::DirectAlphabet" (CAlphabet):
        CDirectAlphabet() except +
        Symbol translate_symb(string)
        clist[Symbol] get_symbols()

    cdef cppclass CEnumAlphabet "Mata::Nfa::EnumAlphabet" (CAlphabet):
        CEnumAlphabet() except +
        CEnumAlphabet(vector[string].iterator, vector[string].iterator) except +
        Symbol translate_symb(string) except +
        clist[Symbol] get_symbols()

    cdef cppclass COnTheFlyAlphabet "Mata::Nfa::OnTheFlyAlphabet" (CAlphabet):
        StringToSymbolMap* symbol_map
        COnTheFlyAlphabet(StringToSymbolMap*, Symbol) except +
        Symbol translate_symb(string)
        clist[Symbol] get_symbols()


cdef class Alphabet:
    cdef CAlphabet* as_base(self)
    cdef get_symbols(self)
