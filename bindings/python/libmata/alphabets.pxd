from libc.stdint cimport uintptr_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.memory cimport shared_ptr
from libcpp.optional cimport optional
from libmata.utils cimport COrdVector


cdef extern from "mata/alphabet.hh" namespace "mata":
    ctypedef uintptr_t Symbol
    ctypedef uintptr_t Level

    cdef cppclass CConstAlphabet "const mata::Alphabet"

cdef extern from "mata/utils/lazy-word-generator.hh" namespace "mata::utils":
    # Constructed only on the C++ side, via e.g. CNfa's/CNft's get_words_lazy_ptr() — never directly from Cython,
    #  since std::generator (what it wraps) is move-only and not default-constructible, which Cython's own
    #  code generation cannot accommodate as an intermediate value.
    cdef cppclass CLazyWordGenerator "mata::utils::LazyWordGenerator":
        bool done()
        vector[Symbol] next() except +


cdef extern from "mata/alphabet.hh" namespace "mata":

    cdef cppclass CAlphabet "mata::Alphabet":
        CAlphabet() except +

        Symbol translate_symb(string) except +
        string reverse_translate_symbol(Symbol) except +
        COrdVector[Symbol] get_alphabet_symbols() except +
        COrdVector[Symbol] get_complement(COrdVector[Symbol]) except +
        bool empty()
        void clear() except +
        bool is_equal(CAlphabet&)

    cdef cppclass CIntAlphabet "mata::IntAlphabet" (CAlphabet):
        COrdVector[Symbol] get_alphabet_symbols() except +

    cdef cppclass CEnumAlphabet "mata::EnumAlphabet" (CAlphabet):
        CEnumAlphabet() except +
        CEnumAlphabet(COrdVector[Symbol]) except +
        COrdVector[Symbol] get_alphabet_symbols()
        COrdVector[Symbol] get_complement(COrdVector[Symbol])
        Symbol translate_symb(string) except +
        string reverse_translate_symbol(Symbol) except +
        void add_symbols_from(COrdVector[Symbol]) except +
        void add_new_symbol(string) except +
        void add_new_symbol(Symbol) except +
        Symbol get_next_value()
        size_t get_number_of_symbols()
        bool empty()
        size_t erase(Symbol)
        void clear()

    cdef cppclass COnTheFlyAlphabet "mata::OnTheFlyAlphabet" (CAlphabet):
        ctypedef umap[string, Symbol] StringToSymbolMap

        StringToSymbolMap symbol_map

        COnTheFlyAlphabet(StringToSymbolMap) except +
        COnTheFlyAlphabet(Symbol) except +
        COnTheFlyAlphabet(COnTheFlyAlphabet) except +
        COnTheFlyAlphabet(vector[string]) except +
        COrdVector[Symbol] get_alphabet_symbols()
        COrdVector[Symbol] get_complement(COrdVector[Symbol])
        StringToSymbolMap get_symbol_map()
        void add_symbols_from(StringToSymbolMap)
        void add_symbols_from(vector[string])
        bool empty()
        void clear()
        size_t erase(Symbol) except +


cdef extern from "mata/alphabet.hh" namespace "mata::AlphabetLevels":
    cdef enum CAlphabetLevelsMode "mata::AlphabetLevels::Mode":
        AlphabetLevelsModeGlobal "mata::AlphabetLevels::Mode::Global"
        AlphabetLevelsModeMultiLevel "mata::AlphabetLevels::Mode::MultiLevel"


cdef extern from "mata/alphabet.hh" namespace "mata":
    cdef cppclass CAlphabetLevels "mata::AlphabetLevels":
        CAlphabetLevels(vector[shared_ptr[CAlphabet]], CAlphabetLevelsMode) except +
        CAlphabetLevels(shared_ptr[CAlphabet]) except +
        CAlphabetLevels(CAlphabetLevels&) except +

        bool operator==(CAlphabetLevels&)

        CAlphabetLevelsMode mode()
        void set_global_mode(Level) except +
        void set_global_mode(shared_ptr[CAlphabet])
        void set_multi_level_mode()
        void set_multi_level_mode(vector[shared_ptr[CAlphabet]])

        Symbol translate_symb(string, optional[Level]) except +
        string reverse_translate_symbol(Symbol, optional[Level]) except +
        COrdVector[Symbol] get_alphabet_symbols(optional[Level]) except +
        COrdVector[Symbol] get_complement(COrdVector[Symbol], optional[Level]) except +
        bool empty(optional[Level]) except +
        void clear(optional[Level]) except +

        shared_ptr[CAlphabet]& at(size_t) except +
        size_t size()

        vector[shared_ptr[CAlphabet]].iterator begin()
        vector[shared_ptr[CAlphabet]].iterator end()

        void push_back(shared_ptr[CAlphabet]) except +
        vector[shared_ptr[CAlphabet]].iterator insert(
            vector[shared_ptr[CAlphabet]].iterator, shared_ptr[CAlphabet]
        ) except +
        void pop_back()
        void reserve(size_t)
        void resize(size_t) except +
        vector[shared_ptr[CAlphabet]].iterator erase(vector[shared_ptr[CAlphabet]].iterator)
        vector[shared_ptr[CAlphabet]].iterator erase(
            vector[shared_ptr[CAlphabet]].iterator, vector[shared_ptr[CAlphabet]].iterator
        )


cdef class Alphabet:
    cdef shared_ptr[CAlphabet] _generic_thisptr
    cdef shared_ptr[CAlphabet] as_base(self)
    cdef get_symbols(self)
    cpdef get_alphabet_symbols(self)


cdef class EnumAlphabet(Alphabet):
    cdef shared_ptr[CEnumAlphabet] thisptr
    cpdef get_alphabet_symbols(self)


cdef class AlphabetLevels:
    cdef shared_ptr[CAlphabetLevels] thisptr
    cdef optional[Level] _c_level(self, level)


cdef object wrap_alphabet(shared_ptr[CAlphabet] ptr)
cdef CAlphabet* unwrap_alphabet_or_null(Alphabet alphabet)
