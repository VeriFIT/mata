from libcpp cimport bool
from libcpp.set cimport set
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.list cimport list
from libcpp.string cimport string
from libc.stdint cimport uintptr_t

cdef extern from "vata2/nfa.hh" namespace "Vata2::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef uintptr_t Symbol
    ctypedef set[State] StateSet
    ctypedef umap[StateSet, State] SubsetMap
    ctypedef umap[string, State] StringToSymbolMap

    cdef cppclass CTrans "Vata2::Nfa::Trans":
        # Public Attributes
        State src
        Symbol symb
        State tgt

        # Constructor
        CTrans() except +
        CTrans(State, Symbol, State) except +

        # Public Functions
        bool operator==(CTrans)
        bool operator!=(CTrans)

    cdef cppclass CNfa "Vata2::Nfa::Nfa":
        # Public Attributes
        set[State] initialstates
        set[State] finalstates

        # Constructor
        CNfa() except +

        # Public Functions
        void add_initial(State)
        void add_initial(vector[State])
        bool has_initial(State)
        void add_final(State)
        bool has_final(State)
        void add_trans(CTrans)
        void add_trans(State, Symbol, State)
        bool has_trans(CTrans)
        bool has_trans(State, Symbol, State)
        bool trans_empty()
        size_t trans_size()

    cdef bool is_deterministic(CNfa&)
    cdef void determinize(CNfa*, CNfa&, SubsetMap*, State*)

    # Alphabets
    cdef cppclass CCharAlphabet "Vata2::Nfa::CharAlphabet":
        CCharAlphabet() except +
        Symbol translate_symb(string)

    cdef cppclass CDirectAlphabet "Vata2::Nfa::DirectAlphabet":
        CDirectAlphabet() except +
        Symbol translate_symb(string)

    cdef cppclass CEnumAlphabet "Vata2::Nfa::EnumAlphabet":
        CEnumAlphabet() except +
        CEnumAlphabet(vector[string].iterator, vector[string].iterator) except +
        Symbol translate_symb(string) except +

    cdef cppclass COnTheFlyAlphabet "Vata2::Nfa::OnTheFlyAlphabet":
        COnTheFlyAlphabet(StringToSymbolMap*, Symbol) except +
        Symbol translate_symb(string)
